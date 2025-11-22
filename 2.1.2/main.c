#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <math.h>

// Выбираем АЦП
#define ADC_UNIT                    ADC_UNIT_1
#define ADC_CHANNEL                 ADC_CHANNEL_4
#define ADC_ATTENUATION             ADC_ATTEN_DB_12

// Параметры для расчета напряжения по формуле
#define ADC_MAX_VALUE               4095  // Максимальное значение для 12-битного АЦП
#define VOLTAGE_MAX                 3.3   // Максимальное напряжение (в вольтах)

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void adc_calibration_deinit(adc_cali_handle_t handle);

void app_main(void)
{
    static int adc_raw;
    static int voltage_mv;  // Напряжение в милливольтах от калибровки
    float voltage_formula;  // Напряжение по формуле

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
    
    // Калибровка АЦП
    adc_cali_handle_t adc_calibration_handle = NULL;
    bool do_calibration = adc_calibration_init(ADC_UNIT, ADC_CHANNEL, ADC_ATTENUATION, &adc_calibration_handle);
    
    while (1) {
        // Чтение "сырых" данных
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw));
        
        
        voltage_formula = VOLTAGE_MAX * (float)adc_raw / ADC_MAX_VALUE;
        
        printf("\nADC%d Channel[%d] Raw Data: %d\n", ADC_UNIT + 1, ADC_CHANNEL, adc_raw);
        printf("Метод 1 - U_изм = U_max * D_изм / D_max: %.3f V (%.1f mV)\n", voltage_formula, voltage_formula * 1000);

        if (do_calibration) {
            // Метод 2: Калиброванный результат (как в этом практикуме)
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_calibration_handle, adc_raw, &voltage_mv));
            printf("Meтод 2 - Кусочно-линейная: %.3f V (%d mV)\n", voltage_mv / 1000.0, voltage_mv);
            
            // Сравнение результатов
            float difference_v = (voltage_mv / 1000.0) - voltage_formula;
            float difference_mv = difference_v * 1000;
            float difference_percent = (difference_v / voltage_formula) * 100;
            
            printf("Разница: %.1f mV (%.2f%%)\n", difference_mv, difference_percent);
            
            if (fabs(difference_v) < 0.01) {  // Меньше 10мВ разницы
                printf("Не большая разница!\n");
            } else {
                printf("Большая разнциа!\n");
            }
        } 
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Никогда не будет выполнено, но удаляем всё, что создали
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc_handle));
    if (do_calibration) {
        adc_calibration_deinit(adc_calibration_handle);
    }
}

/*---------------------------------------------------------------
        Калибровка АЦП
---------------------------------------------------------------*/
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        printf("Calibration scheme is \"Curve Fitting\"\n");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        printf("Calibration scheme is \"Line Fitting\"\n");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        printf("Calibration Success\n");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        printf("eFuse not burnt, skip software calibration\n");
    } else {
        printf("Invalid arg or no memory\n");
    }

    return calibrated;
}

static void adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    printf("deregister \"Curve Fitting\" calibration scheme\n");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    printf("deregister \"Line Fitting\" calibration scheme\n");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}