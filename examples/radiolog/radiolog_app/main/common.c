/**
 * @file common.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "common.h"
#include "connect.h"
#include "mqtt_mgr.h"
#include "cfg.h"
#include "verstag.h"

#include "esp_system.h"

#include "esp_log.h"
#define LOG_LOCAL_LEVEL  ESP_LOG_INFO

static const char *TAG = "Com";

void device_announce(uint32_t mode, QueueHandle_t *queue)
{
    char node[NODEID_STR_LEN];
    int len_node = common_nodeId(node, NODEID_STR_LEN);
    if (len_node == ESP_FAIL) {
        strcpy(node, "-");
        len_node = 1;
    }

    char ipaddr[IDADDR_STR_LEN];
    int len_ipaddr = common_ipAddr(ipaddr, IDADDR_STR_LEN);
    if (len_ipaddr == ESP_FAIL) {
        strcpy(ipaddr, "-");
        len_ipaddr = 1;
    }

    mqttmsg_t jmsg;
    jmsg.json_str_len = sprintf((char *)&jmsg.json_str,
            "{\"name\":\"%s\",\"ip\":\"%s\",\"ver\":\"%s\",\"mode\":\"%s\"}",
            node, ipaddr, vers_tag, node_modes[mode]);

    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, APP_TOPIC_ANNOUNCE);
        jmsg.topic_len = sizeof(APP_TOPIC_ANNOUNCE);

        if (xQueueSend(*queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while annouce to queue");
    }
}
