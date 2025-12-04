/* ----- ----- ----- ----- */
// server_events.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#include "server_events.h"
#include <stdlib.h>
#include <string.h>

#include "platform.h"

// 單向鏈結串列
static ServerEvent* g_head = NULL;
static ServerEvent* g_tail = NULL;
static int g_count = 0;
static int g_shutdown = 0;

static PlatformMutex* g_mutex = NULL;
static PlatformCond*  g_cond  = NULL;

/* 初始化事件佇列：建立 mutex/condvar，清除狀態 */
int server_events_init(void)
{
    g_mutex = platform_mutex_create();
    g_cond  = platform_cond_create();
    g_head = g_tail = NULL;
    g_count = 0;
    g_shutdown = 0;  // 是否進入關閉狀態
    return 0;
}

/* 遞送關閉事件，喚醒等待中的執行緒，並設置 shutdown 標記 */
void server_events_shutdown(void)
{
    platform_mutex_lock(g_mutex);
    g_shutdown = 1;

    // 建立一個新事件
    ServerEvent* ev = (ServerEvent*)calloc(1, sizeof(ServerEvent));
    if(ev){
        ev->type = EVT_SHUTDOWN;
        // tail 非空 => 接在後面
        // tail 空 => 空佇列 => 直接為第一個
        if(g_tail) g_tail->next = ev;
        else g_head = ev;
        g_tail = ev;
        g_count++;
    }
    // 喚醒所有呼叫 server_events_pop() 並在等待中的執行緒
    platform_cond_broadcast(g_cond);
    
    platform_mutex_unlock(g_mutex);
}

/* 將事件加入佇列中 */
static int push_event(ServerEvent* ev)
{
    if(!ev) return -1;
    ev->next = NULL;
    platform_mutex_lock(g_mutex);

    if(g_shutdown){
        platform_mutex_unlock(g_mutex);
        free(ev);
        return -1;
    }
    if(g_tail) g_tail->next = ev;
    else g_head = ev;
    g_tail = ev;
    g_count++;
    // 喚醒單一等待中的執行緒
    platform_cond_signal(g_cond);

    platform_mutex_unlock(g_mutex);
    return 0;
}

/* 推入外呼事件（樓層 + 方向 + client id） */
int server_events_push_outside(int floor, int direction, int client_id)
{
    ServerEvent* ev = (ServerEvent*)calloc(1, sizeof(ServerEvent));
    if(!ev) return -1;
    ev->type = EVT_OUTSIDE_CALL;
    ev->v.outside_call.floor = floor;
    ev->v.outside_call.direction = direction;
    ev->v.outside_call.client_id = client_id;
    return push_event(ev);
}

/* 推入內呼事件（電梯 id + 目的樓層 + client id） */
int server_events_push_inside(int elevator_id, int dest_floor, int client_id)
{
    ServerEvent* ev = (ServerEvent*)calloc(1, sizeof(ServerEvent));
    if(!ev) return -1;
    ev->type = EVT_INSIDE_CALL;
    ev->v.inside_call.elevator_id = elevator_id;
    ev->v.inside_call.dest_floor = dest_floor;
    ev->v.inside_call.client_id = client_id;
    return push_event(ev);
}

/* 推入警衛指令事件（強制移動、額外資訊等） */
int server_events_push_guard(int elevator_id, int floor, int force, int client_id, const char* extra)
{
    ServerEvent* ev = (ServerEvent*)calloc(1, sizeof(ServerEvent));
    if(!ev) return -1;
    ev->type = EVT_GUARD_COMMAND;
    ev->v.guard_cmd.elevator_id = elevator_id;
    ev->v.guard_cmd.floor = floor;
    ev->v.guard_cmd.force = force;
    ev->v.guard_cmd.client_id = client_id;
    return push_event(ev);
}

/* 阻塞式取出事件 */
// 若事件為空 => 等待
// 若 shutdown => 回傳錯誤（-1）
int server_events_pop(ServerEvent** out_event)
{
    if(!out_event) return -1;
    platform_mutex_lock(g_mutex);

    // 沒有事件 + 沒有要關閉 => 等
    while(g_count == 0 && !g_shutdown){
        platform_cond_wait(g_cond, g_mutex);
    }

    if(g_count == 0 && g_shutdown){
        platform_mutex_unlock(g_mutex);
        return -1;
    }

    ServerEvent* ev = g_head;
    if(!ev){
        platform_mutex_unlock(g_mutex);
        return -1;
    }
    g_head = ev->next;
    if(g_head == NULL) g_tail = NULL;
    ev->next = NULL;
    g_count--;

    platform_mutex_unlock(g_mutex);
    *out_event = ev;
    return 0;
}

/* 非阻塞取出事件 */
int server_events_try_pop(ServerEvent** out_event)
{
    if(!out_event) return -1;
    platform_mutex_lock(g_mutex);

    if(g_count == 0){
        platform_mutex_unlock(g_mutex);
        return -1;
    }

    ServerEvent* ev = g_head;
    if(!ev){
        platform_mutex_unlock(g_mutex);
        return -1;
    }
    g_head = ev->next;
    if(g_head == NULL) g_tail = NULL;
    ev->next = NULL;
    g_count--;

    platform_mutex_unlock(g_mutex);
    *out_event = ev;
    return 0;
}

/* 釋放事件記憶體 */
void server_events_free(ServerEvent* ev)
{
    if(!ev) return;
    free(ev);
}

/* 查詢目前佇列內事件數量 */
int server_events_count(void)
{
    // 避免讀 g_count 時被另一個執行緒同時 push/pop
    platform_mutex_lock(g_mutex);

    int c = g_count;

    platform_mutex_unlock(g_mutex);
    return c;
}
