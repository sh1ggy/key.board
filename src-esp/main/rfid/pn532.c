#include "rfid.h"
#include "../constants.h"
#include "../main.h"
#include <pn532.h>
#include <inttypes.h>

#include <stdlib.h>
#include <esp_log.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

pn532_t *pn532 = NULL;
void setup_rfid_reader()
{
    //Make task for monitoring RFID
    pn532 = pn532_init(1, 4, 11, 12, 0x00);
	ESP_LOGI(TAG, "Starting up");
}