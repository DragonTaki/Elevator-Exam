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

// Simple singly-linked queue (head/tail) with mutex+condvar
static ServerEvent* g_head = NULL;
static ServerEvent* g_tail = NULL;
static int g_count = 0;
static int g_shutdown = 0;

static PlatformMutex* g_mutex = NULL;
static PlatformCond*  g_cond  = NULL;

int server_events_init(void)
{
    g_mutex = platform_mutex_create();
    g_cond  = platform_cond_create();
    g_head = g_tail = NULL;
    g_count = 0;
    g_shutdown = 0;
    return 0;
}

void server_events_shutdown(void)
{
    platform_mutex_lock(&g_mutex);
    g_shutdown = 1;
    // push a special EVT_SHUTDOWN to wake consumer(s)
    ServerEvent* ev = (ServerEvent*)calloc(1, sizeof(ServerEvent));
    if(ev){
        ev->type = EVT_SHUTDOWN;
        // push
        if(g_tail) g_tail->next = ev;
        else g_head = ev;
        g_tail = ev;
        g_count++;
    }
    platform_cond_broadcast(&g_cond);
    platform_mutex_unlock(&g_mutex);
}

// internal helper to push generic event
static int push_event(ServerEvent* ev)
{
    if(!ev) return -1;
    ev->next = NULL;
    platform_mutex_lock(&g_mutex);
    if(g_shutdown){
        platform_mutex_unlock(&g_mutex);
        free(ev);
        return -1;
    }
    if(g_tail) g_tail->next = ev;
    else g_head = ev;
    g_tail = ev;
    g_count++;
    platform_cond_signal(&g_cond);
    platform_mutex_unlock(&g_mutex);
    return 0;
}

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

int server_events_push_guard(int elevator_id, int floor, int force, int client_id, const char* extra)
{
    ServerEvent* ev = (ServerEvent*)calloc(1, sizeof(ServerEvent));
    if(!ev) return -1;
    ev->type = EVT_GUARD_COMMAND;
    ev->v.guard_cmd.elevator_id = elevator_id;
    ev->v.guard_cmd.floor = floor;
    ev->v.guard_cmd.force = force;
    ev->v.guard_cmd.client_id = client_id;
    if(extra){
        strncpy(ev->v.guard_cmd.extra, extra, sizeof(ev->v.guard_cmd.extra)-1);
        ev->v.guard_cmd.extra[sizeof(ev->v.guard_cmd.extra)-1] = '\0';
    }
    return push_event(ev);
}

// blocking pop: wait until event available or shutdown
int server_events_pop(ServerEvent** out_event)
{
    if(!out_event) return -1;
    platform_mutex_lock(&g_mutex);
    while(g_count == 0 && !g_shutdown){
        platform_cond_wait(&g_cond, &g_mutex);
    }
    if(g_count == 0 && g_shutdown){
        platform_mutex_unlock(&g_mutex);
        return -1;
    }
    ServerEvent* ev = g_head;
    if(!ev){
        platform_mutex_unlock(&g_mutex);
        return -1;
    }
    g_head = ev->next;
    if(g_head == NULL) g_tail = NULL;
    ev->next = NULL;
    g_count--;
    platform_mutex_unlock(&g_mutex);
    *out_event = ev;
    return 0;
}

// non-blocking try pop
int server_events_try_pop(ServerEvent** out_event)
{
    if(!out_event) return -1;
    platform_mutex_lock(&g_mutex);
    if(g_count == 0){
        platform_mutex_unlock(&g_mutex);
        return -1;
    }
    ServerEvent* ev = g_head;
    if(!ev){
        platform_mutex_unlock(&g_mutex);
        return -1;
    }
    g_head = ev->next;
    if(g_head == NULL) g_tail = NULL;
    ev->next = NULL;
    g_count--;
    platform_mutex_unlock(&g_mutex);
    *out_event = ev;
    return 0;
}

void server_events_free(ServerEvent* ev)
{
    if(!ev) return;
    free(ev);
}

int server_events_count(void)
{
    platform_mutex_lock(&g_mutex);
    int c = g_count;
    platform_mutex_unlock(&g_mutex);
    return c;
}
