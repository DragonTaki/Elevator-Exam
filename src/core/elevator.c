/* ----- ----- ----- ----- */
// elevator.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elevator.h"
#include "status.h"

const double DEFAULT_SPEED_FPS = 1.0 / 1.0;  // 移動 1 層樓 / 每 1 秒
const double DEFAULT_DOOR_OPEN_S = 1.0;      // 電梯開門時長（秒）
const double MIN_DOOR_OPEN_S = 0.1;          // safety lower bound

/* accumulators for fractional movement per elevator (time accumulation)
   static so multiple calls keep progress. Size uses MAX_ELEVATORS. */
static double g_accum_time[MAX_ELEVATORS] = {0.0};

/* ---------------------------
   Flag-based helpers
   --------------------------- */

/* returns 1 if any request exists (inside or external) on floor */
static inline int has_request_on_floor(const Elevator* e, int floor) {
    if (!e || floor < 0 || floor >= MAX_FLOORS) return 0;
    return (e->inside[floor] || e->call_up[floor] || e->call_down[floor]) ? 1 : 0;
}

/* returns nonzero if elevator has any stops at all */
int elevator_has_stops(const Elevator* e) {
    if (!e) return 0;
    for (int f = 0; f < MAX_FLOORS; ++f) {
        if (e->inside[f] || e->call_up[f] || e->call_down[f]) return 1;
    }
    return 0;
}

/* returns nonzero if there are any stops in the given direction relative to current floor */
static int has_stops_in_direction_flag(const Elevator* e, Direction dir) {
    if (!e) return ELEV_ERR_INVALID;
    int cur = e->current_floor;
    if (dir == DIR_UP) {
        for (int f = cur + 1; f < MAX_FLOORS; ++f)
            if (has_request_on_floor(e, f)) return ELEV_OK;
        return ELEV_ERR_INVALID;
    } else if (dir == DIR_DOWN) {
        for (int f = cur - 1; f >= 0; --f)
            if (has_request_on_floor(e, f)) return ELEV_OK;
        return ELEV_ERR_INVALID;
    } else {
        return elevator_has_stops(e);
    }
}

/* pick direction when idle: prefer up if any up, else down, else none.
 * This preserves previous simple behavior.
 */
static Direction pick_direction_when_idle_flag(Elevator* e) {
    if (!e) return DIR_NONE;
    int cur = e->current_floor;
    /* prefer any request above */
    for (int f = cur + 1; f < MAX_FLOORS; ++f)
        if (has_request_on_floor(e, f)) return DIR_UP;
    for (int f = cur - 1; f >= 0; --f)
        if (has_request_on_floor(e, f)) return DIR_DOWN;
    return DIR_NONE;
}

/* remove served requests upon arrival at `floor`:
 * - always clear inside[floor]
 * - clear external requests that match the arrival direction:
 *   - if arrival_dir == DIR_UP: clear call_up[floor]
 *   - if arrival_dir == DIR_DOWN: clear call_down[floor]
 * - if arrival_dir == DIR_NONE (idle stop), clear both externals
 */
static void remove_served_flags_on_arrival(Elevator* e, int floor, Direction arrival_dir) {
    if (!e || floor < 0 || floor >= MAX_FLOORS) return;

    /* always remove inside requests for this floor */
    e->inside[floor] = false;

    if (arrival_dir == DIR_UP) {
        e->call_up[floor] = false;
    } else if (arrival_dir == DIR_DOWN) {
        e->call_down[floor] = false;
    } else {
        /* idle -> remove both sides (policy) */
        e->call_up[floor] = false;
        e->call_down[floor] = false;
    }

    printf("[ELEV_DEBUG] E%d remove_served_at_floor -> up=%d down=%d inside=%d (floor=%d)\n",
           e->id, e->call_up[floor], e->call_down[floor], e->inside[floor], floor);
}

/* 加入電梯排程 */
// return -1：錯誤
// return 0：正常結束
// return 1：重複請求
int elevator_add_request_flag(Elevator *e, int floor, RequestType type) {
    if (!e) return ELEV_ERR_INVALID;
    if (floor < 0 || floor >= MAX_FLOORS) return ELEV_ERR_INVALID;

    switch (type) {
        case REQ_CALL_UP:
            if (e->call_up[floor]) return ELEV_DUPLICATE;
            e->call_up[floor] = true;
            return ELEV_OK;

        case REQ_CALL_DOWN:
            if (e->call_down[floor]) return ELEV_DUPLICATE;
            e->call_down[floor] = true;
            return ELEV_OK;

        case REQ_INSIDE:
            if (e->inside[floor]) return ELEV_DUPLICATE;
            e->inside[floor] = true;
            return ELEV_OK;

        default:
            return ELEV_ERR_INVALID;
    }
}

/* 內呼 => 直接加進該電梯排程 */
int Elevator_push_inside_request(Elevator* e, int dest_floor, int client_id) {
    (void)client_id; /* we don't store client_id in flag-based model */
    return elevator_add_request_flag(e, dest_floor, REQ_INSIDE);
}

/* pick_next_target_flag: locate next target according to SCAN-like policy.
 * If found returns 1 and sets e->target_floor (and maybe changes direction);
 * else returns 0 (no request).
 */
PickResult pick_next_target_flag(Elevator* e) {
    if (!e) return PICK_ERROR;
    int cur = e->current_floor;

    if (e->direction == DIR_UP) {
        for (int f = cur + 1; f < MAX_FLOORS; ++f) {
            if (has_request_on_floor(e, f)) {
                e->target_floor = f;
                return PICK_TARGET_SET;
            }
        }
        for (int f = cur - 1; f >= 0; --f) {
            if (has_request_on_floor(e, f)) {
                e->direction = DIR_DOWN;
                e->target_floor = f;
                return PICK_TARGET_SET;
            }
        }
    } else if (e->direction == DIR_DOWN) {
        for (int f = cur - 1; f >= 0; --f) {
            if (has_request_on_floor(e, f)) {
                e->target_floor = f;
                return PICK_TARGET_SET;
            }
        }
        for (int f = cur + 1; f < MAX_FLOORS; ++f) {
            if (has_request_on_floor(e, f)) {
                e->direction = DIR_UP;
                e->target_floor = f;
                return PICK_TARGET_SET;
            }
        }
    } else {
        /* idle: skip current floor (offset starts at 1) */
        for (int offset = 1; offset < MAX_FLOORS; ++offset) {
            int up = cur + offset;
            int down = cur - offset;
            if (up < MAX_FLOORS && has_request_on_floor(e, up)) {
                e->direction = DIR_UP;
                e->target_floor = up;
                return PICK_TARGET_SET;
            }
            if (down >= 0 && has_request_on_floor(e, down)) {
                e->direction = DIR_DOWN;
                e->target_floor = down;
                return PICK_TARGET_SET;
            }
        }
    }

    return PICK_NONE;
}

/* ---------------------------
   Elevator implementation
   --------------------------- */

/* 初始化 */
void Elevator_init(Elevator* e, int id, int start_floor) {
    if (!e) return;
    e->id = id;
    e->current_floor = start_floor;
    e->target_floor = -1;
    e->task_state = TASK_IDLE;
    e->door_timer_s = 0.0;
    e->speed_fps = DEFAULT_SPEED_FPS;
    e->direction = DIR_NONE;
    /* clear flags */
    for (int f = 0; f < MAX_FLOORS; ++f) {
        e->call_up[f] = false;
        e->call_down[f] = false;
        e->inside[f] = false;
    }
    if (id >= 0 && id < MAX_ELEVATORS) g_accum_time[id] = 0.0;
}

/* 電梯狀態機 */
void Elevator_step(Elevator* e, double dt_seconds) {
    if (!e) return;
    /*printf("[ELEV_STEP] E%d tick dt=%.3f state=%d cur=%d tgt=%d dir=%d door=%d door_timer=%.2f up=%d down=%d\n",
        e->id, dt_seconds, e->task_state, e->current_floor, e->target_floor,
        e->direction, e->door_open, e->door_timer_s, e->stops.up_count, e->stops.down_count);*/
    if (dt_seconds <= 0.0) return;

    switch (e->task_state) {
    case TASK_DOOR_OPENING:  // 當前狀態：正在開門
        printf("[ELEV_STATUS] E%d STATE: DOOR_OPENING (TASK_DOOR_OPENING) - floor=%d target=%d\n",
               e->id, e->current_floor, e->target_floor);
        // 可插入延遲或動畫
        // 目前直接打開
        e->task_state = TASK_DOOR_OPEN;
        e->door_timer_s = DEFAULT_DOOR_OPEN_S;
        return;

    case TASK_DOOR_OPEN:  // 當前狀態：門正開著，計時，計時到了改為關閉
        /*printf("[ELEV_STATUS] E%d STATE: DOOR_OPEN (TASK_DOOR_OPEN) - remaining_sec=%.2f floor=%d\n",
               e->id, e->door_timer_s, e->current_floor);*/

        e->door_timer_s -= dt_seconds;
        // 若時間到 => 取得下個停靠
        if (e->door_timer_s <= 0.0) {
            e->door_timer_s = 0.0;
            printf("[ELEV_STATUS] E%d ACTION: door closed, selecting next target (door_timer expired)\n", e->id);
            remove_served_flags_on_arrival(e, e->current_floor, e->direction);

            /* pick next target according to flags */
            if (pick_next_target_flag(e)) {
                /* ensure direction consistent with target */
                if (e->target_floor > e->current_floor) e->direction = DIR_UP;
                else if (e->target_floor < e->current_floor) e->direction = DIR_DOWN;
                else e->direction = DIR_NONE;
                e->task_state = TASK_PREPARE;
            } else {
                e->task_state = TASK_IDLE;
                e->direction = DIR_NONE;
            }
        }
        return;

    case TASK_DOOR_CLOSING:  // 當前狀態：正在關門
        printf("[ELEV_STATUS] E%d STATE: DOOR_CLOSING (TASK_DOOR_CLOSING) - floor=%d\n",
               e->id, e->current_floor);
        // 可插入延遲或動畫
        // 目前直接關閉
        remove_served_flags_on_arrival(e, e->current_floor, e->direction);
        if (pick_next_target_flag(e)) {
            if (e->target_floor > e->current_floor) e->direction = DIR_UP;
            else if (e->target_floor < e->current_floor) e->direction = DIR_DOWN;
            else e->direction = DIR_NONE;
            e->task_state = TASK_PREPARE;
        } else {
            e->task_state = TASK_IDLE;
            e->direction = DIR_NONE;
        }
        return;

    case TASK_PREPARE:  // 當前狀態：準備移動
        /*printf("[ELEV_STATUS] E%d STATE: PREPARE (TASK_PREPARE) - cur=%d target=%d\n",
               e->id, e->current_floor, e->target_floor);*/
        // 若直接收到相同樓層排程 => 立即開門
        if (e->target_floor == e->current_floor) {
            // 直接當作已抵達 => 開門 & 移除排程
            e->task_state = TASK_DOOR_OPENING;
            remove_served_flags_on_arrival(e, e->current_floor, e->direction);
            return;
        }

        /* 確保 direction 與 target 一致 */
        if (e->target_floor > e->current_floor)
            e->direction = DIR_UP;
        else if (e->target_floor < e->current_floor)
            e->direction = DIR_DOWN;
        else
            e->direction = DIR_NONE;

        e->task_state = TASK_MOVING;
        // 直接進入移動所以不 break

    case TASK_MOVING:  // 當前狀態：正在移動
        double speed = (e->speed_fps > 0.0) ? e->speed_fps : DEFAULT_SPEED_FPS;
        double time_per_floor = 1.0 / speed;
        /*printf("[ELEV_STATUS] E%d STATE: MOVING (TASK_MOVING) - speed=%.3f floors/sec time_per_floor=%.3f cur=%d target=%d\n",
               e->id, speed, time_per_floor, e->current_floor, e->target_floor);*/

        if (e->id >= 0 && e->id < MAX_ELEVATORS) {
            g_accum_time[e->id] += dt_seconds;
            while (g_accum_time[e->id] >= time_per_floor) {
                g_accum_time[e->id] -= time_per_floor;

                if (e->current_floor < e->target_floor) {
                    e->current_floor++;
                    printf("[ELEV_STEP] E%d moved up: %d -> %d\n", e->id, e->current_floor - 1, e->current_floor);
                } else if (e->current_floor > e->target_floor) {
                    e->current_floor--;
                    printf("[ELEV_STEP] E%d moved down: %d -> %d\n", e->id, e->current_floor + 1, e->current_floor);
                }

                // 到達目標 => break 掉 while & 準備開門
                if (e->current_floor == e->target_floor) {
                    printf("[ELEV_STEP] E%d ARRIVED at %d -> opening door\n", e->id, e->current_floor);
                    break;
                }
            }
        } else {
            /* fallback：每 step 最多移一層（保守行為） */
            if (e->current_floor < e->target_floor) {
                e->current_floor++;
                printf("[ELEV_STEP] E%d (fallback) moved up to %d\n", e->id, e->current_floor);
            } else if (e->current_floor > e->target_floor) {
                e->current_floor--;
                printf("[ELEV_STEP] E%d (fallback) moved down to %d\n", e->id, e->current_floor);
            }
        }

        // 到達目標 => 狀態設為到達
        if (e->current_floor == e->target_floor) {
            e->task_state = TASK_ARRIVED;
        }
        return;

    case TASK_ARRIVED:  // 當前狀態：抵達目標樓層 => 開門 & 移除排程
        printf("[ELEV_STATUS] E%d STATE: ARRIVED (TASK_ARRIVED) - floor=%d, opening door and removing stops\n",
               e->id, e->current_floor);
        e->task_state = TASK_DOOR_OPENING;
        e->door_timer_s = DEFAULT_DOOR_OPEN_S;
        // 移除排程
        remove_served_flags_on_arrival(e, e->current_floor, e->direction);
        return;

    case TASK_IDLE:  // 當前狀態：閒置
    default:
        /*printf("[ELEV_STATUS] E%d STATE: IDLE (TASK_IDLE) - up_count=%d down_count=%d\n",
               e->id, e->stops.up_count, e->stops.down_count);*/
        // 如果有排程 => 選方向 & 進準備狀態
        if (pick_next_target_flag(e)) {
            /* ensure direction consistent */
            if (e->target_floor > e->current_floor)
                e->direction = DIR_UP;
            else if (e->target_floor < e->current_floor)
                e->direction = DIR_DOWN;
            else
                e->direction = DIR_NONE;
            e->task_state = TASK_PREPARE;
            printf("[ELEV_STATUS] E%d IDLE -> chose target=%d dir=%d\n", e->id, e->target_floor, e->direction);
        } else {
            /* remain idle */
            e->task_state = TASK_IDLE;
        }
        return;
    } /* end switch */
}

/* status line */
const char* Elevator_status_line(Elevator* e, char* out, int out_size) {
    if (!e || !out || out_size <= 0) return NULL;
    const char* dstr = (e->direction == DIR_UP) ? "UP" : (e->direction == DIR_DOWN) ? "DOWN" : "NONE";
    int off = 0;
    off += snprintf(out+off, (off < out_size)? out_size-off : 0, "[E%d] cur=%d tgt=%d dir=%s | up:", e->id, e->current_floor, e->target_floor, dstr);
    for (int f = 0; f < MAX_FLOORS && off < out_size-8; ++f) {
        if (e->call_up[f]) off += snprintf(out+off, out_size-off, "%d,", f);
    }
    off += snprintf(out+off, (off < out_size)? out_size-off : 0, " down:");
    for (int f = 0; f < MAX_FLOORS && off < out_size-8; ++f) {
        if (e->call_down[f]) off += snprintf(out+off, out_size-off, "%d,", f);
    }
    off += snprintf(out+off, (off < out_size)? out_size-off : 0, " inside:");
    for (int f = 0; f < MAX_FLOORS && off < out_size-8; ++f) {
        if (e->inside[f]) off += snprintf(out+off, out_size-off, "%d,", f);
    }
    return out;
}
