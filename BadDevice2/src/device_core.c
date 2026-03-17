#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"
#include <string.h>

/*------------------------------------------------------------------*/
/* Globals */
/*------------------------------------------------------------------*/

static const char *payload = "";
static int payload_index = 0;
static int payload_len = 0;
static bool sent = false;

static uint32_t last_event = 0;

static bool is_msc_mode = false;

extern tusb_desc_device_t desc_device;
extern uint8_t const desc_configuration_msc[];
extern uint8_t const desc_configuration_hid[];
extern uint8_t const *active_config;
extern char const *string_desc_arr[];

/* ASCII → HID lookup table */
uint8_t const ascii_to_keycode[128][2] = { HID_ASCII_TO_KEYCODE };

/*------------------------------------------------------------------*/
/* HID state machine */
/*------------------------------------------------------------------*/

typedef enum {
    STATE_IDLE,
    STATE_KEY_DOWN,
    STATE_KEY_UP,
    STATE_DELAY
} hid_state_t;

static hid_state_t state = STATE_IDLE;

/*------------------------------------------------------------------*/
/* Payload control */
/*------------------------------------------------------------------*/

void fillPayload(const char* inputPayload)
{
    payload = inputPayload;
    payload_index = 0;
    payload_len = strlen(inputPayload);
    sent = false;
    state = STATE_KEY_DOWN;
}

/*------------------------------------------------------------------*/
/* USB re-enumeration (MSC switch) */
/*------------------------------------------------------------------*/

#define USB_VID 0xbeef
#define USB_PID_MSC 0x1338

void reEnumerate(void)
{
    tud_disconnect();

    is_msc_mode = true;

    desc_device.idVendor  = USB_VID;
    desc_device.idProduct = USB_PID_MSC;

    string_desc_arr[1] = "change manufacturer";
    string_desc_arr[2] = "change product";
    string_desc_arr[3] = "change serials";

    active_config = desc_configuration_msc;

    sleep_ms(100);
    tud_connect();
}

/*------------------------------------------------------------------*/
/* HID task */
/*------------------------------------------------------------------*/

void hid_task(void)
{
    if (is_msc_mode) return;
    if (!tud_mounted()) return;
    if (sent) return;

    // enforce small delay between USB actions (10 ms)
    if (board_millis() - last_event < 10) return;

    if (!tud_hid_ready()) return;

    static hid_keyboard_report_t report;

    switch (state)
    {
        case STATE_IDLE:
            return;

        case STATE_KEY_DOWN:
        {
            if (payload_index >= payload_len)
            {
                sent = true;
                state = STATE_IDLE;
                return;
            }

            uint8_t c = payload[payload_index];

            memset(&report, 0, sizeof(report));

            if (c < 128)
            {
                report.modifier = ascii_to_keycode[c][0];
                report.keycode[0] = ascii_to_keycode[c][1];
            }

            tud_hid_keyboard_report(REPORT_ID_KEYBOARD,
                                    report.modifier,
                                    report.keycode);

            state = STATE_KEY_UP;
            last_event = board_millis();
        }
        break;

        case STATE_KEY_UP:
        {
            // release key
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);

            state = STATE_DELAY;
            last_event = board_millis();
        }
        break;

        case STATE_DELAY:
        {
            payload_index++;
            state = STATE_KEY_DOWN;
        }
        break;
    }
}
