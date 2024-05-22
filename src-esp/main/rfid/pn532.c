#include "rfid.h"
#include "../constants.h"
#include "../main.h"
#include <pn532.h>
#include <inttypes.h>
#include "../cards.h"

#include <stdlib.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

pn532_t *pn532 = NULL;

#define DEFAULT_TASK_STACK_SIZE (4 * 1024)

typedef struct
{
    rfid_db_t *db;
    APP_STATE *state;
    int *currently_scanned_tag_index;
    pn532_t *pn532;
} task_context_t;

void rfid_task(void *arg)
{
    task_context_t *context = (task_context_t *)arg;

    while (1)
    {
        context->currently_scanned_tag_index++;
        if (context->currently_scanned_tag_index > context->db->total_rfid_tags) {
            context->state = APP_STATE_SCANNER_MODE;
        }
        break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "Task finished");
}


// There are 2 options here, have a loop method that checks for new tags, 
// or use a separate task using semaphores to mutate global state, 
// atomicity is guaranteed on the read for small type so it is ok not to have to acquire on read
// See https://github.com/SensorsIot/Morse-Trainer/blob/4ce1086d543dedf9c7add27b27afcdba25f3759c/MTR_V3/MTR_V3/Initialize.h#L146
// and https://github.com/SensorsIot/Morse-Trainer/blob/4ce1086d543dedf9c7add27b27afcdba25f3759c/MTR_V3/MTR_V3/Coordinator.h#L137

void setup_rfid_reader()
{
    // Make task for monitoring RFID
    // Since this device has only 1 core, taskss will run synchronously anyway, so semaphore is not needed (TO TEST)
    pn532 = pn532_init(1, 4, 11, 12, 0x00);

    TaskHandle_t xHandle = NULL;
    int priority = tskIDLE_PRIORITY; // Or 0

    task_context_t context;
    context.db = &rfid_db;
    context.db->total_rfid_tags = 200;
    context.state = &state;
    context.currently_scanned_tag_index = &currently_scanned_tag_index;
    context.pn532 = pn532;

    xTaskCreate(
        rfid_task,
        "rfid_task",
        DEFAULT_TASK_STACK_SIZE,
        &context,
        priority,
        xHandle);


    if (xHandle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create task");
    }
    else
    {
        ESP_LOGI(TAG, "Task created");
    }

    ESP_LOGI(TAG, "Starting up");
}