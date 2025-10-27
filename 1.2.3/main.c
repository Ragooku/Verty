#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_GPIO_R         GPIO_NUM_12
#define LEDC_CHANNEL_R      LEDC_CHANNEL_0
#define LEDC_GPIO_G         GPIO_NUM_13
#define LEDC_CHANNEL_G      LEDC_CHANNEL_1
#define LEDC_GPIO_B         GPIO_NUM_11
#define LEDC_CHANNEL_B      LEDC_CHANNEL_2

#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT
#define LEDC_FADE_TIME      1000

// Преобразование R, G или B каналов в коэффициент заполнения с учетом разрешающей способности
#define RGB_TO_DUTY(x)  ((x) * (1 << LEDC_DUTY_RES) / 255)

void app_main(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 4000,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel_r = {
        .channel    = LEDC_CHANNEL_R,
        .duty       = 0,
        .gpio_num   = LEDC_GPIO_R,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER,
        .flags.output_invert = 0
    };
    ledc_channel_config_t ledc_channel_g = {
            .channel    = LEDC_CHANNEL_G,
            .duty       = 0,
            .gpio_num   = LEDC_GPIO_G,
            .speed_mode = LEDC_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER,
            .flags.output_invert = 0
    };
    ledc_channel_config_t ledc_channel_b = {
            .channel    = LEDC_CHANNEL_B,
            .duty       = 0,
            .gpio_num   = LEDC_GPIO_B,
            .speed_mode = LEDC_MODE,
            .hpoint     = 0,
            .timer_sel  = LEDC_TIMER,
            .flags.output_invert = 0
    };
    ledc_channel_config(&ledc_channel_r);
    ledc_channel_config(&ledc_channel_g);
    ledc_channel_config(&ledc_channel_b);

    // Initialize fade service.
    ledc_fade_func_install(0);

    uint8_t r, g, b;
    uint8_t state = 0;
    
    while (1)
    {
        switch(state++)
        {
            case 0: 
                r = 255; g = 0;   b = 0;
                break;
            case 1: 
                r = 255; g = 127; b = 0;
                break;
            case 2: 
                r = 255; g = 255; b = 0;
                break;
            case 3: 
                r = 0;   g = 255; b = 0;
                break;
            case 4: 
                r = 0;   g = 255; b = 255;
                break;
            case 5: 
                r = 0;   g = 0;   b = 255;
                break;
            case 6: 
                r = 255; g = 0;   b = 255;
                break;
        }
        state %= 7;
        
        ledc_set_fade_with_time(ledc_channel_r.speed_mode, ledc_channel_r.channel, RGB_TO_DUTY(r), LEDC_FADE_TIME);
        ledc_set_fade_with_time(ledc_channel_g.speed_mode, ledc_channel_g.channel, RGB_TO_DUTY(g), LEDC_FADE_TIME);
        ledc_set_fade_with_time(ledc_channel_b.speed_mode, ledc_channel_b.channel, RGB_TO_DUTY(b), LEDC_FADE_TIME);
        
        ledc_fade_start(ledc_channel_r.speed_mode, ledc_channel_r.channel, LEDC_FADE_WAIT_DONE);
        ledc_fade_start(ledc_channel_g.speed_mode, ledc_channel_g.channel, LEDC_FADE_WAIT_DONE);
        ledc_fade_start(ledc_channel_b.speed_mode, ledc_channel_b.channel, LEDC_FADE_WAIT_DONE);
    }
}