#include "proxy_whitelist.h"
#include "hardware/flash.h"
#include "pico/flash.h"
#include <string.h>

#define WHITELIST_MAGIC        0x574C5354 // "WLST"
#define WHITELIST_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define WHITELIST_FLASH_ADDR   ((const uint8_t *)(XIP_BASE + WHITELIST_FLASH_OFFSET))

typedef struct
{
    uint16_t vid;
    uint16_t pid;
} whitelist_entry_t;

// Padded to exactly FLASH_PAGE_SIZE (256 bytes) so flash_range_program is happy
typedef struct
{
    uint32_t          magic;
    uint8_t           count;
    uint8_t           _pad[3];
    whitelist_entry_t entries[WHITELIST_MAX]; // 32 * 4 = 128 bytes
    uint8_t           _reserved[256 - 4 - 4 - (WHITELIST_MAX * 4)];
} whitelist_page_t;

_Static_assert(sizeof(whitelist_page_t) == FLASH_PAGE_SIZE, "whitelist_page_t must be exactly one flash page");

static whitelist_page_t ram_page;

static void __no_inline_not_in_flash_func(do_flash_write)(void *param)
{
    (void)param;
    flash_range_erase(WHITELIST_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(WHITELIST_FLASH_OFFSET, (const uint8_t *)&ram_page, sizeof(whitelist_page_t));
}

static bool whitelist_save(void)
{
    int ret = flash_safe_execute(do_flash_write, NULL, 1000);
    return ret == PICO_OK;
}

void whitelist_init(void)
{
    const whitelist_page_t *flash = (const whitelist_page_t *)WHITELIST_FLASH_ADDR;
    if (flash->magic == WHITELIST_MAGIC && flash->count <= WHITELIST_MAX)
        memcpy(&ram_page, flash, sizeof(whitelist_page_t));
    else
    {
        memset(&ram_page, 0, sizeof(whitelist_page_t));
        ram_page.magic = WHITELIST_MAGIC;
    }
}

bool whitelist_check(uint16_t vid, uint16_t pid)
{
    for (uint8_t i = 0; i < ram_page.count; i++)
    {
        if (ram_page.entries[i].vid == vid && ram_page.entries[i].pid == pid)
            return true;
    }
    return false;
}

bool whitelist_add(uint16_t vid, uint16_t pid)
{
    if (ram_page.count >= WHITELIST_MAX)
        return false;
    if (whitelist_check(vid, pid))
        return true;

    ram_page.entries[ram_page.count].vid = vid;
    ram_page.entries[ram_page.count].pid = pid;
    ram_page.count++;
    return whitelist_save();
}

bool whitelist_remove_index(uint8_t index)
{
    if (index >= ram_page.count)
        return false;

    for (uint8_t i = index; i < ram_page.count - 1; i++)
        ram_page.entries[i] = ram_page.entries[i + 1];

    ram_page.count--;
    memset(&ram_page.entries[ram_page.count], 0, sizeof(whitelist_entry_t));
    return whitelist_save();
}

uint8_t whitelist_count(void)
{
    return ram_page.count;
}

bool whitelist_get(uint8_t index, uint16_t *vid, uint16_t *pid)
{
    if (index >= ram_page.count)
        return false;
    *vid = ram_page.entries[index].vid;
    *pid = ram_page.entries[index].pid;
    return true;
}
