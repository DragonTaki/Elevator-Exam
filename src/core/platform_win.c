/* ----- ----- ----- ----- */
// platform_win.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#ifdef _WIN32

#include "platform.h"
#include <stdlib.h>
#include <windows.h>

// Mutex
struct PlatformMutex {
    CRITICAL_SECTION cs;
};

PlatformMutex* platform_mutex_create(void){
    PlatformMutex* m = malloc(sizeof(PlatformMutex));
    InitializeCriticalSection(&m->cs);
    return m;
}

void platform_mutex_destroy(PlatformMutex* m){
    DeleteCriticalSection(&m->cs);
    free(m);
}

void platform_mutex_lock(PlatformMutex* m){
    EnterCriticalSection(&m->cs);
}

void platform_mutex_unlock(PlatformMutex* m){
    LeaveCriticalSection(&m->cs);
}

// CondVar (ConditionVariable)
struct PlatformCond {
    CONDITION_VARIABLE cv;
};

PlatformCond* platform_cond_create(void){
    PlatformCond* c = malloc(sizeof(PlatformCond));
    InitializeConditionVariable(&c->cv);
    return c;
}

void platform_cond_destroy(PlatformCond* c){
    free(c); // no delete function for CONDITION_VARIABLE
}

void platform_cond_wait(PlatformCond* c, PlatformMutex* m){
    SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
}

void platform_cond_signal(PlatformCond* c){
    WakeConditionVariable(&c->cv);
}

void platform_cond_broadcast(PlatformCond* c){
    WakeAllConditionVariable(&c->cv);
}

// Threads
struct PlatformThread {
    HANDLE handle;
};

static DWORD WINAPI _platform_thread_trampoline(LPVOID arg){
    PlatformThreadFn fn = ((PlatformThreadFn*)arg)[0];
    void* realArg      = ((void**)arg)[1];
    free(arg);
    fn(realArg);
    return 0;
}

PlatformThread* platform_thread_create(PlatformThreadFn func, void* arg){
    PlatformThread* t = malloc(sizeof(PlatformThread));

    void** pack = malloc(sizeof(void*) * 2);
    pack[0] = (void*)func;
    pack[1] = arg;

    t->handle = CreateThread(
        NULL, 0,
        _platform_thread_trampoline,
        pack,
        0, NULL
    );
    return t;
}

void platform_thread_join(PlatformThread* t){
    WaitForSingleObject(t->handle, INFINITE);
    CloseHandle(t->handle);
    free(t);
}

// Time
void platform_sleep_ms(int ms){
    Sleep(ms);
}

long long platform_time_ms(void){
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (long long)(uli.QuadPart / 10000); // to ms
}

#endif // _WIN32
