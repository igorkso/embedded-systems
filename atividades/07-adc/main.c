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

#define btn1 GPIO_NUM_4
#define btn2 GPIO_NUM_1
#define BUZZER GPIO_NUM_37

#define LED0 GPIO_NUM_9
#define LED1 GPIO_NUM_11
#define LED2 GPIO_NUM_13
#define LED3 GPIO_NUM_14  

#define SENSOR_GPIO 1
#define SENSOR_ADC ADC1_CHANNEL_6

#define MAX_ADC 4095.0
#define T0_KELVIN 298.15

//ajeitar o buzzer pwm
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

//static const char *TAG = "BIN_COUNTER";


#define DEBOUNCE_TIME_US 200000

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

void set_pwm_duty(uint32_t duty) {
  ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
  ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

float ler_temp() {
    int adc = adc1_get_raw(SENSOR_ADC);
    float resistencia = 10000.0 / ((MAX_ADC / (float)adc) - 1.0);
    const float BETA = 3950; 
    float celsius = 1 / (log(resistencia / 10000.0) / BETA + 1.0 / 298.15) - 273.15;
    printf("%f",celsius);
    return celsius;

}

void update_leds(int temp, int alarme, bool alarme_ativo) {
    bool ligar = false;

    if (alarme_ativo) {
        if ((esp_timer_get_time() / 500000) % 2) {
            ligar = true;
        } else {
            ligar = false;
        }
    }

    if (ntc >= alarme) {
        gpio_set_level(LED0, ligar);
        gpio_set_level(LED1, ligar);
        gpio_set_level(LED2, ligar);
        gpio_set_level(LED3, ligar);
        vTaskDelay(20/ portTICK_PERIOD_MS);

    } 
      else {
        if (alarme - temp <= 20) {
            gpio_set_level(LED0, true);
        } else {
            gpio_set_level(LED0, false);
        }

        if (alarme - temp <= 15) {
            gpio_set_level(LED1, true);
        } else {
            gpio_set_level(LED1, false);
        }

        if (alarme - temp <= 10) {
            gpio_set_level(LED2, true);
        } else {
            gpio_set_level(LED2, false);
        }

        if (alarme - temp <= 2) {
            gpio_set_level(LED3, true);
        } else {
            gpio_set_level(LED3, false);
        }
    }
}


void init_gpio() {

    // LEDs como saída
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED0) | (1ULL << LED1) | (1ULL << LED2) | (1ULL << LED3),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Botões como entrada
    io_conf.pin_bit_mask = (1ULL << btn1) | (1ULL << btn2);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Interrupções
    gpio_set_intr_type(btn1, GPIO_INTR_NEGEDGE); 
    gpio_set_intr_type(btn2, GPIO_INTR_NEGEDGE); 
    gpio_install_isr_service(0);
    gpio_isr_handler_add(btn1, bnt_a_ISR_handler, NULL);
    gpio_isr_handler_add(btn2, bnt_b_ISR_handler, NULL);

}

void inicializar_pwm_buzzer() {

  // Configuração do PWM (buzzer)
  ledc_timer_config_t ledc_timer = {
      .duty_resolution = LEDC_TIMER_8_BIT, // 0-255
      .freq_hz = 5000,                     // 5kHz
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .timer_num = LEDC_TIMER_0
  };
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
      .channel = LEDC_CHANNEL,
      .duty       = 0,
      .gpio_num   = BUZZER,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .timer_sel  = LEDC_TIMER_0
  };

  ledc_channel_config(&ledc_channel);
}

void ligar_buzzer() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void desligar_buzzer() {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void update_lcd_display(int ntc, int tempdef) {
    char buffer[20];
    
    vTaskDelay(10 / portTICK_PERIOD_MS); 
    lcd_i2c_cursor_set(&lcd, 0, 0);
    snprintf(buffer, sizeof(buffer), "NTC:%d Tem:%d", ntc, tempdef);
    lcd_i2c_print(&lcd, buffer);
    
    //lcd_i2c_cursor_set(&lcd, 0, 1);
    //snprintf(buffer, sizeof(buffer), "Tem: %d C", tempdef);
    //lcd_i2c_print(&lcd, buffer);
    
}


void app_main() {
  init_gpio();
  i2c_master_init();

  lcd.address = LCD_ADDRESS;
  lcd.num = I2C_MASTER_NUM;
  lcd.backlight = 1; 
  lcd.size = LCD_SIZE;
  lcd_i2c_init(&lcd);

  inicializar_pwm_buzzer();


  lcd_i2c_cursor_set(&lcd, 0, 0);
  lcd_i2c_print(&lcd, "Temp");
  lcd_i2c_cursor_set(&lcd, 0, 1);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  lcd_i2c_write(&lcd, 0, CLEAR_DISPLAY);
  
  ntc = (int)ler_temp();

  adc1_config_width(ADC_WIDTH_BIT_12);  // resolução de 12 bits
  adc1_config_channel_atten(SENSOR_ADC, ADC_ATTEN_DB_11);  // atenuação 

  //int temp = adc1_get_raw(ADC1_CHANNEL_17);
  
  last_press_a = esp_timer_get_time(); 
  last_press_b = esp_timer_get_time();

  bool alarme_ativo;
    
    while (1) {
        int64_t now = esp_timer_get_time();

        ntc = (int)ler_temp();
        bool alarme_ativo = ntc >= tempdef;

        if (bnt_a_interrupt) {
            if (now - last_press_a > DEBOUNCE_TIME_US) {
              last_press_a = now;
              tempdef = (tempdef + 5);
              printf("Botão A pressionado. Incremento = %d \n", tempdef);
           
              update_leds(ntc, tempdef, alarme_ativo);
            }
            bnt_a_interrupt = false;
            gpio_intr_enable(btn1);
        }

        if (bnt_b_interrupt) {
            if (now - last_press_b > DEBOUNCE_TIME_US) {
              last_press_b = now;
              last_press_a = now;
              tempdef = (tempdef - 5);
              printf("Botão B pressionado. Decremento = %d\n", tempdef);        

              update_leds(ntc, tempdef, alarme_ativo);

            }
            bnt_b_interrupt = false;
            gpio_intr_enable(btn2);
        }

      if (alarme_ativo) {
          ligar_buzzer();
      } else {
          desligar_buzzer();
      }
      
      update_leds(ntc, tempdef, alarme_ativo);
      update_lcd_display(ntc,tempdef);

      vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}


