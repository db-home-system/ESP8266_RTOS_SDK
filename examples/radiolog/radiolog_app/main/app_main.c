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

#define MQTT_TOPIC_ANNOUNCE  "cover/announce"
#define MQTT_TOPIC_STATUS    "cover/status"
#define MQTT_TOPIC_SET_POS   "cover/set_position"
#define MQTT_TOPIC_POS       "cover/pos"
#define MQTT_TOPIC_SET       "cover/set"
#define MQTT_TOPIC_AVAILABLE "cover/available"
#define MQTT_TOPIC_MEAS      "measure"
#define MQTT_TOPIC_RESET     "reset"

#define MQTT_TOPIC_READ_CFG  "cfg/read"
#define MQTT_TOPIC_WRITE_CFG "cfg/write"
#define MQTT_TOPIC_DUMP_CFG  "cfg/dump"


#define BUTTON_UP    0  // D3
#define BUTTON_DOWN  4 // D6

static const char *TAG = "Radiolog";

static cover_ctx_t cover_ctx;
static button_t btn_up, btn_down;
static char json_str[250];
static bool data_ready = false;
static int json_str_len = 0;

void cmd_readCfg(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    memset(json_str, 0, sizeof(json_str));

    uint32_t raw_value = CFG_NOVALUE;
    esp_err_t ret = cfg_readKey(data, len_data, &raw_value);
    if (ret == ESP_OK) {
        json_str_len = sprintf(json_str,
            "{\"%.*s\":\"%d\"}",
            len_data, data, raw_value);
    }

    if (json_str_len != ESP_FAIL) {
        ESP_LOGI(TAG, "read: %.*s", json_str_len, json_str);
        data_ready = true;
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
    { MQTT_TOPIC_SET       , cmd_coverSet    } ,
    { MQTT_TOPIC_SET_POS   , cmd_coverSetPos } ,
    { MQTT_TOPIC_RESET     , cmd_reset       } ,
    { MQTT_TOPIC_READ_CFG  , cmd_readCfg     } ,
    { MQTT_TOPIC_WRITE_CFG , cmd_writeCfg    } ,
    { MQTT_TOPIC_DUMP_CFG  , cmd_dumpCfg     } ,
    { NULL                 , NULL            } ,
};

static bool announce = true;
static void device_status(void * pvParameter)
{
    while (1) {
        int ret = 0;

        if(announce) {
            mqtt_mgr_pub(MQTT_TOPIC_ANNOUNCE, sizeof(MQTT_TOPIC_ANNOUNCE), "announce", sizeof("announce") -1);
            mqtt_mgr_pub(MQTT_TOPIC_AVAILABLE, sizeof(MQTT_TOPIC_AVAILABLE), "online", sizeof("online") -1);
            announce = false;
        }

        ret = read_dht11(json_str, sizeof(json_str));
        if (ret != ESP_FAIL)
            mqtt_mgr_pub(MQTT_TOPIC_MEAS, sizeof(MQTT_TOPIC_MEAS), json_str, ret);

        ret = cover_status(json_str, sizeof(json_str));
        if (ret != ESP_FAIL)
            mqtt_mgr_pub(MQTT_TOPIC_STATUS, sizeof(MQTT_TOPIC_STATUS), json_str, ret);

        ret = cover_position(json_str, sizeof(json_str));
        if (ret != ESP_FAIL)
            mqtt_mgr_pub(MQTT_TOPIC_POS, sizeof(MQTT_TOPIC_POS), json_str, ret);

        DELAY_S(30);
    }
}

static void event_cover_stop(uint8_t status, uint16_t position) {
    int ret = cover_status(json_str, sizeof(json_str));
    if (ret != ESP_FAIL)
        mqtt_mgr_pub(MQTT_TOPIC_STATUS, sizeof(MQTT_TOPIC_STATUS), json_str, ret);

    ret = cover_position(json_str, sizeof(json_str));
    if (ret != ESP_FAIL)
        mqtt_mgr_pub(MQTT_TOPIC_POS, sizeof(MQTT_TOPIC_POS), json_str, ret);
}

static const char *states[] = {
    [BUTTON_PRESSED]      = "pressed",
    [BUTTON_RELEASED]     = "released",
    [BUTTON_CLICKED]      = "clicked",
    [BUTTON_PRESSED_LONG] = "pressed long",
};

static void on_button(button_t *btn, button_state_t state)
{
    ESP_LOGI(TAG, "%s button %s", btn == &btn_up ? "up" : "down", states[state]);

    if (state == BUTTON_PRESSED || state == BUTTON_PRESSED_LONG) {
        if (btn == &btn_up) {
            cover_run(100);
            return;
        }

        if (btn == &btn_down) {
            cover_run(0);
            return;
        }
    }

    cover_stop();
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
    btn_up.internal_pull = true;
    btn_up.autorepeat = false;
    btn_up.callback = on_button;

    btn_down.gpio = BUTTON_DOWN;
    btn_down.pressed_level = 0;
    btn_down.internal_pull = true;
    btn_down.autorepeat = false;
    btn_down.callback = on_button;

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

    xTaskCreate(&device_status, "device_status_task", 8192, NULL, 5, NULL);
}

