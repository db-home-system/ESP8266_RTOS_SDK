/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/
#ifndef MEASURE_H
#define MEASURE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define MEAS_TOPIC      "measure"

void measure_init(QueueHandle_t *queue);

#endif /* MEASURE_H */
