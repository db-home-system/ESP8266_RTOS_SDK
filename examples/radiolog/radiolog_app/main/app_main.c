/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <button.h>

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

#define BUTTON_UP    0  // D3
#define BUTTON_DOWN  4 // D6

static const char *TAG = "Radiolog";

//{"position":"9999", "ticks":"999"}


static QueueHandle_t mqtt_msg_queue;
static cover_ctx_t cover_ctx;
static button_t btn_up, btn_down;


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

static void cover_status_fill(const cover_ctx_t *ctx) {
    mqttmsg_t jmsg;

    jmsg.json_str_len = cover_position((char *)&jmsg.json_str, MAX_JSON_STR_LEN);
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, COVER_TOPIC_POS);
        jmsg.topic_len = sizeof(COVER_TOPIC_POS);

        if (xQueueSend(mqtt_msg_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while pos to queue");
    }

    jmsg.json_str_len = cover_status((char *)&jmsg.json_str, MAX_JSON_STR_LEN);
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, COVER_TOPIC_STATUS);
        jmsg.topic_len = sizeof(COVER_TOPIC_STATUS);

        if (xQueueSend(mqtt_msg_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while send status to queue");
    }
}


static void cover_status_task(void * pvParameter) {
    while(1) {
        cover_status_fill(&cover_ctx);
        DELAY_S(30);
    }
}

static void event_cover_stop(const cover_ctx_t *ctx) {
    cover_status_fill(ctx);
}

static const char *states[] = {
    [BUTTON_PRESSED]      = "pressed",
    [BUTTON_RELEASED]     = "released",
    [BUTTON_CLICKED]      = "clicked",
    [BUTTON_PRESSED_LONG] = "pressed long",
};

static void on_button_up(button_t *btn, button_state_t state)
{
    ESP_LOGI(TAG, "UP button %s", states[state]);
    if (state == BUTTON_PRESSED || state == BUTTON_PRESSED_LONG) {
        cover_run(100);
    }

    if (state == BUTTON_RELEASED) {
        cover_stop();
    }
}

static void on_button_down(button_t *btn, button_state_t state)
{
    ESP_LOGI(TAG, "DOWN button %s", states[state]);
    if (state == BUTTON_PRESSED || state == BUTTON_PRESSED_LONG) {
        cover_run(0);
    }

    if (state == BUTTON_RELEASED) {
        cover_stop();
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

    btn_up.gpio = BUTTON_UP;
    btn_up.pressed_level = 0;
    btn_up.internal_pull = false;
    btn_up.autorepeat = false;
    btn_up.callback = on_button_up;

    btn_down.gpio = BUTTON_DOWN;
    btn_down.pressed_level = 0;
    btn_down.internal_pull = false;
    btn_down.autorepeat = false;
    btn_down.callback = on_button_down;

    mqtt_msg_queue = xQueueCreate(3, sizeof(mqttmsg_t));
    if(mqtt_msg_queue == 0)
        ESP_LOGE(TAG, "Unable to alloc a queue");
    assert(mqtt_msg_queue != 0);

    mqtt_mgr_init(callback_table);
    cmd_initCfg(&mqtt_msg_queue);
    cover_init(&cover_ctx, event_cover_stop);
    ESP_ERROR_CHECK(button_init(&btn_down));
    ESP_ERROR_CHECK(button_init(&btn_up));

    //uint32_t cfg_mode;
    //esp_err_t ret = cfg_readKey("node_mode", sizeof("node_mode"), &cfg_mode);
    //if (cfg_mode == CFG_COVER) {
    //}
    //if (cfg_mode == CFG_SWITCH)
    //    switch_init();

    xTaskCreate(&publish_msg, "mqtt_pub_task", 8192, NULL, 10, NULL);
    xTaskCreate(&measure, "device_measure_task", 8192, NULL, 10, NULL);
    xTaskCreate(&cover_status_task, "device_measure_task", 8192, NULL, 10, NULL);
}

