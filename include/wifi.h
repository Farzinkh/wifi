#ifndef WIFI
#define WIFI


#include "esp_err.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define ESP_STA_WIFI_SSID CONFIG_ESP_STA_WIFI_SSID
#define ESP_STA_WIFI_PASS CONFIG_ESP_STA_WIFI_PASSWORD
#define ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define ESP_AP_WIFI_SSID CONFIG_ESP_AP_WIFI_SSID
#define ESP_AP_WIFI_PASS CONFIG_ESP_AP_WIFI_PASSWORD
#define ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define STORAGE_NAMESPACE "config"

#define BLINK_GPIO CONFIG_BLINK_GPIO

void check_time(); // useing SNTP
int8_t get_rssi();
esp_err_t wifi_init(); // smart config
esp_err_t wifi_init_sta();
void initialise_mdns(void); // MDNS
void wifi_init_softap(void);
tcpip_adapter_ip_info_t get_ip(); // print ip info

// NVS tasks
esp_err_t save_key_value(char * key, char * value); 
esp_err_t load_key_value(char * key, char * value, size_t size);


#endif