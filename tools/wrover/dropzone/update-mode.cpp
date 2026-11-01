#include <Arduino.h>
#include <esp_ota_ops.h>
#include <nvs.h>
#include "dropzone.h"

void drop_update_info(){
    const auto* running=esp_ota_get_running_partition();
    if(running)Serial.printf("[RUNNING] partition=%s offset=0x%x\n",running->label,unsigned(running->address));
}
bool drop_update_mode() {
    const auto* running=esp_ota_get_running_partition();
    const auto* updater=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_OTA_0,"updater");
    if(!running || !updater){Serial.println("[UPDATE_ERROR] updater partition unavailable");return false;}
    nvs_handle_t settings;
    esp_err_t error=nvs_open("atari_update",NVS_READWRITE,&settings);
    if(error==ESP_OK){
        error=nvs_set_u32(settings,"active",running->address);
        if(error==ESP_OK)error=nvs_commit(settings);
        nvs_close(settings);
    }
    if(error==ESP_OK)error=esp_ota_set_boot_partition(updater);
    if(error!=ESP_OK){Serial.printf("[UPDATE_ERROR] enter=%s\n",esp_err_to_name(error));return false;}
    Serial.println("[UPDATE_MODE] reboot to separate Wi-Fi updater; game stops");
    Serial.flush();delay(100);ESP.restart();return true;
}
