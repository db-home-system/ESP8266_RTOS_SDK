/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef void (*mqtt_sub_callback_t)(const char *topic, size_t len_topic, const char *data, size_t len_data);
typedef struct CmdMQTT {
    const char *topic;
    mqtt_sub_callback_t foo;
} CmdMQTT;

#define CMD_MAX_POOL_NO    3
typedef struct CmdMQTTTable {
    uint8_t fill_index;
    CmdMQTT *pool[CMD_MAX_POOL_NO];
} CmdMQTTTable;

void mqtt_mgr_pub(char *topic, size_t len_topic, const char *data, size_t len_data);
void mqtt_mgr_regiterTable(CmdMQTT *table);
void mqtt_mgr_init(CmdMQTT *table);
