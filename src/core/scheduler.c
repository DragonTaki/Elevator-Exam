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

/* 計算電梯目前所有待處理請求數（內部 + 上 + 下）*/
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

/* 估算電梯載客的成本 */
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

/*
 * 從 pending queue 取一個請求，選擇最適合的電梯分配
 * A) 先選擇最靠近的閒置電梯
 * B) 否則依成本評估選最佳電梯
 * 分配失敗則將請求放回佇列
 */
static int try_assign_one(RequestQueue* pending, Elevator elevators[], int elevator_count)
{
    PendingRequest preq;
    if (rq_pop(pending, &preq) != 0) return 0;

    int pickup_floor = preq.floor;

    // 如果有閒置，先選最近的閒置電梯
    int idle_idx = -1;
    int idle_best_dist = INT_MAX;
    for (int i = 0; i < elevator_count; ++i) {
        Elevator* e = &elevators[i];
        if (!e) continue;
        if (e->task_state == TASK_IDLE) {
            int dist = abs(e->current_floor - pickup_floor);
            // 計算閒置電梯的距離，挑近的
            if (dist < idle_best_dist) {
                idle_best_dist = dist;
                idle_idx = i;
            } else if (dist == idle_best_dist) {
                // 距離相同選擇請求較少的電梯
                int cur_load = 0;
                for (int f = 0; f < MAX_FLOORS; ++f) {
                    cur_load += (e->inside[f] || e->call_up[f] || e->call_down[f]);
                }
                Elevator* prev = &elevators[idle_idx];
                int prev_load = 0;
                for (int f = 0; f < MAX_FLOORS; ++f) {
                    prev_load += (prev->inside[f] || prev->call_up[f] || prev->call_down[f]);
                }
                if (cur_load < prev_load) idle_idx = i;
            }
        }
    }

    int best_idx = -1;
    double best_cost = 1e18;

    // 如果挑到閒置 => 指派過去
    if (idle_idx >= 0) {
        /* Assign to the chosen idle elevator */
        best_idx = idle_idx;
        best_cost = 0.0;
        printf("[SCHED] try_assign_one: picked idle elevator %d for request floor=%d type=%d\n",
               best_idx, pickup_floor, (int)preq.type);
    }
    // 沒閒置的 => 去算載客成本
    else {
        for (int i = 0; i < elevator_count; ++i) {
            Elevator* e = &elevators[i];
            int cur_load = count_requests(e);
            if (cur_load >= MAX_REQUESTS) continue;

            double c = estimate_cost(e, preq.floor);
            if (c < best_cost) {
                best_cost = c;
                best_idx = i;
            }
        }
        if (best_idx >= 0) {
            printf("[SCHED] try_assign_one: picked elevator %d for request floor=%d type=%d (cost=%.2f)\n",
                   best_idx, pickup_floor, (int)preq.type, best_cost);
        }
    }

    // 沒可用電梯 => 將請求丟回佇列等待下次分配
    if (best_idx < 0) {
        rq_push(pending, preq);
        return 0;
    }

    Elevator* chosen = &elevators[best_idx];

    printf("[SCHED] try_assign_one: picked elevator %d for request floor=%d type=%d (cost=%.2f)\n",
           best_idx, preq.floor, (int)preq.type, best_cost);

    int rc = ELEV_ERR_INTERNAL;
    if (preq.type == REQ_INSIDE) {
        // 內部請求 => 直接記錄到相應的電梯
        // 正常不會跑到這（防止意外）
        rc = Elevator_push_inside_request(chosen, preq.to_floor, preq.source_id);
    } else {
        // 外部請求 => 進入請求佇列嘗試分配
        rc = elevator_add_request_flag(chosen, preq.floor, preq.type);
    }

    if (rc == ELEV_OK || rc == ELEV_DUPLICATE) {
        // 成功指派請求
        printf("[SCHED] try_assign_one: elevator_add_request_flag SUCCEEDED for E%d floor=%d (rc=%d)\n",
               chosen->id, preq.floor, rc);
        return 1;
    } else {
        // 錯誤
        printf("[SCHED] try_assign_one: elevator_add_request_flag FAILED for E%d floor=%d (rc=%d) -> pushed back\n",
               chosen->id, preq.floor, rc);
        rq_push(pending, preq);
        return 0;
    }
}

/* 電梯排程器 */
void Scheduler_Process(Elevator elevators[], int elevator_count, RequestQueue* pending)
{
    if (!pending || !elevators) return;

    const int MAX_ASSIGN_PER_TICK = 8;
    for (int i = 0; i < MAX_ASSIGN_PER_TICK; ++i) {
        int ok = try_assign_one(pending, elevators, elevator_count);
        if (!ok) break;
    }
}
