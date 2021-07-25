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
static char buff[240];
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

static mqtt_sub_callback_t mqtt_mgr_searchFoo(const char *topic, size_t len) {
    for (int i = 0; local_callback[i].topic &&
            local_callback[i].foo; i++) {

        memset(buff, 0, sizeof(buff));
        if(sizeof(buff) < len) {
            ESP_LOGE(TAG, "Unable to copy topic buff, is too long!");
            return NULL;
        }
        memcpy(buff, topic, len);

        static char buff2[240];
        sprintf(buff2, "%s/%s", root_url, local_callback[i].topic);
        if (!strcmp(buff, buff2)) {
            return local_callback[i].foo;
        }
    }
    return NULL;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    //ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_mgr_registerCallbacks();
            is_connect = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            is_connect = false;
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGW(TAG, "MQTT_EVENT_DATA");
            ESP_LOGW(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGW(TAG, "DATA=%.*s", event->data_len, event->data);
            mqtt_sub_callback_t handler = mqtt_mgr_searchFoo(event->topic, event->topic_len);
            if (handler) {
                ESP_LOGW(TAG, "Run callback..");
                handler(event->topic, event->topic_len, event->data, event->data_len);
            }
            break;

        default:
            ESP_LOGI(TAG, "MQTT_EVENT id:%d, msg_id=%d", event->event_id, event->msg_id);
            break;
    }
}

void mqtt_mgr_pub(char *topic, size_t len_topic, const char *data, size_t len_data) {
    if(!is_connect) {
        ESP_LOGE(TAG, "MQTT client is not connect");
        return;
    }

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
        .lwt_qos = 0,
        .lwt_retain = 0,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

