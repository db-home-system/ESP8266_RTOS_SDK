/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "common.h"
#include "cover.h"
#include "mqtt_mgr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"

static const char *TAG = "Radiolog";
static cover_ctx_t cover_ctx;

void cmd_coverSet(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    printf("foo TOPIC=%.*s\r\n", len_topic, topic);
    printf("foo DATA=%.*s\r\n", len_data, data);
}

static CmdMQTT callback_table[] = {
    { "cover/set" , cmd_coverSet } ,
    { NULL        , NULL }         ,
};

static void device_status(void * pvParameter)
{
    while (1) {
        DELAY_S(10);
        mqtt_mgr_pub("test", sizeof("test"), "data", sizeof("data"));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(common_connect());

    xTaskCreate(&common_ota_task, "ota_update_task", 8192, NULL, 5, NULL);

    mqtt_mgr_init(callback_table);
    xTaskCreate(&device_status, "device_status_task", 8192, NULL, 5, NULL);

    cover_init(&cover_ctx, NULL);
}

