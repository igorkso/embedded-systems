
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PIN_0 10
#define LED_PIN_1 11
#define LED_PIN_2 12
#define LED_PIN_3 13

#define BUTTON_NEXT 14
#define BUTTON_STEP 7

#define DEBOUNCE_TIME_US 150000

volatile int counter = 0;
volatile int increment = 1;
volatile int64_t last_next_press = 0;
volatile int64_t last_step_press = 0;

void update_leds(int value) {
    gpio_set_level(LED_PIN_0, value & 0x01);
    gpio_set_level(LED_PIN_1, (value >> 1) & 0x01);
    gpio_set_level(LED_PIN_2, (value >> 2) & 0x01);
    gpio_set_level(LED_PIN_3, (value >> 3) & 0x01);
}

void IRAM_ATTR on_next_button_pressed(void* arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_next_press > DEBOUNCE_TIME_US) {
        counter = (counter + increment) & 0x0F;
        update_leds(counter);
        last_next_press = now;
    }
}

void IRAM_ATTR on_step_button_pressed(void* arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_step_press > DEBOUNCE_TIME_US) {
        increment = (increment == 1) ? 2 : 1;
        last_step_press = now;
    }
}

void app_main(void) {
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_PIN_0) | (1ULL << LED_PIN_1) |
                        (1ULL << LED_PIN_2) | (1ULL << LED_PIN_3),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&led_config);

    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_NEXT) | (1ULL << BUTTON_STEP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_POSEDGE
    };
    gpio_config(&button_config);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_NEXT, on_next_button_pressed, NULL);
    gpio_isr_handler_add(BUTTON_STEP, on_step_button_pressed, NULL);

    update_leds(counter);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
