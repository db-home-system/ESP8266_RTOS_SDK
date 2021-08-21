/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct CoverCtx cover_ctx_t;
typedef void (*cover_event_t)(const cover_ctx_t *ctx);

typedef struct CoverCtx {
    uint8_t status;
    uint8_t direction;
    uint32_t curr_pos;
    uint32_t target_pos;
    uint32_t on_ticks;
    uint32_t ticks_th_stop;
    TaskHandle_t run_handler;
    cover_event_t callback_end;
} cover_ctx_t;

void cover_run(int position);
void cover_stop(void);
int cover_status(char *st_str, size_t len);
int cover_position(char *st_str, size_t len);
void cover_init(cover_ctx_t *ctx, cover_event_t callback_end);
