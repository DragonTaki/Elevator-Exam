/* ----- ----- ----- ----- */
// scheduler.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#include "scheduler.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "elevator.h"
#include "status.h"

/* sign helper */
static int sign_int(int x) {
    return (x > 0) ? 1 : ((x < 0) ? -1 : 0);
}

static int count_requests(const Elevator* e) {
    if (!e) return 0;
    int count = 0;
    for (int f = 0; f < MAX_FLOORS; ++f) {
        if (e->inside[f]) ++count;
        if (e->call_up[f]) ++count;
        if (e->call_down[f]) ++count;
    }
    return count;
}

/* Estimate cost: distance + penalties/bonuses.
 * Bonuses:
 *  - idle elevators: -2.0
 *  - elevator moving towards pickup and pickup is along its path: -1.5
 * Penalties:
 *  - door open: +2.0 (slower to respond)
 *  - high local queue load: +(qload*3.0)
 */
static double estimate_cost(const Elevator* e, int pickup_floor)
{
    if (!e) return 1e12;
    double d = fabs((double)e->current_floor - (double)pickup_floor);
    double cost = d;

    if (e->task_state == TASK_IDLE) cost -= 2.0;

    /* direction match bonus */
    if (e->direction != DIR_NONE) {
        int want_dir = sign_int(pickup_floor - e->current_floor);
        if (want_dir != 0 && want_dir == (int)e->direction) {
            /* ensure pickup is ahead (not behind) */
            if ((e->direction == DIR_UP && pickup_floor >= e->current_floor) ||
                (e->direction == DIR_DOWN && pickup_floor <= e->current_floor)) {
                cost -= 1.5;
            }
        }
        if (e->task_state == TASK_DOOR_OPEN || e->task_state == TASK_DOOR_OPENING) cost += 2.0;
    }

    /* qload: normalize by MAX_FLOORS to keep scale moderate */
    double qcount = (double)count_requests(e);
    double qload = qcount / (double)MAX_FLOORS;
    cost += qload * 3.0;

    if (cost < 0.0) cost = 0.0;
    return cost;
}

/* try_assign_one:
 * - pop one PendingRequest from pending queue (rq_pop returns 0 on success)
 * - choose best elevator via estimate_cost
 * - assign using elevator_add_request_flag (flag-based API)
 * - if assignment fails (error), push back to pending and return 0
 * - if no elevator available, push back and return 0
 * - returns 1 if assigned successfully
 */
static int try_assign_one(RequestQueue* pending, Elevator elevators[], int elevator_count)
{
    PendingRequest preq;
    if (rq_pop(pending, &preq) != 0) return 0; /* nothing to pop */

    int best_idx = -1;
    double best_cost = 1e18;

    for (int i = 0; i < elevator_count; ++i) {
        Elevator* e = &elevators[i];
        /* skip if elevator flagged full (optional: use count_requests limit) */
        int cur_load = count_requests(e);
        if (cur_load >= MAX_REQUESTS) continue;

        double c = estimate_cost(e, preq.floor);
        if (c < best_cost) {
            best_cost = c;
            best_idx = i;
        }
    }

    if (best_idx < 0) {
        /* no elevator available now -> push back request */
        rq_push(pending, preq);
        return 0;
    }

    Elevator* chosen = &elevators[best_idx];

    printf("[SCHED] try_assign_one: picked elevator %d for request floor=%d type=%d (cost=%.2f)\n",
           best_idx, preq.floor, (int)preq.type, best_cost);

    /* perform assignment via flag API */
    int rc = ELEV_ERR_INTERNAL;
    if (preq.type == REQ_INSIDE) {
        rc = elevator_add_request_flag(chosen, preq.floor, REQ_INSIDE);
    } else if (preq.type == REQ_CALL_UP) {
        rc = elevator_add_request_flag(chosen, preq.floor, REQ_CALL_UP);
    } else if (preq.type == REQ_CALL_DOWN) {
        rc = elevator_add_request_flag(chosen, preq.floor, REQ_CALL_DOWN);
    } else {
        /* unknown type: push back and bail */
        rq_push(pending, preq);
        return 0;
    }

    if (rc == ELEV_OK || rc == ELEV_DUPLICATE) {
        /* success or duplicate (duplicate treated as assigned) */
        printf("[SCHED] try_assign_one: elevator_add_request_flag SUCCEEDED for E%d floor=%d (rc=%d)\n",
               chosen->id, preq.floor, rc);
        return 1;
    } else {
        /* fatal error -> push back and log */
        printf("[SCHED] try_assign_one: elevator_add_request_flag FAILED for E%d floor=%d (rc=%d) -> pushed back\n",
               chosen->id, preq.floor, rc);
        rq_push(pending, preq);
        return 0;
    }
}

void Scheduler_Process(Elevator elevators[], int elevator_count, RequestQueue* pending)
{
    if (!pending || !elevators) return;

    const int MAX_ASSIGN_PER_TICK = 8;
    for (int i = 0; i < MAX_ASSIGN_PER_TICK; ++i) {
        int ok = try_assign_one(pending, elevators, elevator_count);
        if (!ok) break;
    }
}
