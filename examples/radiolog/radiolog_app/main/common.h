/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "tcpip_adapter.h"

#define OTA_MAX_RETRY 2

#define RADIOLOG_INTERFACE TCPIP_ADAPTER_IF_STA

/**
 * @brief Configure Wi-Fi or Ethernet, connect, wait for IP
 *
 * @return ESP_OK on successful connection
 */
esp_err_t common_connect(void);

/**
 * Counterpart to common_connect, de-initializes Wi-Fi or Ethernet
 */
esp_err_t common_disconnect(void);

/**
 * @brief Configure SSID and password
 */
esp_err_t common_set_connection_info(const char *ssid, const char *passwd);

esp_err_t common_nodeId(char *id, size_t len);
void common_ota_task();

// Simple help for delay functions
#define DELAY_S(s)    (vTaskDelay(((s) * 1000) / portTICK_PERIOD_MS))
#define DELAY_MS(ms)  (vTaskDelay((ms) / portTICK_PERIOD_MS))

#ifdef __cplusplus
}
#endif
