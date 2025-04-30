#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BLINK8  GPIO_NUM_8
#define BLINK37 GPIO_NUM_37

void setup_gpio(gpio_num_t gpio) {
    gpio_reset_pin(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
}

void blink8_task(void *pvParameter) {
    gpio_num_t gpio = (gpio_num_t)pvParameter;
    while (1) {
        gpio_set_level(gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_set_level(gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));  // total de 1000 ms
    }
}

void blink37_task(void *pvParameter) {
    gpio_num_t gpio = (gpio_num_t)pvParameter;
    while (1) {
        gpio_set_level(gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        gpio_set_level(gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(200));  // total de 200 ms
    }
}

void app_main(void) {
    setup_gpio(BLINK8);
    setup_gpio(BLINK37);

    xTaskCreate(blink8_task, "Blink 1Hz Task", 2048, (void *)BLINK8, 1, NULL);
    xTaskCreate(blink37_task, "Blink 5Hz Task", 2048, (void *)BLINK37, 1, NULL);
}
