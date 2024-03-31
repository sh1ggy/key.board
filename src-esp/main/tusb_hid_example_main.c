/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "keyboard_ops.h"
#include "constants.h"

#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#include "nvs_flash.h"
#include "nvs.h"

#define APP_BUTTON (GPIO_NUM_0) // Use BOOT signal by default

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
}

/*******KEYDOTBOARD APP ******/

typedef enum
{
    APP_STATE_RECORDING,
    APP_STATE_APPLY_KEYSTROKES
} APP_STATE;

APP_STATE state = APP_STATE_RECORDING;
nvs_handle_t storage_handle;
#define MAX_PASS_SIZE 50

void get_pass_from_id(char *in_selected_id, size_t max_pass_size, char *out_pass)
{

    // key cannot be longer than 15
    char password_key[16];
    sprintf(password_key, "pass%s", in_selected_id);
    printf("Passkey: %s\n", password_key);

    // Get size first, then save into allocated string
    size_t str_len = 0;
    esp_err_t err = nvs_get_str(storage_handle, password_key, out_pass, &str_len);

    ESP_ERROR_CHECK(err);
    //TODO: place check here that the key is even here

    if (str_len > max_pass_size)
    {
        ESP_LOGE(TAG, "Password for key '%s' is too long to store in memory \n", password_key);
    }

    ESP_LOGI(TAG, "Password value %s", out_pass);
    // TODO return esp error value or make own enum for error handling
}

#define TRIGGER_BUTTON_PIN GPIO_NUM_10
#define TRIGGER_BUTTON_BIT_MASK (1ULL << TRIGGER_BUTTON_PIN)

void app_main(void)
{
    // Initialize button that will trigger HID reports
    const gpio_config_t boot_button_config = {
        .pin_bit_mask = BIT64(APP_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_button_config));

    const gpio_config_t trigger_button_config = {
        .pin_bit_mask = TRIGGER_BUTTON_BIT_MASK,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };

    ESP_ERROR_CHECK(gpio_config(&trigger_button_config));

    initialise_keyboard();

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
    err = nvs_open("storage", NVS_READWRITE, &storage_handle);

    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");

        // Read
        printf("Reading restart counter from NVS ... ");
        int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
        err = nvs_get_i32(storage_handle, "restart_counter", &restart_counter);

        while (1)
        {
            if (tud_mounted())
            {
                static bool send_hid_data = true;

                if (send_hid_data)
                {
                    app_send_hid_demo();
                }
                send_hid_data = !gpio_get_level(APP_BUTTON);

                static bool is_trigger_pressed = false;
                int level = gpio_get_level(TRIGGER_BUTTON_PIN);
                printf("level %d", level);
                if (!level)
                {
                    char outpass[MAX_PASS_SIZE];
                    ESP_LOGI(TAG, "Got trigger");
                    get_pass_from_id("something", MAX_PASS_SIZE, outpass);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}