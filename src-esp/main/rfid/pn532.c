#include "rfid.h"
#include "../constants.h"
#include "../main.h"
#include "../cards.h"
#include <pn532.h>
#include <inttypes.h>

#include <stdlib.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

pn532_t *pn532 = NULL;

#define DEFAULT_TASK_STACK_SIZE (4 * 1024)

// Static here makes this function not accessible by other compilation units
static void rfid_task(void *arg)
{
    // task_context_t *context = (task_context_t *)arg;
    ESP_LOGI(TAG, "Task started");

    while (1)
    {
        int cards = pn532_Cards(pn532);
        vTaskDelay(pdMS_TO_TICKS(20));
        if (cards > 0)
        {
            char id[21];
            pn532_nfcid(pn532, id);
            ESP_LOGI(TAG, "Card ID: %s", id);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Task finished");

    vTaskDelete(NULL);
}

void setup_rfid_reader()
{
    // Make task for monitoring RFID
    // Since this device has only 1 core, taskss will run synchronously anyway, so semaphore is not needed (TO TEST)

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