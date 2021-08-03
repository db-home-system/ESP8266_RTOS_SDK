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


#define COVER_POLLING_TIME 250
#define COVER_OPEN  0
#define COVER_CLOSE 1
#define COVER_STOP  2
#define COVER_TRAVEL_TIME_UP 25
#define COVER_TRAVEL_TIME_DOWN 24

#define POS_TO_TICKS(p, tt) ((((tt) * 1000) / COVER_POLLING_TIME)  * (p) / 100)

static const char *TAG = "cover";

static cover_ctx_t *local_ctx;

static uint16_t ticks_to_pos(cover_ctx_t *ctx) {
    int32_t p = 0;
    if(ctx->direction == COVER_OPEN)
        p = ctx->curr_pos + ((COVER_POLLING_TIME * ctx->on_ticks) / (COVER_TRAVEL_TIME_UP * 10));
    else
        p = ctx->curr_pos - ((COVER_POLLING_TIME * ctx->on_ticks) / (COVER_TRAVEL_TIME_DOWN * 10));

    //ESP_LOGW(TAG, "t->pos [%d] [%d]", ctx->on_ticks, p);
    return (uint16_t)MINMAX(0, p, 100);
}


static void motor_off(void) {
    TRIAC_OFF();
    local_ctx->status = COVER_STOP;
    local_ctx->curr_pos = ticks_to_pos(local_ctx);
    // TODO: Save current pos

    // Call notify callback
    if(local_ctx->callback_end) {
        local_ctx->callback_end(local_ctx->status, local_ctx->curr_pos);
    }
    ESP_LOGW(TAG, "Motor off");
    vTaskSuspend(local_ctx->run_handler);
}

static void cover_run_handler(void * pvParameter)
{
    while (1) {
        DELAY_MS(COVER_POLLING_TIME);
        ESP_LOGI(TAG, "tick..");
        local_ctx->on_ticks++;

        if(local_ctx->on_ticks >= local_ctx->ticks_th_stop)
            motor_off();

        // Call notify callback
        if(local_ctx->callback_run) {
            local_ctx->callback_run(local_ctx->status, ticks_to_pos(local_ctx));
        }
    }
}

void cover_run(int position) {

    // we are just in the same position, do nothing
    if(local_ctx->curr_pos == position)
        return;

    local_ctx->target_pos = position;
    local_ctx->on_ticks = 0;

    // guess direction 0: close; 100: open;
    local_ctx->direction = COVER_CLOSE;
    local_ctx->status = COVER_CLOSE;
    if(local_ctx->target_pos >= local_ctx->curr_pos) {
        local_ctx->direction = COVER_OPEN;
        local_ctx->status = COVER_OPEN;
    }

    //go to desiderate position
    uint16_t delta_pos = ABS(local_ctx->target_pos - local_ctx->curr_pos);
    if(local_ctx->direction == COVER_OPEN) {
        local_ctx->ticks_th_stop = POS_TO_TICKS(delta_pos, COVER_TRAVEL_TIME_UP);
        TRIAC_SEL_DX();
    } else {
        local_ctx->ticks_th_stop = POS_TO_TICKS(delta_pos, COVER_TRAVEL_TIME_DOWN);
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

    if (local_ctx->status == COVER_CLOSE) {
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

void cover_init(cover_ctx_t *ctx, cover_event_t callback_end, cover_event_t callback_run) {
    assert(ctx);
    local_ctx = ctx;
    memset(local_ctx, 0, sizeof(cover_ctx_t));

    local_ctx->callback_end = callback_end;
    local_ctx->callback_run = callback_run;

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

    // Create task to manage cover traveing time
    xTaskCreate(&cover_run_handler, "cover_run_handler", 8192, NULL, 5, &local_ctx->run_handler);
    vTaskSuspend(local_ctx->run_handler);
}

