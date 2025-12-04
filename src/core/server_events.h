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

/* 伺服器事件種類 */
typedef enum {
    EVT_NONE = 0,
    EVT_OUTSIDE_CALL,   // 外呼：呼叫樓層、呼叫方向、client id
    EVT_INSIDE_CALL,    // 內呼：電梯 ID、目標樓層、client id
    EVT_GUARD_COMMAND,  // Guard 指令（force assign, maintenance, etc.）: 使用 payload
    EVT_SHUTDOWN        // 用來通知 consumer 停止
} ServerEventType;

/* 警衛指令 */
typedef struct {
    int elevator_id;
    int floor;
    int force;        // 強制跳過排程
    int client_id;
} GuardCommand;

// 通用事件結構
typedef struct ServerEvent {
    ServerEventType type;
    union {
        struct {
            int floor;      // 在幾樓
            int direction;  // 按上／按下
            int client_id;
        } outside_call;
        struct {
            int elevator_id;  // 哪台電梯
            int dest_floor;   // 按幾樓
            int client_id;
        } inside_call;
        GuardCommand guard_cmd;
    } v;

    // 串列節點
    struct ServerEvent* next;
} ServerEvent;

int server_events_init(void);
void server_events_shutdown(void);

int server_events_push_outside(int floor, int direction, int client_id);
int server_events_push_inside(int elevator_id, int dest_floor, int client_id);
int server_events_push_guard(int elevator_id, int floor, int force, int client_id, const char* extra);

int server_events_pop(ServerEvent** out_event);      // 阻塞式
int server_events_try_pop(ServerEvent** out_event);  // 非阻塞式

void server_events_free(ServerEvent* ev);

int server_events_count(void);

#ifdef __cplusplus
}
#endif

#endif // SERVER_EVENTS_H
