#include "bh1750.h"
#include "esp_log.h"

static const char *TAG = "BH1750";

esp_err_t bh1750_init(i2c_port_t i2c_num) {
    esp_err_t ret;
    uint8_t cmd = BH1750_CMD_POWER_ON;
    ret = i2c_master_write_to_device(i2c_num, BH1750_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Power ON failed");
        return ret;
    }
    cmd = BH1750_CMD_RESET;
    ret = i2c_master_write_to_device(i2c_num, BH1750_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "BH1750 initialized");
    }
    return ret;
}

esp_err_t bh1750_start_measurement(i2c_port_t i2c_num) {
    uint8_t cmd = BH1750_CMD_CONT_H_RES_MODE;
    return i2c_master_write_to_device(i2c_num, BH1750_ADDR, &cmd, 1, pdMS_TO_TICKS(100));
}

esp_err_t bh1750_read_lux(i2c_port_t i2c_num, float *lux) {
    uint8_t data[2];
    esp_err_t ret = i2c_master_read_from_device(i2c_num, BH1750_ADDR, data, 2, pdMS_TO_TICKS(200));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read failed");
        return ret;
    }
    uint16_t raw = (data[0] << 8) | data[1];
    *lux = (float)raw / 1.2f;
    return ESP_OK;
}