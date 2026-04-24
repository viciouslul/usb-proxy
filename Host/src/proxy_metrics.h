#pragma once

#include "proxy_data.h"
#include <stdbool.h>
#include <stdint.h>

#define METRICS_KEYSTROKE_HISTORY_SIZE 256
#define METRICS_DETECTION_EVENT_SIZE   64

typedef struct
{
    uint32_t interval_us;
    uint8_t  keycode;
    bool     is_suspicious;
    uint32_t timestamp_us;
} keystroke_sample_t;

typedef enum
{
    EVENT_ENUM_OK,
    EVENT_ENUM_ERROR,
    EVENT_BOT_DETECTED,
    EVENT_STRIKE,
    EVENT_DEVICE_SELECTED,
    EVENT_DEVICE_UNMOUNTED,
    EVENT_REENUM_DETECTED,
    EVENT_IDLE_TIMEOUT,
} detection_event_type_t;

typedef struct
{
    uint32_t               timestamp_us;
    proxy_device_t         device_type;
    detection_event_type_t event_type;
} detection_event_t;

typedef struct
{
    uint32_t sequence;
    bool     filtering_enabled;
    bool     had_strike;
    uint32_t host_ts_us;
    uint32_t submit_ts_us;
    uint32_t complete_ts_us;
    uint32_t queue_us;
    uint32_t usb_us;
    uint32_t total_us;
} forwarding_sample_t;

void metrics_init(void);

void metrics_record_keystroke(uint32_t interval_us, uint8_t keycode, bool is_suspicious);
void metrics_record_event(detection_event_t event);
void metrics_record_mount(proxy_device_t device_type, uint16_t vid, uint16_t pid);

void metrics_record_report_processed(bool was_blocked);
void metrics_record_enumeration_mismatch(void);
void metrics_reset_transient_state(void);

void metrics_record_latency(uint32_t latency_us);
void metrics_record_forwarding_sample(forwarding_sample_t sample);

void metrics_try_export_cdc(void);

uint16_t metrics_get_keystroke_count(void);
uint16_t metrics_get_event_count(void);
void     metrics_get_stats(uint32_t *total_reports, uint32_t *blocked_reports,
                           uint32_t *enum_mismatches, uint32_t *mean_latency_us);
