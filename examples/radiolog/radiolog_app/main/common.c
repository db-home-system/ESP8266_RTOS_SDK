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

#include "common.h"
#include "esp_log.h"

#include <dht.h>

#include "esp_system.h"


static const char *TAG = "com";

int read_dht11(char *meas, size_t len) {
    assert(meas);
    assert(len > 0);

    memset(meas, 0, len);

    int16_t temperature = 0;
    int16_t humidity = 0;

    if (dht_read_data(DHT_TYPE_DHT11, GPIO_NUM_2, &humidity, &temperature) == ESP_OK) {
        return sprintf(meas,
                "{\"humidity\":\"%d\", \"temperature\":\"%d\"}",
                humidity,
                temperature);
    }

    ESP_LOGE(TAG, "Unable to read dht11 sensor");
    return ESP_FAIL;
}

