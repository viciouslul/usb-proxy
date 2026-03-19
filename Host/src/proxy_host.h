#pragma once
#include "tusb.h"
#include "proxy_data.h"

void host_task();

// Functions
proxy_device_t get_hid_type(uint8_t itf_protocol);

// Callbacks
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len);
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len);

void tuh_msc_mount_cb(uint8_t dev_addr);
void tuh_msc_umount_cb(uint8_t dev_addr);

