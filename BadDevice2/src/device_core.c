#include "device_core.h"
#include "bsp/board_api.h"
#include "hardware/structs/usb.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include <stdlib.h>
#include <string.h>

uint8_t const ascii_to_keycode[128][2] = {HID_ASCII_TO_KEYCODE};

char hid_to_ascii(uint8_t keycode)
{
  char fallback = 0;
  for (int c = 0; c < 128; c++)
  {
    if (ascii_to_keycode[c][1] == keycode)
    {
      if (ascii_to_keycode[c][0] == 0) /* no shift modifier — return immediately */
        return (char)c;
      if (!fallback)
        fallback = (char)c; /* shifted version — keep as last resort */
    }
  }
  return fallback;
}

static uint8_t single_key_modifier = 0;
static uint8_t single_key_keycode = 0;

void sendKey(uint8_t modifier, uint8_t keycode)
{
  /* Don't interrupt an ongoing payload — only fires when idle */
  if (state != STATE_IDLE)
    return;
  single_key_modifier = modifier;
  single_key_keycode = keycode;
  sent = false;
  state = STATE_SINGLE_KEY_DOWN;
}

bool hid_is_idle(void) { return state == STATE_IDLE; }

void fillPayload(const char *inputPayload, bool randomized_param)
{
  payload = inputPayload;
  payload_index = 0;
  payload_len = strlen(inputPayload);
  sent = false;
  randomized = randomized_param;
  state = STATE_WIN_ENTER_DOWN;
}

void reEnumerate(mounted_dev new_type)
{
  tud_disconnect();
  sleep_ms(1000);

  switch (new_type)
  {
  case MSC:
    current_dev = MSC;
    desc_device.idVendor = USB_VID_MSC;
    desc_device.idProduct = USB_PID_MSC;

    string_desc_arr[1] = "TinyUSB";
    string_desc_arr[2] = "BAD MSC";
    string_desc_arr[3] = "1337";

    active_config = desc_configuration_msc;
    break;

  case HID:
    current_dev = HID;
    desc_device.idVendor = USB_VID_HID;
    desc_device.idProduct = USB_PID_HID;

    string_desc_arr[1] = "TinyUSB";
    string_desc_arr[2] = "BAD HID";
    string_desc_arr[3] = "0420";

    active_config = desc_configuration_hid;
    break;
  }

  sleep_ms(500);
  tud_connect();
}

void hidTask(void)
{
  if (current_dev == MSC)
    return;
  if (!tud_mounted())
    return;
  if (sent)
    return;

  /* Enforce small delay between USB actions (10 ms) */
  if (board_millis() - last_event < 10)
    return;

  if (!tud_hid_ready())
    return;

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

    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, report.modifier,
                            report.keycode);

    state = STATE_KEY_UP;
    last_event = board_millis();
  }
  break;

  case STATE_KEY_UP:
  {
    /* Release key */
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);

    state = STATE_DELAY;
    last_event = board_millis();
  }
  break;

  case STATE_DELAY:
  {
    if (randomized)
    {
      /* Generate random delay between 50-250ms */
      random_delay_duration = (rand() % 200) + 50;
      random_delay_start = board_millis();
      state = STATE_RANDOM_DELAY;
    }
    else
    {
      payload_index++;
      state = STATE_KEY_DOWN;
    }
  }
  break;

  case STATE_RANDOM_DELAY:
  {
    if (board_millis() - random_delay_start >= random_delay_duration)
    {
      payload_index++;
      state = STATE_KEY_DOWN;
    }
  }
  break;

  case STATE_WIN_ENTER_DOWN:
  {
    memset(&report, 0, sizeof(report));
    report.modifier = KEYBOAGUIRD_MODIFIER_LEFT; /* Win key */
    report.keycode[0] = HID_KEY_ENTER;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, report.modifier,
                            report.keycode);
    state = STATE_WIN_ENTER_UP;
    last_event = board_millis();
  }
  break;

  case STATE_WIN_ENTER_UP:
  {
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    state = STATE_WIN_ENTER_DELAY;
    last_event = board_millis();
  }
  break;

  case STATE_WIN_ENTER_DELAY:
  {
    if (board_millis() - last_event < 1500)
      return;
    state = STATE_KEY_DOWN;
  }
  break;

  case STATE_SINGLE_KEY_DOWN:
  {
    memset(&report, 0, sizeof(report));
    report.modifier = single_key_modifier;
    report.keycode[0] = single_key_keycode;
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, report.modifier,
                            report.keycode);
    state = STATE_SINGLE_KEY_UP;
    last_event = board_millis();
  }
  break;

  case STATE_SINGLE_KEY_UP:
  {
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
    state = STATE_IDLE;
    last_event = board_millis();
  }
  break;
  }
}
