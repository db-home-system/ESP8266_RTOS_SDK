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


#define COVER_POLLING_TIME 500


static const char *TAG = "cover";

static cover_ctx_t *local_ctx;

static void cover_run_handler(void * pvParameter)
{
    while (1) {
        DELAY_MS(COVER_POLLING_TIME);
        ESP_LOGI(TAG, "tick..");
    }
}


void cover_init(cover_ctx_t *ctx) {
    assert(ctx);
    local_ctx = ctx;
    memset(local_ctx, 0, sizeof(cover_ctx_t));

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
    xTaskCreate(&cover_run_handler, "cover_run_handler", 8192, NULL, tskIDLE_PRIORITY, &local_ctx->run_handler);
}

