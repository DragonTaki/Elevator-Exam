/* ----- ----- ----- ----- */
// elevator.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#ifndef ELEVATOR_H
#define ELEVATOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------
    Configurations
    --------------------------- */

#define MAX_ELEVATORS 8
#define MAX_REQUESTS 256

#ifndef MAX_FLOORS
#define MAX_FLOORS 100
#endif

/* ---------------------------
    Types and enums
    --------------------------- */

/* 電梯工作狀態 */
typedef enum {
    TASK_IDLE = 0,        // 閒置
    TASK_PREPARE,         // 已有下一個目標，還沒開始移動（設定 target，但尚未變 direction/moving）
    TASK_MOVING,          // 正在行駛（使用 g_accum_time 移動 current_floor）
    TASK_ARRIVED,         // 已抵達目標（剛到、剛開門前的過渡）
    TASK_DOOR_OPENING,    // 正在開門（若要做開門動畫，可用）
    TASK_DOOR_OPEN,       // 門開著（目前等候倒數關門）
    TASK_DOOR_CLOSING,    // 正在關門（短暫過渡）
    TASK_ERROR            // 錯誤或緊急停止
} TaskState;

/* 電梯運行方向 */
typedef enum {
    DIR_NONE = 0,
    DIR_UP = 1,
    DIR_DOWN = -1
} Direction;

/* 電梯請求種類 */
typedef enum {
    REQ_CALL_UP,
    REQ_CALL_DOWN,
    REQ_INSIDE
} RequestType;

/* 目標種類 */
typedef enum {
    PICK_ERROR = -1,    /* error (null ptr etc.) */
    PICK_NONE  = 0,     /* no target found */
    PICK_TARGET_SET = 1 /* found and target set in e->target_floor */
} PickResult;

/* 電梯排程請求結構 */
typedef struct {
    int floor;          // 請求樓層（UP/DOWN/INSIDE 都使用）
    RequestType type;   // REQ_CALL_UP / REQ_CALL_DOWN / REQ_INSIDE
    int source_id;      // 外呼來源（client/panel id），INSIDE 可設 -1
    int to_floor;       // INSIDE 用；外呼可設 -1
} PendingRequest;

/* 電梯資料結構 */
typedef struct {
    int id;                   // 電梯 ID
    int current_floor;        // 電梯當前所在樓層
    int target_floor;         // 電梯當前目標樓層
    TaskState task_state;     // 當前運行狀態
    double door_timer_s;      // 開門剩餘時間（秒）
    double speed_fps;         // 電梯運行速率
    Direction direction;      // 電梯運行方向
    bool call_up[MAX_FLOORS];
    bool call_down[MAX_FLOORS];
    bool inside[MAX_FLOORS];
} Elevator;

/* Elevator APIs */
void Elevator_init(Elevator* e, int id, int start_floor);
void Elevator_step(Elevator* e, double dt_seconds);
const char* Elevator_status_line(Elevator* e, char* out, int out_size);

/* Local stop management (single-writer expected) */
/* Add request into elevator stops lists using "elevator algorithm" insertion.
 * Returns 0 on success, -1 on failure (e.g. lists full).
 */
int elevator_add_request_flag(Elevator* e, int floor, RequestType type);

/* Helper to push an inside (car) request into appropriate list.
 * Equivalent to building ElevatorRequest and calling elevator_add_request.
 */
int Elevator_push_inside_request(Elevator* e, int dest_floor, int client_id);

/* Query helpers (optional) */
int elevator_has_stops(const Elevator* e);

#ifdef __cplusplus
}
#endif

#endif /* ELEVATOR_H */
