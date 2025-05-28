#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define LED0 GPIO_NUM_8
#define LED1 GPIO_NUM_3
#define LED2 GPIO_NUM_9
#define LED3 GPIO_NUM_10

#define BTN1 GPIO_NUM_17
#define BTN2 GPIO_NUM_18

#define DEBOUNCE_TIME_US 200000 // 200 ms

uint8_t counter = 0;
uint8_t increment = 1;

int64_t last_press_btn1 = 0;
int64_t last_press_btn2 = 0;

void update_leds(uint8_t value) {
    gpio_set_level(LED0, value & 0x01);
    gpio_set_level(LED1, (value >> 1) & 0x01);
    gpio_set_level(LED2, (value >> 2) & 0x01);
    gpio_set_level(LED3, (value >> 3) & 0x01);
}

void init_gpio() {
    // LEDs como saída
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED0) | (1ULL << LED1) | (1ULL << LED2) | (1ULL << LED3),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    // Botões como entrada
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BTN1) | (1ULL << BTN2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,  // Ativa resistor pull-up interno
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);
}

void app_main() {
    init_gpio();
    update_leds(counter);

    last_press_btn1 = esp_timer_get_time();
    last_press_btn2 = esp_timer_get_time();

    while (1) {
        int64_t now = esp_timer_get_time();

        if (gpio_get_level(BTN1) == 0 && now - last_press_btn1 > DEBOUNCE_TIME_US) {
            last_press_btn1 = now;
            counter = (counter + increment) & 0x0F; // Mantém em 4 bits
            update_leds(counter);
            printf("BTN1 pressionado: contador = %d\n", counter);
        }

        if (gpio_get_level(BTN2) == 0 && now - last_press_btn2 > DEBOUNCE_TIME_US) {
            last_press_btn2 = now;
            increment = (increment == 1) ? 2 : 1;
            printf("BTN2 pressionado: novo incremento = %d\n", increment);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
