#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#define LED_GPIO_PIN GPIO_NUM_18 // Define the GPIO pin where the LED is connected

typedef enum {
    STROBE_START,
    STROBE_STOP,
    BLINK_START
} led_command_t;

typedef struct {
    led_command_t command;
    int period; // Blink period in milliseconds
} led_message_t;

extern QueueHandle_t command_queue;
extern QueueHandle_t ack_queue;


void led_init();
void strobe_led_task(void *param);