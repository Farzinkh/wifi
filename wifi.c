#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "mdns.h"
#include "esp_sntp.h"

#include "lwip/dns.h"

#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "esp_tls.h"

#include "wifi.h"

int led_state;
bool led_end=false;
esp_err_t err;
TaskHandle_t blink1Handle;
TaskHandle_t blink2Handle;
TaskHandle_t blink3Handle;

static void configure_led(void)
{
    gpio_reset_pin(BLINK_GPIO);
    led_state=gpio_get_level(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BLINK_GPIO,led_state);
}

static void blink_mode_1(void *pvParameter)
{
    int counter;
    uint8_t s_led_state=led_state;
    for(counter=0;counter<6;counter=counter+1){
        /* Set the GPIO level according to the state (LOW or HIGH)*/
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(500 / portTICK_PERIOD_MS);  
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    led_end=true;
    gpio_reset_pin(BLINK_GPIO);
    vTaskDelete(NULL);
}

static void blink_mode_2(void *pvParameter)
{
    int counter;
    uint8_t s_led_state=led_state;
    for(counter=0;counter<12;counter=counter+1){
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
    s_led_state = !s_led_state;
    vTaskDelay(100 / portTICK_PERIOD_MS);  
    gpio_set_level(BLINK_GPIO, s_led_state);
    s_led_state = !s_led_state;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, s_led_state);
    s_led_state = !s_led_state;
    vTaskDelay(100 / portTICK_PERIOD_MS);  
    gpio_set_level(BLINK_GPIO, s_led_state);
    s_led_state = !s_led_state;
    vTaskDelay(100 / portTICK_PERIOD_MS);
    /* Toggle the LED state */
    vTaskDelay(1000 / portTICK_PERIOD_MS);  
    }
    gpio_reset_pin(BLINK_GPIO);
    vTaskDelete(NULL);
}

static void blink_mode_3(void *pvParameter)
{
    int counter;
    uint8_t s_led_state=led_state;
    while(1){
        /* Set the GPIO level according to the state (LOW or HIGH)*/
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(100 / portTICK_PERIOD_MS);  
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(100 / portTICK_PERIOD_MS);  
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_GPIO, s_led_state);
        s_led_state = !s_led_state;
        vTaskDelay(100 / portTICK_PERIOD_MS);  
        gpio_set_level(BLINK_GPIO, s_led_state);
        /* Toggle the LED state */
        vTaskDelay(1000 / portTICK_PERIOD_MS);  
    }
    gpio_reset_pin(BLINK_GPIO);
    vTaskDelete(NULL);
}

#if CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
#include "esp_efuse.h"
#endif

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
#include "ble_api.h"
#endif

static const char *TAG2 = "OTA";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    err=esp_ota_get_partition_description(running, &running_app_info);
    if ( err== ESP_OK) {
        ESP_LOGI(TAG2, "Running firmware %s version: %s time: %s date: %s",running_app_info.project_name,running_app_info.version,running_app_info.time,running_app_info.date);
    }
    ESP_LOGI(TAG2, "Updating to firmware %s version: %s time: %s date: %s",new_app_info->project_name,new_app_info->version,new_app_info->time,new_app_info->date);
#ifndef CONFIG_OTA_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG2, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
#endif

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version) {
        ESP_LOGW(TAG2, "New firmware security version is less than eFuse programmed, %d < %d", new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

static EventGroupHandle_t s_ota_event_group;

void ota_task(void *pvParameter)
{
    ESP_LOGI(TAG2, "Starting OTA");
    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = CONFIG_OTA_FIRMWARE_UPGRADE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
        .keep_alive_enable = true,
    };
#ifdef CONFIG_OTA_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG2, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_OTA_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
#ifdef CONFIG_OTA_ENABLE_PARTIAL_HTTP_DOWNLOAD
        .partial_http_download = true,
        .max_http_request_size = CONFIG_OTA_HTTP_REQUEST_SIZE,
#endif
    };
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG2, "ESP HTTPS OTA Begin failed");
        xEventGroupSetBits(s_ota_event_group, WIFI_FAIL_BIT);
        vTaskDelete(blink3Handle);
        vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG2, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG2, "image header verification failed");
        goto ota_end;
    }
    ESP_LOGI(TAG2, "Downloading...");
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG2, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }
    ESP_LOGI(TAG2, "Downloaded Image byte size: %d \n", esp_https_ota_get_image_len_read(https_ota_handle));
    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG2, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG2, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            xEventGroupSetBits(s_ota_event_group, WIFI_CONNECTED_BIT);
            vTaskDelay(1000 / portTICK_PERIOD_MS);  
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG2, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG2, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            xEventGroupSetBits(s_ota_event_group, WIFI_FAIL_BIT);
            vTaskDelete(NULL);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG2, "ESP_HTTPS_OTA upgrade failed");
    xEventGroupSetBits(s_ota_event_group, WIFI_FAIL_BIT);
    vTaskDelete(blink3Handle);
    gpio_reset_pin(BLINK_GPIO);
    vTaskDelete(NULL);
}

static const char *TAG = "WIFI";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */


static int s_retry_num = 0;
char key[64];
char parameter[128];
int status; // 0 station 1 ap
bool is_nvs_init=false;
bool is_netif_init=false;

void init_nvs()
{
	// Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

esp_err_t save_key_value(char * key, char * value)
{
	nvs_handle_t my_handle;
	esp_err_t err;

	// Open
	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK) return err;

	// Write
	err = nvs_set_str(my_handle, key, value);
	if (err != ESP_OK) return err;

	// Commit written value.
	// After setting any values, nvs_commit() must be called to ensure changes are written
	// to flash storage. Implementations may write to storage at other times,
	// but this is not guaranteed.
	err = nvs_commit(my_handle);
	if (err != ESP_OK) return err;

	// Close
	nvs_close(my_handle);
	return ESP_OK;
}

esp_err_t load_key_value(char *key, char *value, size_t size)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // Read
    size_t _size = size;
    err = nvs_get_str(my_handle, key, value, &_size);
    // if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (err != ESP_OK)
        return err;

    // Close
    nvs_close(my_handle);
    // return ESP_OK;
    return err;
}

#if CONFIG_STA || CONFIG_SMART

static void sta_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "connecting to the AP...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            esp_restart();
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        strcpy(key, "mode");
        save_key_value(key,"1");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED){  
        ESP_LOGI(TAG, "Connected");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_STOP){  
        ESP_LOGI(TAG, "station mode stoped");    
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_BSS_RSSI_LOW){ //week signal
        ESP_LOGE(TAG,"LOW RSSI");
        #if CONFIG_SMART
            strcpy(key, "mode");
            save_key_value(key,"0");
            esp_restart();
        #endif
    } else{
        ESP_LOGI(TAG, "Event %d happend" ,event_id);
    }
}


esp_err_t wifi_init_sta()
{	
    if (!is_nvs_init){
        init_nvs();
    }

	configure_led();
	
	esp_err_t ret_value = ESP_OK;
    s_wifi_event_group = xEventGroupCreate();
    if (!is_netif_init){
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    }
    
#if CONFIG_STATIC_IP

	ESP_LOGI(TAG, "CONFIG_STATIC_IP_ADDRESS=[%s]",CONFIG_STATIC_IP_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_GW_ADDRESS=[%s]",CONFIG_STATIC_GW_ADDRESS);
	ESP_LOGI(TAG, "CONFIG_STATIC_NM_ADDRESS=[%s]",CONFIG_STATIC_NM_ADDRESS);

	/* Stop DHCP client */
	ESP_ERROR_CHECK(esp_netif_dhcpc_stop(sta_netif));
	ESP_LOGI(TAG, "Stop DHCP Services");

	/* Set STATIC IP Address */
	esp_netif_ip_info_t ip_info;
	memset(&ip_info, 0 , sizeof(esp_netif_ip_info_t));
	ip_info.ip.addr = ipaddr_addr(CONFIG_STATIC_IP_ADDRESS);
	ip_info.netmask.addr = ipaddr_addr(CONFIG_STATIC_NM_ADDRESS);
	ip_info.gw.addr = ipaddr_addr(CONFIG_STATIC_GW_ADDRESS);;
	esp_netif_set_ip_info(sta_netif, &ip_info);

	ip_addr_t d;
	d.type = IPADDR_TYPE_V4;
	d.u_addr.ip4.addr = 0x08080808; //8.8.8.8 dns
	dns_setserver(0, &d);
	d.u_addr.ip4.addr = 0x08080404; //8.8.4.4 dns
	dns_setserver(1, &d);

#endif // CONFIG_STATIC_IP
    if (!is_netif_init){
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        is_netif_init=true;
    }    
    ret_value=esp_wifi_set_bandwidth(WIFI_IF_STA,WIFI_BW_HT40);
    if (ret_value != ESP_OK){
        ESP_LOGE(TAG, "Error in setting bandwidth %s",esp_err_to_name(ret_value));
    }
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_STA_WIFI_SSID,
            .password = ESP_STA_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    xTaskCreate(&blink_mode_1, "blink_1", 1024 * 2, NULL, 4, &blink1Handle);

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */ 
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_STA_WIFI_SSID, ESP_STA_WIFI_PASS);              
                 
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_STA_WIFI_SSID, ESP_STA_WIFI_PASS);
                 esp_restart();
                 ret_value = ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        ret_value = ESP_ERR_INVALID_STATE;
    }

    /* The event will not be processed after unregister */
    ESP_LOGI(TAG, "wifi_init_sta finished.");
	ESP_LOGI(TAG, "connect to ap SSID:%s", ESP_STA_WIFI_SSID);
	status=0;
	vEventGroupDelete(s_wifi_event_group); 
	err = esp_wifi_set_rssi_threshold(-80);
    if (err != ESP_OK )
    {
        ESP_LOGE(TAG, "Error (%s)", esp_err_to_name(err));
    }
	return ret_value; 
}

#endif 

#if CONFIG_AP || CONFIG_SMART

static void AP_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    ESP_LOGI(TAG, "Event %d happend" ,event_id);
}

void wifi_init_softap(void)
{	
    if (!is_nvs_init){
        init_nvs();
    }
	configure_led();
	
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &AP_wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_AP_WIFI_SSID,
            .ssid_len = strlen(ESP_AP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .password = ESP_AP_WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(ESP_AP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             ESP_AP_WIFI_SSID, ESP_AP_WIFI_PASS, ESP_WIFI_CHANNEL);
    xTaskCreate(&blink_mode_2, "blink_2", 1024 * 2, NULL, 4, &blink2Handle);
    status=1;
}
#endif 

#if CONFIG_SMART

esp_err_t wifi_init()
{
    tcpip_adapter_ip_info_t ip_info;
    init_nvs();
	is_nvs_init=true;
    if (CONFIG_WIFI_MODE==0){
    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    /* Get the local IP address */
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
    } else if (CONFIG_WIFI_MODE==2){
        int Mode=1;
        esp_err_t er;
        strcpy(key, "mode");
        er = load_key_value(key, parameter, sizeof(parameter));
        if (er != ESP_OK )
        {
            ESP_LOGE(TAG, "Error (%s) loading to NVS", esp_err_to_name(er));
        }
        else
        {
            ESP_LOGI(TAG, "Readed config: %s", parameter);
            Mode=atoi(parameter);
        }
        if (Mode==0){
            ESP_LOGI(TAG, "ESP_WIFI_MODE_AP loaded from NVS");
            save_key_value(key,"1");
            wifi_init_softap();
            /* Get the local IP address */
            ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
        } else {
            ESP_LOGI(TAG, "ESP_WIFI_MODE_STA loaded from NVS");
            save_key_value(key,"0");
            err=wifi_init_sta();
            if (err!=ESP_OK){
                return ESP_ERR_WIFI_NOT_STARTED;
            }
            /* Get the local IP address */
            ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
        }
    } else {
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    err=wifi_init_sta();
    if (err!=ESP_OK){
        return ESP_ERR_WIFI_NOT_STARTED;
    }
    }   
    return ESP_OK;
}
#endif 

tcpip_adapter_ip_info_t get_ip()
{
    uint8_t derived_mac_addr[6] = {0};
    tcpip_adapter_ip_info_t ip_info;
    if (status==0){
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
        err=esp_read_mac(derived_mac_addr,ESP_MAC_WIFI_STA);
    } else {
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
        err=esp_read_mac(derived_mac_addr,ESP_MAC_WIFI_SOFTAP);
    }
    /* Print the local IP address */
    ESP_LOGI(TAG, "IP Address : %s", ip4addr_ntoa(&ip_info.ip));
    ESP_LOGI(TAG, "Subnet mask: %s", ip4addr_ntoa(&ip_info.netmask));
    ESP_LOGI(TAG, "Gateway    : %s", ip4addr_ntoa(&ip_info.gw));
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Faild to read MAC Address %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG,"MAC Address: %x:%x:%x:%x:%x:%x",
             derived_mac_addr[0], derived_mac_addr[1], derived_mac_addr[2],
             derived_mac_addr[3], derived_mac_addr[4], derived_mac_addr[5]);
    }
    return ip_info;
}

void initialise_mdns(void)
{
	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(CONFIG_MDNS_HOSTNAME) );
	ESP_LOGI(TAG, "mdns hostname set to: [%s]", CONFIG_MDNS_HOSTNAME);
}

void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
	ESP_LOGI(TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	//sntp_setservername(0, "pool.ntp.org");
	ESP_LOGI(TAG, "Your NTP Server is %s", CONFIG_NTP_SERVER);
	sntp_setservername(0, CONFIG_NTP_SERVER);
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	sntp_init();
}

static esp_err_t obtain_time(void)
{
	initialize_sntp();
	// wait for time to be set
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}

	if (retry == retry_count) return ESP_FAIL;
	return ESP_OK;
}

void check_time()
{
	// obtain time over NTP
	ESP_LOGI(TAG, "Getting time over NTP.");
	err = obtain_time();
	if(err != ESP_OK) {
		ESP_LOGE(TAG, "Fail to getting time over NTP useing ofline time.");
	}

	// update 'now' variable with current time
	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];
	time(&now);
	now = now + (CONFIG_LOCAL_TIMEZONE*60*60);
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "The local date/time is: %s", strftime_buf);
	ESP_LOGW(TAG, "This server manages file timestamps in GMT.");

}

//betwen -100 to 0
int8_t get_rssi() 
{
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);
    ESP_LOGI(TAG,"%d", ap.rssi);
    return ap.rssi;
}

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

/* Initialize Wi-Fi as sta and set scan method */
void wifi_scan(void)
{
    if (!is_nvs_init){
        init_nvs();
        is_nvs_init=true;
    }
    if (!is_netif_init){
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        assert(sta_netif);
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        is_netif_init=true;
    }
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
    esp_wifi_stop();

}

void ota_verify()
{
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        ESP_LOGI(TAG2, "App is valid, rollback cancelled successfully");
    } else {
        ESP_LOGE(TAG2, "Failed to cancel rollback");
    }
}

void ota_discredit()
{
    err=esp_ota_mark_app_invalid_rollback_and_reboot();
    if ( err== ESP_FAIL) {
        ESP_LOGE(TAG2, "Rollback set failed");
    } else if (err==ESP_ERR_OTA_ROLLBACK_FAILED){
        ESP_LOGE(TAG2, "OTA app not found!!!");
    } else {
        ESP_LOGI(TAG2, "App is invalid, rollback in progress");
    }
}
static const char *TAG3 = "HTTPS";
esp_tls_client_session_t *tls_client_session = NULL;
char version[16];

static const char REQUEST[] = "GET " CONFIG_OTA_SERVER_ROOT " HTTP/1.1\r\n"
                             "Host: "CONFIG_OTA_SERVER"\r\n"
                             "User-Agent: esp-idf/4.0 esp32\r\n"
                             "\r\n";
                             
static void https_get_request(esp_tls_cfg_t cfg)
{
    int count=0;
    char buf[512];
    char v[16];
    int ret, len;
    const char s[]="\n";
    char *token;
    struct esp_tls *tls = esp_tls_conn_http_new(CONFIG_OTA_SERVER_ROOT, &cfg);

    if (tls != NULL) {
        ESP_LOGI(TAG3, "Connection established...");
    } else {
        ESP_LOGE(TAG3, "Connection failed...");
        goto exit;
    }

    /* The TLS session is successfully established, now saving the session ctx for reuse */
    if (tls_client_session == NULL) {
        tls_client_session = esp_tls_get_client_session(tls);
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 REQUEST + written_bytes,
                                 sizeof(REQUEST) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG3, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG3, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto exit;
        }
    } while (written_bytes < sizeof(REQUEST));

    ESP_LOGI(TAG3, "Reading HTTPS response...");
    do {
        len = sizeof(buf) - 1;
        bzero(buf, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        }

        if (ret < 0) {
            ESP_LOGE(TAG3, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        }

        if (ret == 0) {
            ESP_LOGI(TAG3, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGD(TAG3, "%d bytes read", len);
        /* Print response directly to stdout as it is read */
        for (int i = 0; i < len; i++) {
            putchar(buf[i]);
        }
        putchar('\n'); // JSON output doesn't have a newline at end
        token=strtok(buf,s);
        while (token !=NULL)
        {
            //printf("token : %s\n",token);
            if (count==6)
            {
                strcpy(v,token);
            }
            
            token=strtok(NULL,s);
            count+=1;
        }
    } while (1);
    int j = 0;
    for (int i = 0; i < sizeof(v); i ++) {
        if (v[i] != '"' && v[i] != '\n') { 
            version[j] = v[i];
            j++;
        }}    
    ESP_LOGI(TAG3, "Last version on server:%s",version);

exit:
    esp_tls_conn_delete(tls);
}

static void https_get_request_using_cacert_buf(void)
{
    ESP_LOGI(TAG3, "https_request using cacert_buf");
    esp_tls_cfg_t cfg = {
        .cacert_buf = (const unsigned char *) server_cert_pem_start,
        .cacert_bytes = server_cert_pem_end - server_cert_pem_start,
    };
    https_get_request(cfg);
}

static void https_get_request_using_already_saved_session(void)
{
    ESP_LOGI(TAG3, "https_request using saved client session");
    esp_tls_cfg_t cfg = {
        .client_session = tls_client_session,
    };
    https_get_request(cfg);
    free(tls_client_session);
    tls_client_session = NULL;
}

esp_err_t check_version(char old_version[32])
{
    https_get_request_using_cacert_buf();
    if (strcmp(old_version,version)==0){
        ESP_LOGE(TAG2,"No update available."); 
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t download_ota(void)
{
    if (status==1){
        ESP_LOGE(TAG2,"OTA is not available on AP"); 
        return ESP_FAIL;
    }
    s_ota_event_group = xEventGroupCreate();
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
    */
    uint8_t otas = esp_ota_get_app_partition_count();
    ESP_LOGI(TAG2,"OTA partions %d.",otas); 
    #if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE)
    /**
     * We are treating successful WiFi connection as a checkpoint to cancel rollback
     * process and mark newly updated firmware image as active. For production cases,
     * please tune the checkpoint behavior per end application requirement.
     */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    err=esp_ota_get_state_partition(running, &ota_state) ;
    if (err== ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG2,"App state is PENDING_VERIFY");
        } else if (ota_state == ESP_OTA_IMG_UNDEFINED)
        {
            ESP_LOGI(TAG2,"App state is UNDEFINED");
        } else if (ota_state == ESP_OTA_IMG_NEW)
        {
            ESP_LOGI(TAG2,"App state is NEW");
        } else if (ota_state == ESP_OTA_IMG_VALID)
        {
            ESP_LOGI(TAG2,"App state is VALID");
        }
    } else if (err==ESP_ERR_NOT_SUPPORTED)
    {
        ESP_LOGW(TAG2, "It's not ota app.");
    } else {
        ESP_LOGE(TAG2, "%s", esp_err_to_name(err));
    }
    #endif
    esp_app_desc_t running_app_info;
    esp_ota_get_partition_description(running, &running_app_info);
    ESP_LOGI(TAG2,"App version %s.",running_app_info.version); 
    err=check_version(running_app_info.version);
    if (err==ESP_OK)
    {
        #if !CONFIG_BT_ENABLED
            /* Ensure to disable any WiFi power save mode, this allows best throughput
            * and hence timings for overall OTA operation.
            */
            esp_wifi_set_ps(WIFI_PS_NONE);
        #else
            /* WIFI_PS_MIN_MODEM is the default mode for WiFi Power saving. When both
            * WiFi and Bluetooth are running, WiFI modem has to go down, hence we
            * need WIFI_PS_MIN_MODEM. And as WiFi modem goes down, OTA download time
            * increases.
            */
            esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        #endif // CONFIG_BT_ENABLED

        #if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
            esp_ble_helper_init();
        #endif
        if (!led_end){
            vTaskDelete(blink1Handle);
        }
        configure_led();
        xTaskCreate(&blink_mode_3, "blink_3", 1024 * 2, NULL, 4, &blink3Handle);
        xTaskCreate(&ota_task, "ota_task", 1024 * 8, NULL, 4, NULL);
        EventBits_t bits = xEventGroupWaitBits(s_ota_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);    
        vEventGroupDelete(s_ota_event_group);
        return ESP_OK;
    }
    return ESP_FAIL;
}