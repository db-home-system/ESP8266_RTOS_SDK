/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "common.h"
#include "connect.h"
#include "cover.h"
#include "mqtt_mgr.h"
#include "cfg.h"
#include "measure.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"

static const char *TAG = "Meas";

static QueueHandle_t *measure_module_queue;
static uint32_t cfg_dh11_enable = false;

static void measure(void * pvParameter) {
    while(1) {
        mqttmsg_t jmsg;
        memset((void *)&jmsg, 0, sizeof(jmsg));

        if (cfg_dh11_enable) {
            jmsg.json_str_len = read_dht11(jmsg.json_str, MAX_JSON_STR_LEN);
            if (jmsg.json_str_len != ESP_FAIL) {
                strcpy(jmsg.topic, MEAS_TOPIC);
                jmsg.topic_len = sizeof(MEAS_TOPIC);

                if (xQueueSend(*measure_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
                    ESP_LOGE(TAG, "Error while send meas to queue");
            }
        }
        DELAY_S(30);
    }
}

void measure_init(QueueHandle_t *queue) {
    measure_module_queue = queue;

    CFG_INIT_VALUE("dht11_enable", cfg_dh11_enable, false);

    xTaskCreate(&measure, "device_measure_task", 8192, NULL, 10, NULL);
}

