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
#define CFG_COVER  0
#define CFG_SWITCH 1

#define CFG_TOPIC_CFG   "cfg"
#define CFG_TOPIC_READ  "cfg/read"
#define CFG_TOPIC_WRITE "cfg/write"
#define CFG_TOPIC_DUMP  "cfg/dump"

esp_err_t cfg_writeKey(const char *key, size_t len_key, uint32_t value);
esp_err_t cfg_readKey(const char *key, size_t len_key, uint32_t *value);
void cmd_initCfg(QueueHandle_t *queue);


#endif /* CFG_H */
