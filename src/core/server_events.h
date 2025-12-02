/* ----- ----- ----- ----- */
// server_events.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#ifndef SERVER_EVENTS_H
#define SERVER_EVENTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Event types: 增加新事件時在這裡擴充
typedef enum {
    EVT_NONE = 0,
    EVT_OUTSIDE_CALL,   // 外呼：樓層、方向、client id
    EVT_INSIDE_CALL,    // 車廂內按鈕：elevator id、dest floor、client id
    EVT_GUARD_COMMAND,  // Guard 指令（force assign, maintenance, etc.）: 使用 payload
    EVT_SHUTDOWN        // 用來通知 consumer 停止
} ServerEventType;

// Guard command payload (簡單範例，可按需擴充)
typedef struct {
    int elevator_id; // -1 = broadcast / apply to scheduler
    int floor;
    int force;       // 0/1
    int client_id;
    char extra[128]; // 可放 JSON string 或文字說明
} GuardCommand;

// 通用事件結構
typedef struct ServerEvent {
    ServerEventType type;
    union {
        struct {
            int floor;
            int direction; // DIR_UP / DIR_DOWN / DIR_NONE (int)
            int client_id;
        } outside_call;
        struct {
            int elevator_id;
            int dest_floor;
            int client_id;
        } inside_call;
        GuardCommand guard_cmd;
    } v;

    // internal use: next pointer for linked-list queue
    struct ServerEvent* next;
} ServerEvent;

// 初始化 / 清理
int server_events_init(void);
void server_events_shutdown(void);

// 推入事件（thread-safe）
// 返回 0 成功，非 0 失敗（如 OOM）
int server_events_push_outside(int floor, int direction, int client_id);
int server_events_push_inside(int elevator_id, int dest_floor, int client_id);
int server_events_push_guard(int elevator_id, int floor, int force, int client_id, const char* extra);

// consumer 端取事件（blocking / non-blocking）
// blocking: 等待事件或 EVT_SHUTDOWN
// 返回 0 成功並把 result 存入 *out (caller must free event via server_events_free if used)
// 返回 -1 表示隊列已經 shutdown 且無事件
int server_events_pop(ServerEvent** out_event); // blocking
int server_events_try_pop(ServerEvent** out_event); // non-blocking, returns 0 if popped, -1 if none

// 釋放 event 記憶體（若 pop 出來後 caller 不需要保留）
void server_events_free(ServerEvent* ev);

// 取得目前隊列長度（僅供監控）
int server_events_count(void);

#ifdef __cplusplus
}
#endif

#endif // SERVER_EVENTS_H
