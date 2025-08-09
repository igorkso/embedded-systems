#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "int_i2c.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define BOTAO_CIMA GPIO_NUM_4
#define BOTAO_BAIXO GPIO_NUM_1
#define BUZZER GPIO_NUM_37

#define LED0 GPIO_NUM_9
#define LED1 GPIO_NUM_11
#define LED2 GPIO_NUM_13
#define LED3 GPIO_NUM_14

#define DISP_SEG_A GPIO_NUM_15
#define DISP_SEG_B GPIO_NUM_16
#define DISP_SEG_C GPIO_NUM_17
#define DISP_SEG_D GPIO_NUM_8
#define DISP_SEG_E GPIO_NUM_3
#define DISP_SEG_F GPIO_NUM_42
#define DISP_SEG_G GPIO_NUM_41
#define DISP_COM   GPIO_NUM_40

#define I2C_SDA GPIO_NUM_10
#define I2C_SCL GPIO_NUM_18
#define LCD_ADDR 0x27
#define LCD_SIZE DISPLAY_16X02
#define I2C_NUM I2C_NUM_0

#define SD_MISO GPIO_NUM_2
#define SD_MOSI GPIO_NUM_36
#define SD_CLK  GPIO_NUM_35
#define SD_CS   GPIO_NUM_38
#define SD_MOUNT_POINT "/files"

#define TEMP_ADC ADC1_CHANNEL_6
#define MAX_ADC 4095.0

#define INTERVALO_LCD_MS       500
#define INTERVALO_DISPLAY_MS   50
#define INTERVALO_BUZZER_MS    200
#define INTERVALO_LOG_MS       10000

SemaphoreHandle_t mutex_temp;
lcd_i2c_handle_t lcd;
int temp_atual = 20;
int temp_limite = 25;
bool alarme_ativo = false;
bool sd_pronto = false;

float ler_temp() {
    int adc = adc1_get_raw(TEMP_ADC);
    float resistencia = 10000.0 / ((MAX_ADC / (float)adc) - 1.0);
    const float BETA = 3950;
    float temp = 1 / (log(resistencia / 10000.0) / BETA + 1.0 / 298.15) - 273.15;
    return temp;
}

void escrever_sd() {
    FILE *f = fopen(SD_MOUNT_POINT"/log.txt", "a");
    if (f) {
        fprintf(f, "NTC:%d Al:%d\n", temp_atual, temp_limite);
        fclose(f);
    }
}

void mostrar_digito(char d) {
    gpio_set_level(DISP_SEG_A, 1);
    gpio_set_level(DISP_SEG_B, 1);
    gpio_set_level(DISP_SEG_C, 1);
    gpio_set_level(DISP_SEG_D, 1);
    gpio_set_level(DISP_SEG_E, 1);
    gpio_set_level(DISP_SEG_F, 1);
    gpio_set_level(DISP_SEG_G, 1);
    switch (d) {
        case '0': gpio_set_level(DISP_SEG_A, 0); gpio_set_level(DISP_SEG_B, 0);
                  gpio_set_level(DISP_SEG_C, 0); gpio_set_level(DISP_SEG_D, 0);
                  gpio_set_level(DISP_SEG_E, 0); gpio_set_level(DISP_SEG_F, 0); break;
        case '3': gpio_set_level(DISP_SEG_A, 0); gpio_set_level(DISP_SEG_B, 0);
                  gpio_set_level(DISP_SEG_C, 0); gpio_set_level(DISP_SEG_D, 0);
                  gpio_set_level(DISP_SEG_G, 0); break;
        case '7': gpio_set_level(DISP_SEG_A, 0); gpio_set_level(DISP_SEG_B, 0);
                  gpio_set_level(DISP_SEG_C, 0); break;
        case 'D': gpio_set_level(DISP_SEG_B, 0); gpio_set_level(DISP_SEG_C, 0);
                  gpio_set_level(DISP_SEG_D, 0); gpio_set_level(DISP_SEG_E, 0);
                  gpio_set_level(DISP_SEG_G, 0); break;
        case 'F': gpio_set_level(DISP_SEG_A, 0); gpio_set_level(DISP_SEG_E, 0);
                  gpio_set_level(DISP_SEG_F, 0); gpio_set_level(DISP_SEG_G, 0); break;
        default: break;
    }
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
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000
    };

    if (spi_bus_initialize(host.slot, &bus_config, SDSPI_DEFAULT_DMA) != ESP_OK) return;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = host.slot;

    if (esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &config, &card) == ESP_OK) {
        sd_pronto = true;
    }
}

void fsm_task(void *pv) {
    int64_t t_lcd = 0, t_display = 0, t_buzzer = 0, t_log = 0;
    bool ligado = true;
    char digito = ' ';

    while (1) {
        int64_t agora = esp_timer_get_time() / 1000;
        float temp = ler_temp();

        if (xSemaphoreTake(mutex_temp, portMAX_DELAY)) {
            temp_atual = (int)temp;
            alarme_ativo = temp_atual >= temp_limite;
            xSemaphoreGive(mutex_temp);
        }

        if (!gpio_get_level(BOTAO_CIMA)) {
            xSemaphoreTake(mutex_temp, portMAX_DELAY);
            temp_limite += 5;
            xSemaphoreGive(mutex_temp);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        if (!gpio_get_level(BOTAO_BAIXO)) {
            xSemaphoreTake(mutex_temp, portMAX_DELAY);
            temp_limite -= 5;
            xSemaphoreGive(mutex_temp);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        if (agora - t_lcd >= INTERVALO_LCD_MS) {
            char buf[20];
            xSemaphoreTake(mutex_temp, portMAX_DELAY);
            int temp = temp_atual, limite = temp_limite;
            xSemaphoreGive(mutex_temp);
            lcd_i2c_cursor_set(&lcd, 0, 0);
            snprintf(buf, sizeof(buf), "Temp: %d C", temp);
            lcd_i2c_print(&lcd, buf);
            lcd_i2c_cursor_set(&lcd, 0, 1);
            snprintf(buf, sizeof(buf), "Limite: %d C", limite);
            lcd_i2c_print(&lcd, buf);
            t_lcd = agora;
        }

        if (agora - t_display >= INTERVALO_DISPLAY_MS) {
            int temp = temp_atual, limite = temp_limite;
            bool alarme = alarme_ativo;
            int diff = limite - temp;

            if (alarme) digito = 'F';
            else if (diff <= 2) digito = 'D';
            else if (diff <= 10) digito = '7';
            else if (diff <= 15) digito = '3';
            else if (diff <= 20) digito = '0';
            else digito = ' ';

            if (alarme) ligado = !ligado;
            else ligado = true;

            if (ligado) mostrar_digito(digito);
            else mostrar_digito(' ');

            t_display = agora;
        }

        if (agora - t_buzzer >= INTERVALO_BUZZER_MS) {
            if (alarme_ativo) {
                static bool on = false;
                on = !on;
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 128 : 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            } else {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            }
            t_buzzer = agora;
        }

        if (sd_pronto && agora - t_log >= INTERVALO_LOG_MS) {
            escrever_sd();
            t_log = agora;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main() {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << LED0) | (1ULL << LED1) |
                        (1ULL << LED2) | (1ULL << LED3) |
                        (1ULL << DISP_SEG_A) | (1ULL << DISP_SEG_B) |
                        (1ULL << DISP_SEG_C) | (1ULL << DISP_SEG_D) |
                        (1ULL << DISP_SEG_E) | (1ULL << DISP_SEG_F) |
                        (1ULL << DISP_SEG_G) | (1ULL << DISP_COM),
        .mode = GPIO_MODE_OUTPUT
    };
    gpio_config(&io);
    gpio_set_level(DISP_COM, 1);

    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BOTAO_CIMA) | (1ULL << BOTAO_BAIXO),
        .mode = GPIO_MODE_INPUT
    };
    gpio_config(&btn);

    ledc_timer_config_t t = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .channel = LEDC_CHANNEL_0,
        .gpio_num = BUZZER,
        .duty = 0,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0
    };
    ledc_channel_config(&c);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(TEMP_ADC, ADC_ATTEN_DB_12);

    i2c_config_t i2c = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000
    };
    i2c_param_config(I2C_NUM, &i2c);
    i2c_driver_install(I2C_NUM, i2c.mode, 0, 0, 0);

    lcd.address = LCD_ADDR;
    lcd.num = I2C_NUM;
    lcd.backlight = 1;
    lcd.size = LCD_SIZE;
    lcd_i2c_init(&lcd);

    sd_card_init();
    mutex_temp = xSemaphoreCreateMutex();
    xTaskCreate(fsm_task, "fsm", 4096, NULL, 1, NULL);
}

