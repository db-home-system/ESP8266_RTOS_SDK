/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct CoverCtx {
    uint16_t curr_pos;
    uint16_t target_pos;
    uint16_t direction;
    uint32_t on_ticks;
    TaskHandle_t run_handler;
} cover_ctx_t;


void cover_run(int position);
void cover_stop(void);
void cover_init(cover_ctx_t *ctx);
