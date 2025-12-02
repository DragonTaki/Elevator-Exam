/* ----- ----- ----- ----- */
// request_queue.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#ifndef REQUEST_QUEUE_H
#define REQUEST_QUEUE_H

#include "elevator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 電梯工作 Request Queue */
typedef struct {
    PendingRequest items[MAX_REQUESTS];
    int head;
    int tail;
    int count;
} RequestQueue;

/* Initialize queue */
void rq_init(RequestQueue* q);

/* Push item to tail. Returns 0 on success, -1 on full or error. */
int rq_push(RequestQueue* q, PendingRequest r);

/* Pop item from head. Returns 0 on success and writes to out (if non-NULL), -1 if empty. */
int rq_pop(RequestQueue* q, PendingRequest* out);

/* Peek head element without popping. Returns 0 on success, -1 if empty. */
int rq_peek(RequestQueue* q, PendingRequest* out);

/* Return non-zero if empty, else 0 */
int rq_empty(RequestQueue* q);

/* Return number of items */
int rq_count(RequestQueue* q);

/* Clear queue */
void rq_clear(RequestQueue* q);

#ifdef __cplusplus
}
#endif

#endif /* REQUEST_QUEUE_H */
