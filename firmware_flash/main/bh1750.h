#ifndef BH1750_H
#define BH1750_H

#include "driver/i2c.h"
#include "esp_err.h"

#define BH1750_ADDR 0x23
#define BH1750_CMD_POWER_ON 0x01
#define BH1750_CMD_RESET 0x07
#define BH1750_CMD_CONT_H_RES_MODE 0x10

esp_err_t bh1750_init(i2c_port_t i2c_num);
esp_err_t bh1750_start_measurement(i2c_port_t i2c_num);
esp_err_t bh1750_read_lux(i2c_port_t i2c_num, float *lux);

#endif