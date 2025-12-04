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
#include <string.h>
#include <windows.h>

// =====================
// Mutex API
// =====================

struct PlatformMutex {
    CRITICAL_SECTION cs;
};

/* 建立互斥鎖（保護多執行緒共享資源）*/
PlatformMutex* platform_mutex_create(void){
    PlatformMutex* m = malloc(sizeof(PlatformMutex));
    InitializeCriticalSection(&m->cs);
    return m;
}

/* 銷毀互斥鎖（釋放資源）*/
void platform_mutex_destroy(PlatformMutex* m){
    DeleteCriticalSection(&m->cs);
    free(m);
}

/* 上鎖（進入關鍵區）*/
void platform_mutex_lock(PlatformMutex* m){
    EnterCriticalSection(&m->cs);
}

/* 解鎖（離開關鍵區）*/
void platform_mutex_unlock(PlatformMutex* m){
    LeaveCriticalSection(&m->cs);
}

// =====================
// Condition Variable API
// =====================

struct PlatformCond {
    CONDITION_VARIABLE cv;
};

// 建立條件變數（用於執行緒同步）
PlatformCond* platform_cond_create(void){
    PlatformCond* c = malloc(sizeof(PlatformCond));
    InitializeConditionVariable(&c->cv);
    return c;
}

// 銷毀條件變數（僅釋放記憶體）
void platform_cond_destroy(PlatformCond* c){
    free(c); // no delete function for CONDITION_VARIABLE
}

// 等待條件成立（需搭配互斥鎖）
void platform_cond_wait(PlatformCond* c, PlatformMutex* m){
    SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
}

// 單一喚醒等待中的執行緒
void platform_cond_signal(PlatformCond* c){
    WakeConditionVariable(&c->cv);
}

// 喚醒所有等待中的執行緒
void platform_cond_broadcast(PlatformCond* c){
    WakeAllConditionVariable(&c->cv);
}

// =====================
// Thread API
// =====================

struct PlatformThread {
    HANDLE handle;
};

// 執行緒啟動入口（橋接使用者函式）
static DWORD WINAPI _platform_thread_trampoline(LPVOID arg){
    //   pack[0] = 函式指標 PlatformThreadFn
    //   pack[1] = 傳給該函式的參數 void*
    PlatformThreadFn fn = ((PlatformThreadFn*)arg)[0];
    void* realArg       = ((void**)arg)[1];
    free(arg);
    fn(realArg);
    return 0;
}

// 建立新執行緒，傳入執行函式與參數
PlatformThread* platform_thread_create(PlatformThreadFn func, void* arg){
    PlatformThread* t = malloc(sizeof(PlatformThread));

    void** pack = malloc(sizeof(void*) * 2);
    pack[0] = (void*)func;                   // 以 void* 形式存放函式指標
    pack[1] = arg;                           // 存放傳入的參數指標

    // 呼叫 Windows API 建立執行緒
    t->handle = CreateThread(
        NULL, 0,
        _platform_thread_trampoline,
        pack,
        0, NULL
    );
    return t;
}

// 等待執行緒結束並清理資源
void platform_thread_join(PlatformThread* t){
    WaitForSingleObject(t->handle, INFINITE);  // 阻塞等到執行緒結束
    CloseHandle(t->handle);
    free(t);
}

// =====================
// Time / Sleep
// =====================

// 睡眠指定毫秒（阻塞）
void platform_sleep_ms(int ms){
    Sleep(ms);
}

// 回傳目前系統時間（以毫秒為單位）
long long platform_time_ms(void){
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;

    // Initialize frequency once
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }

    QueryPerformanceCounter(&counter);

    // counter / freq = seconds
    // convert to milliseconds
    return (long long)(counter.QuadPart * 1000 / freq.QuadPart);
}

// =====================
// String
// =====================

// 比較兩個字串（忽略大小寫）
int platform_stricmp(const char* s1, const char* s2) {
    return _stricmp(s1, s2);
}

// ---------------------
// Socket API
// ---------------------

int platform_socket_init(void) {
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2,2), &wsa);
    if (rc != 0) {
        return rc; // WSA error code
    }
    return 0;
}

void platform_socket_cleanup(void) {
    WSACleanup();
}

void platform_socket_close(platform_socket_t s) {
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
}

int platform_socket_last_error(void) {
    return WSAGetLastError();
}

#endif // _WIN32
