#include "rfid.h"
#include <string>
#include <iostream>

#include "rfid.h"
#include "../constants.h"
#include "../main.h"
#include "../cards.h"
#include <inttypes.h>

#include <stdlib.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEFAULT_TASK_STACK_SIZE (4 * 1024)

// typedef struct
// {
//     RFID_DB_t *db;
//     APP_STATE *state;
//     int *currently_scanned_tag_index;
//     pn532_t *pn532;
// } task_context_t;

//Static here makes this function not accessible by other compilation units
void rfid_task(void *arg)
{
    // task_context_t *context = (task_context_t *)arg;
    ESP_LOGI(TAG, "Task started");

    while (1)
    {
        // ESP_LOGI(TAG, "Incrementing");
        currently_scanned_tag_index += 1;
        if (currently_scanned_tag_index > rfid_db.total_rfid_tags)
        {
            ESP_LOGI(TAG, "Reached end state");
            state = APP_STATE_SCANNER_MODE;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGI(TAG, "Task finished");

    vTaskDelete(NULL);
}

// There are 2 options here, have a loop method that checks for new tags,
// or use a separate task using semaphores to mutate global state,
// atomicity is guaranteed on the read for small type so it is ok not to have to acquire on read
// See https://github.com/SensorsIot/Morse-Trainer/blob/4ce1086d543dedf9c7add27b27afcdba25f3759c/MTR_V3/MTR_V3/Initialize.h#L146
// and https://github.com/SensorsIot/Morse-Trainer/blob/4ce1086d543dedf9c7add27b27afcdba25f3759c/MTR_V3/MTR_V3/Coordinator.h#L137

std::string test_str;
void setup_rfid_reader()
{
    test_str = "Mock tester";

    // Make task for monitoring RFID
    // Since this device has only 1 core, taskss will run synchronously anyway, so semaphore is not needed (TO TEST)
    TaskHandle_t xHandle = NULL;
    int priority = tskIDLE_PRIORITY; // Or 0
    std::cout << "Running setup for " << test_str << "with task priority: " << priority << '\n';

    // Has to be malloced because otherwise stack mem dies at the end of stack frame
    //  task_context_t context;
    //  context.db = &rfid_db;
    //  context.db->total_rfid_tags = 200;
    //  context.state = &state;
    //  context.currently_scanned_tag_index = &currently_scanned_tag_index;
    //  context.pn532 = pn532;

    rfid_db.total_rfid_tags = 200;

    xTaskCreate(
        rfid_task,
        "rfid_task",
        DEFAULT_TASK_STACK_SIZE,
        // &context,
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
