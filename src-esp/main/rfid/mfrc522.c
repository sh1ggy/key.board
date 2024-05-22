/*
 * IMPLEMENTATION FOR RFID USING THE MFRC522 CHIP 
 * https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf
 */


#include "rfid.h"
#include "../constants.h"
#include "rc522.h"
#include "../cards.h"
#include "../main.h"
#include <esp_log.h>

#include "class/hid/hid_device.h"
#include "driver/gpio.h"

static void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_event_data_t *data = (rc522_event_data_t *)event_data;

    switch (event_id)
    {
    case RC522_EVENT_TAG_SCANNED:
    {
        rc522_tag_t *tag = (rc522_tag_t *)data->ptr;
        ESP_LOGI(TAG, "Tag scanned (sn: %" PRIu64 ")", tag->serial_number);

        currently_scanned_tag = tag->serial_number;

        switch (state)
        {
        case APP_STATE_SCANNER_MODE:
        {
            state = APP_STATE_SEND_RFID;
            break;
        }
        case APP_STATE_MASTER_MODE:
        {
            // Check if the tag is in the database
            for (size_t i = 0; i < rfid_db.total_rfid_tags; i++)
            {
                if (rfid_db.serial_number_buffer[i] == tag->serial_number)
                {
                    currently_scanned_tag_index = i;
                    ESP_LOGI(TAG, "Found tag %" PRIu64 " in db", rfid_db.serial_number_buffer[i]);
                    state = APP_STATE_SCANNED_CARD;
                    return;
                }
            }
            ESP_LOGE(TAG, "Couldnt find Found tag %" PRIu64 " in db", tag->serial_number);
        }

        default:
            break;
        }
    }
    break;
    }
}

rc522_handle_t scanner;
void setup_rfid_reader()
{

    rc522_config_t config = {
        .spi.host = SPI3_HOST,
        .spi.miso_gpio = GPIO_NUM_11,
        .spi.mosi_gpio = GPIO_NUM_9,
        .spi.sck_gpio = GPIO_NUM_7,
        .spi.sda_gpio = GPIO_NUM_5,
    };
    // TODO: Add error handling
    rc522_create(&config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_ANY, rc522_handler, NULL);
    // Dont need to pause the scanner in whatever mode we operate under
    rc522_start(scanner);
}