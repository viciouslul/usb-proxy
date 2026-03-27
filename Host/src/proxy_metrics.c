#include "proxy_metrics.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#define FORWARDING_BUFFER_SIZE 256

static forwarding_sample_t forwarding_buffer[FORWARDING_BUFFER_SIZE] = {0};
static uint16_t forwarding_head = 0;
static uint16_t forwarding_count = 0;
static uint32_t forwarding_total = 0;
static uint32_t forwarding_exported = 0;
static bool csv_header_exported = false;

static uint32_t total_reports = 0;
static uint32_t blocked_reports = 0;
static uint32_t enum_mismatches = 0;
static uint32_t latency_sum_us = 0;

static bool cdc_write_record(char const* data, uint32_t len)
{
    if (!tud_cdc_connected()) return false;
    if (tud_cdc_write_available() < len) return false;
    if (tud_cdc_write((uint8_t const*)data, len) != len) return false;
    return true;
}

void metrics_init(void)
{
    forwarding_head = 0;
    forwarding_count = 0;
    forwarding_total = 0;
    forwarding_exported = 0;
    csv_header_exported = false;

    total_reports = 0;
    blocked_reports = 0;
    enum_mismatches = 0;
    latency_sum_us = 0;
}

void metrics_record_keystroke(uint32_t interval_us, uint8_t keycode, bool is_suspicious)
{
    (void)interval_us;
    (void)keycode;
    (void)is_suspicious;
}

void metrics_record_event(detection_event_t event)
{
    (void)event;
}

void metrics_record_report_processed(bool was_blocked)
{
    total_reports++;
    if (was_blocked)
    {
        blocked_reports++;
    }
}

void metrics_record_enumeration_mismatch(void)
{
    enum_mismatches++;
}

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
        .host_ts_us = 0,
        .submit_ts_us = 0,
        .complete_ts_us = time_us_32(),
        .queue_us = 0,
        .usb_us = 0,
        .total_us = latency_us,
    };
    metrics_record_forwarding_sample(sample);
}

void metrics_record_forwarding_sample(forwarding_sample_t sample)
{
    sample.sequence = forwarding_total;
    forwarding_buffer[forwarding_head] = sample;
    forwarding_head = (forwarding_head + 1) % FORWARDING_BUFFER_SIZE;

    if (forwarding_count < FORWARDING_BUFFER_SIZE)
    {
        forwarding_count++;
    }

    forwarding_total++;
    latency_sum_us += sample.total_us;
}

uint16_t metrics_get_keystroke_count(void)
{
    return 0;
}

uint16_t metrics_get_event_count(void)
{
    return 0;
}

void metrics_get_stats(uint32_t *total, uint32_t *blocked, uint32_t *mismatches, uint32_t *mean_latency)
{
    *total = total_reports;
    *blocked = blocked_reports;
    *mismatches = enum_mismatches;
    *mean_latency = (forwarding_total > 0) ? (latency_sum_us / forwarding_total) : 0;
}

// Emit CSV header once, then one compact forwarding row per call.
// Header: H,seq,mode,queue_us,usb_us,total_us
// Row:    M,<seq>,<mode>,<queue>,<usb>,<total>
void metrics_try_export_cdc(void)
{
    if (!csv_header_exported)
    {
        char const *header = "H,seq,mode,queue_us,usb_us,total_us\r\n";
        uint32_t hlen = (uint32_t)strlen(header);
        if (!cdc_write_record(header, hlen)) return;
        csv_header_exported = true;
        tud_cdc_write_flush();
    }

    uint32_t pending = forwarding_total - forwarding_exported;
    if (pending == 0) return;

    if (pending > FORWARDING_BUFFER_SIZE)
    {
        forwarding_exported = forwarding_total - FORWARDING_BUFFER_SIZE;
        pending = FORWARDING_BUFFER_SIZE;
    }

    uint16_t start = (forwarding_head + FORWARDING_BUFFER_SIZE - pending) % FORWARDING_BUFFER_SIZE;
    forwarding_sample_t *s = &forwarding_buffer[start];
    char mode = s->filtering_enabled ? 'F' : 'N';

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "M,%lu,%c,%lu,%lu,%lu\r\n",
                       s->sequence,
                       mode,
                       s->queue_us,
                       s->usb_us,
                       s->total_us);
    if (len <= 0) return;

    if (!cdc_write_record(buf, (uint32_t)len)) return;

    forwarding_exported++;
    tud_cdc_write_flush();
}
