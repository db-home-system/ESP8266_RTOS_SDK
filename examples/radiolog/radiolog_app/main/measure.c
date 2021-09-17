/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <dht.h>
#include <ds18x20.h>
#include <ads111x.h>

#include "common.h"
#include "connect.h"
#include "cover.h"
#include "mqtt_mgr.h"
#include "cfg.h"
#include "measure.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define LOG_LOCAL_LEVEL  ESP_LOG_INFO
#include "esp_log.h"

#define ONE_WIRE_PIN    GPIO_NUM_2
#define MAX_SENSORS              5

#define GAIN        ADS111X_GAIN_4V096 // +-4.096V
#define GAIN_VALUE  4096

#define I2C_PORT 0
#define SDA_GPIO 4
#define SCL_GPIO 5

static const char *TAG = "Meas";

// Descriptors
static i2c_dev_t adc_devices;

static QueueHandle_t *measure_module_queue;
static uint32_t cfg_dh11_enable = false;
static uint32_t cfg_ds18x20_sens_enable = false;
static uint32_t cfg_ads111x_adc_enable = false;
static uint32_t cfg_measure_poll_time = 60;

static ds18x20_addr_t sensor_addrs[MAX_SENSORS];
static uint32_t sensor_count = 0;

static int16_t current_sample[24];

static int read_dht11(char *meas, size_t len) {
    assert(meas);
    assert(len > 0);

    memset(meas, 0, len);

    int16_t temperature = 0;
    int16_t humidity = 0;

    if (dht_read_data(DHT_TYPE_DHT11, ONE_WIRE_PIN, &humidity, &temperature) == ESP_OK) {
        return sprintf(meas,
                "{\"humidity\":\"%d\", \"temperature\":\"%d\"}",
                humidity,
                temperature);
    }

    ESP_LOGE(TAG, "Unable to read dht11 sensor");
    return ESP_FAIL;
}

static void measure(void * pvParameter) {
    while(1) {
        mqttmsg_t jmsg;
        memset((void *)&jmsg, 0, sizeof(jmsg));

        if (cfg_dh11_enable) {
            jmsg.json_str_len = read_dht11(jmsg.json_str, MAX_JSON_STR_LEN);
            if (jmsg.json_str_len != ESP_FAIL) {
                strcpy(jmsg.topic, MEAS_TOPIC);
                jmsg.topic_len = sizeof(MEAS_TOPIC);

                if (xQueueSend(*measure_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
                    ESP_LOGE(TAG, "Error while send meas to queue");
            }
        }

        if (cfg_ds18x20_sens_enable) {
            float temps[sensor_count];
            if (ds18x20_measure_and_read_multi(ONE_WIRE_PIN, sensor_addrs, sensor_count, temps) == ESP_OK)
            {
                char *p = jmsg.json_str;
                int len = sprintf(p, "{");
                jmsg.json_str_len = 1;
                p++;
                for (int j = 0; j < sensor_count; j++)
                {
                    uint32_t temp_c = (uint32_t)(temps[j] * 1000);

                    ESP_LOGI(TAG, "Sensor %08x%08x (%s) reports %d°mC",
                            (uint32_t)(sensor_addrs[j] >> 32), (uint32_t)sensor_addrs[j],
                            (sensor_addrs[j] & 0xff) == DS18B20_FAMILY_ID ? "DS18B20" : "DS18S20",
                            temp_c);

                    len = sprintf(p, "\"temp%d\":\"%d\",", j, temp_c);
                    if (len <= 0)
                        continue;

                    p += len;
                    jmsg.json_str_len += len;
                }
                p--;
                sprintf(p, "}");

                strcpy(jmsg.topic, MEAS_TOPIC);
                jmsg.topic_len = sizeof(MEAS_TOPIC);

                if (xQueueSend(*measure_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
                    ESP_LOGE(TAG, "Error while send meas to queue");

            } else {
                ESP_LOGE(TAG, "Sensors read error");
            }
        }

        if (cfg_ads111x_adc_enable) {
            ESP_ERROR_CHECK(ads111x_set_input_mux(&adc_devices, ADS111X_MUX_0_GND));    // positive = AIN0, negative = GND
            int idx = 0;
            uint32_t avg = 0;
            for (int i = 0; i < 24; i++) {
                bool busy;
                do
                {
                    ads111x_is_busy(&adc_devices, &busy);
                } while (busy);

                int16_t raw;
                if (ads101x_get_value(&adc_devices, &raw) == ESP_OK)
                {
                    current_sample[i] = GAIN_VALUE / ADS101X_MAX_VALUE * raw;
                    avg += current_sample[i];
                    idx++;
                }
            }
            printf("ADC sample: %d, avg: %d\n", idx, avg/idx);

            memset((void *)&jmsg, 0, sizeof(jmsg));
            char *p = jmsg.json_str;
            int len = sprintf(p, "{\"curr\":[");
            jmsg.json_str_len += len;
            p += len;
            for (int j = 0; j < idx; j++)
            {
                len = sprintf(p, "%d,", current_sample[j]);
                if (len <= 0)
                    continue;

                p += len;
                jmsg.json_str_len += len;
            }
            p--;
            jmsg.json_str_len++;
            sprintf(p, "]}");

            strcpy(jmsg.topic, MEAS_TOPIC);
            jmsg.topic_len = sizeof(MEAS_TOPIC);

            if (xQueueSend(*measure_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
                ESP_LOGE(TAG, "Error while send meas to queue");

        }
        DELAY_S(cfg_measure_poll_time);
    }
}

#define CHECK_RET(foo, msg) \
    do { \
        if ((foo) != ESP_OK) { \
            ESP_LOGE(TAG, (msg)); \
            cfg_ads111x_adc_enable = 0; \
            goto exit; \
        } \
    } while (0)

void measure_init(QueueHandle_t *queue) {
    measure_module_queue = queue;

    CFG_INIT_VALUE("dht11_enable", cfg_dh11_enable, false);
    CFG_INIT_VALUE("ds18x20_sens_enable", cfg_ds18x20_sens_enable, false);
    CFG_INIT_VALUE("ads111x_adc_enable", cfg_ads111x_adc_enable, false);
    CFG_INIT_VALUE("measure_poll_time", cfg_measure_poll_time, 60);

    if (cfg_ds18x20_sens_enable)
    {
        gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);

        /* Go to search connected 1-wire device, we do it only at boot time */
        if (ds18x20_scan_devices(ONE_WIRE_PIN, sensor_addrs, MAX_SENSORS, &sensor_count) == ESP_OK)
        {
            if (!sensor_count)
            {
                ESP_LOGW(TAG, "No sensors detected!");
            }

            if (sensor_count > MAX_SENSORS)
                sensor_count = MAX_SENSORS;

            ESP_LOGI(TAG, "%d sensors detected", sensor_count);
        }
    }

    if (cfg_ads111x_adc_enable) {
        memset(&adc_devices, 0, sizeof(adc_devices));

        CHECK_RET(i2cdev_init(), "Can't init i2c");
        CHECK_RET(ads111x_init_desc(&adc_devices, ADS111X_ADDR_GND, I2C_PORT, SDA_GPIO, SCL_GPIO), "Can't init adc");
        CHECK_RET(ads111x_set_mode(&adc_devices, ADS111X_MODE_CONTINUOUS), "ADC setting error");
        CHECK_RET(ads111x_set_data_rate(&adc_devices, ADS111X_DATA_RATE_475) , "ADC setting error");
        CHECK_RET(ads111x_set_gain(&adc_devices, GAIN) , "ADC setting error");
    }

exit:
    xTaskCreate(&measure, "device_measure_task", 8192, NULL, 10, NULL);
}

