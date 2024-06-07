#include "keydot_led.h"
#include "esp_log.h"

QueueHandle_t command_queue;
QueueHandle_t ack_queue;



void led_init() {
    ESP_LOGI("LED", "Initialising LED on GPIO %d", LED_GPIO_PIN);
    gpio_reset_pin(LED_GPIO_PIN);
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_FADE_END,
        .gpio_num       = LED_GPIO_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);
}


void strobe_led_task(void *param) {
    led_message_t msg;

    while (1) {
        if (xQueueReceive(command_queue, &msg, portMAX_DELAY)) {
            if (msg.command == STROBE_START) {
                while (1) {
                    // Check if a stop command or blink command is received
                    if (xQueuePeek(command_queue, &msg, 0) && (msg.command == STROBE_STOP || msg.command == BLINK_START)) {
                        xQueueReceive(command_queue, &msg, 0); // Remove the command from the queue
                        break;
                    }

                    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8191, 500); // Fade to max brightness in 500 ms
                    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
                    vTaskDelay(500 / portTICK_PERIOD_MS);

                    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0, 500); // Fade to off in 500 ms
                    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                }
            }

            if (msg.command == BLINK_START) {
                int blink_delay = msg.period; // Start with the specified initial blink period
                while (1) {
                    // Check if a stop command or strobe command is received
                    if (xQueuePeek(command_queue, &msg, 0) && (msg.command == STROBE_STOP || msg.command == STROBE_START)) {
                        xQueueReceive(command_queue, &msg, 0); // Remove the command from the queue
                        break;
                    }

                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 8191);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                    vTaskDelay(blink_delay / portTICK_PERIOD_MS / 2);

                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                    vTaskDelay(blink_delay / portTICK_PERIOD_MS / 2);

                    // Gradually increase the blinking frequency
                    blink_delay -= 50; // Decrease delay to increase frequency
                    if (blink_delay <= 200) { // Threshold: 5 Hz blinking (200 ms period)
                        blink_delay = 200;
                        led_message_t ack = { .command = BLINK_START };
                        xQueueSend(ack_queue, &ack, portMAX_DELAY); // Send acknowledgment
                        break;
                    }
                }
            }

            if (msg.command == STROBE_STOP) {
                ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                led_message_t ack = { .command = STROBE_STOP };
                xQueueSend(ack_queue, &ack, portMAX_DELAY); // Send acknowledgment
            }
        }
    }
}
