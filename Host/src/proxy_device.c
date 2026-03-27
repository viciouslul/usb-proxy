#include "proxy_device.h"
#include "tusb.h"
#include "proxy_metrics.h"
#include "pico/stdlib.h"

static bool kb_latency_pending = false;
static uint32_t kb_pending_pkt_timestamp_us = 0;
static uint32_t kb_pending_submit_timestamp_us = 0;
static bool kb_pending_filtering_enabled = false;

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

    while(proxy_dequeue(&pkt))
    {
        proxy_device_t selected_device = ui_get_selected_device();
        if (selected_device == HID_NONE) return;

        if (pkt.msg_t == PROXY_MSG_MOUNT)
        {
            ui_set_vid_pid(pkt.vid, pkt.pid);
        }

#if defined(PROXY_FILTERING) && (PROXY_FILTERING == 1)
        bool filtering_enabled = true;
#else
        bool filtering_enabled = false;
#endif

        enumeration_result_t enum_result = enumerationCheck(&pkt, selected_device);
        if (enum_result == ENUM_CHECK_UNMOUNT)
        {
            botDetection_reset();
            proxy_queue_reset();
            ui_on_unmount();
            metrics_record_event((detection_event_t){.device_type = selected_device, .event_type = EVENT_DEVICE_UNMOUNTED});
            return;
        }

        if (enum_result == ENUM_CHECK_ERROR)
        {
            ui_on_enumeration_result(false);
            metrics_record_event((detection_event_t){.device_type = selected_device, .event_type = EVENT_ENUM_ERROR});
            metrics_record_enumeration_mismatch();

            if (filtering_enabled)
            {
                metrics_record_report_processed(true);
                return;
            }
        }

        if (enum_result == ENUM_CHECK_OK)
        {
            ui_on_enumeration_result(true);
            metrics_record_event((detection_event_t){.device_type = selected_device, .event_type = EVENT_ENUM_OK});
        }

        if (filtering_enabled)
        {
            bool bot_detected = botDetection(&pkt);
            if (bot_detected)
            {
                botDetection_reset();
                proxy_queue_reset();
                ui_on_security_error();
                metrics_record_event((detection_event_t){.device_type = selected_device, .event_type = EVENT_BOT_DETECTED});
                metrics_record_report_processed(true);
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