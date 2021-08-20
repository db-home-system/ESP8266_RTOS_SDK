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
#include <stdlib.h>

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
} cfgmap_t;

static const cfgmap_t map[] = {
    { "node_mode"           },
    { "cover_open"          },
    { "cover_close"         },
    { "cover_up_time"       },
    { "cover_down_time"     },
    { "cover_polling_time"  },
    { "cover_last_position" },
    { NULL                  },
};

esp_err_t cfg_dump(void) {
    ESP_LOGW(TAG, "dump");
    // Get key, go to read value in config partition
    const esp_partition_t *prt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_NVS, "config");

    if(prt) {
        //ESP_LOGI(TAG, "Get partition %d %s", prt->address, prt->label);
        for (size_t i = 0; i < prt->size / SPI_FLASH_SEC_SIZE; i++) {
            uint32_t tmp[SPI_FLASH_SEC_SIZE/sizeof(uint32_t)];

            esp_err_t ret = esp_partition_read(prt, i * SPI_FLASH_SEC_SIZE,\
                    (uint8_t *)tmp, sizeof(tmp));
            if (ret == ESP_OK) {
                for (int j = 0; j < SPI_FLASH_SEC_SIZE/sizeof(uint32_t); j++) {
                    printf("%0x ", tmp[j]);
                    if (!(j%8) && j)
                        printf("\n");
                }
            } else {
                ESP_LOGE(TAG, "Unable to read block [%d]", ret);
                return ESP_FAIL;
            }
        }
    }
    return ESP_OK;
}


esp_err_t cfg_readKey(const char *key, size_t len_key, uint32_t *value) {
    //ESP_LOGW(TAG, "Read: %.*s", len_key, key);
    *value = CFG_NOVALUE;
    for (size_t i = 0; map[i].key; i++) {
        if (!strncmp(key, map[i].key, len_key)) {
            ESP_LOGI(TAG, "key: %s", map[i].key);

            // Get key, go to read value in config partition
            const esp_partition_t *prt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                    ESP_PARTITION_SUBTYPE_DATA_NVS, "config");
            if(prt) {
                //ESP_LOGI(TAG, "Get partition %d %s", prt->address, prt->label);
                size_t offset = i * sizeof(uint32_t);
                esp_err_t ret = esp_partition_read(prt, offset, (uint8_t *)value, sizeof(uint32_t));
                if (ret == ESP_OK)
                    return ESP_OK;

                ESP_LOGE(TAG, "Unable to read data [%d]", ret);
                return ret;
            }
        }
    }
    return ESP_FAIL;
}


// read block page to chage, we like aligned to 32bit
static uint32_t tmp[SPI_FLASH_SEC_SIZE/sizeof(uint32_t)];
esp_err_t cfg_writeKey(const char *key, size_t len_key, uint32_t value) {
    ESP_LOGW(TAG, "Write: %.*s [%d]", len_key, key, value);
    for (size_t i = 0; map[i].key; i++) {
        if (!strncmp(key, map[i].key, len_key)) {

            // Get key, go to read value in config partition
            const esp_partition_t *prt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                    ESP_PARTITION_SUBTYPE_DATA_NVS, "config");

            if(prt) {
                memset(tmp, 0xff, sizeof(tmp));
                size_t offset = i * sizeof(uint32_t);
                ESP_LOGI(TAG, "key: %s offset: %d size: %d", map[i].key, offset, sizeof(uint32_t));
                size_t page = (offset / SPI_FLASH_SEC_SIZE) * SPI_FLASH_SEC_SIZE;

                esp_err_t ret = esp_partition_read(prt, page, (uint8_t *)tmp, sizeof(tmp));
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Unable to read data block [%d]", ret);
                    return ret;
                }

                ret = esp_partition_erase_range(prt, page, SPI_FLASH_SEC_SIZE);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Unable to erase partition block [%d]", ret);
                    return ret;
                }
                tmp[offset/sizeof(uint32_t)] = value;
                ESP_LOGI(TAG, "page %d off %d v %d", page, offset, value);

                ret = esp_partition_write(prt, page, (char *)tmp, sizeof(tmp));
                if (ret == ESP_OK) {
                    return ESP_OK;
                }
                ESP_LOGE(TAG, "Unable to write data block [%d]", ret);
                return ret;
            }
        }

    }
    return ESP_FAIL;
}

void cmd_initCfg(void) {
}
