#include "device_core.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"


/*file-scope variables*/
static const char *payload = "";
static int index_char = 0;
static bool key_pressed = false;
static uint32_t last_ms = 0;
static int payload_index = 0;
static bool sent = false;
uint8_t const ascii_to_keycode[128][2] = { HID_ASCII_TO_KEYCODE };
static bool is_msc_mode = false;
extern tusb_desc_device_t desc_device;

void fillPayload(const char* inputPayload)
{
    payload_index = 0;
    payload = inputPayload;
    sent = false;
}

#define USB_VID 0xbeef
#define USB_BCD 0x0200
#define USB_PID 0x1337
#define USB_PID_MSC 0x1338

void reEnumerate(void)
{
    //manually disconnect and reconnect
    tud_disconnect();

    is_msc_mode = 1;
    
    desc_device.bLength = sizeof(tusb_desc_device_t);   // Size of this descriptor in bytes
    desc_device.bDescriptorType = 0x01;                 // Descriptor type = Device
    desc_device.bcdUSB = 0x0200;                        // USB spec version (2.0)
    desc_device.bDeviceClass = 0x00;                    // Device class (0 = defined in interface)
    desc_device.bDeviceSubClass = 0x00;                 // Device subclass (0 = defined in interface)
    desc_device.bDeviceProtocol = 0x00;                 // Device protocol (0 = defined in interface)
    desc_device.bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE;// Max packet size for endpoint 0 (control)
    desc_device.idVendor = USB_VID;                     // Vendor ID (assigned by USB org)
    desc_device.idProduct = USB_PID_MSC;                    // Product ID (assigned by manufacturer)
    desc_device.bcdDevice = 0x0100;                     // Device release number (1.00)
    desc_device.iManufacturer = 0x01;                   // Index of manufacturer string descriptor
    desc_device.iProduct = 0x02;                        // Index of product string descriptor
    desc_device.iSerialNumber = 0x03;                   // Index of serial number string descriptor
    desc_device.bNumConfigurations = 0x01;             // Number of possible configurations

    string_desc_arr[1] = "change manufacturer";
    string_desc_arr[2] = "change product";
    string_desc_arr[3] = "change serials";

    active_config = desc_configuration_msc;

    sleep_ms(100);
    tud_connect();
}

void hid_task(void)
{
    if(is_msc_mode) return;

    static uint32_t ms = 0;
    if (board_millis() - ms < 10) return;
    ms = board_millis();

    if (!tud_mounted()) return;
    if (ms < 1000) return;

    if (sent) return;

    hid_keyboard_report_t report = {0};
    static bool key_down = false;

    if (!tud_hid_ready()) return;

    if (key_down)
    {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        key_down = false;
        payload_index++;
        return;
    }

    if (payload_index>= (int)strlen(payload))
    {
        sent = true;
        return;
    }

    uint8_t c = payload[payload_index];
    if (c == 0x01)
    {
        report.modifier = KEYBOAGUIRD_MODIFIER_LEFT;
        report.keycode[0] = HID_KEY_R;
    }
    else if (c == 0x02)
    {
        report.modifier = KEYBOAGUIRD_MODIFIER_LEFT;
        report.keycode[0] = HID_KEY_R;
    }
    else
    {
        report.modifier = ascii_to_keycode[c][0];
        report.keycode[0] = ascii_to_keycode[c][1];
    }
    
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, report.modifier, report.keycode);
    key_down = true;
}

