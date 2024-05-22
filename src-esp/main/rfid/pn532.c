#include "rfid.h"
#include "../constants.h"
#include "../main.h"
#include "../cards.h"
#include "pn532.h"
#include <inttypes.h>

#include <stdlib.h> /* strtoull */
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

pn532_t *pn532 = NULL;

#define DEFAULT_TASK_STACK_SIZE (4 * 1024)

static void rfid_task(void *arg)
{
    ESP_LOGI(TAG, "Task started");

    while (1)
    {
        //This is more for sanity checking, ideally the wiring is all done correctly
        pn532_err_t err =pn532_lasterr(pn532);
        if (err != PN532_ERR_OK)
        {
            ESP_LOGE(TAG, "PN532 error: %s, code: %d", pn532_err_to_name(err), err);
            vTaskDelay(pdMS_TO_TICKS(1000));
            pn532->lasterr = PN532_ERR_OK;
            //Maybe we should reset the device here, i.e clean and reinit
            continue;
        }
        int cards = pn532_Cards(pn532);
        // vTaskDelay(pdMS_TO_TICKS(20));
        if (cards > 0)
        {
            char id[21];
            pn532_nfcid(pn532, id);
            ESP_LOGI(TAG, "Card ID: %s", id);
            // String to unsigned long long   (str, endptr, base)
            currently_scanned_tag = strtoull(id, NULL, 16);
            ESP_LOGI(TAG, "Tag scanned (sn: %" PRIu64 ")", currently_scanned_tag);

            // Handle state
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
                    if (rfid_db.serial_number_buffer[i] == currently_scanned_tag)
                    {
                        ESP_LOGI(TAG, "Found tag %" PRIu64 " in db", rfid_db.serial_number_buffer[i]);
                        currently_scanned_tag_index = i;
                        state = APP_STATE_SCANNED_CARD;
                        return;
                    }
                }
                ESP_LOGE(TAG, "Couldnt find Found tag %" PRIu64 " in db", currently_scanned_tag);
            }

            default:
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        // taskYIELD();
    }
    ESP_LOGI(TAG, "Task finished");

    vTaskDelete(NULL);
}

void setup_rfid_reader()
{
    // Make task for monitoring RFID
    ESP_LOGI(TAG, "Initing Pn532");
    pn532 = pn532_init(1, 4, 11, 12, 0x00);
    if (pn532 == NULL)
    {
        ESP_LOGE(TAG, "Failed to init pn532");
        return;
    }

    TaskHandle_t xHandle = NULL;
    int priority = tskIDLE_PRIORITY;

    xTaskCreate(
        rfid_task,
        "rfid_task",
        DEFAULT_TASK_STACK_SIZE,
        NULL,
        priority,
        &xHandle);
    if (xHandle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create task");
    }
    else
    {
        ESP_LOGI(TAG, "Task created");
    }
}