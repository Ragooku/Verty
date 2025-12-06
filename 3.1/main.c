#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// Настройки Wi-Fi
#define WIFI_SSID      "ustu_open"
#define WIFI_PASS      ""
#define MAX_APs        20
#define MAIN_TASK_STACK_SIZE 8192  // Увеличиваем размер стека

static const char *TAG = "wifi_scanner";

// Функция для преобразования типа безопасности в строку
static const char* wifi_auth_mode_str(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2_WPA3_PSK";
        default:
            return "UNKNOWN";
    }
}

// Функция для форматированного вывода MAC адреса
static void print_mac_address(const char* label, const uint8_t *mac) {
    if (mac) {
        printf("%s: %02x:%02x:%02x:%02x:%02x:%02x\n", 
               label, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

// Функция сканирования Wi-Fi сетей
static void wifi_scan(void) {
    ESP_LOGI(TAG, "Starting WiFi scan...");

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(ret));
        return;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Found %d networks", ap_count);

    if (ap_count == 0) {
        ESP_LOGI(TAG, "No APs found");
        return;
    }

    // Ограничиваем количество для экономии памяти
    uint16_t number = (ap_count < MAX_APs) ? ap_count : MAX_APs;
    wifi_ap_record_t *ap_records = malloc(number * sizeof(wifi_ap_record_t));
    
    if (ap_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for AP records");
        return;
    }

    esp_wifi_scan_get_ap_records(&number, ap_records);

    printf("\n=== Wi-Fi Networks Scan Results ===\n");
    printf("№ | SSID | RSSI | Channel | Security | MAC Address\n");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < number; i++) {
        printf("%2d | %-20.20s | %4d | %7d | %-8s | %02x:%02x:%02x:%02x:%02x:%02x\n",
               i + 1,
               (char*)ap_records[i].ssid,
               ap_records[i].rssi,
               ap_records[i].primary,
               wifi_auth_mode_str(ap_records[i].authmode), // преобразует тип безопасности в читаемую строку
               ap_records[i].bssid[0], ap_records[i].bssid[1], ap_records[i].bssid[2],
               ap_records[i].bssid[3], ap_records[i].bssid[4], ap_records[i].bssid[5]);
    }
    printf("---------------------------------------------------\n");

    free(ap_records);
}

// Функция для вывода информации о подключении
static void print_connection_info(void) {
    wifi_ap_record_t ap_info;
    esp_netif_ip_info_t ip_info;
    uint8_t mac[6];
    
    // Получаем информацию о подключенной AP
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        printf("\n=== Connected Network Information ===\n");
        printf("SSID: %s\n", ap_info.ssid);
        printf("RSSI: %d dBm\n", ap_info.rssi);
        printf("Channel: %d\n", ap_info.primary);
        printf("Security: %s\n", wifi_auth_mode_str(ap_info.authmode));
        print_mac_address("AP MAC Address", ap_info.bssid);
    }
    
    // Получаем IP информацию
    esp_netif_t* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        printf("IP Address: " IPSTR "\n", IP2STR(&ip_info.ip));
        printf("Netmask: " IPSTR "\n", IP2STR(&ip_info.netmask));
        printf("Gateway: " IPSTR "\n", IP2STR(&ip_info.gw));
    }
    
    // Получаем MAC адрес станции (нашего ESP32)
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        print_mac_address("Station MAC", mac);
    }
    
    printf("=====================================\n");
}

// Функция подключения к Wi-Fi
static void wifi_connect(void) {
    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        return;
    }

    // Ждем подключения с таймаутом
    int retry_count = 0;
    while (retry_count < 20) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Connected to WiFi successfully!");
            
            // Выводим подробную информацию о подключении
            print_connection_info();
            return;
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retry_count++;
        ESP_LOGI(TAG, "Waiting for connection... (%d/20)", retry_count);
    }
    
    ESP_LOGE(TAG, "Failed to connect to WiFi within timeout");
}

// Обработчик событий Wi-Fi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi station started");
        
        // Выводим MAC адрес станции при старте
        uint8_t mac[6];
        if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
            ESP_LOGI(TAG, "Station MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Функция инициализации WiFi
static void wifi_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация сетевого стека
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Инициализация Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Регистрация обработчиков событий
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Настройка Wi-Fi режима
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Основная рабочая задача (отдельная от app_main)
static void main_task(void *pvParameters) {
    ESP_LOGI(TAG, "Main task started");
    
    // Даем время для инициализации Wi-Fi
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // Сканирование сетей
    wifi_scan();

    // Подключение к выбранной сети
    wifi_connect();

    // Основной цикл - периодическое сканирование и вывод информации
    while (1) {
        vTaskDelay(30000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Performing periodic scan...");
        wifi_scan();
        
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Still connected to: %s (RSSI: %d)", ap_info.ssid, ap_info.rssi);
        }
    }
}

void app_main(void) {

    wifi_init();

    xTaskCreate(main_task,
                "main_task",
                MAIN_TASK_STACK_SIZE,
                NULL,
                1,
                NULL);
    
}
