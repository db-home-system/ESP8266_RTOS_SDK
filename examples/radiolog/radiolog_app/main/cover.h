/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef void (*cover_event_t)(uint8_t status, uint16_t position);

typedef struct CoverCtx {
    uint8_t status;
    uint16_t curr_pos;
    uint16_t target_pos;
    uint16_t direction;
    uint32_t on_ticks;
    uint32_t ticks_th_stop;
    TaskHandle_t run_handler;
    cover_event_t callback_end;
    cover_event_t callback_run;
} cover_ctx_t;

void cover_run(int position);
void cover_stop(void);
int cover_status(char *st_str, size_t len);
int cover_position(char *st_str, size_t len);
void cover_init(cover_ctx_t *ctx, cover_event_t callback_end, cover_event_t callback_run);
