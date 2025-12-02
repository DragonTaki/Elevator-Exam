/* ----- ----- ----- ----- */
// request_queue.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#include "request_queue.h"
#include <string.h>

/* ---------------------------
   RequestQueue implementation
   --------------------------- */

void rq_init(RequestQueue* q) {
    if (!q) return;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    /* optional: zero memory */
    /* memset(q->items, 0, sizeof(q->items)); */
}

int rq_push(RequestQueue* q, PendingRequest r) {
    if (!q) return -1;
    if (q->count >= MAX_REQUESTS) return -1;
    q->items[q->tail] = r;
    q->tail = (q->tail + 1) % MAX_REQUESTS;
    q->count++;
    return 0;
}

int rq_pop(RequestQueue* q, PendingRequest* out) {
    if (!q) return -1;
    if (q->count == 0) return -1;
    if (out) *out = q->items[q->head];
    q->head = (q->head + 1) % MAX_REQUESTS;
    q->count--;
    return 0;
}

int rq_peek(RequestQueue* q, PendingRequest* out) {
    if (!q) return -1;
    if (q->count == 0) return -1;
    if (out) *out = q->items[q->head];
    return 0;
}

int rq_empty(RequestQueue* q) {
    if (!q) return 1;
    return (q->count == 0);
}

int rq_count(RequestQueue* q) {
    if (!q) return 0;
    return q->count;
}

void rq_clear(RequestQueue* q) {
    if (!q) return;
    q->head = q->tail = q->count = 0;
}
