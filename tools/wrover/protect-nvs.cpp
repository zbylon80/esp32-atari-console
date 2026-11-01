#include <esp_partition.h>
#include <esp_err.h>
#include <rom/ets_sys.h>
// Arduino startup can try to erase all NVS on a version/full-page error.
// Fail that operation instead; page-level normal NVS maintenance remains valid.
extern "C" esp_err_t __real_esp_partition_erase_range(const esp_partition_t*, size_t, size_t);
extern "C" esp_err_t __wrap_esp_partition_erase_range(const esp_partition_t* part, size_t off, size_t size) {
    if (part && part->type==ESP_PARTITION_TYPE_DATA && part->subtype==ESP_PARTITION_SUBTYPE_DATA_NVS
        && off==0 && size==part->size) {
        ets_printf("[NVS_GUARD] blocked automatic whole-NVS erase\n");
        return ESP_ERR_INVALID_STATE;
    }
    return __real_esp_partition_erase_range(part,off,size);
}
