#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_sleep.h" // Thư viện cho Deep Sleep

// Include các thư viện tự viết
#include "bh1750.h"
#include "wifi_config.h"

static const char *TAG = "MAIN_APP";

// --- CẤU HÌNH PHẦN CỨNG ---
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_FREQ_HZ 100000

#define LED_GPIO GPIO_NUM_2         // Đèn báo trạng thái (D2)
#define BOOT_BUTTON_GPIO GPIO_NUM_0 // Nút BOOT để Factory Reset

// --- CẤU HÌNH LOGIC ---
#define SERVER_URL_DEFAULT "http://192.168.1.10:8000/api/sensor" // URL mặc định nếu chưa chỉnh
#define POST_INTERVAL_SECONDS 10    // Gửi dữ liệu mỗi 10 giây
#define WIFI_FAIL_THRESHOLD 5       // Mất mạng 5 lần liên tiếp thì đi ngủ
#define SLEEP_DURATION_SEC 60       // Thời gian ngủ (60 giây)

char g_server_url[256]; // Biến toàn cục lưu URL Server

/* --- KHỞI TẠO I2C --- */
static void i2c_master_init(void){
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/* --- HÀM GỬI DỮ LIỆU LÊN SERVER (HTTP POST) --- */
static void send_lux_to_server(float lux){
    char post_data[128];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    // Tạo JSON: {"device_id":"MAC_ADDRESS", "lux": 123.45}
    snprintf(post_data, sizeof(post_data), 
             "{\"device_id\":\"%02x%02x%02x%02x%02x%02x\",\"lux\":%.2f}",
             mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], lux);

    esp_http_client_config_t config = {
        .url = g_server_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .buffer_size = 512,
        .disable_auto_redirect = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Khong khoi tao duoc HTTP Client");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "DA GUI: %s | Status: %d", post_data, esp_http_client_get_status_code(client));
        // Nháy đèn 1 cái báo hiệu thành công
        gpio_set_level(LED_GPIO, 1); 
        vTaskDelay(pdMS_TO_TICKS(100)); 
        gpio_set_level(LED_GPIO, 0);
    } else {
        ESP_LOGE(TAG, "Loi HTTP POST: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

/* --- TASK CẢM BIẾN & XỬ LÝ DEEP SLEEP --- */
void sensor_task(void *pvParameter) {
    // 1. Khởi tạo cảm biến
    i2c_master_init();
    if (bh1750_init(I2C_MASTER_NUM) != ESP_OK) {
        ESP_LOGE(TAG, "Loi khoi tao BH1750! Task se tu huy.");
        vTaskDelete(NULL);
    }
    bh1750_start_measurement(I2C_MASTER_NUM);

    int fail_count = 0; // Biến đếm số lần lỗi liên tiếp

    while(1) {
        float lux = 0;
        
        // 2. Đọc giá trị cảm biến
        if (bh1750_read_lux(I2C_MASTER_NUM, &lux) == ESP_OK) {
            ESP_LOGI(TAG, "Do duoc: %.2f Lux", lux);

            // 3. Kiểm tra kết nối mạng
            if (wifi_is_connected()) {
                // Có mạng -> Gửi dữ liệu
                send_lux_to_server(lux);
                fail_count = 0; // Reset bộ đếm lỗi vì gửi thành công
            } else {
                // Mất mạng
                ESP_LOGW(TAG, "Mat ket noi WiFi! (Lan %d/%d)", fail_count + 1, WIFI_FAIL_THRESHOLD);
                fail_count++;
            }
        } else {
            ESP_LOGE(TAG, "Khong doc duoc cam bien BH1750");
            fail_count++; // Lỗi phần cứng cũng tính là lỗi
        }

        // 4. Logic Kích hoạt DEEP SLEEP (Tiết kiệm điện)
        if (fail_count >= WIFI_FAIL_THRESHOLD) {
            ESP_LOGE(TAG, "Mat ket noi qua lau! Di ngu %d giay de tiet kiem dien...", SLEEP_DURATION_SEC);
            
            // Nháy đèn 3 lần báo hiệu sắp tắt máy
            for(int i=0; i<3; i++) {
                gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(200));
                gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(200));
            }

            // Cấu hình thời gian ngủ (đổi giây sang micro-giây)
            esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SEC * 1000000ULL);
            
            // Bắt đầu ngủ (Chip tắt tại đây)
            esp_deep_sleep_start();
        }

        // Chờ 10 giây trước khi đo lần tiếp theo
        vTaskDelay(pdMS_TO_TICKS(POST_INTERVAL_SECONDS * 1000));
    }
}

/* --- TASK NÚT NHẤN RESET (FACTORY RESET) --- */
void reset_button_task(void *pvParameter) {
    // Cấu hình nút BOOT (GPIO 0) là Input, kéo lên nguồn
    gpio_reset_pin(BOOT_BUTTON_GPIO);
    gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_BUTTON_GPIO, GPIO_PULLUP_ONLY);

    while (1) {
        // Nút BOOT nhấn xuống là mức 0 (LOW)
        if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
            ESP_LOGW(TAG, "Nut nhan duoc bam! Giu 3s de Reset...");
            
            int hold_time_ms = 0;
            // Vòng lặp kiểm tra giữ nút
            while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                hold_time_ms += 100;
                
                // Đèn nháy chậm báo hiệu đang đếm
                if (hold_time_ms % 500 == 0) {
                    gpio_set_level(LED_GPIO, !gpio_get_level(LED_GPIO));
                }

                // Nếu giữ đủ 3000ms (3 giây)
                if (hold_time_ms >= 3000) {
                    ESP_LOGE(TAG, "--- FACTORY RESET KICH HOAT ---");
                    
                    // Nháy đèn loạn xạ báo hiệu xóa
                    for(int i=0; i<10; i++){
                         gpio_set_level(LED_GPIO, 1); vTaskDelay(50/portTICK_PERIOD_MS);
                         gpio_set_level(LED_GPIO, 0); vTaskDelay(50/portTICK_PERIOD_MS);
                    }
                    
                    // Xóa sạch bộ nhớ NVS (Bay màu SSID/Pass)
                    nvs_flash_erase();
                    nvs_flash_init();
                    
                    // Khởi động lại chip
                    esp_restart();
                }
            }
        }
        // Kiểm tra nút bấm mỗi 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* --- HÀM CHÍNH (ENTRY POINT) --- */
void app_main(void){
    // 1. Khởi tạo NVS (Bộ nhớ lưu trữ)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Khởi tạo Hệ thống mạng & Sự kiện
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Cấu hình đèn LED
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // 3. Chạy Task Nút Reset (Luôn chạy song song để canh me người dùng bấm)
    xTaskCreate(&reset_button_task, "reset_btn", 2048, NULL, 5, NULL);

    // 4. Lấy URL Server từ NVS
    if (nvs_get_server_url(g_server_url, sizeof(g_server_url)) != ESP_OK){
        // Nếu không có, dùng mặc định
        strcpy(g_server_url, SERVER_URL_DEFAULT);
    }
    ESP_LOGI(TAG, "Server URL: %s", g_server_url);

    // 5. Kiểm tra và Khởi tạo WiFi
    // Hàm này sẽ kiểm tra xem trong NVS đã có WiFi chưa
    esp_err_t wifi_status = wifi_prov_init();

    if (wifi_status == ESP_ERR_NVS_NOT_FOUND) {
        // TRƯỜNG HỢP 1: Chưa có WiFi (Máy mới hoặc vừa Reset)
        ESP_LOGW(TAG, "Chua co cau hinh WiFi -> Bat che do AP...");
        
        // Bật đèn sáng đứng báo hiệu đang chờ cấu hình
        gpio_set_level(LED_GPIO, 1); 
        
        // Bật chế độ phát WiFi để người dùng vào cài đặt
        wifi_prov_start_ap();
        
        // Main dừng tại đây, không chạy Sensor Task
    } else {
        // TRƯỜNG HỢP 2: Đã có WiFi -> Chạy luôn
        ESP_LOGI(TAG, "Da co cau hinh -> Chay Sensor Task...");
        
        // Tạo Task Cảm biến (Task này sẽ tự xử lý việc gửi và Deep Sleep)
        xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    }
}