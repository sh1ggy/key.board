#include "profile.h"

esp_chip_info_t chip_info;

// TODO : find where this is from
void print_memory_sizes(void)
{

    // uint32_t flash_size = ESP.getFlashChipSize();
    // printf("--------> Flash size: %PRIu32 bytes\n", flash_size);

    // Flash Size
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (partition)
    {
        ESP_LOGI("Memory Info", "Found App partition");
        ESP_LOGI("Memory Info", "Partition Label: %s", partition->label);
        ESP_LOGI("Memory Info", "Partition Type: %d", partition->type);
        ESP_LOGI("Memory Info", "Partition Subtype: %d", partition->subtype);
        ESP_LOGI("Memory Info", "Partition Size: %" PRIu32 " bytes", partition->size);
    }
    else
    {
        ESP_LOGE("Memory Info", "Failed to get the App partition");
    }

    // Total SPIRAM (PSRAM) Size
    size_t spiram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (spiram_size)
    {
        ESP_LOGI("Memory Info", "PSRAM Size: %zu bytes", spiram_size);
    }
    else
    {
        ESP_LOGI("Memory Info", "No PSRAM detected");
    }

    uint32_t total_internal_memory = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    uint32_t free_internal_memory = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t largest_contig_internal_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

    ESP_LOGI("Memory Info", "Total DRAM (internal memory): %" PRIu32 " bytes", total_internal_memory);
    ESP_LOGI("Memory Info", "Free DRAM (internal memory): %" PRIu32 " bytes", free_internal_memory);
    ESP_LOGI("Memory Info", "Largest free contiguous DRAM block: %" PRIu32 " bytes", largest_contig_internal_block);
}

void print_system_info_task(void *pvParameters)
{
    while (1)
    {
        size_t free_memory = esp_get_free_heap_size();
        printf("Free Memory: %d bytes\n", free_memory);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

