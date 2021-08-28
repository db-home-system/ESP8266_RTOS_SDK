/**
 * @file common.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */
#ifndef COMMON_H
#define COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"


// Simple help for delay functions
#define DELAY_S(s)    (vTaskDelay(((s) * 1000) / portTICK_PERIOD_MS))
#define DELAY_MS(ms)  (vTaskDelay((ms) / portTICK_PERIOD_MS))

#define APP_TOPIC_ANNOUNCE  "announce"

void device_announce(uint32_t mode, QueueHandle_t *queue);

#endif /* COMMON_H */
