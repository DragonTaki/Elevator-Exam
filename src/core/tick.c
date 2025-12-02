/* ----- ----- ----- ----- */
// tick.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/12/01
// Update Date: 2025/12/01
// Version: v1.0
/* ----- ----- ----- ----- */

#include "tick.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheduler.h"
#include "server_events.h"
#include "status.h"
#include "remote_server.h"

/* Use same SIM_TICK_MS define as remote_server or pass in. */
static Elevator* g_elevators = NULL;
static int g_elevator_count = 0;
static RequestQueue* g_pending = NULL;
static int g_tick_ms = 300;
static DWORD g_last_tick = 0;

/* previous status strings (for change detection) */
static char* g_prev_status = NULL; /* single big buffer of size elevator_count * 256 */
static int g_prev_status_size = 0;
static int* s_assigned_drop = NULL;

/* control flag (default 0) */
int REMOTE_TICK_ALWAYS_PRINT_STATUS = 0;

int remote_tick_init(Elevator elevators[], int elevator_count, RequestQueue* pending, int tick_ms, int assigned_drop_array[]) {
    if (!elevators || elevator_count <= 0 || !pending) return -1;
    g_elevators = elevators;
    g_elevator_count = elevator_count;
    g_pending = pending;
    g_tick_ms = (tick_ms > 0) ? tick_ms : 300;
    g_last_tick = 0;
    s_assigned_drop = assigned_drop_array;

    /* allocate prev status buffer */
    g_prev_status_size = elevator_count * 256;
    g_prev_status = (char*)malloc((size_t)g_prev_status_size);
    if (!g_prev_status) {
        g_prev_status_size = 0;
        return -1;
    }
    memset(g_prev_status, 0, g_prev_status_size);
    return 0;
}

static void print_status_if_changed(void) {
    /* For each elevator, get status line and compare to previous */
    char buf[256];
    for (int i = 0; i < g_elevator_count; ++i) {
        Elevator_status_line(&g_elevators[i], buf, sizeof(buf));
        char* prev_ptr = g_prev_status + i * 256;
        if (REMOTE_TICK_ALWAYS_PRINT_STATUS || strncmp(prev_ptr, buf, 255) != 0) {
            printf("%s\n", buf);
            /* copy to prev */
            strncpy(prev_ptr, buf, 255);
            prev_ptr[255] = '\0';
        }
    }
}

/* Single tick action (do the actual processing). Called when time to tick */
static void remote_tick_perform(void) {
    double dt_seconds = ((double)g_tick_ms) / 1000.0;

    /* 0) scheduler assigns pending requests first (so elevator can move same tick) */
    Scheduler_Process(g_elevators, g_elevator_count, g_pending);

    /* 1) step elevators */
    for (int i = 0; i < g_elevator_count; ++i) {
        Elevator_step(&g_elevators[i], dt_seconds);
    }

    /* 1.5) inject assigned drop if elevator just opened and had assigned drop */
    for (int i = 0; i < g_elevator_count; ++i) {
        Elevator* e = &g_elevators[i];
        if (!s_assigned_drop || e->id < 0 || e->id >= g_elevator_count) continue;

        int assigned = s_assigned_drop[e->id];

        if (assigned >= 0 &&
            (e->current_floor == e->target_floor) &&
            (e->task_state == TASK_DOOR_OPEN || e->task_state == TASK_DOOR_OPENING)) {

            int rc = elevator_add_request_flag(e, assigned, REQ_INSIDE);

            if (rc == ELEV_OK) {
                printf("[TICK] Elevator %d: injected inside-drop to %d (ok)\n", e->id, assigned);
            } else if (rc == ELEV_DUPLICATE) {
                /* already present — treat as handled */
                printf("[TICK] Elevator %d: inside-drop to %d already queued (duplicate)\n", e->id, assigned);
            } else {
                /* rc < 0 : error (invalid, etc.) — log and continue (do not fallback) */
                printf("[TICK] Elevator %d: failed to inject drop %d (error=%d)\n", e->id, assigned, rc);
            }

            /* clear assigned marker (do not retry) */
            s_assigned_drop[e->id] = -1;
        }
    }

    /* 2) print status (only when changed unless ALWAYS flag set) */
    print_status_if_changed();

    /* 3) broadcast status to watchers */
    broadcast_status_to_guards(g_elevators, g_elevator_count, 1);
}

/* Try to execute tick if enough time elapsed.
 * now_ms = 0 -> will call GetTickCount() internally
 */
int remote_tick_try_do(DWORD now_ms) {
    if (!g_elevators || !g_pending) return -1;
    DWORD now = now_ms ? now_ms : GetTickCount();
    if (g_last_tick == 0) { g_last_tick = now; return 0; }
    if ((int)(now - g_last_tick) >= g_tick_ms) {
        remote_tick_perform();
        g_last_tick = now;
        return 1;
    }
    return 0;
}

int remote_tick_do_now(void) {
    if (!g_elevators || !g_pending) return -1;
    remote_tick_perform();
    g_last_tick = GetTickCount();
    return 0;
}

void remote_tick_shutdown(void) {
    if (g_prev_status) { free(g_prev_status); g_prev_status = NULL; g_prev_status_size = 0; }
    g_elevators = NULL;
    g_pending = NULL;
    g_elevator_count = 0;
    g_tick_ms = 0;
    g_last_tick = 0;
}
