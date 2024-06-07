#include "keydot_led.h"


void start_strobe() {
    ESP_LOGI("LED", "Starting strobe effect");
    led_message_t msg = { .command = STROBE_START };
    xQueueSend(command_queue, &msg, portMAX_DELAY);
}

void stop_strobe() {
    ESP_LOGI("LED", "Stopping strobe effect");
    led_message_t msg = { .command = STROBE_STOP };
    xQueueSend(command_queue, &msg, portMAX_DELAY);

    // Wait for acknowledgment
    led_message_t ack;
    xQueueReceive(ack_queue, &ack, portMAX_DELAY);
    if (ack.command == STROBE_STOP) {
        ESP_LOGI("LED", "Strobe stopped and fade ended");
    }
}

void start_blink(int initial_period) {
    ESP_LOGI("LED", "Starting blinking effect with initial period %d ms", initial_period);
    led_message_t msg = { .command = BLINK_START, .period = initial_period };
    xQueueSend(command_queue, &msg, portMAX_DELAY);

    // Wait for acknowledgment
    led_message_t ack;
    xQueueReceive(ack_queue, &ack, portMAX_DELAY);
    if (ack.command == BLINK_START) {
        ESP_LOGI("LED", "Blinking reached the threshold frequency");
    }
}

void app_main() {
    led_init();

    command_queue = xQueueCreate(2, sizeof(led_message_t));
    ack_queue = xQueueCreate(1, sizeof(led_message_t)); // Acknowledgment queue
    xTaskCreate(strobe_led_task, "strobe_led_task", 2048, NULL, 5, NULL);

    start_strobe(); // Start the strobe effect
    vTaskDelay(10000 / portTICK_PERIOD_MS); // Strobe for 10 seconds
    stop_strobe(); // Stop the strobe effect and wait for fade to end

    start_blink(1000); // Start the blinking effect with an initial period of 1000 ms
}

