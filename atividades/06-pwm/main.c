#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "int_i2c.h"

#define LED_PIN_0     GPIO_NUM_10
#define LED_PIN_1     GPIO_NUM_11
#define LED_PIN_2     GPIO_NUM_12
#define LED_PIN_3     GPIO_NUM_13
#define LED_PWM_PIN   GPIO_NUM_15
#define BUTTON_NEXT   GPIO_NUM_14
#define BUTTON_STEP   GPIO_NUM_7

#define LEDC_TIMER LEDC_TIMER_0 
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY 5000

#define I2C_MASTER_SCL_IO  GPIO_NUM_2
#define I2C_MASTER_SDA_IO  GPIO_NUM_1
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define LCD_ADDRESS        0x27      
#define LCD_SIZE           DISPLAY_16X02
static const char *TAG = "BIN_COUNTER";

#define DEBOUNCE_TIME_US 200000

lcd_i2c_handle_t lcd;
uint8_t counter = 0;

int64_t last_press_next = 0;
int64_t last_press_step = 0;

volatile bool next_interrupt = false;
volatile bool step_interrupt = false;

void init_i2c_master() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

static void IRAM_ATTR button_next_isr_handler(void* arg) {
    gpio_intr_disable(BUTTON_NEXT);
    next_interrupt = true;
}

static void IRAM_ATTR button_step_isr_handler(void* arg) {
    gpio_intr_disable(BUTTON_STEP);
    step_interrupt = true;
}

void update_led_display_and_pwm(uint8_t value) {
    gpio_set_level(LED_PIN_0, value & 0x01);
    gpio_set_level(LED_PIN_1, (value >> 1) & 0x01);
    gpio_set_level(LED_PIN_2, (value >> 2) & 0x01);
    gpio_set_level(LED_PIN_3, (value >> 3) & 0x01);

    uint32_t duty = (value & 0x0F) * (255 / 15);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void configure_gpio_and_pwm() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN_0) | (1ULL << LED_PIN_1) | (1ULL << LED_PIN_2) | (1ULL << LED_PIN_3),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << BUTTON_NEXT) | (1ULL << BUTTON_STEP);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_set_intr_type(BUTTON_NEXT, GPIO_INTR_NEGEDGE); 
    gpio_set_intr_type(BUTTON_STEP, GPIO_INTR_NEGEDGE); 
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_NEXT, button_next_isr_handler, NULL);
    gpio_isr_handler_add(BUTTON_STEP, button_step_isr_handler, NULL);

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = LEDC_FREQUENCY,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL,
        .duty       = 0,
        .gpio_num   = LED_PWM_PIN,
        .speed_mode = LEDC_MODE,
        .timer_sel  = LEDC_TIMER
    };
    ledc_channel_config(&ledc_channel);
}

static void lcd_display_counter_value(uint8_t count) {
    char buffer[20];
    lcd_i2c_write(&lcd, 0, CLEAR_DISPLAY);
    vTaskDelay(10 / portTICK_PERIOD_MS); 

    lcd_i2c_cursor_set(&lcd, 0, 0);
    snprintf(buffer, sizeof(buffer), "Bin: 0x%X", count);
    lcd_i2c_print(&lcd, buffer);
    
    lcd_i2c_cursor_set(&lcd, 0, 1);
    snprintf(buffer, sizeof(buffer), "Hex: %d", count);
    lcd_i2c_print(&lcd, buffer);
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void app_main() {
    configure_gpio_and_pwm();
    init_i2c_master();

    lcd.address = LCD_ADDRESS;
    lcd.num = I2C_MASTER_NUM;
    lcd.backlight = 1; 
    lcd.size = LCD_SIZE;
    lcd_i2c_init(&lcd);

    lcd_i2c_cursor_set(&lcd, 0, 0);
    lcd_i2c_print(&lcd, "Contador");
    lcd_i2c_cursor_set(&lcd, 0, 1);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    lcd_i2c_write(&lcd, 0, CLEAR_DISPLAY);

    lcd_display_counter_value(counter);

    last_press_next = esp_timer_get_time(); 
    last_press_step = esp_timer_get_time();
    
    while (1) {
        int64_t now = esp_timer_get_time();

        if (next_interrupt) {
            if (now - last_press_next > DEBOUNCE_TIME_US) {
                last_press_next = now;
                counter = (counter + 1) & 0x0F;
                update_led_display_and_pwm(counter);
                printf("BUTTON_NEXT pressionado. Contador = %d (0x%X)\n", counter, counter);
                lcd_display_counter_value(counter);
            }
            next_interrupt = false;
            gpio_intr_enable(BUTTON_NEXT);
        }

        if (step_interrupt) {
            if (now - last_press_step > DEBOUNCE_TIME_US) {
                last_press_step = now;
                last_press_next = now;
                counter = (counter - 1) & 0x0F;
                update_led_display_and_pwm(counter);
                printf("BUTTON_STEP pressionado. Contador = %d (0x%X)\n", counter, counter);
                lcd_display_counter_value(counter);
            }
            step_interrupt = false;
            gpio_intr_enable(BUTTON_STEP);
        }

        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}
