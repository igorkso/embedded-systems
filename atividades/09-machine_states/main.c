#include <stdio.h>
#include <math.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "int_i2c.h"
#include "esp_log.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#define btn1 GPIO_NUM_4
#define btn2 GPIO_NUM_1
#define BUZZER GPIO_NUM_37

#define LED0 GPIO_NUM_9
#define LED1 GPIO_NUM_11
#define LED2 GPIO_NUM_13
#define LED3 GPIO_NUM_14

#define SENSOR_ADC ADC1_CHANNEL_6
#define MAX_ADC 4095.0

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_8_BIT
#define LEDC_FREQUENCY 2000

#define I2C_MASTER_SCL_IO  GPIO_NUM_18
#define I2C_MASTER_SDA_IO  GPIO_NUM_10
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define LCD_ADDRESS        0x27
#define LCD_SIZE           DISPLAY_16X02

#define SD_MNT_POINT "/files"
#define MISO GPIO_NUM_2
#define MOSI GPIO_NUM_36
#define SCLK GPIO_NUM_35
#define CS   GPIO_NUM_38

#define DEBOUNCE_TIME_US 200000

typedef enum {
    STATE_INIT,
    STATE_IDLE,
    STATE_READ_TEMP,
    STATE_UPDATE_DISPLAY,
    STATE_UPDATE_LEDS,
    STATE_HANDLE_ALARM,
    STATE_LOG_SD
} app_state_t;

lcd_i2c_handle_t lcd;
int tempdef = 25;
int ntc = 20;

int64_t last_press_a = 0;
int64_t last_press_b = 0;

volatile bool bnt_a_interrupt = false;
volatile bool bnt_b_interrupt = false;

void i2c_master_init() {
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

static void IRAM_ATTR bnt_a_ISR_handler(void* arg) {
    gpio_intr_disable(btn1);
    bnt_a_interrupt = true;
}

static void IRAM_ATTR bnt_b_ISR_handler(void* arg) {
    gpio_intr_disable(btn2);
    bnt_b_interrupt = true;
}

float ler_temp() {
    int adc = adc1_get_raw(SENSOR_ADC);
    float resistencia = 10000.0 / ((MAX_ADC / (float)adc) - 1.0);
    const float BETA = 3950;
    float celsius = 1 / (log(resistencia / 10000.0) / BETA + 1.0 / 298.15) - 273.15;
    printf("Temp: %.2f\n", celsius);
    return celsius;
}

void update_leds(int temp, int alarme, bool alarme_ativo) {
    bool ligar = false;
    if (alarme_ativo) ligar = (esp_timer_get_time() / 500000) % 2;
    gpio_set_level(LED0, (alarme - temp <= 20 || alarme_ativo) && ligar);
    gpio_set_level(LED1, (alarme - temp <= 15 || alarme_ativo) && ligar);
    gpio_set_level(LED2, (alarme - temp <= 10 || alarme_ativo) && ligar);
    gpio_set_level(LED3, (alarme - temp <= 2  || alarme_ativo) && ligar);
}

void update_lcd_display(int ntc, int tempdef) {
    char buffer[20];
    lcd_i2c_cursor_set(&lcd, 0, 0);
    snprintf(buffer, sizeof(buffer), "NTC:%d Tem:%d", ntc, tempdef);
    lcd_i2c_print(&lcd, buffer);
}

void ligar_buzzer() {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 128);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void desligar_buzzer() {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void init_gpio() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED0) | (1ULL << LED1) | (1ULL << LED2) | (1ULL << LED3),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << btn1) | (1ULL << btn2);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_set_intr_type(btn1, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(btn2, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(btn1, bnt_a_ISR_handler, NULL);
    gpio_isr_handler_add(btn2, bnt_b_ISR_handler, NULL);
}

void inicializar_pwm_buzzer() {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL,
        .duty = 0,
        .gpio_num = BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&ledc_channel);
}

void sd_card_init() {
    esp_vfs_fat_sdmmc_mount_config_t config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    spi_bus_config_t bus_config = {
        .mosi_io_num = MOSI,
        .miso_io_num = MISO,
        .sclk_io_num = SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000
    };

    if (spi_bus_initialize(host.slot, &bus_config, SDSPI_DEFAULT_DMA) != ESP_OK) return;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdspi_mount(SD_MNT_POINT, &host, &slot_config, &config, &card);
}

void log_temp_to_sdcard(int ntc, int alarm) {
    FILE *file = fopen(SD_MNT_POINT"/log.txt", "a");
    if (file) {
        fprintf(file, "NTC:%d Alarme:%d\n", ntc, alarm);
        fclose(file);
    }
}

void app_main() {
    app_state_t state = STATE_INIT;
    bool alarme_ativo = false;

    while (1) {
        switch (state) {
            case STATE_INIT:
                init_gpio();
                i2c_master_init();
                lcd.address = LCD_ADDRESS;
                lcd.num = I2C_MASTER_NUM;
                lcd.backlight = 1;
                lcd.size = LCD_SIZE;
                lcd_i2c_init(&lcd);
                inicializar_pwm_buzzer();
                adc1_config_width(ADC_WIDTH_BIT_12);
                adc1_config_channel_atten(SENSOR_ADC, ADC_ATTEN_DB_11);
                sd_card_init();
                last_press_a = esp_timer_get_time();
                last_press_b = esp_timer_get_time();
                state = STATE_IDLE;
                break;

            case STATE_IDLE:
                if (esp_timer_get_time() - last_press_a > DEBOUNCE_TIME_US && bnt_a_interrupt) {
                    last_press_a = esp_timer_get_time();
                    tempdef += 5;
                    bnt_a_interrupt = false;
                    gpio_intr_enable(btn1);
                }
                if (esp_timer_get_time() - last_press_b > DEBOUNCE_TIME_US && bnt_b_interrupt) {
                    last_press_b = esp_timer_get_time();
                    tempdef -= 5;
                    bnt_b_interrupt = false;
                    gpio_intr_enable(btn2);
                }
                state = STATE_READ_TEMP;
                break;

            case STATE_READ_TEMP:
                ntc = (int)ler_temp();
                alarme_ativo = ntc >= tempdef;
                state = STATE_UPDATE_DISPLAY;
                break;

            case STATE_UPDATE_DISPLAY:
                update_lcd_display(ntc, tempdef);
                state = STATE_UPDATE_LEDS;
                break;

            case STATE_UPDATE_LEDS:
                update_leds(ntc, tempdef, alarme_ativo);
                state = STATE_HANDLE_ALARM;
                break;

            case STATE_HANDLE_ALARM:
                if (alarme_ativo) ligar_buzzer();
                else desligar_buzzer();
                state = STATE_LOG_SD;
                break;

            case STATE_LOG_SD:
                log_temp_to_sdcard(ntc, tempdef);
                vTaskDelay(pdMS_TO_TICKS(2000));
                state = STATE_IDLE;
                break;
        }
    }
}
