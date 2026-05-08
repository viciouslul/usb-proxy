#include "proxy_metrics.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

#define FORWARDING_BUFFER_SIZE 256
#define EVENT_BUFFER_SIZE      64

typedef struct
{
    uint32_t       timestamp_us;
    char           code;
    proxy_device_t device_type;
    uint16_t       vid;
    uint16_t       pid;
} metrics_event_record_t;

static forwarding_sample_t forwarding_buffer[FORWARDING_BUFFER_SIZE] = {0};
static uint16_t            forwarding_head                           = 0;
static uint16_t            forwarding_count                          = 0;
static uint32_t            forwarding_total                          = 0;
static uint32_t            forwarding_exported                       = 0;

static metrics_event_record_t event_buffer[EVENT_BUFFER_SIZE] = {0};
static uint16_t               event_head                      = 0;
static uint16_t               event_count                     = 0;
static uint32_t               event_total                     = 0;
static uint32_t               event_exported                  = 0;

static uint32_t total_reports   = 0;
static uint32_t blocked_reports = 0;
static uint32_t enum_mismatches = 0;
static uint32_t latency_sum_us  = 0;
static uint32_t keystroke_total = 0;

static bool strike_pending      = false;
static bool csv_header_exported = false;

static bool cdc_write_record(char const *data, uint32_t len)
{
    if (!tud_cdc_connected())
        return false;
    if (tud_cdc_write_available() < len)
        return false;
    if (tud_cdc_write((uint8_t const *)data, len) != len)
        return false;
    return true;
}

static void push_event_record(char code, proxy_device_t device_type,
                              uint16_t vid, uint16_t pid)
{
    metrics_event_record_t ev = {
        .timestamp_us = time_us_32(),
        .code         = code,
        .device_type  = device_type,
        .vid          = vid,
        .pid          = pid,
    };

    event_buffer[event_head] = ev;
    event_head               = (event_head + 1) % EVENT_BUFFER_SIZE;

    if (event_count < EVENT_BUFFER_SIZE)
    {
        event_count++;
    }

    event_total++;
}

void metrics_init(void)
{
    forwarding_head     = 0;
    forwarding_count    = 0;
    forwarding_total    = 0;
    forwarding_exported = 0;

    event_head     = 0;
    event_count    = 0;
    event_total    = 0;
    event_exported = 0;

    total_reports   = 0;
    blocked_reports = 0;
    enum_mismatches = 0;
    latency_sum_us  = 0;
    keystroke_total = 0;

    strike_pending      = false;
    csv_header_exported = false;
}

void metrics_record_keystroke(uint32_t interval_us, uint8_t keycode,
                              bool is_suspicious)
{
    (void)interval_us;
    (void)keycode;

    keystroke_total++;
    if (is_suspicious)
    {
        strike_pending = true;
    }
}

void metrics_record_event(detection_event_t event)
{
    char code = 'N';
    switch (event.event_type)
    {
    case EVENT_ENUM_OK:
        code = 'O';
        break;
    case EVENT_ENUM_ERROR:
        code = 'R';
        break;
    case EVENT_PKT_BLOCKED:
        code = 'B';
        break;
    case EVENT_STRIKE:
        code = 'S';
        break;
    case EVENT_DEVICE_SELECTED:
        code = 'D';
        break;
    case EVENT_DEVICE_UNMOUNTED:
        code = 'U';
        break;
    default:
        code = 'N';
        break;
    }

    push_event_record(code, event.device_type, 0, 0);
}

void metrics_record_mount(proxy_device_t device_type, uint16_t vid,
                          uint16_t pid)
{
    push_event_record('M', device_type, vid, pid);
}

void metrics_record_report_processed(bool was_blocked)
{
    total_reports++;
    if (was_blocked)
    {
        blocked_reports++;
    }
}

void metrics_record_enumeration_mismatch(void) { enum_mismatches++; }

void metrics_reset_transient_state(void) { strike_pending = false; }

void metrics_record_latency(uint32_t latency_us)
{
    forwarding_sample_t sample = {
        .sequence = forwarding_total,
        .filtering_enabled =
#if defined(PROXY_FILTERING) && (PROXY_FILTERING == 1)
            true,
#else
            false,
#endif
        .had_strike     = false,
        .host_ts_us     = 0,
        .submit_ts_us   = 0,
        .complete_ts_us = time_us_32(),
        .queue_us       = 0,
        .usb_us         = 0,
        .total_us       = latency_us,
    };

    metrics_record_forwarding_sample(sample);
}

void metrics_record_forwarding_sample(forwarding_sample_t sample)
{
    sample.sequence   = forwarding_total;
    sample.had_strike = sample.had_strike || strike_pending;
    strike_pending    = false;

    forwarding_buffer[forwarding_head] = sample;
    forwarding_head                    = (forwarding_head + 1) % FORWARDING_BUFFER_SIZE;

    if (forwarding_count < FORWARDING_BUFFER_SIZE)
    {
        forwarding_count++;
    }

    forwarding_total++;
    latency_sum_us += sample.total_us;
}

uint16_t metrics_get_keystroke_count(void) { return (uint16_t)keystroke_total; }

uint16_t metrics_get_event_count(void) { return (uint16_t)event_total; }

void metrics_get_stats(uint32_t *total, uint32_t *blocked, uint32_t *mismatches,
                       uint32_t *mean_latency)
{
    *total      = total_reports;
    *blocked    = blocked_reports;
    *mismatches = enum_mismatches;
    *mean_latency =
        (forwarding_total > 0) ? (latency_sum_us / forwarding_total) : 0;
}

// Emit compact records that fit in one full-speed CDC packet (<64 bytes each).
// Header: H,fmt=K:seq,mode,total,strk;E:ts,ev,dev,vid,pid
// Key:    K,<seq>,<mode>,<total_us>,<strike>
// Event:  E,<ts_us>,<event>,<dev>,<vid_hex>,<pid_hex>
void metrics_try_export_cdc(void)
{
    if (!csv_header_exported)
    {
        char const *header = "H,fmt=K:seq,mode,total,strk;E:ts,ev,dev,vid,pid\r\n";
        uint32_t    hlen   = (uint32_t)strlen(header);
        if (!cdc_write_record(header, hlen))
            return;
        csv_header_exported = true;
        tud_cdc_write_flush();
    }

    uint32_t event_pending = event_total - event_exported;
    if (event_pending > 0)
    {
        if (event_pending > EVENT_BUFFER_SIZE)
        {
            event_exported = event_total - EVENT_BUFFER_SIZE;
            event_pending  = EVENT_BUFFER_SIZE;
        }

        uint16_t ev_start =
            (event_head + EVENT_BUFFER_SIZE - event_pending) % EVENT_BUFFER_SIZE;
        metrics_event_record_t *ev = &event_buffer[ev_start];

        char ev_buf[64];
        int  ev_len = snprintf(ev_buf, sizeof(ev_buf), "E,%lu,%c,%u,%04x,%04x\r\n",
                               (unsigned long)ev->timestamp_us, ev->code,
                               (unsigned int)ev->device_type, (unsigned int)ev->vid,
                               (unsigned int)ev->pid);

        if (ev_len <= 0 || ev_len >= (int)sizeof(ev_buf))
            return;
        if (!cdc_write_record(ev_buf, (uint32_t)ev_len))
            return;

        event_exported++;
        tud_cdc_write_flush();
        return;
    }

    uint32_t key_pending = forwarding_total - forwarding_exported;
    if (key_pending == 0)
        return;

    if (key_pending > FORWARDING_BUFFER_SIZE)
    {
        forwarding_exported = forwarding_total - FORWARDING_BUFFER_SIZE;
        key_pending         = FORWARDING_BUFFER_SIZE;
    }

    uint16_t             start = (forwarding_head + FORWARDING_BUFFER_SIZE - key_pending) %
                                 FORWARDING_BUFFER_SIZE;
    forwarding_sample_t *s     = &forwarding_buffer[start];
    char                 mode  = s->filtering_enabled ? 'F' : 'N';

    char buf[64];
    int  len = snprintf(buf, sizeof(buf), "K,%lu,%c,%lu,%u\r\n",
                        (unsigned long)s->sequence, mode,
                        (unsigned long)s->total_us, s->had_strike ? 1u : 0u);

    if (len <= 0 || len >= (int)sizeof(buf))
        return;
    if (!cdc_write_record(buf, (uint32_t)len))
        return;

    forwarding_exported++;
    tud_cdc_write_flush();
}
