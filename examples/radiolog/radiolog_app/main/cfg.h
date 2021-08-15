/**
 * @file cfg.h
 *
 * RadioLog
 *
 * Copyright (c) 2021 Daniele Basile <asterix24@gmail.com>
 */

#ifndef CFG_H
#define CFG_H


#define CFG_NOVALUE 0xFFFFFFFF
#define CFG_COVER  0
#define CFG_SWITCH 1

esp_err_t cfg_writeKey(const char *key, size_t len_key, uint32_t value);
esp_err_t cfg_readKey(const char *key, size_t len_key, uint32_t *value);
esp_err_t cfg_dump(void);
void cmd_initCfg(void);


#endif /* CFG_H */
