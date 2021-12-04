/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define COVER_TOPIC_STATUS     "cover/status"
#define COVER_TOPIC_STATUS_LOG "cover/status/log"
#define COVER_TOPIC_POS        "cover/position"
#define COVER_TOPIC_SET_POS    "cover/set/position"
#define COVER_TOPIC_SET        "cover/set"
#define COVER_TOPIC_TEST_DIR   "cover/test/direction"


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
void cover_prepareStatusMsg(const cover_ctx_t *ctx);
void cover_init(cover_event_t callback_end, QueueHandle_t *queue);
