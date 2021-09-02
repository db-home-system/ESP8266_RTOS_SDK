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

#include "common.h"
#include "connect.h"
#include "cover.h"
#include "mqtt_mgr.h"
#include "cfg.h"
#include "switch.h"
#include "macros.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"

#define BTN_PIN        0
#define TRIAC_SX_PIN  16
#define VOLT_PRESENT  14
#define SWITCH_ON()  (gpio_set_level(TRIAC_SX_PIN, 1))
#define SWITCH_OFF() (gpio_set_level(TRIAC_SX_PIN, 0))

#define CFG_SWITCH_PULSE_WIDTH    1000 //ms

#define SWITCH_PULSE(delay) \
    do { \
        SWITCH_ON(); \
        DELAY_MS((delay)); \
        SWITCH_OFF(); \
        DELAY_MS((delay)); \
    } while (0)

static const char *TAG = "Switch";

static QueueHandle_t *switch_module_queue;
static uint32_t cfg_switch_mode;
static uint32_t cfg_switch_pulse_width;
static bool is_switch_on = false;
static button_t btn_switch;

static void switch_sendStatus(void) {
    mqttmsg_t jmsg;
    memset((void *)&jmsg, 0, sizeof(jmsg));

    jmsg.json_str_len = sprintf((char *)&jmsg.json_str,
            "{\"status\":\"%s\"}", is_switch_on ? "on":"off");
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, SWITCH_TOPIC);
        jmsg.topic_len = sizeof(SWITCH_TOPIC);

        if (xQueueSend(*switch_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while send switch to queue");
    }
}

static void switch_status(void * pvParameter) {
    while(1) {
        switch_sendStatus();
        ESP_LOGE(TAG, "[%d]", gpio_get_level(VOLT_PRESENT));
        DELAY_S(10);
    }
}

static void switch_on(bool on)
{
    if (on) {
        if (cfg_switch_mode == CFG_SWITCH_MAKE_PULSE) {
            SWITCH_PULSE(cfg_switch_pulse_width);
        } else {
            SWITCH_ON();
        }

        is_switch_on = true;
    } else {
        if (cfg_switch_mode == CFG_SWITCH_MAKE_PULSE) {
            SWITCH_PULSE(cfg_switch_pulse_width);
        } else {
            SWITCH_OFF();
        }

        is_switch_on = false;
    }

    ESP_LOGE(TAG, "Set switch[%d]", is_switch_on);
    switch_sendStatus();
}

static void cmd_switch(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    if (len_data == 0 && !data) {
        ESP_LOGE(TAG, "Invalid paylod in switch set");
        return;
    }

    if (!strncmp("on", data, len_data)) {
        switch_on(true);
        return;
    }

    if (!strncmp("off", data, len_data)) {
        switch_on(false);
        return;
    }
    ESP_LOGE(TAG, "invalid option");
}

static CmdMQTT callback_table[] = {
    { SWITCH_TOPIC_SET    , cmd_switch      } ,
    { NULL                , NULL            } ,
};

static void on_button(button_t *btn, button_state_t state)
{
    ESP_LOGI(TAG, "switch button %d", state);
    if (state == BUTTON_PRESSED || state == BUTTON_PRESSED_LONG) {
        switch_on(true);
    }

    if (state == BUTTON_RELEASED) {
        switch_on(false);
    }
}

void switch_init(QueueHandle_t *queue) {
    switch_module_queue = queue;

    // Register on mqtt table cover callbacks
    mqtt_mgr_regiterTable(callback_table);

    CFG_INIT_VALUE("switch_mode", cfg_switch_mode, CFG_NOVALUE);
    CFG_INIT_VALUE("switch_pulse_width", cfg_switch_pulse_width, CFG_SWITCH_PULSE_WIDTH);

    gpio_set_direction(VOLT_PRESENT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(VOLT_PRESENT, GPIO_FLOATING);

    gpio_set_direction(TRIAC_SX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(TRIAC_SX_PIN, GPIO_PULLUP_ONLY);


    /* Configure gpio to read buttons status */
    btn_switch.gpio = BTN_PIN;
    btn_switch.pressed_level = 0;
    btn_switch.internal_pull = true;
    btn_switch.autorepeat = false;
    btn_switch.callback = on_button;

    ESP_ERROR_CHECK(button_init(&btn_switch));

    SWITCH_OFF();
    is_switch_on = false;

    xTaskCreate(&switch_status, "device_switch_task", 8192, NULL, 10, NULL);
}

