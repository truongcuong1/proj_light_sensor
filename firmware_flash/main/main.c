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
#include "esp_http_client.h" // Sử dụng giao thức HTTP
#include "esp_mac.h"
#include "esp_sleep.h"

// Import các thư viện module
#include "bh1750.h"
#include "wifi_config.h"

static const char *TAG = "MAIN_APP";

/* =============================================================== */
/* KHU VỰC CẤU HÌNH PHẦN CỨNG (ĐÃ CHUẨN HÓA)               */
/* =============================================================== */

// 1. Cấu hình I2C (Cảm biến BH1750)
// Mặc định ESP32: SDA=21, SCL=22. Nếu mạch in khác, hãy sửa số ở đây.
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   21
#define I2C_MASTER_SCL_IO   22
#define I2C_MASTER_FREQ_HZ  100000

// 2. Cấu hình Đèn LED báo trạng thái
#define LED_GPIO            GPIO_NUM_2 

// 3. Cấu hình Nút Chức Năng (Nút SW3 trên mạch in)
// Theo sơ đồ bạn cung cấp: Nút Function nối vào GPIO 32
#define FUNC_BUTTON_GPIO    GPIO_NUM_32 

// 4. Cấu hình Mức Logic của Nút Nhấn (Active Level)
// Theo sơ đồ: Có trở R6 kéo lên 3V3, nhấn nối xuống GND
// -> Nhấn = Mức 0 (Active Low)
#define BUTTON_ACTIVE_LEVEL 0 

/* =============================================================== */

// --- CẤU HÌNH LOGIC HỆ THỐNG ---
#define SERVER_URL_DEFAULT "http://192.168.1.10:8000/api/sensor"
#define POST_INTERVAL_SECONDS 10    // Chu kỳ gửi dữ liệu (giây)
#define WIFI_FAIL_THRESHOLD 5       // Số lần mất mạng cho phép trước khi ngủ
#define SLEEP_DURATION_SEC 60       // Thời gian ngủ tiết kiệm pin (giây)

char g_server_url[256];

/* --- 1. KHỞI TẠO I2C --- */
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

/* --- 2. GỬI DỮ LIỆU HTTP POST --- */
static void send_lux_to_server(float lux){
    // Chỉ gửi nếu đã kết nối WiFi thành công
    if (!wifi_is_connected()) return; 

    char post_data[128];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    // Đóng gói JSON: {"device_id": "...", "lux": 123.4}
    snprintf(post_data, sizeof(post_data), 
             "{\"device_id\":\"%02x%02x%02x%02x%02x%02x\",\"lux\":%.2f}",
             mac[0],mac[1],mac[2],mac[3],mac[4],mac[5], lux);

    esp_http_client_config_t config = {
        .url = g_server_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .buffer_size = 512,
        .disable_auto_redirect = true,
        // .is_async = false // Bỏ comment dòng này nếu gặp lỗi trên một số bản IDF cũ
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Loi khoi tao HTTP Client");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Gui thanh cong: %s | Status: %d", post_data, esp_http_client_get_status_code(client));
        // Nháy đèn 1 cái báo hiệu OK
        gpio_set_level(LED_GPIO, 1); 
        vTaskDelay(pdMS_TO_TICKS(100)); 
        gpio_set_level(LED_GPIO, 0);
    } else {
        ESP_LOGE(TAG, "Loi gui HTTP: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

/* --- 3. TASK ĐO CẢM BIẾN & XỬ LÝ MẤT MẠNG --- */
void sensor_task(void *pvParameter) {
    // Khởi tạo phần cứng đo đạc
    i2c_master_init();
    bh1750_init(I2C_MASTER_NUM);
    bh1750_start_measurement(I2C_MASTER_NUM);

    int fail_count = 0; // Đếm số lần mất mạng liên tiếp

    while(1) {
        float lux = 0;
        // Đọc cảm biến
        if (bh1750_read_lux(I2C_MASTER_NUM, &lux) == ESP_OK) {
            ESP_LOGI(TAG, "Gia tri Lux: %.2f", lux);

            // Kiểm tra WiFi
            if (wifi_is_connected()) {
                send_lux_to_server(lux);
                fail_count = 0; // Reset lỗi nếu gửi thành công
            } else {
                ESP_LOGW(TAG, "Mat WiFi (Lan %d)", fail_count + 1);
                fail_count++;
            }
        } else {
            ESP_LOGE(TAG, "Loi: Khong doc duoc cam bien BH1750");
            fail_count++; // Lỗi phần cứng cũng tính vào fail count để ngủ đông
        }

        // Logic Deep Sleep (Ngủ đông nếu mất mạng quá lâu để tiết kiệm pin)
        if (fail_count >= WIFI_FAIL_THRESHOLD) {
            ESP_LOGE(TAG, "Mat ket noi qua lau -> Deep Sleep %d giay...", SLEEP_DURATION_SEC);
            
            // Nháy đèn liên tục 5 lần báo hiệu sắp ngủ
            for(int i=0; i<5; i++) {
                gpio_set_level(LED_GPIO, 1); vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(LED_GPIO, 0); vTaskDelay(pdMS_TO_TICKS(100));
            }

            // Cài đặt thời gian ngủ và bắt đầu ngủ
            esp_sleep_enable_timer_wakeup(SLEEP_DURATION_SEC * 1000000ULL);
            esp_deep_sleep_start();
            // Sau dòng này chip sẽ tắt, code không chạy nữa. 
            // Khi tỉnh dậy nó sẽ chạy lại từ app_main.
        }

        // Chờ chu kỳ tiếp theo
        vTaskDelay(pdMS_TO_TICKS(POST_INTERVAL_SECONDS * 1000));
    }
}

/* --- 4. TASK NÚT CHỨC NĂNG (Factory Reset) --- */
void function_button_task(void *pvParameter) {
    // Cấu hình chân GPIO 32
    gpio_reset_pin(FUNC_BUTTON_GPIO);
    gpio_set_direction(FUNC_BUTTON_GPIO, GPIO_MODE_INPUT);
    
    // Cấu hình trở kéo (Internal Pull-up/down)
    // Mạch bạn đã có trở ngoài, nhưng bật thêm trở trong cho chắc chắn
    if (BUTTON_ACTIVE_LEVEL == 0) {
        // Nhấn nối đất -> Kéo lên nguồn để bình thường là 1
        gpio_set_pull_mode(FUNC_BUTTON_GPIO, GPIO_PULLUP_ONLY); 
    } else {
        // Nhấn nối nguồn -> Kéo xuống đất để bình thường là 0
        gpio_set_pull_mode(FUNC_BUTTON_GPIO, GPIO_PULLDOWN_ONLY); 
    }

    while (1) {
        // Kiểm tra trạng thái nút nhấn
        if (gpio_get_level(FUNC_BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL) {
            ESP_LOGW(TAG, "Phat hien nhan nut! Giu 3s de Reset...");
            
            int hold_time = 0;
            // Vòng lặp chờ giữ nút (Debounce & Hold detection)
            while (gpio_get_level(FUNC_BUTTON_GPIO) == BUTTON_ACTIVE_LEVEL) {
                vTaskDelay(pdMS_TO_TICKS(100));
                hold_time += 100;
                
                // Nháy đèn chậm (chu kỳ 0.5s) để báo hiệu đang đếm giờ
                if (hold_time % 500 == 0) {
                    gpio_set_level(LED_GPIO, !gpio_get_level(LED_GPIO));
                }

                // Nếu giữ đủ 3 giây (3000ms) -> Kích hoạt Factory Reset
                if (hold_time >= 3000) {
                    ESP_LOGE(TAG, ">>> FACTORY RESET: XOA TOAN BO CAU HINH <<<");
                    
                    // Nháy đèn nhanh báo hiệu đang thực thi lệnh xóa
                    for(int i=0; i<10; i++){
                         gpio_set_level(LED_GPIO, 1); vTaskDelay(50/portTICK_PERIOD_MS);
                         gpio_set_level(LED_GPIO, 0); vTaskDelay(50/portTICK_PERIOD_MS);
                    }
                    
                    nvs_flash_erase(); // Xóa sạch NVS (SSID, Pass, URL)
                    nvs_flash_init();  // Khởi tạo lại NVS
                    esp_restart();     // Khởi động lại chip ngay lập tức
                }
            }
            // Nếu thả nút ra trước 3s thì tắt đèn (hoặc trả về trạng thái cũ)
            gpio_set_level(LED_GPIO, 0); 
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Kiểm tra nút mỗi 100ms
    }
}

/* --- MAIN --- */
void app_main(void){
    // 1. Khởi tạo bộ nhớ NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        nvs_flash_erase(); nvs_flash_init();
    }

    // 2. Khởi tạo TCP/IP Stack & Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 3. Khởi tạo LED báo hiệu
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    // 4. Chạy Task Nút Chức Năng (Luôn chạy nền để sẵn sàng Reset)
    xTaskCreate(&function_button_task, "func_btn", 2048, NULL, 5, NULL);

    // 5. Lấy URL Server từ bộ nhớ (Nếu đã lưu trước đó)
    if (nvs_get_server_url(g_server_url, sizeof(g_server_url)) != ESP_OK){
        // Nếu chưa có, dùng URL mặc định
        strcpy(g_server_url, SERVER_URL_DEFAULT);
    }
    ESP_LOGI(TAG, "Target Server URL: %s", g_server_url);

    // 6. Quy trình kết nối WiFi thông minh
    esp_err_t wifi_status = wifi_prov_init();

    if (wifi_status == ESP_ERR_NVS_NOT_FOUND) {
        // TRƯỜNG HỢP 1: Chưa có cấu hình WiFi (Máy mới hoặc vừa Reset)
        ESP_LOGW(TAG, "Chua co WiFi -> Bat che do AP (Phat WiFi) de cau hinh...");
        
        // Bật đèn sáng liên tục để người dùng biết là cần cấu hình
        gpio_set_level(LED_GPIO, 1); 
        
        wifi_prov_start_ap();
        // Main dừng tại đây, nhường quyền cho các task Wifi/Http Server chạy
    } else {
        // TRƯỜNG HỢP 2: Đã có WiFi -> Chạy đo đạc
        ESP_LOGI(TAG, "Da co WiFi -> Bat dau do va gui du lieu...");
        
        // Chạy task cảm biến
        xTaskCreate(&sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    }
}