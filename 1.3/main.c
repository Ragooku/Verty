#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_console.h"
#include "driver/uart.h"
#include "string.h"

#define LED GPIO_NUM_13
#define RELAY GPIO_NUM_6
#define BUTTON GPIO_NUM_19

#define PASSWORD "1111"
#define PASSWORD_LENGTH 4
#define UNLOCK_DURATION_MS 10000
#define INPUT_TIMEOUT_MS 10000

QueueHandle_t button_queue = NULL;

typedef enum {
    STATE_LOCKED,
    STATE_WAITING_PASSWORD,
    STATE_UNLOCKED
} lock_state_t;

typedef enum {
    EVENT_BUTTON_PRESSED,
    EVENT_TIMEOUT,
    EVENT_UNLOCK_TIMEOUT
} event_t;

static lock_state_t current_state = STATE_LOCKED;
static char input_buffer[PASSWORD_LENGTH + 1];
static int input_index = 0;
static int64_t input_start_time = 0;
static int64_t unlock_start_time = 0;

void init_console(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
}

void handle_button_press(void)
{
    if (current_state == STATE_WAITING_PASSWORD) {
        if (input_index == PASSWORD_LENGTH) {
            input_buffer[PASSWORD_LENGTH] = '\0';
            
            if (strcmp(input_buffer, PASSWORD) == 0) {
                printf("\nПароль верный\n");
                current_state = STATE_UNLOCKED;
                gpio_set_level(RELAY, 1);
                unlock_start_time = esp_timer_get_time();
            } else {
                printf("\nНеверный пароль\n");
                current_state = STATE_LOCKED;
            }
            
            input_index = 0;
            memset(input_buffer, 0, sizeof(input_buffer));
        } else {
            printf("\nНедостаточно цифр\n");
        }
    }
    else if (current_state == STATE_LOCKED) {
        printf("Пароль\n");
    }
    else if (current_state == STATE_UNLOCKED) {
        printf("Открыт\n");
    }
}

void check_console_input(void)
{
    if (current_state == STATE_LOCKED || current_state == STATE_WAITING_PASSWORD) {
        uint8_t data;
        int length = uart_read_bytes(UART_NUM_0, &data, 1, 0);
        
        if (length > 0) {
            if (data >= '0' && data <= '9') {
                if (input_index < PASSWORD_LENGTH) {
                    if (current_state == STATE_LOCKED) {
                        current_state = STATE_WAITING_PASSWORD;
                        input_start_time = esp_timer_get_time();
                        printf("Ввод пароля: ");
                    }
                    
                    input_buffer[input_index++] = data;
                    printf("%c", data);
                    fflush(stdout);
                    
                    if (input_index >= PASSWORD_LENGTH) {
                    }
                } else {
                }
            }
            else if (data == '\r' || data == '\n') {
                if (input_index > 0) {
                }
            }
            else if (data == 0x7F || data == 0x08) {
                if (input_index > 0) {
                    input_index--;
                    printf("\b \b");
                    fflush(stdout);
                    
                    if (input_index == 0) {
                        current_state = STATE_LOCKED;
                    }
                }
            }
        }
    }
}

void check_input_timeout(void)
{
    if (current_state == STATE_WAITING_PASSWORD) {
        int64_t current_time = esp_timer_get_time();
        if ((current_time - input_start_time) >= (INPUT_TIMEOUT_MS * 1000)) {
            printf("\nПерезапуск домофона\n");
            current_state = STATE_LOCKED;
            input_index = 0;
            memset(input_buffer, 0, sizeof(input_buffer));
        }
    }
}

void check_unlock_timeout(void)
{
    if (current_state == STATE_UNLOCKED) {
        int64_t current_time = esp_timer_get_time();
        if ((current_time - unlock_start_time) >= (UNLOCK_DURATION_MS * 1000)) {
            printf("Закрыто\n");
            current_state = STATE_LOCKED;
            gpio_set_level(RELAY, 0);
        }
    }
}

void update_led(void)
{
    static uint32_t led_ticks = 0;
    
    if (current_state == STATE_LOCKED) {
        gpio_set_level(LED, 1);
    } else if (current_state == STATE_UNLOCKED) {
        gpio_set_level(LED, (led_ticks / 5) % 2);
        led_ticks++;
    } else if (current_state == STATE_WAITING_PASSWORD) {
        gpio_set_level(LED, 1);
    }
}

static void IRAM_ATTR button_isr_handler(void* arg)
{
    event_t event = EVENT_BUTTON_PRESSED;
    xQueueSendFromISR(button_queue, &event, NULL);
}

void app_main(void)
{
    button_queue = xQueueCreate(10, sizeof(event_t));

    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(RELAY, GPIO_MODE_OUTPUT);
    gpio_set_direction(BUTTON, GPIO_MODE_INPUT);

    gpio_set_level(RELAY, 0);
    gpio_set_level(LED, 1);

    gpio_pullup_en(BUTTON);

    gpio_set_intr_type(BUTTON, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON, button_isr_handler, NULL);

    init_console();

    event_t event;
    while (true) {
        if (xQueueReceive(button_queue, &event, 0) == pdTRUE) {
            if (event == EVENT_BUTTON_PRESSED) {
                handle_button_press();
            }
        }
        
        check_console_input();
        check_input_timeout();
        check_unlock_timeout();
        update_led();
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}