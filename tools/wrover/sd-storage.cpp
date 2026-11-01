#include <Arduino.h>
#include <driver/spi_master.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>
#include "sd-storage.h"
static sdmmc_card_t* card;
static esp_err_t lastError=ESP_OK;
bool console_sd_no_response(){return lastError==ESP_ERR_TIMEOUT||lastError==ESP_ERR_INVALID_RESPONSE;}
bool console_sd_mount(int clockKhz) {
    if(card) return true;
    spi_bus_config_t bus = {};
    bus.mosi_io_num=23;bus.miso_io_num=19;bus.sclk_io_num=22;
    bus.quadwp_io_num=bus.quadhd_io_num=-1;bus.max_transfer_sz=4096;
    esp_err_t e=spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO);
    lastError=e;
    if(e!=ESP_OK){Serial.printf("[SD_MOUNT] bus=%s\n",esp_err_to_name(e));return false;}
    sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=SPI3_HOST;host.max_freq_khz=clockKhz;
    sdspi_device_config_t device=SDSPI_DEVICE_CONFIG_DEFAULT();
    device.host_id=SPI3_HOST;device.gpio_cs=GPIO_NUM_21;
    esp_vfs_fat_mount_config_t config={};
    config.format_if_mount_failed=false;config.max_files=3;
    e=esp_vfs_fat_sdspi_mount("/sdcard",&host,&device,&config,&card);
    lastError=e;
    Serial.printf("[SD_MOUNT] result=%s format=0 clock_khz=%d\n",esp_err_to_name(e),clockKhz);
    if(e!=ESP_OK){card=nullptr;spi_bus_free(SPI3_HOST);return false;}
    return true;
}
void console_sd_unmount() {
    if(!card)return;
    esp_vfs_fat_sdcard_unmount("/sdcard",card);card=nullptr;
    spi_bus_free(SPI3_HOST);
    Serial.println("[SD_UNMOUNT] bus_released=1");
}
