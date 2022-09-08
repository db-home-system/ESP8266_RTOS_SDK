/* RadioLog
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

#include "verstag.h"
#include "common.h"
#include "connect.h"
#include "cover.h"
#include "mqtt_mgr.h"
#include "cfg.h"
#include "measure.h"
#include "switch.h"
#include "verstag.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"

#define APP_TOPIC_RESET     "reset"

static const char *TAG = "Radiolog";

static QueueHandle_t mqtt_msg_queue;

void cmd_reset(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    esp_restart();
}

static CmdMQTT callback_table[] = {
    { APP_TOPIC_RESET     , cmd_reset       } ,
    { NULL                , NULL            } ,
};


static void publish_msg(void * pvParameter)
{
    while (1) {
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
        DELAY_MS(100);
    }
}

#define UDP_PORT 3333

static void event_cover_stop(const cover_ctx_t *ctx) {
    cover_prepareStatusMsg(ctx);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(common_connect());

    ESP_LOGI(TAG, "== RadioLog Start ==");
    ESP_LOGI(TAG, "ver.%s", vers_tag);
    ESP_LOGI(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_ERROR);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_ERROR);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_NONE);
    esp_log_level_set("TRANSPORT", ESP_LOG_NONE);
    esp_log_level_set("OUTBOX", ESP_LOG_NONE);


    // check if we should update the fw via ota
    common_ota_task();

    mqtt_msg_queue = xQueueCreate(3, sizeof(mqttmsg_t));
    if(mqtt_msg_queue == 0)
        ESP_LOGE(TAG, "Unable to alloc a queue");
    assert(mqtt_msg_queue != 0);

    mqtt_mgr_init(callback_table);
    xTaskCreate(&publish_msg, "mqtt_pub_task", 8192, NULL, 10, NULL);

    cmd_initCfg(&mqtt_msg_queue);
    measure_init(&mqtt_msg_queue);

    uint32_t cfg_module_mode;
    CFG_INIT_VALUE("node_mode", cfg_module_mode, CFG_UNSET_MODE);
    if (cfg_module_mode == CFG_COVER_MODE) {
        ESP_LOGI(TAG, "COVER MODE Enable");
        cover_init(event_cover_stop, &mqtt_msg_queue);
    }
    if (cfg_module_mode == CFG_SWITCH_MODE) {
        ESP_LOGI(TAG, "SWITCH MODE Enable");
        switch_init(&mqtt_msg_queue);
    }

    device_announce(cfg_module_mode, &mqtt_msg_queue);
}

