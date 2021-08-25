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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char *TAG = "Switch";

static QueueHandle_t *switch_module_queue;

static void measure(void * pvParameter) {
    while(1) {
        mqttmsg_t jmsg;
        memset((void *)&jmsg, 0, sizeof(jmsg));

        //jmsg.json_str_len = read_dht11(jmsg.json_str, MAX_JSON_STR_LEN);
        if (jmsg.json_str_len != ESP_FAIL) {
            strcpy(jmsg.topic, SWITCH_TOPIC);
            jmsg.topic_len = sizeof(SWITCH_TOPIC);

            if (xQueueSend(*switch_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
                ESP_LOGE(TAG, "Error while send switch to queue");
        }
        DELAY_S(30);
    }
}

void switch_init(QueueHandle_t *queue) {
    switch_module_queue = queue;

    xTaskCreate(&measure, "device_switch_task", 8192, NULL, 10, NULL);
}

