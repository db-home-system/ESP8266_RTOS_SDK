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
#include "connect.h"
#include "cover.h"
#include "mqtt_mgr.h"
#include "cfg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"

#define APP_TOPIC_ANNOUNCE  "announce"
#define APP_TOPIC_STATUS    "status"
#define APP_TOPIC_MEAS      "measure"
#define APP_TOPIC_RESET     "reset"


static const char *TAG = "Radiolog";

//{"position":"9999", "ticks":"999"}


static QueueHandle_t mqtt_msg_queue;
static cover_ctx_t cover_ctx;


void cmd_reset(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    esp_restart();
}

static CmdMQTT callback_table[] = {
    { APP_TOPIC_RESET     , cmd_reset       } ,
    { NULL                , NULL            } ,
};


static bool announce = true;
static void publish_msg(void * pvParameter)
{
    while (1) {
        if(announce) {
            mqtt_mgr_pub(APP_TOPIC_ANNOUNCE, sizeof(APP_TOPIC_ANNOUNCE), "announce", sizeof("announce") -1);
            announce = false;
        }

        if(!mqtt_msg_queue)
            return;

        mqttmsg_t buff;
        if(xQueueReceive(mqtt_msg_queue, &(buff), (TickType_t)0))
        {
            ESP_LOGI(TAG, "send: %.*s %.*s len %d %d", buff.json_str_len, buff.json_str, \
                    buff.topic_len, buff.topic, \
                    buff.json_str_len, buff.topic_len);
            mqtt_mgr_pub(buff.topic, buff.topic_len, buff.json_str, buff.json_str_len);

        }
        DELAY_MS(500);
    }
}

static void measure(void * pvParameter) {
    while(1) {
        mqttmsg_t jmsg;
        memset((void *)&jmsg, 0, sizeof(jmsg));

        jmsg.json_str_len = read_dht11(jmsg.json_str, MAX_JSON_STR_LEN);
        if (jmsg.json_str_len != ESP_FAIL) {
            strcpy(jmsg.topic, APP_TOPIC_MEAS);
            jmsg.topic_len = sizeof(APP_TOPIC_MEAS);

            if (xQueueSend(mqtt_msg_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
                ESP_LOGE(TAG, "Error while send meas to queue");
        }

        DELAY_S(30);
    }
}


static void event_cover_stop(const cover_ctx_t *ctx) {
    cover_prepareStatusMsg(ctx);
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


    mqtt_msg_queue = xQueueCreate(3, sizeof(mqttmsg_t));
    if(mqtt_msg_queue == 0)
        ESP_LOGE(TAG, "Unable to alloc a queue");
    assert(mqtt_msg_queue != 0);

    mqtt_mgr_init(callback_table);
    cmd_initCfg(&mqtt_msg_queue);
    cover_init(&cover_ctx, event_cover_stop, &mqtt_msg_queue);

    //uint32_t cfg_mode;
    //esp_err_t ret = cfg_readKey("node_mode", sizeof("node_mode"), &cfg_mode);
    //if (cfg_mode == CFG_COVER) {
    //}
    //if (cfg_mode == CFG_SWITCH)
    //    switch_init();

    xTaskCreate(&publish_msg, "mqtt_pub_task", 8192, NULL, 10, NULL);
    xTaskCreate(&measure, "device_measure_task", 8192, NULL, 10, NULL);
}

