#pragma once
#include "tusb.h"

#define MAX_HID_REPORT_SIZE 8
#define PROXY_QUEUE_SIZE    64

typedef enum {
    PROXY_MSG_NONE = 0,
    PROXY_MSG_MOUNT,
    PROXY_MSG_UNMOUNT,
    PROXY_MSG_REPORT
} proxy_msg_t;

typedef enum {
    HID_NONE = 0,
    HID_KEYBOARD,
    HID_MOUSE,
    MSC
} proxy_device_t;

typedef struct {
    proxy_msg_t msg_t;
    proxy_device_t dev_t;
    uint32_t timestamp_us;

    uint16_t report_len;
    uint8_t report[MAX_HID_REPORT_SIZE];
} proxy_packet_t;

typedef struct {
    volatile uint32_t head;
    volatile uint32_t tail;
    proxy_packet_t buffer[PROXY_QUEUE_SIZE];
} proxy_queue_t;

extern proxy_queue_t proxy_queue;

/* Producer (Core 1 calls this ONLY)*/
static inline bool proxy_enqueue(proxy_packet_t *pkt)
{
    uint32_t next = (proxy_queue.head + 1) % PROXY_QUEUE_SIZE;

    if (next == proxy_queue.tail) return false; // Queue full

    proxy_queue.buffer[proxy_queue.head] = *pkt;
    proxy_queue.head = next;

    return true;
}

/* Consumer (Core 0 calls this ONLY)*/
static inline bool proxy_dequeue(proxy_packet_t *pkt)
{
    if (proxy_queue.tail == proxy_queue.head ) return false; // Queue empty

    *pkt = proxy_queue.buffer[proxy_queue.tail];
    proxy_queue.tail = (proxy_queue.tail + 1) % PROXY_QUEUE_SIZE;

    return true;
}