/*
 * SPDX-FileCopyrightText: 2020-2024 Kongi Systems Brisbane, Australia
 *
 */

#include "keyboard_ops.h"
#include "constants.h"

#include <inttypes.h>

#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"

#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#include "sdkconfig.h"

#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "rc522.h"
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
typedef struct
{
    uint64_t *serial_number_buffer;
    uint32_t total_rfid_tags;
} RFID_DB_t;

typedef enum
{
    APP_STATE_BOOT,
    APP_STATE_SCANNER_MODE,
    APP_STATE_MASTER_MODE,
    APP_STATE_APPLY_KEYSTROKES,
    APP_STATE_SEND_PASSWORD_DB,
    APP_STATE_SEND_RFID,
    APP_STATE_MAX
} APP_STATE;

typedef enum
{
    REQUEST_TYPE_GET_PASSWORD_DESCS,
    REQUEST_TYPE_MAX
} REQUEST_TYPE;

static const char *REQUEST_TYPE_STR[REQUEST_TYPE_MAX] =
    {
        [REQUEST_TYPE_GET_PASSWORD_DESCS] = "get_pass_descs",
};

typedef enum
{
    RESPONSE_TYPE_GET_PASSWORD_DESCS,
    RESPONSE_TYPE_DETECTED_RFID,
    RESPONSE_TYPE_MAX

} RESPONSE_TYPE;

static const char *RESPONSE_TYPE_STR[RESPONSE_TYPE_MAX] =
    {
        [RESPONSE_TYPE_GET_PASSWORD_DESCS] = "get_pass_descs",
        [RESPONSE_TYPE_DETECTED_RFID] = "detected_rfid",
};

#define MAX_PASS_SIZE 50
#define MAX_DESC_SIZE 50
#define NVS_STORAGE_NAMESPACE "kb"
#define TRIGGER_BUTTON_PIN GPIO_NUM_12
#define TRIGGER_BUTTON_BIT_MASK (1ULL << TRIGGER_BUTTON_PIN)
#define STATE_PRINT_INTERVAL 500 * 1000

APP_STATE state = APP_STATE_BOOT;
nvs_handle_t storage_handle;
rc522_handle_t scanner;
RFID_DB_t rfid_db;
uint64_t currently_scanned_tag;
char currently_scanned_pass[MAX_PASS_SIZE];

void get_pass_from_id(size_t in_selected_id, char *out_pass)
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
        return;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(err);

    ESP_LOGI(TAG, "Password value:%s, length:%d", out_pass, str_len);
    // TODO return esp error value or make own enum for error handling
}

static void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_event_data_t *data = (rc522_event_data_t *)event_data;

    switch (event_id)
    {
    case RC522_EVENT_TAG_SCANNED:
    {
        rc522_tag_t *tag = (rc522_tag_t *)data->ptr;
        ESP_LOGI(TAG, "Tag scanned (sn: %" PRIu64 ")", tag->serial_number);

        switch (state)
        {
        case APP_STATE_SCANNER_MODE:
        {
            currently_scanned_tag = tag->serial_number;
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
                    char outpass[MAX_PASS_SIZE];
                    get_pass_from_id(i, outpass);
                    strcpy(currently_scanned_pass, outpass);
                    state = APP_STATE_APPLY_KEYSTROKES;
                    break;
                }
            }
        }

        default:
            break;
        }
    }
    break;
    }
}

static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
// Itf stands for interface, it is the number that is specified in the configuration
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Data from channel %d:", itf);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buf, rx_size, ESP_LOG_INFO);
    }
    else
    {
        ESP_LOGE(TAG, "Read error");
    }

    /* write back */
    // tinyusb_cdcacm_write_queue(itf, buf, rx_size);
    // tinyusb_cdcacm_write_flush(itf, 0);
    cJSON *root = cJSON_Parse((char *)buf);
    char *request_type = cJSON_GetObjectItem(root, "request_type")->valuestring;
    ESP_LOGI(TAG, "request_type=%s", request_type);

    cJSON_Delete(root);
}

void handle_request(cJSON *root)
{
    cJSON *request_type = cJSON_GetObjectItem(root, "request_type");
    if (request_type == NULL)
    {
        ESP_LOGE(TAG, "Request type not found in request");
        return;
    }

    if (strcmp(request_type->valuestring, REQUEST_TYPE_STR[REQUEST_TYPE_GET_PASSWORD_DESCS]) == 0)
    {
        state = APP_STATE_SEND_PASSWORD_DB;
    }
}

// TODO: Add error handling
void create_get_db_response(cJSON *root, char **json_str)
{
    cJSON_AddStringToObject(root, "response_type", RESPONSE_TYPE_STR[RESPONSE_TYPE_GET_PASSWORD_DESCS]);
    cJSON *tags = cJSON_CreateArray();
    for (size_t i = 0; i < rfid_db.total_rfid_tags; i++)
    {
        cJSON *tag = cJSON_CreateObject();
        cJSON_AddNumberToObject(tag, "rfid", rfid_db.serial_number_buffer[i]);

        // Get the description from nvs, its ok if this takes long, we don't need to cache descs in mem
        char desc_key[16];
        sprintf(desc_key, "pass%zu", i);
        printf("Desckey: %s\n", desc_key);

        char out_desc[MAX_DESC_SIZE];
        size_t str_len = MAX_DESC_SIZE;
        esp_err_t err = nvs_get_str(storage_handle, desc_key, out_desc, &str_len);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Desc for key '%s' not found in NVS\n", desc_key);
            continue;
        }

        ESP_ERROR_CHECK_WITHOUT_ABORT(err);

        cJSON_AddStringToObject(tag, "description", out_desc);
        cJSON_AddItemToArray(tags, tag);
    }
    cJSON_AddItemToObject(root, "descriptions", tags);
    *json_str = cJSON_Print(root);
}

void send_serial_msg()
{
    char msg[] = "Hello World!\n";

    tud_cdc_write(msg, sizeof(msg));
    tud_cdc_write_flush();
}

void init_rfid_tags()
{
    ESP_LOGI(TAG, "Initialising Tag DB");
    // Get the total number of tags from nvs
    esp_err_t err = nvs_get_u32(storage_handle, "total_tags", &rfid_db.total_rfid_tags);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No tags found in NVS");
        return;
    }
    // malloc the buffer to match the number of tags
    rfid_db.serial_number_buffer = (uint64_t *)malloc(rfid_db.total_rfid_tags * sizeof(uint64_t));

    // Get the serial numbers from nvs
    for (size_t i = 0; i < rfid_db.total_rfid_tags; i++)
    {
        char tag_key[16];
        sprintf(tag_key, "tag%zu", i);
        err = nvs_get_u64(storage_handle, tag_key, &rfid_db.serial_number_buffer[i]);
        ESP_ERROR_CHECK(err);

        ESP_LOGI(TAG, "Tag %s: (sn: %" PRIu64 "): ", tag_key, rfid_db.serial_number_buffer[i]);
    }
}

void print_state_cb(void *arg)
{
    ESP_LOGI(TAG, "State: %d", state);
}

void app_main(void)
{
    const gpio_config_t trigger_button_config = {
        .pin_bit_mask = TRIGGER_BUTTON_BIT_MASK,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };

    ESP_ERROR_CHECK(gpio_config(&trigger_button_config));

    initialise_keyboard();

    const tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = &tinyusb_cdc_rx_callback,
        // .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL};
    // tinyusb_config_cdcacm_t acm_cfg = {0}; // the configuration uses default values, these are kinda bad and cause the device to stop working after sleep
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
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

    state = APP_STATE_MASTER_MODE;
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &print_state_cb,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic state print"
        // By default the timer is in task callback mode
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    // This is in microseconds
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, STATE_PRINT_INTERVAL));

    while (1)
    {
        switch (state)
        {
        case APP_STATE_MASTER_MODE:
        {
            // if (tud_mounted())
            int level = gpio_get_level(TRIGGER_BUTTON_PIN);
            if (!level)
            {
                char outpass[MAX_PASS_SIZE];
                ESP_LOGI(TAG, "Got trigger");
                get_pass_from_id(1, outpass);

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

            break;
        }
        case APP_STATE_SEND_PASSWORD_DB:
        {
            cJSON *root = cJSON_CreateObject();
            char *json_str;
            create_get_db_response(root, &json_str);

            cJSON_Delete(root);
            ESP_LOGI(TAG, "Sending JSON: %s", json_str);
            // Send the json string
            tud_cdc_write(json_str, strlen(json_str));
            tud_cdc_write_flush();
            free(json_str);
            state = APP_STATE_SCANNER_MODE;
            break;
        }
        case APP_STATE_SCANNER_MODE:
        {
            // state = APP_STATE_APPLY_KEYSTROKES;
            break;
        }
        case APP_STATE_APPLY_KEYSTROKES:
        {
            // Apply the keystrokes
            state = APP_STATE_MASTER_MODE;
            break;
        }
        case APP_STATE_SEND_RFID:
        {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "response_type", RESPONSE_TYPE_STR[RESPONSE_TYPE_DETECTED_RFID]);
            cJSON_AddNumberToObject(root, "rfid", currently_scanned_tag);
            char *json_str = cJSON_Print(root);
            cJSON_Delete(root);
            ESP_LOGI(TAG, "Sending JSON: %s", json_str);
            // Send the json string
            tud_cdc_write(json_str, strlen(json_str));
            tud_cdc_write_flush();
            free(json_str);
            state = APP_STATE_SCANNER_MODE;
            break;
        }

        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
