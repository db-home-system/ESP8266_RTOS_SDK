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
#include "mqtt_mgr.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_mgr";

static char root_url[64];
static char buff[120];
static esp_mqtt_client_handle_t mqtt_client;

static CmdMQTT *local_callback;
static bool is_connect;

static void mqtt_mgr_registerCallbacks(void) {
    if (!local_callback) {
        ESP_LOGE(TAG, "Invalid callback table.");
        return;
    }
    for (int i = 0; local_callback[i].topic &&
            local_callback[i].foo; i++) {
        sprintf(buff, "%s/%s", root_url, local_callback[i].topic);
        int msg_id = esp_mqtt_client_subscribe(mqtt_client, buff, 0);
        ESP_LOGI(TAG, "subscribe %s successful, msg_id=%d", buff, msg_id);
    }

}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event_data->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            sprintf(buff, "%s/status", root_url);
            msg_id = esp_mqtt_client_subscribe(client, root_url, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            mqtt_mgr_registerCallbacks();
            is_connect = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            is_connect = false;
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            sprintf(buff, "%s/status", root_url);
            msg_id = esp_mqtt_client_publish(client, buff, "online", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other client id:%d", event->event_id);
            break;
    }
}

void mqtt_mgr_pub(char *topic, size_t len_topic, const char *data, size_t len_data) {

    if (len_topic > sizeof(buff)) {
        ESP_LOGE(TAG, "Topic to long! check internal buff lenght.");
        return;
    }

    sprintf(buff, "%s/%s", root_url, topic);
    int msg_id = esp_mqtt_client_publish(mqtt_client, buff, data, len_data, 0, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
}

void mqtt_mgr_init(CmdMQTT *table) {
    assert(table);

    is_connect = false;
    local_callback = table;

    char node_id[24];
    if (common_nodeId(node_id, sizeof(node_id)) < 0) {
        ESP_LOGE(TAG, "Unable to get Node ID use default");
        sprintf(node_id, "Node_UNKNOW");
    }
    sprintf(root_url, "radiolog/%s", node_id);

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

