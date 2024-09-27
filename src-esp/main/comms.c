#include "comms.h"
#include "constants.h"
#include "keyboard.h"

#include "esp_log.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"

#define MAX_CDC_BUF_SIZE 4096 // 2^12, 4KB

static uint8_t cdc_buf[MAX_CDC_BUF_SIZE];
static size_t current_cdc_payload_size = 0;
static size_t currently_written_size = 0;
static uint32_t runs = 0;

bool write_to_cdc_loop()
{
    if (currently_written_size == current_cdc_payload_size)
    {
        ESP_LOGI(TAG, "Finished writing buffer, written=%zu", current_cdc_payload_size);
        return true;
    }
    size_t to_write = current_cdc_payload_size - currently_written_size;

    uint8_t *write_ptr = cdc_buf + currently_written_size;

    // TODO : if we need to use raw tinyusb api, check we are allowed to write to buffer
    //  uint32_t write_size = tud_cdc_write(write_ptr, to_write);

    size_t write_size = tinyusb_cdcacm_write_queue(ITF_NUM_CDC, write_ptr, to_write);
    ESP_LOGI(TAG, "Sending cdc: to write=%zu, written=%zu", to_write, write_size);
    currently_written_size += write_size;
    return false;
}

void init_write_to_cdc(char *payload)
{
    current_cdc_payload_size = strlen(payload);
    runs = 0;
    currently_written_size = 0;

    ESP_LOGI(TAG, "Sending JSON: %s, size: %zu", payload, current_cdc_payload_size);

    if (current_cdc_payload_size > MAX_CDC_BUF_SIZE)
    {
        ESP_LOGE("CDC", "Payload too large");
        return;
    }
    memcpy(cdc_buf, payload, current_cdc_payload_size);
}