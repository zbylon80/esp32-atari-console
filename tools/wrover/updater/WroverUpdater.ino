#include <Arduino.h>
#include <atomic>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <nvs.h>
#include <mbedtls/sha256.h>
#include "private-network.h"
#include "updater-screen.h"
#include "perf-report.h"

static const esp_partition_t* active;
static httpd_handle_t server;
static std::atomic<bool> rebootRequested{false};
static uint32_t rebootAt,serviceStarted;
static constexpr const char* marker="ATARI-WROVER-GAME-OTA-v1";
static bool allowed(const esp_partition_t* p) {
    return p && ((p->address==0x10000 && p->subtype==ESP_PARTITION_SUBTYPE_APP_FACTORY)
        || (p->address==0x410000 && p->subtype==ESP_PARTITION_SUBTYPE_APP_OTA_1)
        || (p->address==0x610000 && p->subtype==ESP_PARTITION_SUBTYPE_APP_OTA_2));
}
static esp_err_t saveActive(uint32_t address) {
    nvs_handle_t n;esp_err_t e=nvs_open("atari_update",NVS_READWRITE,&n);
    if(e==ESP_OK){e=nvs_set_u32(n,"active",address);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}return e;
}
static bool authorized(httpd_req_t* request) {
    char supplied[96]={};
    if(httpd_req_get_hdr_value_str(request,"Authorization",supplied,sizeof(supplied))!=ESP_OK)return false;
    const char expected[]="Bearer " OTA_TOKEN;
    unsigned diff=strlen(supplied)^strlen(expected);
    for(size_t i=0;i<sizeof(expected)-1;i++)diff|=uint8_t(supplied[i])^uint8_t(expected[i]);
    return diff==0;
}
static esp_err_t deny(httpd_req_t* r){return httpd_resp_send_err(r,HTTPD_401_UNAUTHORIZED,"Authentication required");}
static esp_err_t statusHandler(httpd_req_t* r) {
    char out[1200];int used=snprintf(out,sizeof(out),"{\"service\":\"WroverUpdater-v2\",\"active\":\"%s\",\"active_address\":%u,\"heap\":%u,\"up_ms\":%u,\"perf\":",
        active->label,unsigned(active->address),ESP.getFreeHeap(),unsigned(millis()));
    PerfReport report{};size_t size=sizeof(report);nvs_handle_t n;
    esp_err_t result=nvs_open("atari_perf",NVS_READONLY,&n);
    if(result==ESP_OK){result=nvs_get_blob(n,"result",&report,&size);nvs_close(n);}
    if(result==ESP_OK && size==sizeof(report) && report.magic==perfMagic){
        snprintf(out+used,sizeof(out)-used,"{\"variant\":\"%.31s\",\"request\":%u,\"frames\":%u,\"elapsed_ms\":%u,\"fps_x100\":%u,\"core_us\":%u,\"copy_us\":%u,\"audio_us\":%u,\"guards\":%u,\"connected\":%u,\"frame_hash\":%u,\"ram_hash\":%u,\"pc\":%u,\"a\":%u,\"x\":%u,\"y\":%u,\"s\":%u,\"p\":%u,\"internal_free\":%u}}",
            report.variant,report.request,report.frames,report.elapsedMs,report.elapsedMs?unsigned(uint64_t(report.frames)*100000/report.elapsedMs):0,
            report.frames?report.coreUs/report.frames:0,report.frames?report.copyUs/report.frames:0,report.frames?report.audioUs/report.frames:0,
            report.guards,report.connected,report.frameHash,report.ramHash,report.pc,report.a,report.x,report.y,report.s,report.p,report.internalFree);
    }else snprintf(out+used,sizeof(out)-used,"null}");
    httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,out);
}
static esp_err_t benchmarkHandler(httpd_req_t* r){
    if(!authorized(r))return deny(r);
    uint32_t request=esp_random();if(!request)request=1;
    nvs_handle_t n;esp_err_t error=nvs_open("atari_perf",NVS_READWRITE,&n);
    if(error==ESP_OK){error=nvs_set_u32(n,"request",request);if(error==ESP_OK)error=nvs_commit(n);nvs_close(n);}
    if(error!=ESP_OK)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot queue benchmark");
    char out[64];snprintf(out,sizeof(out),"{\"request\":%u}",request);
    rebootAt=millis()+350;rebootRequested.store(true);return httpd_resp_sendstr(r,out);
}
static esp_err_t returnHandler(httpd_req_t* r) {
    if(!authorized(r))return deny(r);
    rebootAt=millis()+350;rebootRequested.store(true);return httpd_resp_sendstr(r,"Returning to game");
}
static esp_err_t updateHandler(httpd_req_t* r) {
    if(!authorized(r))return deny(r);
    char expected[65]={};
    if(httpd_req_get_hdr_value_str(r,"X-Image-SHA256",expected,sizeof(expected))!=ESP_OK || strlen(expected)!=64)
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"SHA256 required");
    for(char c:expected)if(c && !((c>='0'&&c<='9')||(c>='a'&&c<='f')))
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid SHA256");
    const char* label=active->address==0x410000?"game_b":"game_a";
    const auto* target=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,label);
    if(!allowed(target) || target==active || r->content_len<1024 || r->content_len>target->size)
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid image size/target");
    esp_ota_handle_t handle=0;
    esp_err_t result=esp_ota_begin(target,r->content_len,&handle);
    if(result!=ESP_OK)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot begin OTA");
    updater_message("WGRYWANIE...","Nie odlaczaj zasilania");
    Serial.printf("[OTA_BEGIN] target=%s bytes=%u\n",label,unsigned(r->content_len));
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts_ret(&sha,0);
    uint8_t buffer[1024],digest[32];char window[sizeof(buffer)+64]={};size_t tail=0,received=0;
    bool compatible=false;int timeouts=0;
    while(received<r->content_len){
        int n=httpd_req_recv(r,(char*)buffer,min(sizeof(buffer),r->content_len-received));
        if(n==HTTPD_SOCK_ERR_TIMEOUT && ++timeouts<4)continue;
        if(n<=0){result=ESP_FAIL;break;}
        timeouts=0;
        if(received==0 && (buffer[0]!=0xe9 || (n>=14 && (buffer[12] || buffer[13])))){
            result=ESP_ERR_INVALID_ARG;break;
        }
        mbedtls_sha256_update_ret(&sha,buffer,n);
        memcpy(window+tail,buffer,n);
        size_t count=tail+n;
        for(size_t i=0;i+strlen(marker)<=count;i++)if(!memcmp(window+i,marker,strlen(marker)))compatible=true;
        tail=min(count,strlen(marker)-1);memmove(window,window+count-tail,tail);
        result=esp_ota_write(handle,buffer,n);if(result!=ESP_OK)break;
        received+=n;
    }
    mbedtls_sha256_finish_ret(&sha,digest);mbedtls_sha256_free(&sha);
    char actual[65];for(int i=0;i<32;i++)snprintf(actual+i*2,3,"%02x",digest[i]);
    if(result!=ESP_OK || received!=r->content_len || !compatible || strcmp(expected,actual)){
        esp_ota_abort(handle);updater_message("BLAD AKTUALIZACJI","Poprzednia gra zachowana");
        Serial.println("[OTA_REJECT] transfer/hash/compatibility failure; active unchanged");
        return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Transfer/hash/compatibility failure");
    }
    result=esp_ota_end(handle);
    if(result==ESP_OK)result=saveActive(target->address);
    if(result==ESP_OK)result=esp_ota_set_boot_partition(target);
    if(result!=ESP_OK){
        saveActive(active->address);esp_ota_set_boot_partition(active);
        updater_message("BLAD AKTUALIZACJI","Poprzednia gra zachowana");
        return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Image validation/activation failed");
    }
    Serial.printf("[OTA_OK] target=%s bytes=%u sha256=%s\n",label,unsigned(received),actual);
    updater_message("AKTUALIZACJA GOTOWA","Uruchamiam gre...");
    rebootAt=millis()+500;rebootRequested.store(true);
    httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,"{\"result\":\"OK\"}");
}
void setup(){
    Serial.begin(115200);delay(500);setCpuFrequencyMhz(240);
    Serial.println("[BOOT] WroverUpdater-v2; separate updater; game/Bluetooth not linked");
    uint32_t address=0x10000;nvs_handle_t n;
    if(nvs_open("atari_update",NVS_READONLY,&n)==ESP_OK){nvs_get_u32(n,"active",&address);nvs_close(n);}
    for(auto label:{"factory","game_a","game_b"}){
        const auto* p=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,label);
        if(allowed(p)&&p->address==address)active=p;
    }
    esp_app_desc_t info;
    if(!active || esp_ota_get_partition_description(active,&info)!=ESP_OK){
        active=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY,"factory");
    }
    if(!allowed(active) || esp_ota_set_boot_partition(active)!=ESP_OK){
        active=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY,"factory");
        if(!allowed(active) || esp_ota_set_boot_partition(active)!=ESP_OK){Serial.println("[FAIL] no return game");return;}
    }
    // A power cycle or an interrupted transfer always boots the previous game.
    updater_screen_begin();updater_message("AKTUALIZACJA WI-FI","Laczenie z siecia...");
    WiFi.persistent(false);WiFi.mode(WIFI_STA);WiFi.setHostname("atari-console");WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
    uint32_t start=millis();while(WiFi.status()!=WL_CONNECTED && millis()-start<25000)delay(100);
    if(WiFi.status()!=WL_CONNECTED){
        Serial.println("[UPDATE_ERROR] Wi-Fi unavailable; restarting to game");
        updater_message("BRAK SIECI WI-FI","Wracam do gry...");delay(2000);ESP.restart();return;
    }
    MDNS.begin("atari-console");MDNS.addService("http","tcp",80);
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.stack_size=8192;config.lru_purge_enable=true;
    config.recv_wait_timeout=5;config.send_wait_timeout=5;
    if(httpd_start(&server,&config)!=ESP_OK){Serial.println("[FAIL] HTTP startup");delay(1000);ESP.restart();return;}
    httpd_uri_t status={};status.uri="/status";status.method=HTTP_GET;status.handler=statusHandler;httpd_register_uri_handler(server,&status);
    httpd_uri_t update={};update.uri="/update";update.method=HTTP_POST;update.handler=updateHandler;httpd_register_uri_handler(server,&update);
    httpd_uri_t leave={};leave.uri="/return";leave.method=HTTP_POST;leave.handler=returnHandler;httpd_register_uri_handler(server,&leave);
    httpd_uri_t bench={};bench.uri="/benchmark";bench.method=HTTP_POST;bench.handler=benchmarkHandler;httpd_register_uri_handler(server,&bench);
    auto ip=WiFi.localIP().toString();updater_message("GOTOWY DO AKTUALIZACJI",ip.c_str());
    Serial.printf("[UPDATE_READY] ip=%s active=%s heap=%u internal=%u\n",ip.c_str(),active->label,ESP.getFreeHeap(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    serviceStarted=millis();
}
void loop(){
    if((rebootRequested.load()&&int32_t(millis()-rebootAt)>=0) || (serviceStarted&&millis()-serviceStarted>600000))ESP.restart();
    delay(20);
}
