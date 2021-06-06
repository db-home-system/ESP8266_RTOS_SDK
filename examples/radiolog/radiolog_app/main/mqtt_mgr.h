/*
 * RadioLog
 *
 * Daniele Basile <asterix24@gmail.com>
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

void mqtt_mgr_pub(char *topic, size_t len_topic, const char *data, size_t len_data);
void mqtt_mgr_init(void);
