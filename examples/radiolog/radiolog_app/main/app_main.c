/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <dht.h>

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

void cmd_coverSetPos(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    if (len_data == 0 && !data) {
        ESP_LOGE(TAG, "Invalid paylod in cover set");
        return;
    }
    cover_run(atoi(data));
}

void cmd_coverSet(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    if (len_data == 0 && !data) {
        ESP_LOGE(TAG, "Invalid paylod in cover set");
        return;
    }

    if (!strncmp("OPEN", data, len_data)) {
        cover_run(100);
        return;
    }

    if (!strncmp("CLOSE", data, len_data)) {
        cover_run(0);
        return;
    }

    if (!strncmp("STOP", data, len_data)) {
        cover_stop();
        return;
    }

}

void cmd_reset(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    esp_restart();
}

#define MQTT_TOPIC_ANNOUNCE  "cover/announce"
#define MQTT_TOPIC_STATUS    "cover/status"
#define MQTT_TOPIC_SET_POS   "cover/set_position"
#define MQTT_TOPIC_POS       "cover/pos"
#define MQTT_TOPIC_SET       "cover/set"
#define MQTT_TOPIC_AVAILABLE "cover/available"
#define MQTT_TOPIC_RESET     "reset"

static CmdMQTT callback_table[] = {
    { MQTT_TOPIC_SET     , cmd_coverSet }    ,
    { MQTT_TOPIC_SET_POS , cmd_coverSetPos } ,
    { MQTT_TOPIC_RESET   , cmd_reset }       ,
    { NULL               , NULL }            ,
};

static bool announce = true;
static void device_status(void * pvParameter)
{
    while (1) {
        if(announce) {
            mqtt_mgr_pub(MQTT_TOPIC_ANNOUNCE, sizeof(MQTT_TOPIC_ANNOUNCE), "announce", sizeof("announce") -1);
            mqtt_mgr_pub(MQTT_TOPIC_AVAILABLE, sizeof(MQTT_TOPIC_AVAILABLE), "online", sizeof("online") -1);
            announce = false;
        }
        mqtt_mgr_pub(MQTT_TOPIC_STATUS, sizeof(MQTT_TOPIC_STATUS), "open", sizeof("open") -1);
        mqtt_mgr_pub(MQTT_TOPIC_POS, sizeof(MQTT_TOPIC_POS), "pos", sizeof("pos") -1);

        int16_t temperature = 0;
        int16_t humidity = 0;
        if (dht_read_data(DHT_TYPE_DHT11, 2, &humidity, &temperature) == ESP_OK)
            printf("Humidity: %d%% Temp: %dC\n", humidity, temperature);
        else
            printf("Could not read data from sensor\n");


        DELAY_S(30);
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

    // check if we should update the fw via ota
    common_ota_task();

    mqtt_mgr_init(callback_table);
    xTaskCreate(&device_status, "device_status_task", 8192, NULL, 5, NULL);

    cover_init(&cover_ctx, NULL);

}

