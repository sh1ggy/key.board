/*
 * SPDX-FileCopyrightText: 2020-2024 Kongi Systems Brisbane, Australia
 *
 */

#include "keyboard.h"
#include "constants.h"
#include "cards.h"
#include "comms.h"
#include "main.h"
#include "rfid/rfid.h"
#include "util/profile.h"
#include "led/keydot_led.h"

#include <inttypes.h>

#include <stdlib.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "tinyusb.h"

#ifdef LOG_DETAILS
#include "util/profile.h"
#endif

#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#include "sdkconfig.h"

#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "cJSON.h"

// https://wokwi.com/projects/395704595737846785

/********* Application ***************/

typedef enum
{
    MOUSE_DIR_RIGHT,
    MOUSE_DIR_DOWN,
    MOUSE_DIR_LEFT,
    MOUSE_DIR_UP,
    MOUSE_DIR_MAX,
} mouse_dir_t;

#define DISTANCE_MAX 125
#define DELTA_SCALAR 5

static void mouse_draw_square_next_delta(int8_t *delta_x_ret, int8_t *delta_y_ret)
{
    static mouse_dir_t cur_dir = MOUSE_DIR_RIGHT;
    static uint32_t distance = 0;

    // Calculate next delta
    if (cur_dir == MOUSE_DIR_RIGHT)
    {
        *delta_x_ret = DELTA_SCALAR;
        *delta_y_ret = 0;
    }
    else if (cur_dir == MOUSE_DIR_DOWN)
    {
        *delta_x_ret = 0;
        *delta_y_ret = DELTA_SCALAR;
    }
    else if (cur_dir == MOUSE_DIR_LEFT)
    {
        *delta_x_ret = -DELTA_SCALAR;
        *delta_y_ret = 0;
    }
    else if (cur_dir == MOUSE_DIR_UP)
    {
        *delta_x_ret = 0;
        *delta_y_ret = -DELTA_SCALAR;
    }

    // Update cumulative distance for current direction
    distance += DELTA_SCALAR;
    // Check if we need to change direction
    if (distance >= DISTANCE_MAX)
    {
        distance = 0;
        cur_dir++;
        if (cur_dir == MOUSE_DIR_MAX)
        {
            cur_dir = 0;
        }
    }
}

static void app_send_hid_demo(void)
{
    // Keyboard output: Send key 'a/A' pressed and released
    ESP_LOGI(TAG, "Sending Keyboard report");
    // This syntax only initializes the first element of the array
    uint8_t keycode[6] = {HID_KEY_A};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
    vTaskDelay(pdMS_TO_TICKS(50));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

    // Mouse output: Move mouse cursor in square trajectory
    ESP_LOGI(TAG, "Sending Mouse report");
    int8_t delta_x;
    int8_t delta_y;
    for (int i = 0; i < (DISTANCE_MAX / DELTA_SCALAR) * 4; i++)
    {
        // Get the next x and y delta in the draw square pattern
        mouse_draw_square_next_delta(&delta_x, &delta_y);
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, delta_x, delta_y, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "Sending BEANS");
    uint8_t msg[6] = "beans";
    // This sends nothing
    tud_hid_report(HID_ITF_PROTOCOL_NONE, &msg, 6);
}

/*******KEYDOTBOARD APP ******/

typedef enum
{
    REQUEST_TYPE_GET_PASSWORD_DESCS,
    REQUEST_TYPE_NEW_CARD,
    REQUEST_TYPE_CLEAR_CARD,
    REQUEST_TYPE_CLEAR_DB,
    REQUEST_TYPE_MAX
} REQUEST_TYPE;

static const char *REQUEST_TYPE_STR[REQUEST_TYPE_MAX] =
    {
        [REQUEST_TYPE_GET_PASSWORD_DESCS] = "get_pass_descs",
        [REQUEST_TYPE_NEW_CARD] = "send_new_card",
        [REQUEST_TYPE_CLEAR_CARD] = "clear_card",
        [REQUEST_TYPE_CLEAR_DB] = "clear_db",
};

typedef enum
{
    RESPONSE_TYPE_GET_PASSWORD_DESCS,
    RESPONSE_TYPE_DETECTED_RFID,
    RESPONSE_TYPE_NEW_CARD,
    RESPONSE_TYPE_CLEAR_CARD,
    RESPONSE_TYPE_MAX

} RESPONSE_TYPE;

static const char *RESPONSE_TYPE_STR[RESPONSE_TYPE_MAX] =
    {
        [RESPONSE_TYPE_GET_PASSWORD_DESCS] = "get_pass_descs",
        [RESPONSE_TYPE_DETECTED_RFID] = "detected_rfid",
        [RESPONSE_TYPE_NEW_CARD] = "send_new_card",
};

APP_STATE state = APP_STATE_BOOT;
nvs_handle_t storage_handle;
RFID_DB_t rfid_db;

uint64_t currently_scanned_tag = 0;
NEW_CARD_t current_new_card;
int currently_scanned_tag_index = -1;
int current_clear_card_index;

esp_err_t get_pass_from_id(size_t in_selected_id, char *out_pass)
{
    // key cannot be longer than 15
    char password_key[16];
    sprintf(password_key, "pass%zu", in_selected_id);
    printf("Passkey: %s\n", password_key);

    // https://github.com/espressif/esp-idf/blob/v5.2.1/examples/storage/nvsgen/main/nvsgen_example_main.c
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/storage/nvs_flash.html#_CPPv411nvs_get_str12nvs_handle_tPKcPcP6size_t
    size_t str_len = MAX_PASS_SIZE;
    esp_err_t err = nvs_get_str(storage_handle, password_key, out_pass, &str_len);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Password for key '%s' not found in NVS\n", password_key);
        return err;
    }
    if (err != ESP_OK)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        return err;
    }

    ESP_LOGI(TAG, "Password value:%s, length:%d", out_pass, str_len);

    return ESP_OK;
}

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/size.html#idf-py-size
// This also helps https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/mem_alloc.html#_CPPv425heap_caps_print_heap_info8uint32_t

#define JSON_PAYLOAD_TIMEOUT_US (100 * 1000)

static uint8_t json_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
static uint8_t *json_buf_ptr = json_buf;
static int64_t last_payload_time_us = 0;

// Keep in mind fs means normal speed lol
#define FULL_SPEED_RX_BITRATE 64

static uint8_t buf[FULL_SPEED_RX_BITRATE + 1];
void handle_request(cJSON *root);
// Itf stands for interface, it is the number that is specified in the configuration
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Data from channel %d, length %d:", itf, rx_size);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buf, rx_size, ESP_LOG_INFO);
    }
    else
    {
        ESP_LOGE(TAG, "Read error");
    }

    /***** Buffering JSON *****/
    // The limit of the read is 64 bytes over USB

    // If the time between the last payload and the current payload is greater than the timeout, reset the buffer
    int64_t time_us = esp_timer_get_time();
    if (time_us - last_payload_time_us > JSON_PAYLOAD_TIMEOUT_US)
    {
        json_buf_ptr = json_buf;
    }

    memcpy(json_buf_ptr, buf, rx_size);
    json_buf_ptr += rx_size;
    // TODO: This is a really inefficient way of checking the payload is all here, check instead for newline as last char
    cJSON *root = cJSON_Parse((char *)json_buf);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "This is NOT valid json: %s", json_buf);
    }
    else
    {
        // p here prints in hex
        ESP_LOGI(TAG, "valid json: %s, this was the last char %p", json_buf, (void *)*(json_buf_ptr - 1));
        handle_request(root);
        // MAKE SURE WE RESET THE BUFFER AFTER WE HAVE HANDLED THE REQUEST
        json_buf_ptr = json_buf;
    }
    cJSON_Delete(root);
    last_payload_time_us = time_us;
}

void handle_request(cJSON *root)
{
    cJSON *request_type_json = cJSON_GetObjectItem(root, "request_type");
    if (request_type_json == NULL)
    {
        ESP_LOGE(TAG, "Request type not found in request");
        return;
    }

    char *request_type = request_type_json->valuestring;

    ESP_LOGI(TAG, "request_type=%s", request_type);

    // Should be able to do this regardless of what state we are in
    if (strcmp(request_type, REQUEST_TYPE_STR[REQUEST_TYPE_GET_PASSWORD_DESCS]) == 0)
    {
        state = APP_STATE_SEND_PASSWORD_DB;
    }
    else if (strcmp(request_type, REQUEST_TYPE_STR[REQUEST_TYPE_NEW_CARD]) == 0)
    {
        if (currently_scanned_tag == 0)
        {
            ESP_LOGE(TAG, "No tag scanned, cannot save new card");
            return;
        }

        cJSON *pass_json = cJSON_GetObjectItem(root, "password");
        cJSON *desc_json = cJSON_GetObjectItem(root, "name");

        if (pass_json == NULL || desc_json == NULL)
        {
            ESP_LOGE(TAG, "pass or desc not found in request");
            return;
        }

        strcpy(current_new_card.pass, pass_json->valuestring);
        strcpy(current_new_card.desc, desc_json->valuestring);

        state = APP_STATE_SAVE_NEW_CARD;
    }
    else if (strcmp(request_type, REQUEST_TYPE_STR[REQUEST_TYPE_CLEAR_CARD]) == 0)
    {
        cJSON *index_json = cJSON_GetObjectItem(root, "index");
        if (index_json == NULL)
        {
            ESP_LOGE(TAG, "index not found in request");
            return;
        }

        current_clear_card_index = index_json->valueint;
        state = APP_STATE_CLEAR_CARD;
    }
    else if (strcmp(request_type, REQUEST_TYPE_STR[REQUEST_TYPE_CLEAR_DB]) == 0)
    {
        state = APP_STATE_CLEAR_DB;
    }
}

void send_serial_msg()
{
    char msg[] = "Hello World!\n";
    size_t len = strlen(msg);

    size_t write_size = tinyusb_cdcacm_write_queue(ITF_NUM_CDC, (uint8_t *)msg, len);
    esp_err_t err = tinyusb_cdcacm_write_flush(ITF_NUM_CDC, 0);

    ESP_ERROR_CHECK_WITHOUT_ABORT(err);

    // This works too, does the same damn thing
    //  tud_cdc_write(msg, sizeof(msg));
    //  tud_cdc_write_flush();
}

#define SPACE_PRESS_COUNT 5

// The reason why we have to do all this weird shit is because we are effectively mimicking a HID device.
// They have nuance such as repeating keys when you hold down a key and such so passing strings across exactly will be difficult
// My windows account is especiialy sensitive to this so its a good test case

// https://forums.raspberrypi.com/viewtopic.php?t=317187#p1898513
// https://electronics.stackexchange.com/questions/609090/stm32-usb-hid-keyboard-skipping-key-presses
//  These both show that the host controls when it wants to accept a HUD report, so sending more meaninglessly does nothing.

void send_password_keystrokes()
{
    char currently_scanned_pass[MAX_PASS_SIZE];
    esp_err_t err = get_pass_from_id(currently_scanned_tag_index, currently_scanned_pass);
    if (err != ESP_OK)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        return;
    }

    // release all keys between two characters; otherwise two identical
    // consecutive characters are treated as just one key press

    ESP_LOGI(TAG, "Sending password report");

    for (size_t i = 0; i < SPACE_PRESS_COUNT; i++)
    {
        wait_for_hid_ready();
        uint8_t keycode[6] = {HID_KEY_SPACE};
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_for_hid_ready();
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Wait for UI to appear, pressing the deploy button again should work also
    vTaskDelay(pdMS_TO_TICKS(500));

    wait_for_hid_ready();

    uint8_t keycode[6] = {HID_KEY_A};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, KEYBOARD_MODIFIER_LEFTCTRL, keycode);
    wait_for_hid_ready();
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

    vTaskDelay(pdMS_TO_TICKS(50));

    int len = strlen(currently_scanned_pass);
    // TODO: figure out condensed version of password payload.
    // It has to split on capital letters/chars that need modifier keys
    for (size_t i = 0; i < len; i++)
    {
        Keyboard_payload_t payload = ascii_2_keyboard_payload(currently_scanned_pass[i]);
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, payload.modifier, payload.keycode);
        wait_for_hid_ready();
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    wait_for_hid_ready();
    uint8_t enter_keycode[6] = {HID_KEY_ENTER};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, enter_keycode);
    wait_for_hid_ready();
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

    ESP_LOGI(TAG, "Password sent");
}

void print_state_cb(void *arg)
{
    ESP_LOGI(TAG, "State: %d, Currently Scanned: %d", state, currently_scanned_tag_index);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Keydotboard");
    const gpio_config_t trigger_button_config = {
        .pin_bit_mask = TRIGGER_BUTTON_BIT_MASK,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };

    ESP_ERROR_CHECK(gpio_config(&trigger_button_config));

    const gpio_config_t programming_button_config = {
        .pin_bit_mask = PROGRAMMING_MODE_BIT_MASK,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };

    ESP_ERROR_CHECK(gpio_config(&programming_button_config));

    led_init();

    initialise_keyboard();

    const tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        // This actually does mean itf (interface)
        .cdc_port = ITF_NUM_CDC,
        .rx_unread_buf_sz = 64,
        .callback_rx = &tinyusb_cdc_rx_callback,
        // .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL};
    // tusb_cdc_acm_deinit(ITF_NUM_CDC);
    // tinyusb_config_cdcacm_t acm_cfg = {0}; // the configuration uses default values, these are kinda bad and cause the device to stop working after sleep
    // ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    // This allows for usb debugging over cdc
    // esp_tusb_init_console(TINYUSB_CDC_ACM_0);

    // This uses the default nvs partition (nvs) (use menuconfig to specify a custom partition table csv)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    // Namespace, read/write, handle
    err = nvs_open(NVS_STORAGE_NAMESPACE, NVS_READWRITE, &storage_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
        return;
    }
    printf("Done\n");

    init_rfid_tags();

    setup_rfid_reader();

#ifdef LOG_DETAILS
    esp_chip_info(&chip_info);
    print_memory_sizes();

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &print_state_cb,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic state print"
        // By default the timer is in task callback mode
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    // This is in microseconds
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, STATE_PRINT_INTERVAL_US));
#endif

    state = APP_STATE_MASTER_MODE;

    while (1)
    {
        switch (state)
        {
        case APP_STATE_MASTER_MODE:
        {

            int programming_mode_level = gpio_get_level(PROGRAMMING_MODE_PIN);

            if (!programming_mode_level)
            {
                ESP_LOGI(TAG, "Starting scanner mode");
                // Light strobe routine
                ESP_LOGI("LED", "Starting strobe effect");
                led_message_t msg = {.command = STROBE_START};
                xQueueSend(command_queue, &msg, portMAX_DELAY);

                state = APP_STATE_SCANNER_MODE;
                // Initialise scanner by turning on serial
                ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
                // Need to wait here for a bit for the serial port to open
                vTaskDelay(pdMS_TO_TICKS(100));
            }

#ifdef DEMO_BUTTON_ENABLED
            int level = gpio_get_level(TRIGGER_BUTTON_PIN);
            if (!level)
            {
                char outpass[MAX_PASS_SIZE];
                ESP_LOGI(TAG, "Got trigger");
                get_pass_from_id(0, outpass);

                // For some reason (composite device I think) tud dismounts after sleep
                // TODO: Invetigate this with base HID example that doesnt do this
                // This may also be the cause of light sleep
                if (!tud_mounted())
                {
                    ESP_LOGE(TAG, "TUD not connected restarting");
                    esp_restart();
                }

                app_send_hid_demo();
                send_serial_msg();
            }
#else
            int level = gpio_get_level(TRIGGER_BUTTON_PIN);
            if (!level)
            {
                ESP_LOGI(TAG, "Got trigger without card womp womp");
            }

#endif

            break;
        }
        case APP_STATE_SCANNED_CARD:
        {

            // Test if trigger is pressed
            // Send a blink command
            xQueueReset(ack_queue); // Maybe not needed
            led_message_t send_msg = {.command = BLINK_START};
            xQueueSend(command_queue, &send_msg, 0);

            while (1)
            {
                // If the gpio is high (trigger is pressed) or if we get a message from the queue
                int level = gpio_get_level(TRIGGER_BUTTON_PIN);
                led_message_t msg;

                if (!level)
                {
                    state = APP_STATE_TRIGGER_BUTTON_PRESSED;
                    break;
                }
                else if (xQueuePeek(ack_queue, &msg, 0) && msg.command == BLINK_START)
                {
                    xQueueReceive(command_queue, &msg, 0); // Remove the command from the queue
                    state = APP_STATE_MASTER_MODE;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            break;
        }
        case APP_STATE_TRIGGER_BUTTON_PRESSED:
        {
            uint32_t timeout_ticker = xTaskGetTickCount();
            const TickType_t timeout_ticker_timeout = 20000 / portTICK_PERIOD_MS; // 20 seconds
            while (1)
            {
                led_message_t msg;
                int level = gpio_get_level(TRIGGER_BUTTON_PIN);
                // This might need to be a task but should be fine
                wait_for_hid_ready();
                uint8_t keycode[6] = {HID_KEY_SPACE};
                tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);

                // queue peek doesnt block, it returns true if the queue is not empty, use receive to block and extract the value out of the queue
                if (xQueuePeek(ack_queue, &msg, 0) && msg.command == BLINK_START)
                {
                    xQueueReceive(command_queue, &msg, 0); // Remove the command from the queue
                    ESP_LOGI(TAG, "Trigger button release and blink timeout, going back to master");
                    state = APP_STATE_MASTER_MODE;
                    break;
                }
                else if (level)
                {
                    ESP_LOGI(TAG, "Trigger button release, sending password");
                    led_message_t send_msg = {.command = STROBE_STOP};
                    xQueueSend(command_queue, &send_msg, 0);
                    state = APP_STATE_APPLY_KEYSTROKES;
                    break;
                }
                else
                {
                    // Check if we have reached the timeout
                    if (xTaskGetTickCount() - timeout_ticker >= timeout_ticker_timeout)
                    {
                        ESP_LOGE(TAG, "QUEUE DID NOT RETURN IN TIME, UNKNOWN ERROR, going back to master");
                        xQueueReset(ack_queue);
                        state = APP_STATE_MASTER_MODE;
                        // perhaps we should abort here instead
                        break;
                    }
                    // Delay for 10 ms
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            }
            break;
        }
        case APP_STATE_SEND_PASSWORD_DB:
        {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "response_type", RESPONSE_TYPE_STR[RESPONSE_TYPE_GET_PASSWORD_DESCS]);
            char *json_str;
            create_get_db_response(root, &json_str);
            cJSON_Delete(root);

            size_t len = strlen(json_str);

            init_write_to_cdc(json_str);

            const int max_loops = 500;

            for (size_t i = 0; i < max_loops; i++)
            {
                if (write_to_cdc_loop())
                {
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(10));
            }
            tinyusb_cdcacm_write_queue_char(ITF_NUM_CDC, '\n');

            esp_err_t err = tinyusb_cdcacm_write_flush(ITF_NUM_CDC, 0);

            ESP_ERROR_CHECK_WITHOUT_ABORT(err);

            free(json_str);
            state = APP_STATE_SCANNER_MODE;
            break;
        }
        case APP_STATE_SCANNER_MODE:
        {
            // if button press is detected, we need to wait for the strobe to stop and move back to master
            int programming_mode_level = gpio_get_level(PROGRAMMING_MODE_PIN);
            led_message_t msg;

            if (!programming_mode_level)
            {
                ESP_LOGI(TAG, "Finished scanner mode, going back to master");

                tusb_cdc_acm_deinit(ITF_NUM_CDC);
                led_message_t stop_msg = {.command = STROBE_STOP};
                xQueueSend(command_queue, &stop_msg, portMAX_DELAY);

                // Wait for acknowledgment
                led_message_t ack;
                xQueueReceive(ack_queue, &ack, portMAX_DELAY);
                if (ack.command == STROBE_STOP)
                {
                    ESP_LOGI("LED", "Strobe stopped and fade ended");
                    state = APP_STATE_MASTER_MODE;
                }
                else
                {
                    ESP_LOGE(TAG, "Error state in scanner mode, aborting");
                    abort();
                }
            }
            break;
        }
        case APP_STATE_APPLY_KEYSTROKES:
        {
            // Turn on blink routine
            // We dont need to send this in multiple waves in a for loop since it might already be logged in by the time were tyring to press more
            // Instead rely on the user to press the deploy button again
            send_password_keystrokes();

            state = APP_STATE_MASTER_MODE;
            break;
        }
        case APP_STATE_SEND_RFID:
        {
            // Doesnt need to be streamed because of the small payload
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "response_type", RESPONSE_TYPE_STR[RESPONSE_TYPE_DETECTED_RFID]);
            cJSON_AddNumberToObject(root, "rfid", currently_scanned_tag);
            char *json_str = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            ESP_LOGI(TAG, "Sending JSON: %s", json_str);
            // Send the json string
            size_t write_size = tinyusb_cdcacm_write_queue(ITF_NUM_CDC, (uint8_t *)json_str, strlen(json_str));
            tinyusb_cdcacm_write_queue_char(ITF_NUM_CDC, '\n');
            esp_err_t err = tinyusb_cdcacm_write_flush(ITF_NUM_CDC, 0);
            ESP_ERROR_CHECK_WITHOUT_ABORT(err);

            free(json_str);
            state = APP_STATE_SCANNER_MODE;
            break;
        }

        case APP_STATE_SAVE_NEW_CARD:
        {
            save_new_card(&current_new_card, &currently_scanned_tag);

            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "response_type", RESPONSE_TYPE_STR[RESPONSE_TYPE_NEW_CARD]);
            cJSON_AddNumberToObject(root, "rfid", currently_scanned_tag);
            char *json_str = cJSON_PrintUnformatted(root);

            // comment this out to test saving test cards
            state = APP_STATE_SCANNER_MODE;
            break;
        }

        case APP_STATE_CLEAR_CARD:
        {
            clear_card(current_clear_card_index);
            state = APP_STATE_SCANNER_MODE;
            break;
        }
        case APP_STATE_CLEAR_DB:
        {
            clear_db();
            state = APP_STATE_SCANNER_MODE;
            break;
        }

        default:
            ESP_LOGI(TAG, "State not implemented: %d, going to MASTER", state);
            state = APP_STATE_MASTER_MODE;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_INTERVAL_MS));
    }
}
