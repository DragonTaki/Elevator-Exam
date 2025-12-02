/* ----- ----- ----- ----- */
// server_core.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#include "server_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
#include "server_events.h"

/* Config */
#define DEFAULT_ELEVATOR_COUNT 2
#define TICK_DT_SECONDS 0.1
#define BUF_SZ 4096

/* Globals internal to server_core */
static Elevator g_elevators[MAX_ELEVATORS];
static int g_elevator_count = DEFAULT_ELEVATOR_COUNT;
static RequestQueue g_pending_requests;
static int g_running = 0;

/* Core thread handle */
static PlatformThread* g_core_thread = NULL;

static server_core_status_cb_t g_status_cb = NULL;
/* set status callback (optional) */
void server_core_set_status_callback(server_core_status_cb_t cb) {
    g_status_cb = cb;
}

/* Initialization */
int server_core_init(int elevator_count)
{
    if (elevator_count <= 0 || elevator_count > MAX_ELEVATORS) elevator_count = DEFAULT_ELEVATOR_COUNT;
    g_elevator_count = elevator_count;

    // init pending queue
    rq_init(&g_pending_requests);

    // init elevators
    for (int i = 0; i < g_elevator_count; ++i) {
        Elevator_init(&g_elevators[i], i, 1);
    }

    // init server_events system (network will push into this)
    server_events_init();

    return 0; /* success */
}

/* Shutdown helper */
void server_core_shutdown(void)
{
    // notify server_events to shutdown (wakes consumer)
    server_events_shutdown();
}

/* Expose pointer to pending requests so tests or init can pre-fill */
RequestQueue* server_core_get_pending_queue(void) {
    return &g_pending_requests;
}

/* Process server_events queue into pending/local queues. Non-blocking: try to drain current events. */
static void process_incoming_events_once(void)
{
    ServerEvent* ev = NULL;
    while (server_events_try_pop(&ev) == 0) {
        if (!ev) continue;

        switch (ev->type) {
            case EVT_OUTSIDE_CALL: {
                PendingRequest p;
                p.floor     = ev->v.outside_call.floor;
                p.source_id = ev->v.outside_call.client_id;
                p.to_floor  = -1;  /* 外呼沒有目的樓層 */

                if (ev->v.outside_call.direction == DIR_UP)
                    p.type = REQ_CALL_UP;
                else
                    p.type = REQ_CALL_DOWN;
                rq_push(&g_pending_requests, p);
            } break;

            case EVT_INSIDE_CALL: {
                int eid = ev->v.inside_call.elevator_id;
                if (eid >= 0 && eid < g_elevator_count) {
                    /* push into elevator local queue via helper (or direct push) */
                    Elevator_push_inside_request(&g_elevators[eid], ev->v.inside_call.dest_floor, ev->v.inside_call.client_id);
                } else {
                    // invalid elevator id: ignore or log
                    // fprintf(stderr, "[CORE] invalid inside call elevator id %d\n", eid);
                }
            } break;

            case EVT_GUARD_COMMAND: {
                // Implement guard handling as needed: e.g., force assign, maintenance flag
                // For now we optionally support a simple "force assign" where guard requests direct push to specific elevator
                int eid = ev->v.guard_cmd.elevator_id;
                if (eid >= 0 && eid < g_elevator_count && ev->v.guard_cmd.force) {

                    PendingRequest p;
                    p.floor     = ev->v.guard_cmd.floor;
                    p.source_id = ev->v.guard_cmd.client_id;
                    p.to_floor  = -1;
                    p.type      = REQ_CALL_UP;  /* 你可依需求改成 guard_cmd.direction */

                    /* 強制加入該電梯 */
                    int rc = elevator_add_request_flag(&g_elevators[eid], p.floor, p.type);
                    if (rc < 0) {
                        printf("[CORE] guard forced call rejected: E%d floor=%d\n", eid, p.floor);
                    }
                }
            } break;

            case EVT_SHUTDOWN: {
                // set running to 0 to exit loop gracefully
                g_running = 0;
            } break;

            default:
                break;
        }

        server_events_free(ev);
        ev = NULL;
    }
}

/* Build a textual snapshot and call status callback if available; otherwise print to stdout */
static void publish_state_once(void)
{
    // build a compact multi-line snapshot into a buffer
    // Estimate buffer size: per elevator ~128 chars
    char buf[BUF_SZ];
    int off = 0;
    off += snprintf(buf + off, BUF_SZ - off, "=== ELEVATORS STATUS ===\n");
    for (int i = 0; i < g_elevator_count && off < BUF_SZ; ++i) {
        char line[256];
        Elevator_status_line(&g_elevators[i], line, sizeof(line));
        off += snprintf(buf + off, BUF_SZ - off, "%s\n", line);
    }
    // publish via callback if set, else print
    if (g_status_cb) {
        g_status_cb(buf);
    } else {
        // default: print to stdout (useful for demo)
        fputs(buf, stdout);
    }
}

/* Core loop function (PlatformThreadFn signature) */
static void* core_thread_fn(void* arg)
{
    (void)arg;
    const double dt = TICK_DT_SECONDS;
    g_running = 1;

    while (g_running) {
        // 1 process events
        process_incoming_events_once();

        // 2 scheduler
        Scheduler_Process(g_elevators, g_elevator_count, &g_pending_requests);

        // 3 step elevators
        for (int i = 0; i < g_elevator_count; ++i) {
            Elevator_step(&g_elevators[i], dt);
        }

        // 4 publish state (could be every N ticks; here every tick)
        publish_state_once();

        // 5 sleep dt
        platform_sleep_ms((int)(dt * 1000.0));
    }

    return NULL;
}

/* Start core loop in a thread (non-blocking). Returns 0 on success. */
int server_core_start(void)
{
    if (g_core_thread != NULL) return -1; // already running
    g_core_thread = platform_thread_create(core_thread_fn, NULL);
    if (!g_core_thread) return -1;
    return 0;
}

/* Stop core loop and join thread */
void server_core_stop(void)
{
    // set running flag to 0 and also trigger server_events shutdown to wake potential blocking calls
    g_running = 0;
    server_events_shutdown();
    if (g_core_thread) {
        platform_thread_join(g_core_thread);
        g_core_thread = NULL;
    }
}

/* Blocking join (if needed) */
void server_core_join(void)
{
    if (g_core_thread) {
        platform_thread_join(g_core_thread);
        g_core_thread = NULL;
    }
}

Elevator* server_core_get_elevators(void) {
    return g_elevators; /* pointer to internal array (read-only to caller) */
}

int server_core_get_elevator_count(void) {
    return g_elevator_count;
}
