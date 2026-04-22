#include "proxy_device.h"
#include "tusb.h"
#include "proxy_metrics.h"
#include "pico/stdlib.h"

static bool kb_latency_pending = false;
static uint32_t kb_pending_pkt_timestamp_us = 0;
static uint32_t kb_pending_submit_timestamp_us = 0;
static bool kb_pending_filtering_enabled = false;

// A physical unplug + replug is bounded below by human motor timing; anything
// faster than this is software re-enumeration (BadUSB-style spoof).
#define REENUM_WINDOW_US 5000000

// AFK protection: force a re-selection if no HID activity for this long.
#define IDLE_TIMEOUT_US (1 * 60 * 1000000) / 2

static bool pending_unmount = false;
static uint32_t pending_unmount_ts_us = 0;
static proxy_device_t pending_unmount_device = HID_NONE;

static bool idle_armed = false;
static uint32_t last_activity_us = 0;

static void commit_pending_unmount(void)
{
    proxy_device_t dev = pending_unmount_device;
    pending_unmount = false;
    pending_unmount_ts_us = 0;
    pending_unmount_device = HID_NONE;

    botDetection_reset();
    proxy_queue_reset();
    kb_latency_pending = false;
    kb_pending_pkt_timestamp_us = 0;
    kb_pending_submit_timestamp_us = 0;
    metrics_reset_transient_state();
    idle_armed = false;
    ui_on_unmount();
    metrics_record_event((detection_event_t){.device_type = dev, .event_type = EVENT_DEVICE_UNMOUNTED});
    metrics_try_export_cdc();
}

static void commit_idle_timeout(proxy_device_t dev)
{
    idle_armed = false;
    pending_unmount = false;
    pending_unmount_ts_us = 0;
    pending_unmount_device = HID_NONE;

    botDetection_reset();
    proxy_queue_reset();
    kb_latency_pending = false;
    kb_pending_pkt_timestamp_us = 0;
    kb_pending_submit_timestamp_us = 0;
    metrics_reset_transient_state();
    ui_on_idle_timeout();
    metrics_record_event((detection_event_t){.device_type = dev, .event_type = EVENT_IDLE_TIMEOUT});
    metrics_try_export_cdc();
}

void device_task()
{
    while (1)
    {
        tud_task(); // tinyusb device task
        process_hid();
        ui_task();
    }
}
/* Functions */
void process_hid()
{
    proxy_packet_t pkt;

    // Arm / check the AFK idle timer based on current selection state.
    proxy_device_t armed_device = ui_get_selected_device();
    if (armed_device != HID_NONE)
    {
        if (!idle_armed)
        {
            idle_armed = true;
            last_activity_us = time_us_32();
        }
        else if ((time_us_32() - last_activity_us) >= IDLE_TIMEOUT_US)
        {
            commit_idle_timeout(armed_device);
            return;
        }
    }
    else
    {
        idle_armed = false;
    }

    while(proxy_dequeue(&pkt))
    {
        proxy_device_t selected_device = ui_get_selected_device();
        if (selected_device == HID_NONE) return;

        if (pkt.msg_t == PROXY_MSG_REPORT)
        {
            last_activity_us = time_us_32();
        }

        if (pending_unmount && pkt.msg_t == PROXY_MSG_MOUNT)
        {
            bool within_window =
                (pkt.timestamp_us >= pending_unmount_ts_us) &&
                ((pkt.timestamp_us - pending_unmount_ts_us) < REENUM_WINDOW_US);
            bool class_switch = (pkt.dev_t != pending_unmount_device);

            if (within_window && class_switch)
            {
                proxy_device_t old_dev = pending_unmount_device;
                proxy_device_t new_dev = pkt.dev_t;

                pending_unmount = false;
                pending_unmount_ts_us = 0;
                pending_unmount_device = HID_NONE;

                botDetection_reset();
                proxy_queue_reset();
                kb_latency_pending = false;
                kb_pending_pkt_timestamp_us = 0;
                kb_pending_submit_timestamp_us = 0;
                metrics_reset_transient_state();
                ui_on_reenumeration(old_dev, new_dev);
                metrics_record_event((detection_event_t){.device_type = new_dev, .event_type = EVENT_REENUM_DETECTED});
                metrics_try_export_cdc();
                return;
            }

            // Same class, or outside the window: treat it as a legitimate
            // unplug/replug - commit the deferred unmount and drop this MOUNT.
            commit_pending_unmount();
            return;
        }

        if (pkt.msg_t == PROXY_MSG_MOUNT)
        {
            ui_set_vid_pid(pkt.vid, pkt.pid);
            metrics_record_mount(pkt.dev_t, pkt.vid, pkt.pid);
            metrics_try_export_cdc();
        }

#if defined(PROXY_FILTERING) && (PROXY_FILTERING == 1)
        bool filtering_enabled = true;
#else
        bool filtering_enabled = false;
#endif

        proxy_device_t event_device = (pkt.dev_t != HID_NONE) ? pkt.dev_t : selected_device;
        if (pkt.msg_t == PROXY_MSG_UNMOUNT)
        {
            pending_unmount = true;
            pending_unmount_ts_us = pkt.timestamp_us;
            pending_unmount_device = event_device;
            return;
        }

        if (filtering_enabled)
        {
            enumeration_result_t enum_result = enumerationCheck(&pkt, selected_device);
            if (enum_result == ENUM_CHECK_ERROR)
            {
                ui_on_enumeration_result(false);
                metrics_record_event((detection_event_t){.device_type = event_device, .event_type = EVENT_ENUM_ERROR});
                metrics_record_enumeration_mismatch();
                metrics_try_export_cdc();

                metrics_record_report_processed(true);
                return;
            }

            if (enum_result == ENUM_CHECK_OK)
            {
                ui_on_enumeration_result(true);
                metrics_record_event((detection_event_t){.device_type = event_device, .event_type = EVENT_ENUM_OK});
                metrics_try_export_cdc();
            }

            bool bot_detected = botDetection(&pkt);
            if (bot_detected)
            {
                botDetection_reset();
                proxy_queue_reset();
                kb_latency_pending = false;
                kb_pending_pkt_timestamp_us = 0;
                kb_pending_submit_timestamp_us = 0;
                metrics_reset_transient_state();
                ui_on_security_error();
                metrics_record_event((detection_event_t){.device_type = event_device, .event_type = EVENT_BOT_DETECTED});
                metrics_record_report_processed(true);
                // Drain queued events so BOT_DETECTED is emitted immediately.
                for (uint8_t i = 0; i < 16; i++)
                {
                    metrics_try_export_cdc();
                }
                return;
            }
        }

        switch(pkt.msg_t)
        {
            case PROXY_MSG_REPORT:
                if(pkt.dev_t == HID_KEYBOARD)
                {
                    bool any_key_down = false;
                    for (uint16_t i = 2; i < pkt.report_len && i < 8; i++)
                    {
                        if (pkt.report[i] != 0)
                        {
                            any_key_down = true;
                            break;
                        }
                    }

                    bool sent = tud_hid_report(pkt.dev_t, pkt.report, pkt.report_len);
                    if (sent)
                    {
                        metrics_record_report_processed(false);
                        if (any_key_down)
                        {
                            // Measure end-to-end latency when USB transfer completes.
                            kb_pending_pkt_timestamp_us = pkt.timestamp_us;
                            kb_pending_submit_timestamp_us = time_us_32();
                            kb_pending_filtering_enabled = filtering_enabled;
                            kb_latency_pending = true;
                        }
                    }
                }
                else if(pkt.dev_t == HID_MOUSE)
                {
                    tud_hid_report(pkt.dev_t, pkt.report, pkt.report_len);
                    metrics_record_report_processed(false);
                }
                break;

            case PROXY_MSG_MOUNT:
                tud_hid_report(pkt.dev_t, pkt.report, pkt.report_len);
                break;

            case PROXY_MSG_UNMOUNT:
                break;

            case PROXY_MSG_NONE:
                //do nothing
                break;

            default:
                break;
        }

        metrics_try_export_cdc();
    }

    // Queue drained: finalize a deferred unmount once the re-enum window closes.
    if (pending_unmount &&
        (time_us_32() - pending_unmount_ts_us) >= REENUM_WINDOW_US)
    {
        commit_pending_unmount();
    }
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;

    if (kb_latency_pending)
    {
        uint32_t complete_ts_us = time_us_32();
        uint32_t queue_us = kb_pending_submit_timestamp_us - kb_pending_pkt_timestamp_us;
        uint32_t usb_us = complete_ts_us - kb_pending_submit_timestamp_us;
        uint32_t total_us = complete_ts_us - kb_pending_pkt_timestamp_us;

        forwarding_sample_t sample = {
            .sequence = 0,
            .filtering_enabled = kb_pending_filtering_enabled,
            .had_strike = false,
            .host_ts_us = kb_pending_pkt_timestamp_us,
            .submit_ts_us = kb_pending_submit_timestamp_us,
            .complete_ts_us = complete_ts_us,
            .queue_us = queue_us,
            .usb_us = usb_us,
            .total_us = total_us,
        };

        metrics_record_forwarding_sample(sample);
        metrics_try_export_cdc();
        kb_latency_pending = false;
    }
}