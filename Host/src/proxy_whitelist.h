#pragma once
#include <stdbool.h>
#include <stdint.h>

#define WHITELIST_MAX 32

void    whitelist_init(void);
bool    whitelist_check(uint16_t vid, uint16_t pid);
bool    whitelist_add(uint16_t vid, uint16_t pid);
bool    whitelist_remove_index(uint8_t index);
uint8_t whitelist_count(void);
bool    whitelist_get(uint8_t index, uint16_t *vid, uint16_t *pid);
