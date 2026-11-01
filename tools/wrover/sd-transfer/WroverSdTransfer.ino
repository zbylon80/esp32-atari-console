#include <Arduino.h>
#include <esp_ota_ops.h>
#include <nvs.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mbedtls/sha256.h>
#include "sd-storage.h"
#include "transfer-catalog.h"
static uint8_t buffer[4096];
static uint32_t lastCommand;
static bool mounted;
static bool digest(const char* path,const Media& item) {
    FILE* f=fopen(path,"rb");if(!f)return false;
    mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts_ret(&sha,0);
    size_t total=0,n;
    while((n=fread(buffer,1,sizeof(buffer),f))){total+=n;mbedtls_sha256_update_ret(&sha,buffer,n);}
    bool ok=!ferror(f)&&total==item.bytes;fclose(f);
    uint8_t hash[32];char hex[65];mbedtls_sha256_finish_ret(&sha,hash);mbedtls_sha256_free(&sha);
    for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",hash[i]);
    return ok&&!strcmp(hex,item.sha);
}
static void put(int index) {
    if(!mounted||index<0||index>=media_count){Serial.println("[COPY_ERROR] invalid request");return;}
    const auto& item=media[index];char path[192],temp[208];
    snprintf(path,sizeof(path),"%s/%s",console_sd_root,item.file);
    struct stat st;
    if(stat(path,&st)==0){
        Serial.printf(digest(path,item)?"[COPY_SKIP] %d verified\n":"[COPY_ERROR] %d existing_file_preserved\n",index);return;
    }
    if(errno!=ENOENT){Serial.println("[COPY_ERROR] stat");return;}
    snprintf(temp,sizeof(temp),"%s/.copy-%08x.tmp",console_sd_root,unsigned(esp_random()));
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0){Serial.println("[COPY_ERROR] create_temp");return;}
    Serial.printf("[COPY_READY] %d %u\n",index,unsigned(item.bytes));
    size_t total=0;bool ok=true;
    while(total<item.bytes){
        size_t count=min(sizeof(buffer),item.bytes-total);
        size_t received=Serial.readBytes(buffer,count);
        if(received!=count||write(fd,buffer,count)!=int(count)){ok=false;break;}
        total+=count;Serial.printf("[COPY_ACK] %u\n",unsigned(total));
    }
    if(fsync(fd)!=0)ok=false;
    if(close(fd)!=0)ok=false;
    if(ok)ok=digest(temp,item);
    // No other writer runs in this image. Refuse to replace any existing file.
    if(ok){errno=0;if(stat(path,&st)==0||errno!=ENOENT)ok=false;}
    if(ok)ok=rename(temp,path)==0;
    if(!ok)unlink(temp); // Only the temporary file created by this request.
    Serial.printf(ok?"[COPY_OK] %d sha256_verified=1\n":"[COPY_ERROR] %d transfer_failed\n",index);
}
static void updater(){
    console_sd_unmount();
    const auto* running=esp_ota_get_running_partition();
    const auto* target=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,"updater");
    if(!running||!target)return;
    nvs_handle_t n;esp_err_t e=nvs_open("atari_update",NVS_READWRITE,&n);
    if(e==ESP_OK){e=nvs_set_u32(n,"active",running->address);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
    if(e==ESP_OK)e=esp_ota_set_boot_partition(target);
    if(e==ESP_OK){Serial.println("[COPY_RETURN] updater");Serial.flush();delay(100);ESP.restart();}
}
void setup(){
    Serial.begin(115200);Serial.setTimeout(15000);delay(700);lastCommand=millis();
    Serial.println("[BOOT] WroverSdTransfer-v1 ATARI-WROVER-GAME-OTA-v1");
    mounted=console_sd_mount();
    if(mounted){
        const char* dirs[]={"/sdcard/atari-console",console_sd_root};
        for(auto path:dirs){struct stat st;if(stat(path,&st)==0){if(!S_ISDIR(st.st_mode))mounted=false;}
            else if(errno!=ENOENT||mkdir(path,0700)!=0)mounted=false;}
    }
    Serial.printf("[COPY_SERVICE] mounted=%d files=%d no_format=1 no_overwrite=1\n",mounted,media_count);
}
void loop(){
    if(Serial.available()){
        char line[40];size_t n=Serial.readBytesUntil('\n',line,sizeof(line)-1);line[n]=0;lastCommand=millis();
        int index;
        if(!strcmp(line,"u"))updater();
        else if(sscanf(line,"PUT %d",&index)==1)put(index);
        else if(!strcmp(line,"VERIFY")){
            for(int i=0;i<media_count;i++){char path[192];snprintf(path,sizeof(path),"%s/%s",console_sd_root,media[i].file);
                Serial.printf("[COPY_VERIFY] %d %d\n",i,mounted&&digest(path,media[i]));}
            Serial.println("[COPY_VERIFIED]");
        }
    }
    if(millis()-lastCommand>600000)updater();
    delay(2);
}
