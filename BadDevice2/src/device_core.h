#pragma once

#include "tusb.h"
#include "usb_descriptors.h"

#define USB_VID_MSC  0xbeef
#define USB_PID_MSC  0x1337

#define USB_VID_HID  0xfeeb
#define USB_PID_HID  0x0420

static const char *payload = "";
static int payload_index = 0;
static int payload_len = 0;
static bool sent = false;
static bool randomized = false;

static uint32_t last_event = 0;
static uint32_t random_delay_start = 0;
static uint32_t random_delay_duration = 0;

extern tusb_desc_device_t desc_device;
extern uint8_t const desc_configuration_msc[];
extern uint8_t const desc_configuration_hid[];
extern uint8_t const *active_config;
extern char const *string_desc_arr[];

typedef enum 
{
    STATE_IDLE,
    STATE_KEY_DOWN,
    STATE_KEY_UP,
    STATE_DELAY,
    STATE_RANDOM_DELAY,
    STATE_WIN_ENTER_DOWN,
    STATE_WIN_ENTER_UP,
    STATE_WIN_ENTER_DELAY,
    STATE_SINGLE_KEY_DOWN, /* Macropad: send one keycode */
    STATE_SINGLE_KEY_UP,   /* Macropad: release key */
} hid_state_t;

static hid_state_t state = STATE_IDLE;

typedef enum 
{
    HID,
    MSC,
} mounted_dev;

static mounted_dev current_dev = HID;

void hidTask(void);
void fillPayload(const char *inputPayload, bool randomized);
void reEnumerate(mounted_dev new_type);
void sendKey(uint8_t modifier, uint8_t keycode);
bool hid_is_idle(void);
