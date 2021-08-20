/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "cover.h"
#include "cfg.h"
#include "common.h"
#include "macros.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TRIAC_ENABLE    14
#define TRIAC_DIR       16

#define TRIAC_SEL_DX() (gpio_set_level(TRIAC_DIR, 1))
#define TRIAC_SEL_SX() (gpio_set_level(TRIAC_DIR, 0))
#define TRIAC_ON() (gpio_set_level(TRIAC_ENABLE, 0))
#define TRIAC_OFF() (gpio_set_level(TRIAC_ENABLE, 1))


#define COVER_OPEN  0
#define COVER_CLOSE 1
#define COVER_STOP  2
#define COVER_POLLING_TIME 250
#define COVER_TRAVEL_TIME_UP 25
#define COVER_TRAVEL_TIME_DOWN 24

#define POS_TO_TICKS(p, tt, pollingtime) ((((tt) * 1000) / (pollingtime))  * (p) / 100)

static const char *TAG = "cover";

static cover_ctx_t *local_ctx;
static uint32_t cfg_cover_open = COVER_OPEN;
static uint32_t cfg_cover_close = COVER_CLOSE;
static uint32_t cfg_cover_up_time = COVER_TRAVEL_TIME_UP;
static uint32_t cfg_cover_down_time = COVER_TRAVEL_TIME_DOWN;
static uint32_t cfg_cover_polling_time = COVER_POLLING_TIME;

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
        local_ctx->callback_end(local_ctx->status, local_ctx->curr_pos);
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
        ESP_LOGI(TAG, "tick..");
        local_ctx->on_ticks++;

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
        TRIAC_SEL_DX();
    } else {
        local_ctx->ticks_th_stop = POS_TO_TICKS(delta_pos, cfg_cover_down_time, cfg_cover_polling_time);
        TRIAC_SEL_SX();
    }

    ESP_LOGI(TAG, "Run from pos[%d] for on[%u] to DIR[%d]",
            local_ctx->curr_pos, local_ctx->ticks_th_stop, local_ctx->direction);

    TRIAC_ON();
    vTaskResume(local_ctx->run_handler);
}

void cover_stop(void) {
    motor_off();
}

int cover_position(char *st_str, size_t len) {
    assert(st_str);
    assert(len > 0);

    memset(st_str, 0, len);
    return sprintf(st_str,
        "{\"position\":\"%d\", \"ticks\":\"%d\"}",
        local_ctx->curr_pos,
        local_ctx->on_ticks);
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


#define COVER_CFG_INIT(key, value, default_value) \
    do { \
        if (cfg_readKey((key), sizeof((key)), &(value)) != ESP_OK) { \
            ESP_LOGW(TAG, "Use default "key": %d", (value)); \
            (value) = (default_value); \
        } else { \
            ESP_LOGI(TAG, "Get "key": %d", (value)); \
        } \
    } while(0)

void cover_init(cover_ctx_t *ctx, cover_event_t callback_end) {
    assert(ctx);
    local_ctx = ctx;
    memset(local_ctx, 0, sizeof(cover_ctx_t));

    local_ctx->callback_end = callback_end;

    gpio_config_t io_conf;

    /* Configure gpio to drive the triacs */
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BV(TRIAC_ENABLE)|BV(TRIAC_DIR);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;

    gpio_config(&io_conf);

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
}

