/**
 * @file cfg.c
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "esp_partition.h"
#include "common.h"
#include "cfg.h"

#include "esp_log.h"

static const char *TAG = "cfg";

struct RadioLogCfg {
    uint32_t conver_enable;
    uint32_t conver_open;
    uint32_t conver_close;
    uint32_t conver_up_time;
    uint32_t conver_down_time;
    uint32_t conver_polling_time;
    uint32_t conver_last_position;
};

typedef struct CfgMap {
    char *key;
    uint8_t len;
} cfgmap_t;

static const cfgmap_t map[] = {
    { "cover_enable"        , 4 },
    { "cover_open"          , 4 },
    { "cover_close"         , 4 },
    { "cover_up_time"       , 4 },
    { "cover_down_time"     , 4 },
    { "cover_polling_time"  , 4 },
    { "cover_last_position" , 4 },
    { NULL                   , 0 },
};

static uint8_t tmp_data[128];
void cmd_readCfg(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    ESP_LOGI(TAG, "read");
    const esp_partition_t *prt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_NVS, "config");
    if(prt) {
        esp_err_t ret = esp_partition_read(prt, 0, (uint8_t *)tmp_data, sizeof(tmp_data));
        ESP_LOGI(TAG, "find partition %d %d %s", ret, prt->address, prt->label);
        if (ret == ESP_OK)
            for (int i = 0; i < sizeof(tmp_data); i++) {
                printf("%0x ", tmp_data[i]);
                if (!(i % 8))
                    printf("\n");
            }
    }
}

void cmd_writeCfg(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    ESP_LOGI(TAG, "write");

    size_t key_len = 0;
    bool found_key = false;
    char *value = NULL;
    for(size_t i = 0; i < len_data; i++) {
        if(data[i] == ':') {
            key_len = i - 1;
            if ((i + 1) < len_data)
                value = (char *)&data[i+1];
            found_key = true;
        }
    }

    size_t offset = 0;
    if (found_key) {
        ESP_LOGW(TAG, "%.*s", len_data, data);
        for (size_t i = 0; map[i].key && map[i].len; i++) {
            printf("> %s\n", map[i].key);
            //offset += map[i].key_len;
            if (!strncmp(data, map[i].key, key_len)) {
                printf("key: %s\n", map[i].key);
                if (value) {
                    printf("value: %s\n", value);
                }

                //const esp_partition_t *prt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                //        ESP_PARTITION_SUBTYPE_DATA_NVS, "config");
                //if(prt) {
                //    esp_err_t ret = esp_partition_read(prt, 0, (uint8_t *)tmp_data, sizeof(tmp_data));
                //    ESP_LOGI(TAG, "find partition %d %d %s", ret, prt->address, prt->label);
                //    if (ret == ESP_OK)
                //        for (int i = 0; i < sizeof(tmp_data); i++) {
                //            printf("%0x ", tmp_data[i]);
                //            if (!(i % 8))
                //                printf("\n");
                //        }
                //}

            }
        }
    }
}


void cmd_initCfg(void) {
}
