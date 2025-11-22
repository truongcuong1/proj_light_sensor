#include "wifi_config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI_PROV";

// Namespace và Keys NVS
#define NVS_NAMESPACE "storage"
#define NVS_KEY_SSID "wifi_ssid"
#define NVS_KEY_PASS "wifi_pass"
#define NVS_KEY_SERVER_URL "server_url"

// Cấu hình AP mặc định
static const char *AP_SSID = "ESP_Config";
static const char *AP_PASS = "12345678"; // Pass mặc định cho AP (để trống nếu muốn open)

// Trạng thái kết nối (0: chưa, 1: đã có IP)
static int s_wifi_connected_flag = 0;

// HTML Form
static const char *prov_page =
"<html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>ESP Setup</title>"
"<style>body{font-family:sans-serif;padding:20px;} input{width:100%;margin-bottom:10px;padding:8px;} button{padding:10px 20px;}</style></head>"
"<body><h2>Cài đặt WiFi & Server</h2>"
"<form method=\"POST\" action=\"/save\">"
"<label>WiFi SSID:</label><input name=\"ssid\" placeholder=\"Ten WiFi\">"
"<label>WiFi Pass:</label><input name=\"pass\" type=\"password\" placeholder=\"Mat khau\">"
"<label>Server URL:</label><input name=\"server\" value=\"http://192.168.1.10:8000/api/sensor\">"
"<button type=\"submit\">Lưu & Khởi động lại</button>"
"</form></body></html>";

static httpd_handle_t prov_server = NULL;

/* --- NVS HELPERS --- */
static esp_err_t nvs_set_kv_str(const char *key, const char *val){
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_get_kv_str(const char *key, char *out, size_t *len){
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_str(h, key, out, len);
    nvs_close(h);
    return err;
}

esp_err_t nvs_get_server_url(char *buf, size_t buf_len){
    size_t len = buf_len;
    return nvs_get_kv_str(NVS_KEY_SERVER_URL, buf, &len);
}

/* --- HTTP SERVER HANDLERS --- */
static void url_decode_inplace(char *s){
    char *dst = s;
    while(*s){
        if (*s == '+') { *dst++ = ' '; s++; }
        else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])){
            char hex[3] = { s[1], s[2], 0 };
            *dst++ = (char) strtol(hex, NULL, 16);
            s += 3;
        } else { *dst++ = *s++; }
    }
    *dst = '\0';
}

static esp_err_t handle_root(httpd_req_t *req){
    httpd_resp_send(req, prov_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t handle_save(httpd_req_t *req){
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    char ssid[64]={0}, pass[64]={0}, server[256]={0};
    char *p = buf;
    while(p){
        char *amp = strchr(p, '&');
        if (amp) *amp = '\0';
        if (strncmp(p, "ssid=", 5)==0) strncpy(ssid, p+5, 63);
        else if (strncmp(p, "pass=", 5)==0) strncpy(pass, p+5, 63);
        else if (strncmp(p, "server=", 7)==0) strncpy(server, p+7, 255);
        if (!amp) break;
        p = amp + 1;
    }
    url_decode_inplace(ssid); url_decode_inplace(pass); url_decode_inplace(server);

    if (strlen(ssid) > 0) {
        nvs_set_kv_str(NVS_KEY_SSID, ssid);
        nvs_set_kv_str(NVS_KEY_PASS, pass);
        if(strlen(server)>0) nvs_set_kv_str(NVS_KEY_SERVER_URL, server);
        
        httpd_resp_send(req, "Da luu! Dang khoi dong lai...", HTTPD_RESP_USE_STRLEN);
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        httpd_resp_send(req, "Loi: SSID khong duoc de trong", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

static esp_err_t start_prov_http_server(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = handle_root };
    httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = handle_save };
    
    if (httpd_start(&prov_server, &config) == ESP_OK) {
        httpd_register_uri_handler(prov_server, &root_uri);
        httpd_register_uri_handler(prov_server, &save_uri);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* --- WIFI EVENTS --- */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected_flag = 0;
        ESP_LOGW(TAG, "Mat ket noi WiFi, thu lai...");
        // Thử kết nối lại nhưng không loop chết
        esp_wifi_connect(); 
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Da co IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_connected_flag = 1;
    }
}

/* --- PUBLIC FUNCTIONS --- */
int wifi_is_connected(void) {
    return s_wifi_connected_flag;
}

esp_err_t wifi_prov_start_ap(void){
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = { .ssid_len = 0, .max_connection = 4, .authmode = WIFI_AUTH_WPA_WPA2_PSK }
    };
    strcpy((char*)ap_config.ap.ssid, AP_SSID);
    strcpy((char*)ap_config.ap.password, AP_PASS);
    ap_config.ap.ssid_len = strlen(AP_SSID);

    if (strlen(AP_PASS) == 0) ap_config.ap.authmode = WIFI_AUTH_OPEN;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    
    ESP_LOGI(TAG, "AP Started: %s", AP_SSID);
    return start_prov_http_server();
}

esp_err_t wifi_prov_init(void){
    size_t needed = 0;
    // Check xem có SSID trong NVS chưa
    esp_err_t err = nvs_get_kv_str(NVS_KEY_SSID, NULL, &needed);
    
    if (err != ESP_OK) {
        return ESP_ERR_NVS_NOT_FOUND; // Báo cho main biết là chưa có cấu hình
    }

    // Nếu có rồi thì connect STA (Station Mode)
    char *ssid = malloc(needed + 1);
    nvs_get_kv_str(NVS_KEY_SSID, ssid, &needed);
    
    size_t pass_len = 0;
    nvs_get_kv_str(NVS_KEY_PASS, NULL, &pass_len);
    char *pass = NULL;
    if (pass_len > 0) {
        pass = malloc(pass_len + 1);
        nvs_get_kv_str(NVS_KEY_PASS, pass, &pass_len);
    }

    ESP_LOGI(TAG, "Tim thay WiFi: %s. Dang ket noi...", ssid);

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {0};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    if (pass) strcpy((char*)wifi_config.sta.password, pass);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    free(ssid);
    if(pass) free(pass);

    return ESP_OK; // Trả về OK để main chạy tiếp, việc connect diễn ra ngầm
}