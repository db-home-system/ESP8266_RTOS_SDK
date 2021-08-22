/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <button.h>

#include "cover.h"
#include "cfg.h"
#include "common.h"
#include "mqtt_mgr.h"
#include "macros.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define TRIAC_ENABLE    14
#define TRIAC_DIR       16

#define TRIAC_SEL_DX() (gpio_set_level(TRIAC_DIR, 1))
#define TRIAC_SEL_SX() (gpio_set_level(TRIAC_DIR, 0))
#define TRIAC_ON() (gpio_set_level(TRIAC_ENABLE, 1))
#define TRIAC_OFF() (gpio_set_level(TRIAC_ENABLE, 0))

#define BUTTON_UP    0  // D3
#define BUTTON_DOWN  4  // D6

#define COVER_OPEN  0
#define COVER_CLOSE 1
#define COVER_STOP  2
#define COVER_POLLING_TIME 250
#define COVER_TRAVEL_TIME_UP 25
#define COVER_TRAVEL_TIME_DOWN 24

#define POS_TO_TICKS(p, tt, pollingtime) ((((tt) * 1000) / (pollingtime))  * (p) / 100)

static const char *TAG = "cover";

static uint32_t cfg_cover_open = COVER_OPEN;
static uint32_t cfg_cover_close = COVER_CLOSE;
static uint32_t cfg_cover_up_time = COVER_TRAVEL_TIME_UP;
static uint32_t cfg_cover_down_time = COVER_TRAVEL_TIME_DOWN;
static uint32_t cfg_cover_polling_time = COVER_POLLING_TIME;

static button_t btn_up, btn_down;
static QueueHandle_t *cover_module_queue;
static cover_ctx_t *local_ctx;

static uint16_t ticks_to_pos(cover_ctx_t *ctx) {
    int32_t p = 0;
    if(ctx->direction == cfg_cover_open)
        p = ctx->curr_pos + ((cfg_cover_polling_time * ctx->on_ticks) / (cfg_cover_up_time * 10));
    else
        p = ctx->curr_pos - ((cfg_cover_polling_time * ctx->on_ticks) / (cfg_cover_down_time * 10));

    //ESP_LOGW(TAG, "t->pos [%d] [%d]", ctx->on_ticks, p);
    return (uint16_t)MINMAX(0, p, 100);
}


static void motor_off(void) {
    TRIAC_OFF();
    local_ctx->status = COVER_STOP;
    local_ctx->curr_pos = ticks_to_pos(local_ctx);

    // Call notify callback
    if(local_ctx->callback_end) {
        local_ctx->callback_end((const cover_ctx_t *)local_ctx);
    }
    ESP_LOGW(TAG, "Motor off");

    uint32_t saved_pos = 0;
    if (cfg_readKey("cover_last_position", sizeof("cover_last_position"), &saved_pos) == ESP_OK)
        if (local_ctx->curr_pos != saved_pos) {
            if (cfg_writeKey("cover_last_position", sizeof("cover_last_position"), \
                        local_ctx->curr_pos) != ESP_OK)
                ESP_LOGE(TAG, "Unable to save pos");
        }

    vTaskSuspend(local_ctx->run_handler);
}

static void cover_run_handler(void * pvParameter)
{
    while (1) {
        local_ctx->on_ticks++;
        putchar('.');
        if (!(local_ctx->on_ticks % 10))
            putchar('\n');

        if(local_ctx->on_ticks >= local_ctx->ticks_th_stop)
            motor_off();

        DELAY_MS(cfg_cover_polling_time);
    }
}

void cover_run(int position) {

    // we are just in the same position, do nothing
    if(local_ctx->curr_pos == position)
        return;

    local_ctx->target_pos = position;
    local_ctx->on_ticks = 0;

    // guess direction 0: close; 100: open;
    local_ctx->direction = cfg_cover_close;
    local_ctx->status = cfg_cover_close;
    if(local_ctx->target_pos >= local_ctx->curr_pos) {
        local_ctx->direction = cfg_cover_open;
        local_ctx->status = cfg_cover_open;
    }

    //go to desiderate position
    uint32_t delta_pos = ABS((int32_t)(local_ctx->target_pos - local_ctx->curr_pos));
    if(local_ctx->direction == cfg_cover_open) {
        local_ctx->ticks_th_stop = POS_TO_TICKS(delta_pos, cfg_cover_up_time, cfg_cover_polling_time);
        TRIAC_SEL_SX();
    } else {
        local_ctx->ticks_th_stop = POS_TO_TICKS(delta_pos, cfg_cover_down_time, cfg_cover_polling_time);
        TRIAC_SEL_DX();
    }

    ESP_LOGI(TAG, "Run from pos[%d] for on[%u] to DIR[%d]",
            local_ctx->curr_pos, local_ctx->ticks_th_stop, local_ctx->direction);

    TRIAC_ON();
    vTaskResume(local_ctx->run_handler);
}

void cover_stop(void) {
    motor_off();
}

int cover_status(char *st_str, size_t len) {
    assert(st_str);
    assert(len > 0);

    memset(st_str, 0, len);
    if (local_ctx->status == cfg_cover_close) {
        strcpy(st_str, "close");
        return sizeof("close") -1;
    }
    if (local_ctx->status == COVER_STOP) {
        strcpy(st_str, "stop");
        return sizeof("stop") -1;
    }

    strcpy(st_str, "open");
    return sizeof("open") -1;
}

int cover_position(char *st_str, size_t len) {
    assert(st_str);
    assert(len > 0);

    memset(st_str, 0, len);

    int ret = sprintf(st_str,
            "{\"position\":\"%d\", \"ticks\":\"%d\"}",
            local_ctx->curr_pos,
            local_ctx->on_ticks);
    if (ret > 0)
        return ret;

    return ESP_FAIL;
}

void cover_prepareStatusMsg(const cover_ctx_t *ctx) {
    mqttmsg_t jmsg;

    jmsg.json_str_len = cover_position((char *)&jmsg.json_str, MAX_JSON_STR_LEN);
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, COVER_TOPIC_POS);
        jmsg.topic_len = sizeof(COVER_TOPIC_POS);

        if (xQueueSend(*cover_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while pos to queue");
    }

    jmsg.json_str_len = cover_status((char *)&jmsg.json_str, MAX_JSON_STR_LEN);
    if (jmsg.json_str_len != ESP_FAIL) {
        strcpy(jmsg.topic, COVER_TOPIC_STATUS);
        jmsg.topic_len = sizeof(COVER_TOPIC_STATUS);

        if (xQueueSend(*cover_module_queue, (void *)&jmsg, (TickType_t)10) != pdPASS)
            ESP_LOGE(TAG, "Error while send status to queue");
    }
}


static void cover_status_task(void * pvParameter) {
    while(1) {
        cover_prepareStatusMsg(local_ctx);
        DELAY_S(30);
    }
}


static void cmd_coverSetPos(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    if (len_data == 0 && !data) {
        ESP_LOGE(TAG, "Invalid paylod in cover set");
        return;
    }
    cover_run(atoi(data));
}

static void cmd_coverSet(const char *topic, size_t len_topic, const char *data, size_t len_data) {
    if (len_data == 0 && !data) {
        ESP_LOGE(TAG, "Invalid paylod in cover set");
        return;
    }

    if (!strncmp("open", data, len_data)) {
        cover_run(100);
        return;
    }

    if (!strncmp("close", data, len_data)) {
        cover_run(0);
        return;
    }

    if (!strncmp("stop", data, len_data)) {
        cover_stop();
        return;
    }

}

static void on_button_up(button_t *btn, button_state_t state)
{
    ESP_LOGI(TAG, "UP button %d", state);
    if (state == BUTTON_PRESSED || state == BUTTON_PRESSED_LONG) {
        cover_run(100);
    }

    if (state == BUTTON_RELEASED) {
        cover_stop();
    }
}

static void on_button_down(button_t *btn, button_state_t state)
{
    ESP_LOGI(TAG, "DOWN button %d", state);
    if (state == BUTTON_PRESSED || state == BUTTON_PRESSED_LONG) {
        cover_run(0);
    }

    if (state == BUTTON_RELEASED) {
        cover_stop();
    }
}

static CmdMQTT callback_table[] = {
    { COVER_TOPIC_SET     , cmd_coverSet    } ,
    { COVER_TOPIC_SET_POS , cmd_coverSetPos } ,
    { NULL                , NULL            } ,
};


#define COVER_CFG_INIT(key, value, default_value) \
    do { \
        if (cfg_readKey((key), sizeof((key)), &(value)) != ESP_OK) { \
            ESP_LOGW(TAG, "Use default "key": %d", (value)); \
            (value) = (default_value); \
        } else { \
            ESP_LOGI(TAG, "Get "key": %d", (value)); \
        } \
    } while(0)


void cover_init(cover_ctx_t *ctx, cover_event_t callback_end, QueueHandle_t *queue) {
    assert(ctx);
    local_ctx = ctx;
    memset(local_ctx, 0, sizeof(cover_ctx_t));

    local_ctx->callback_end = callback_end;
    // register the queue where to put out message for mqtt
    cover_module_queue = queue;

    // Register on mqtt table cover callbacks
    mqtt_mgr_regiterTable(callback_table);

    gpio_config_t io_conf;

    /* Configure gpio to drive the triacs */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BV(TRIAC_ENABLE)|BV(TRIAC_DIR);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;

    /* Configure gpio to read buttons status */
    btn_up.gpio = BUTTON_UP;
    btn_up.pressed_level = 0;
    btn_up.internal_pull = false;
    btn_up.autorepeat = false;
    btn_up.callback = on_button_up;

    btn_down.gpio = BUTTON_DOWN;
    btn_down.pressed_level = 0;
    btn_down.internal_pull = false;
    btn_down.autorepeat = false;
    btn_down.callback = on_button_down;

    ESP_ERROR_CHECK(button_init(&btn_down));
    ESP_ERROR_CHECK(button_init(&btn_up));
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    TRIAC_OFF();
    TRIAC_SEL_DX();


    COVER_CFG_INIT("cover_open", cfg_cover_open, COVER_OPEN);
    COVER_CFG_INIT("cover_close", cfg_cover_close, COVER_CLOSE);
    COVER_CFG_INIT("cover_up_time", cfg_cover_up_time, COVER_TRAVEL_TIME_UP);
    COVER_CFG_INIT("cover_down_time", cfg_cover_down_time, COVER_TRAVEL_TIME_DOWN);
    COVER_CFG_INIT("cover_polling_time", cfg_cover_polling_time, COVER_POLLING_TIME);
    COVER_CFG_INIT("cover_last_position", local_ctx->curr_pos, 0);

    // Create task to manage cover traveing time
    xTaskCreate(&cover_run_handler, "cover_run_handler", 8192, NULL, 5, &local_ctx->run_handler);
    vTaskSuspend(local_ctx->run_handler);

    // monitoring status task, publish the cover status
    xTaskCreate(&cover_status_task, "device_measure_task", 8192, NULL, 10, NULL);
}

