#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO           9     // GPIO для SCL
#define I2C_MASTER_SDA_IO           8     // GPIO для SDA
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000 // 100kHz
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0

#define ADXL345_ADDR                0x53   // Адрес ADXL345 (SDO на GND)
#define ADXL345_REG_POWER_CTL       0x2D
#define ADXL345_REG_DATA_FORMAT     0x31
#define ADXL345_REG_DATAX0          0x32

static const char *TAG = "ADXL345";

// Инициализация I2C
esp_err_t i2c_master_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

// Запись одного байта в регистр ADXL345
esp_err_t adxl345_write_reg(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t err;

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADXL345_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return err;
}

// Чтение нескольких байт из регистра ADXL345
esp_err_t adxl345_read_regs(uint8_t reg_addr, uint8_t *data, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t err;

    // Отправляем адрес регистра
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADXL345_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    // Запрос чтения
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ADXL345_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return err;
}

void app_main(void)
{
    esp_err_t err = i2c_master_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "I2C initialized");

    // Инициализация ADXL345
    err = adxl345_write_reg(ADXL345_REG_POWER_CTL, 0x08);  // Включаем измерения
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADXL345 POWER_CTL write failed");
        return;
    }

    err = adxl345_write_reg(ADXL345_REG_DATA_FORMAT, 0x08); // FULL_RES, +-2g
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ADXL345 DATA_FORMAT write failed");
        return;
    }

    while (1) {
        uint8_t data[6];
        err = adxl345_read_regs(ADXL345_REG_DATAX0, data, 6);
        if (err == ESP_OK) {
            int16_t x = (int16_t)((data[1] << 8) | data[0]);
            int16_t y = (int16_t)((data[3] << 8) | data[2]);
            int16_t z = (int16_t)((data[5] << 8) | data[4]);

            ESP_LOGI(TAG, "Accel X: %d, Y: %d, Z: %d", x, y, z);
        } else {
            ESP_LOGE(TAG, "Error reading accelerometer data: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}