/**
 * @file cfg.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */

#ifndef CFG_H
#define CFG_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define CFG_NOVALUE 0xFFFFFFFF
#define CFG_UNSET_MODE  0
#define CFG_COVER_MODE  1
#define CFG_SWITCH_MODE 2

#define CFG_TOPIC_CFG   "cfg"
#define CFG_TOPIC_READ  "cfg/read"
#define CFG_TOPIC_WRITE "cfg/write"
#define CFG_TOPIC_DUMP  "cfg/dump"

#define CFG_INIT_VALUE(key, value, default_value) \
    do { \
        if (cfg_readKey((key), sizeof((key)), &(value)) != ESP_OK) { \
            ESP_LOGW(TAG, "Use default "key": %d", (value)); \
            (value) = (default_value); \
        } else { \
            ESP_LOGI(TAG, "Get "key": %d", (value)); \
        } \
        if ((value) == CFG_NOVALUE) { \
            (value) = (default_value); \
            ESP_LOGW(TAG, "Unset value, use default "key": %d", (value)); \
        }\
    } while(0)


esp_err_t cfg_writeKey(const char *key, size_t len_key, uint32_t value);
esp_err_t cfg_readKey(const char *key, size_t len_key, uint32_t *value);
void cmd_initCfg(QueueHandle_t *queue);

extern const char *node_modes[];

#endif /* CFG_H */
