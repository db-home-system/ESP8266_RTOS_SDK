/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <string.h>

#include "common.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#define GOT_IPV4_BIT BIT(0)
#define GOT_IPV6_BIT BIT(1)

#ifdef CONFIG_RADIOLOG_CONNECT_IPV6
#define CONNECTED_BITS (GOT_IPV4_BIT | GOT_IPV6_BIT)
#if defined(CONFIG_RADIOLOG_CONNECT_IPV6_PREF_LOCAL_LINK)
#define RADIOLOG_CONNECT_PREFERRED_IPV6_TYPE 0
#elif defined(CONFIG_RADIOLOG_CONNECT_IPV6_PREF_GLOBAL)
#define RADIOLOG_CONNECT_PREFERRED_IPV6_TYPE 1
#endif
#else
#define CONNECTED_BITS (GOT_IPV4_BIT)
#endif

static EventGroupHandle_t s_connect_event_group;
static ip4_addr_t s_ip_addr;
static char s_connection_name[32] = CONFIG_RADIOLOG_WIFI_SSID;
static char s_connection_passwd[32] = CONFIG_RADIOLOG_WIFI_PASSWORD;

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#ifdef CONFIG_RADIOLOG_CONNECT_IPV6
static ip6_addr_t s_ipv6_addr;
#endif

static const char *TAG = "connect";

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    system_event_sta_disconnected_t *event = (system_event_sta_disconnected_t *)event_data;

    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    if (event->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
        /*Switch to 802.11 bgn mode */
        esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    }
    ESP_ERROR_CHECK(esp_wifi_connect());
}

#ifdef CONFIG_RADIOLOG_CONNECT_IPV6
static void on_wifi_connect(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    tcpip_adapter_create_ip6_linklocal(TCPIP_ADAPTER_IF_STA);
}
#endif

static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
    xEventGroupSetBits(s_connect_event_group, GOT_IPV4_BIT);
}

#ifdef CONFIG_RADIOLOG_CONNECT_IPV6

static void on_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    memcpy(&s_ipv6_addr, &event->ip6_info.ip, sizeof(s_ipv6_addr));
    if (RADIOLOG_CONNECT_PREFERRED_IPV6_TYPE) {
        if (ip6_addr_isglobal(&s_ipv6_addr)) {
            xEventGroupSetBits(s_connect_event_group, GOT_IPV6_BIT);
        }
    } else {
        xEventGroupSetBits(s_connect_event_group, GOT_IPV6_BIT);
    }
}

#endif // CONFIG_RADIOLOG_CONNECT_IPV6

static void start(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
#ifdef CONFIG_RADIOLOG_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &on_got_ipv6, NULL));
#endif

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = { 0 };

    strncpy((char *)&wifi_config.sta.ssid, s_connection_name, 32);
    strncpy((char *)&wifi_config.sta.password, s_connection_passwd, 32);
    //strncpy((char *)&wifi_config.sta.bssid);

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

static void stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
#ifdef CONFIG_RADIOLOG_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, &on_got_ipv6));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect));
#endif

    ESP_ERROR_CHECK(esp_wifi_deinit());
}

esp_err_t common_connect(void)
{
    if (s_connect_event_group != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_connect_event_group = xEventGroupCreate();
    start();
    xEventGroupWaitBits(s_connect_event_group, CONNECTED_BITS, true, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to %s", s_connection_name);
    ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));
#ifdef CONFIG_RADIOLOG_CONNECT_IPV6
    ESP_LOGI(TAG, "IPv6 address: " IPV6STR, IPV62STR(s_ipv6_addr));
#endif
    return ESP_OK;
}

esp_err_t common_disconnect(void)
{
    if (s_connect_event_group == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    vEventGroupDelete(s_connect_event_group);
    s_connect_event_group = NULL;
    stop();
    ESP_LOGI(TAG, "Disconnected from %s", s_connection_name);
    s_connection_name[0] = '\0';
    return ESP_OK;
}

esp_err_t common_set_connection_info(const char *ssid, const char *passwd)
{
    strncpy(s_connection_name, ssid, sizeof(s_connection_name));
    strncpy(s_connection_passwd, passwd, sizeof(s_connection_passwd));

    return ESP_OK;
}

esp_err_t common_nodeId(char *id, size_t len)
{
    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_MODE_STA, mac) < 0) {
        ESP_LOGE(TAG, "Unable to get wifi mac");
        return ESP_FAIL;
    }

    if (len < sizeof("Node_AABBCC")) {
        ESP_LOGE(TAG, "Buffer to short for nodeid");
        return ESP_FAIL;
    }

    sprintf(id, "Node_%0x%0x%0x", mac[3], mac[4], mac[5]);
    return ESP_OK;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void common_ota_task(void)
{
    ESP_LOGI(TAG, "Starting OTA...");

    esp_http_client_config_t config = {
        .url = CONFIG_FIRMWARE_UPGRADE_URL,
        .cert_pem = (char *)server_cert_pem_start,
        .event_handler = _http_event_handler,
    };

    for (int i = 0; i < OTA_MAX_RETRY; i++) {
        esp_err_t ret = esp_https_ota(&config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Firmware Upgrades Success!");
            return;
            //esp_restart();
        }

        DELAY_S(i + 1);
    }
    ESP_LOGE(TAG, "Firmware Upgrades Failed");
}

