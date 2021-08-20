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

#define COVER_TOPIC_STATUS    "cover/status"
#define COVER_TOPIC_SET_POS   "cover/set_position"
#define COVER_TOPIC_POS       "cover/pos"
#define COVER_TOPIC_SET       "cover/set"
#define COVER_TOPIC_AVAILABLE "cover/available"

#define CFG_TOPIC_READ  "cfg/read"
#define CFG_TOPIC_WRITE "cfg/write"
#define CFG_TOPIC_DUMP  "cfg/dump"

#define APP_TOPIC_ANNOUNCE  "announce"
#define APP_TOPIC_STATUS    "status"
#define APP_TOPIC_MEAS      "measure"
#define APP_TOPIC_RESET     "reset"


#define BUTTON_UP    0  // D3
#define BUTTON_DOWN  4 // D6

static const char *TAG = "Radiolog";

//{"position":"9999", "ticks":"999"}

#define MAX_JSON_STR_LEN 80
#define MAX_TOPIC_LEN 50

typedef struct MqttMsg
{
    int json_str_len;
    char json_str[MAX_JSON_STR_LEN];
    int topic_len;
    char topic[MAX_TOPIC_LEN];
} mqttmsg_t;

QueueHandle_t mqtt_msg_queue;

static cover_ctx_t cover_ctx;
static button_t btn_up, btn_down;



void cmd_readCfg(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    mqttmsg_t jmsg;
    memset((void *)&jmsg, 0, sizeof(jmsg));

    uint32_t raw_value = CFG_NOVALUE;
    esp_err_t ret = cfg_readKey(data, len_data, &raw_value);
    if (ret == ESP_OK) {
        jmsg.json_str_len = sprintf(jmsg.json_str,
                "{\"%.*s\":\"%d\"}",
                len_data, data, raw_value);

        if (jmsg.json_str_len != ESP_FAIL) {
            strcpy(jmsg.topic, APP_TOPIC_STATUS);
            jmsg.topic_len = sizeof(APP_TOPIC_STATUS);
            if (xQueueSend(mqtt_msg_queue, (void *)&jmsg, (TickType_t)0) != pdPASS)
                ESP_LOGE(TAG, "Error while send cfg to queue");
        }
    }
}

void cmd_writeCfg(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    ESP_LOGI(TAG, "write");

    size_t len_key = 0;
    bool found_key = false;
    char *value = NULL;
    for(size_t i = 0; i < len_data; i++) {
        if(data[i] == ':') {
            len_key = i;
            if ((i + 1) < len_data)
                value = (char *)&data[i+1];
            found_key = true;
        }
    }

    if (found_key) {
        char *p;
        uint32_t v = strtol(value, &p, 10);
        esp_err_t ret = cfg_writeKey(data, len_key, v);
        if (ret == ESP_OK)
            ESP_LOGI(TAG, ">> %d << %.*s", v, len_key, data);
    }
}

void cmd_dumpCfg(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    cfg_dump();
}


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

    if (!strncmp("open", data, len_data)) {
        cover_run(100);
        return;
    }

    if (!strncmp("close", data, len_data)) {
        cover_run(0);
        return;
    }

    if (!strncmp("stop", data, len_data)) {
        cover_stop();
        return;
    }

}

void cmd_reset(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    esp_restart();
}

static CmdMQTT callback_table[] = {
    { COVER_TOPIC_SET     , cmd_coverSet    } ,
    { COVER_TOPIC_SET_POS , cmd_coverSetPos } ,
    { APP_TOPIC_RESET     , cmd_reset       } ,
    { CFG_TOPIC_READ      , cmd_readCfg     } ,
    { CFG_TOPIC_WRITE     , cmd_writeCfg    } ,
    { CFG_TOPIC_DUMP      , cmd_dumpCfg     } ,
    { NULL                , NULL            } ,
};


static bool announce = true;
static void publish_msg(void * pvParameter)
{
    while (1) {

        mqttmsg_t buff;

        if(!mqtt_msg_queue)
            return;

        if(announce) {
            mqtt_mgr_pub(APP_TOPIC_ANNOUNCE, sizeof(APP_TOPIC_ANNOUNCE), "announce", sizeof("announce") -1);
            mqtt_mgr_pub(COVER_TOPIC_AVAILABLE, sizeof(COVER_TOPIC_AVAILABLE), "online", sizeof("online") -1);
            announce = false;
        }

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

static void event_cover_stop(uint8_t status, uint16_t position) {
    mqttmsg_t jmsg;
    memset((void *)&jmsg, 0, sizeof(jmsg));
    jmsg.json_str_len = cover_status(jmsg.json_str, MAX_JSON_STR_LEN);
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, COVER_TOPIC_STATUS);
        jmsg.topic_len = sizeof(COVER_TOPIC_STATUS);

        if (xQueueSend(mqtt_msg_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while send status to queue");
    }

    jmsg.json_str_len = cover_position(jmsg.json_str, MAX_JSON_STR_LEN);
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, COVER_TOPIC_POS);
        jmsg.topic_len = sizeof(COVER_TOPIC_POS);

        if (xQueueSend(mqtt_msg_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while send pos to queue");
    }
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
    cover_init(&cover_ctx, event_cover_stop);
    ESP_ERROR_CHECK(button_init(&btn_down));
    ESP_ERROR_CHECK(button_init(&btn_up));

    //uint32_t cfg_mode;
    //esp_err_t ret = cfg_readKey("node_mode", sizeof("node_mode"), &cfg_mode);
    //if (cfg_mode == CFG_COVER) {
    //}
    //if (cfg_mode == CFG_SWITCH)
    //    switch_init();

    xTaskCreate(&publish_msg, "device_status_task", 8192, NULL, 10, NULL);
    xTaskCreate(&measure, "device_measure_task", 8192, NULL, 10, NULL);
}

