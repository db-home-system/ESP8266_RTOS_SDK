/**
 * @file cfg.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */

#ifndef CFG_H
#define CFG_H

void cmd_readCfg(const char *topic, size_t len_topic, const char *data, size_t len_data);
void cmd_writeCfg(const char *topic, size_t len_topic, const char *data, size_t len_data);
void cmd_dumpCfg(const char *topic, size_t len_topic, const char *data, size_t len_data);
void cmd_initCfg(void);


#endif /* CFG_H */
