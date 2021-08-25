/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/
#ifndef SWITCH_H
#define SWITCH_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define SWITCH_TOPIC      "switch"

void switch_init(QueueHandle_t *queue);

#endif /* SWITCH_H */
