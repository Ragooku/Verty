#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

// Выбираем АЦП
#define ADC_UNIT                    ADC_UNIT_1
#define ADC_CHANNEL                 ADC_CHANNEL_4
#define ADC_ATTENUATION             ADC_ATTEN_DB_12#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

// Выбираем АЦП
#define ADC_UNIT                    ADC_UNIT_1
#define ADC_CHANNEL                 ADC_CHANNEL_4
#define ADC_ATTENUATION             ADC_ATTEN_DB_12

// Параметры для расчета напряжения
#define ADC_MAX_VALUE               4095  // Максимальное значение для 12-битного АЦП
#define VOLTAGE_MAX                 3.3   // Максимальное напряжение (в вольтах)

void app_main(void)
{
    static int adc_raw;
    float voltage;

    // Инициализация АЦП
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // Настройка АЦП
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTENUATION,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));
    
    while (1)
    {
        // Чтение "сырых" данных
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw));
        
        // Расчет напряжения по формуле: U_изм = U_max * D_изм / D_max
        voltage = VOLTAGE_MAX * (float)adc_raw / ADC_MAX_VALUE;
        
        printf("ADC%d Channel[%d] Raw Data: %d, Voltage: %.2f V\n", 
               ADC_UNIT + 1, ADC_CHANNEL, adc_raw, voltage);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Никогда не выполнется, но удаляем всё, что создали
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle));
}
#define ADC_MAX_VALUE               4095  
#define VOLTAGE_MAX                 3.3   

void app_main(void)
{
    static int adc_raw;
    float voltage;

    // Инициализация АЦП
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // Настройка АЦП
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTENUATION,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));
    
    while (1)
    {
        // Чтение "сырых" данных
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw));
        
        voltage = VOLTAGE_MAX * (float)adc_raw / ADC_MAX_VALUE;
        
        printf("ADC%d Channel[%d] Raw Data: %d, Voltage: %.2f V\n", 
               ADC_UNIT + 1, ADC_CHANNEL, adc_raw, voltage);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Никогда не выполнется, но удаляем всё, что создали
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle));
}