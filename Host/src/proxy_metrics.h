#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "proxy_data.h"

#define METRICS_KEYSTROKE_HISTORY_SIZE 256
#define METRICS_DETECTION_EVENT_SIZE   64

typedef struct {
    uint32_t interval_us;
    uint8_t keycode;
    bool is_suspicious;
    uint32_t timestamp_us;
} keystroke_sample_t;

typedef struct {
    uint32_t timestamp_us;
    proxy_device_t device_type;

    enum {
        EVENT_ENUM_OK,
        EVENT_ENUM_ERROR,
        EVENT_BOT_DETECTED,
        EVENT_DEVICE_SELECTED,
        EVENT_DEVICE_UNMOUNTED,
    } event_type;
} detection_event_t;

typedef struct {
    uint32_t sequence;
    bool filtering_enabled;
    uint32_t host_ts_us;
    uint32_t submit_ts_us;
    uint32_t complete_ts_us;
    uint32_t queue_us;
    uint32_t usb_us;
    uint32_t total_us;
} forwarding_sample_t;

void metrics_init(void);

// Keystroke tracking for RQ2 analysis
void metrics_record_keystroke(uint32_t interval_us, uint8_t keycode, bool is_suspicious);

// Detection events for RQ1 and RQ3
void metrics_record_event(detection_event_t event);

// Security stats for RQ1
void metrics_record_report_processed(bool was_blocked);
void metrics_record_enumeration_mismatch(void);

// Performance tracking for RQ3 (records latency, maintains running average)
void metrics_record_latency(uint32_t latency_us);
void metrics_record_forwarding_sample(forwarding_sample_t sample);

// Data export (call periodically, e.g., every 5 seconds)
// Exports: K (keystroke), E (event), L (latency avg), S (summary)
void metrics_try_export_cdc(void);

// Query functions
uint16_t metrics_get_keystroke_count(void);
uint16_t metrics_get_event_count(void);
void metrics_get_stats(uint32_t *total_reports, uint32_t *blocked_reports,
                       uint32_t *enum_mismatches, uint32_t *mean_latency_us);
