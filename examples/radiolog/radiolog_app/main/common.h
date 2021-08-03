/**
 * @file common.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */

#ifndef COMMON_H
#define COMMON_H

// Simple help for delay functions
#define DELAY_S(s)    (vTaskDelay(((s) * 1000) / portTICK_PERIOD_MS))
#define DELAY_MS(ms)  (vTaskDelay((ms) / portTICK_PERIOD_MS))

int read_dht11(char *meas, size_t len);

#endif /* COMMON_H */
