#include "keydot_led.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "blink_time.h"

QueueHandle_t command_queue;
QueueHandle_t ack_queue;
ledc_channel_config_t ledc_channel;

void led_init()
{
    // ledc is good for pwm, not for blinking, for that you would ideally use a timer
    // https://github.com/espressif/esp-idf/blob/v5.2.1/examples/peripherals/ledc/ledc_fade/main/ledc_fade_example_main.c
    ESP_LOGI("LED", "Initialising LED on GPIO %d", LED_GPIO_PIN);
    gpio_reset_pin(LED_GPIO_PIN);
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK};
    ledc_timer_config(&ledc_timer);

    ledc_channel = (ledc_channel_config_t){
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_FADE_END,
        .gpio_num = LED_GPIO_PIN,
        .duty = 0,
        .hpoint = 0};
    ledc_channel_config(&ledc_channel);

    ledc_fade_func_install(0);

    // ledc_cb_register(ledc_channel.speed_mode, ledc_channel.channel, cb_ledc_fade_end_event, (void *)NULL);
}

void strobe_led_task(void *param)
{
    led_message_t msg;

    while (1)
    {
        if (xQueueReceive(command_queue, &msg, portMAX_DELAY))
        {
            if (msg.command == STROBE_START)
            {
                while (1)
                {
                    // Check if a stop command or blink command is received, the way this is setup is that by breaking, it will go to the other command
                    if (xQueuePeek(command_queue, &msg, 0) && (msg.command == STROBE_STOP || msg.command == BLINK_START))
                    {
                        xQueueReceive(command_queue, &msg, 0); // Remove the command from the queue
                        break;
                    }

                    ledc_set_fade_with_time(ledc_channel.speed_mode, ledc_channel.channel, FADE_MAX_DUTY, FADE_TIME); // Fade to max brightness in 500 ms
                    ledc_fade_start(ledc_channel.speed_mode, ledc_channel.channel, LEDC_FADE_WAIT_DONE);

                    ledc_set_fade_with_time(ledc_channel.speed_mode, ledc_channel.channel, 0, FADE_TIME); // Fade to off in 500 ms
                    ledc_fade_start(ledc_channel.speed_mode, ledc_channel.channel, LEDC_FADE_WAIT_DONE);
                }
            }

            if (msg.command == BLINK_START)
            {
                int blink_delay = initial_blink_period; // Start with the specified initial blink period
                bool is_decreasing = true;
                uint32_t extra_blink_dur_ticker = xTaskGetTickCount();
                const TickType_t extra_blink_dur_ticker_timeout = EXTRA_BLINK_DURATION / portTICK_PERIOD_MS; // 20 seconds
                while (1)
                {
                    // Check if a stop command or strobe command is received
                    if (xQueuePeek(command_queue, &msg, 0) && (msg.command == STROBE_STOP || msg.command == STROBE_START))
                    {
                        xQueueReceive(command_queue, &msg, 0); // Remove the command from the queue
                        break;
                    }

                    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, SOLID_COLOR_DUTY);
                    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
                    vTaskDelay(blink_delay / portTICK_PERIOD_MS / 2);

                    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
                    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
                    vTaskDelay(blink_delay / portTICK_PERIOD_MS / 2);
                    ESP_LOGI("LED", "Blinking for %d ms", blink_delay);

                    if (is_decreasing)
                    {
                        // Gradually increase the blinking frequency
                        blink_delay -= decrease_blink_duration; // Decrease delay to increase frequency

                        // if finished decreasing, set the current ticks time, then check against extra blink duration
                        if (blink_delay <= min_blink_period)
                        { 
                            blink_delay = min_blink_period;
                            extra_blink_dur_ticker = xTaskGetTickCount();
                            is_decreasing = false;
                            ESP_LOGI("LED", "Finished decreasing, starting extra blink duration");
                        }
                    }
                    else
                    {
                        if (xTaskGetTickCount() - extra_blink_dur_ticker >= extra_blink_dur_ticker_timeout)
                        {
                            ESP_LOGI("LED", "Finished extra blink duration, sending ack message");
                            led_message_t ack = {.command = BLINK_START};
                            xQueueSend(ack_queue, &ack, portMAX_DELAY); // Send acknowledgment
                            break;
                        }
                    }
                }
            }

            // This is really only reached if we are in an idle state and we receive a stop command
            if (msg.command == STROBE_STOP)
            {
                ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                led_message_t ack = {.command = STROBE_STOP};
                xQueueSend(ack_queue, &ack, portMAX_DELAY); // Send acknowledgment
            }
        }
    }
}


// SemaphoreHandle_t ledc_fade_end_sem;
// use binary semaphore here to wait for the fade to finish
// https://www.youtube.com/watch?v=5JcMtbA9QEE&t=3s
// ISRs are the perfect place to use semaphores like this since they kinda work like mini signals telling the main thread
static IRAM_ATTR bool cb_ledc_fade_end_event(const ledc_cb_param_t *param, void *user_arg)
{
    // if (param->event == LEDC_FADE_END_EVT)
    // {
    // }
    return true;

    // return (taskAwoken == pdTRUE);
}