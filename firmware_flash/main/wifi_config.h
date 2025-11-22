#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

// Hàm check và khởi tạo kết nối
esp_err_t wifi_prov_init(void);

// Hàm bật AP để cấu hình
esp_err_t wifi_prov_start_ap(void);

// Helper check IP (để main check trước khi gửi data)
int wifi_is_connected(void);

// NVS Helpers
esp_err_t nvs_get_server_url(char *buf, size_t buf_len);
esp_err_t nvs_set_server_url(const char *url);