#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define LED_GPIO_PIN GPIO_NUM_18 // Define the GPIO pin where the LED is connected
#define TOTAL_BLINK_TIME 2000
#define FADE_TIME 500
#define SOLID_COLOR_DUTY 4000
#define FADE_MAX_DUTY 4000
// #define FADE_MAX_DUTY 8191

typedef enum {
    STROBE_START,
    STROBE_STOP,
    BLINK_START
} led_command_t;

typedef struct {
    led_command_t command;
    // int period; // Blink period in milliseconds
} led_message_t;

extern QueueHandle_t command_queue;
/// @brief The ack queue will return the command that was sent to it
extern QueueHandle_t ack_queue;

void led_init();
void strobe_led_task(void *param);