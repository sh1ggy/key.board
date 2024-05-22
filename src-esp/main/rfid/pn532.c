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
#define UART_PORT 1
#define BAUD_SETTING 4 // This is 115200
#define TX_UART_PIN 11
#define RX_UART_PIN 12
#define MAX_RETRIES 15 // We dont want the user to wait longer than 1.5 seconds for a retry

static void rfid_task(void *arg)
{
    ESP_LOGI(TAG, "Task started");
    int retries = 0;

    while (1)
    {
        int gpio_test = pn532_read_GPIO(pn532);

        if (gpio_test < 0)
        {
            ESP_LOGE(TAG, "PN532 error: %s, code: %d, %dth time restarting connection, restarting...", pn532_err_to_name(-gpio_test), gpio_test, retries);
            pn532_end(pn532);
            vTaskDelay(pdMS_TO_TICKS(100));
            pn532 = pn532_init(UART_PORT, BAUD_SETTING, TX_UART_PIN, RX_UART_PIN, 0x00);
            // Apply exponential backoff to the initialisation, and the amoutn of time you wait between then
            while (!pn532)
            {
                ESP_LOGE(TAG, "Failed to init again trying again");
                pn532_end(pn532);
                vTaskDelay(pdMS_TO_TICKS(100));
                pn532 = pn532_init(UART_PORT, BAUD_SETTING, TX_UART_PIN, RX_UART_PIN, 0x00);

                retries++;
                if (retries > MAX_RETRIES)
                {
                    break;
                }
            }

            retries++;
            if (retries > MAX_RETRIES)
            {
                break;
            }

            continue;
        }
        int cards = pn532_Cards(pn532);
        // If the result is negative, this means that there was an error, run it through  err to name and reinit
        if (cards < 0)
        {
            ESP_LOGE(TAG, "Reading cards PN532 error: %s, code: %d", pn532_err_to_name(-cards), cards);
        }

        if (cards >= 0)
        {
            retries = 0;
        }

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
                bool isFound = false;
                for (size_t i = 0; i < rfid_db.total_rfid_tags; i++)
                {
                    if (rfid_db.serial_number_buffer[i] == currently_scanned_tag)
                    {
                        ESP_LOGI(TAG, "Found tag %" PRIu64 " in db", rfid_db.serial_number_buffer[i]);
                        currently_scanned_tag_index = i;
                        state = APP_STATE_SCANNED_CARD;
                        isFound = true;
                        break;
                    }
                }
                if (!isFound)
                {
                    ESP_LOGE(TAG, "Couldnt find Found tag %" PRIu64 " in db", currently_scanned_tag);
                }
            }

            default:
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        // taskYIELD();
    }
    ESP_LOGI(TAG, "Task finished");
    esp_restart();

    vTaskDelete(NULL);
}

void setup_rfid_reader()
{
    // Make task for monitoring RFID
    ESP_LOGI(TAG, "Initing Pn532");
    pn532 = pn532_init(UART_PORT, BAUD_SETTING, TX_UART_PIN, RX_UART_PIN, 0x00);
    if (pn532 == NULL)
    {
        ESP_LOGE(TAG, "Failed to init pn532");
        return;
    }
    // Wait for the device to startup
    vTaskDelay(pdMS_TO_TICKS(100));

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