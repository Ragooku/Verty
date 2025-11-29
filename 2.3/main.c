#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "i2c_bus.h"
#include "bme280.h"

#define LED_GREEN_GPIO GPIO_NUM_13
#define LED_RED_GPIO GPIO_NUM_12
#define BUTTON_GPIO GPIO_NUM_21
#define I2C_MASTER_SCL_IO GPIO_NUM_9
#define I2C_MASTER_SDA_IO GPIO_NUM_8
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_BME280_ADDR BME280_I2C_ADDRESS_DEFAULT

#define MIN_TEMP 20.0
#define MAX_TEMP 30.0
#define MIN_HUMIDITY 20.0
#define MAX_HUMIDITY 60.0

typedef enum {
    SYSTEM_NORMAL,
    SYSTEM_ALERT
} system_state_t;

typedef struct {
    float temperature;
    float humidity;
} sensor_data_t;

typedef struct {
    system_state_t state;
    sensor_data_t data;
} system_message_t;

static QueueHandle_t sensor_queue = NULL;
static QueueHandle_t timer_queue = NULL;
static SemaphoreHandle_t print_mutex = NULL;
static volatile system_state_t current_state = SYSTEM_NORMAL;
static char last_alert_message[256] = "";
static bme280_handle_t bme280_handle = NULL;
static TimerHandle_t sensor_timer = NULL;

static volatile bool button_pressed_flag = false;

//прерывания
static void IRAM_ATTR button_isr_handler(void* arg) {
    button_pressed_flag = true;
}

void init_gpio(void) {
    gpio_config_t io_conf = {};
    
    io_conf.pin_bit_mask = (1ULL << LED_GREEN_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << LED_RED_GPIO);
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    gpio_set_level(LED_GREEN_GPIO, 0);
    gpio_set_level(LED_RED_GPIO, 0);
    
    gpio_install_isr_service(0);
    //прерывания 
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}
//ласт ошибка
void print_last_alert(void) {
    xSemaphoreTake(print_mutex, portMAX_DELAY);
    if (strlen(last_alert_message) > 0) {
        printf("Последняя ошибка: %s\n", last_alert_message);
    } else {
        printf("Ошибок нет\n");
    }
    xSemaphoreGive(print_mutex);
}

//выход за лимиты
int check_limits(float temperature, float humidity) {
    return (temperature < MIN_TEMP || temperature > MAX_TEMP || 
            humidity < MIN_HUMIDITY || humidity > MAX_HUMIDITY);
}

//вызов каждые
void read_sensor_data(TimerHandle_t xTimer) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int timer_event = 1;
    xQueueSendFromISR(timer_queue, &timer_event, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
//управление и обработка нажатия
void sensor_task(void *pvParameters) {
    sensor_data_t sensor_data;
    system_message_t message;
    float pressure;
    int timer_event;
    
    while (1) {
        if (xQueueReceive(timer_queue, &timer_event, portMAX_DELAY) == pdTRUE) {
            if (ESP_OK == bme280_read_temperature(bme280_handle, &sensor_data.temperature) &&
                ESP_OK == bme280_read_humidity(bme280_handle, &sensor_data.humidity) &&
                ESP_OK == bme280_read_pressure(bme280_handle, &pressure)) {
                
                if (check_limits(sensor_data.temperature, sensor_data.humidity)) {
                    message.state = SYSTEM_ALERT;
                    
                    xSemaphoreTake(print_mutex, portMAX_DELAY);
                    snprintf(last_alert_message, sizeof(last_alert_message),
                            "Ошибка!: Температура %.1f°C (range: %.1f-%.1f°C), "
                            "Влажность %.1f%% (range: %.1f-%.1f%%)",
                            sensor_data.temperature, MIN_TEMP, MAX_TEMP,
                            sensor_data.humidity, MIN_HUMIDITY, MAX_HUMIDITY);
                    printf("%s\n", last_alert_message);
                    xSemaphoreGive(print_mutex);
                } else {
                    message.state = SYSTEM_NORMAL;
                }
                
                message.data = sensor_data;
                
                if (xQueueSend(sensor_queue, &message, 0) != pdTRUE) {
                    printf("Не удалось отправить данные датчика в очередь\n");
                }
                
                xSemaphoreTake(print_mutex, portMAX_DELAY);
                printf("Температура: %.1f°C, Влажность: %.1f%%\n",
                       sensor_data.temperature, sensor_data.humidity);
                xSemaphoreGive(print_mutex);
            } else {
                printf("Ошибка сенсорного датчикa\n");
            }
        }
    }
}

void led_task(void *pvParameters) {
    system_message_t message;
    TickType_t last_blink_time = 0;
    int led_state = 0;
    
    while (1) {
        if (xQueueReceive(sensor_queue, &message, 0) == pdTRUE) {
            current_state = message.state;
        }
        
        // прерывания кнопки
        if (button_pressed_flag) {
            button_pressed_flag = false;  // дропаем 
            print_last_alert();
        }
        
        if (current_state == SYSTEM_NORMAL) {
            if ((xTaskGetTickCount() - last_blink_time) >= pdMS_TO_TICKS(1000)) {
                led_state = !led_state;
                gpio_set_level(LED_GREEN_GPIO, led_state);
                gpio_set_level(LED_RED_GPIO, 0);
                last_blink_time = xTaskGetTickCount();
            }
        } else {
            if ((xTaskGetTickCount() - last_blink_time) >= pdMS_TO_TICKS(250)) {
                led_state = !led_state;
                gpio_set_level(LED_GREEN_GPIO, 0);
                gpio_set_level(LED_RED_GPIO, led_state);
                last_blink_time = xTaskGetTickCount();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);
    bme280_handle = bme280_create(i2c_bus, I2C_BME280_ADDR);
    
    bme280_default_init(bme280_handle);
    
    init_gpio();
    
    sensor_queue = xQueueCreate(10, sizeof(system_message_t));
    timer_queue = xQueueCreate(5, sizeof(int));
    print_mutex = xSemaphoreCreateMutex();
    
    if (sensor_queue == NULL || timer_queue == NULL || print_mutex == NULL) {
        return;
    }
    
    sensor_timer = xTimerCreate(
        "SensorTimer",
        pdMS_TO_TICKS(5000), //5 сек
        pdTRUE,
        NULL,
        read_sensor_data
    );
    
    if (sensor_timer != NULL) {
        xTimerStart(sensor_timer, 0);
    }
    
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(led_task, "led_task", 2048, NULL, 4, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    
    if (sensor_timer != NULL) {
        xTimerDelete(sensor_timer, 0);
    }
    bme280_delete(&bme280_handle);
    i2c_bus_delete(&i2c_bus);
    vQueueDelete(sensor_queue);
    vQueueDelete(timer_queue);
    vSemaphoreDelete(print_mutex);
}