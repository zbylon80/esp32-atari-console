#pragma once
#include <cstddef>
using nvs_handle_t=int;
using esp_err_t=int;
constexpr int ESP_OK=0,NVS_READONLY=0,NVS_READWRITE=1;
int nvs_open(const char*,int,nvs_handle_t*);
int nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
int nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
inline const char* esp_err_to_name(int){return "fake NVS error";}
