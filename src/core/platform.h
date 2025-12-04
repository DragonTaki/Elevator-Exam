/* ----- ----- ----- ----- */
// platform.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// =====================
// Socket basic type
// =====================

#ifdef _WIN32
    #include <winsock2.h>
    typedef SOCKET platform_socket_t;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    typedef int platform_socket_t;
#endif

// =====================
// Common Types
// =====================

typedef struct PlatformMutex PlatformMutex;
typedef struct PlatformCond  PlatformCond;
typedef struct PlatformThread PlatformThread;

// =====================
// Mutex API
// =====================

PlatformMutex* platform_mutex_create(void);
void platform_mutex_destroy(PlatformMutex* m);
void platform_mutex_lock(PlatformMutex* m);
void platform_mutex_unlock(PlatformMutex* m);

// =====================
// Condition Variable API
// =====================

PlatformCond* platform_cond_create(void);
void platform_cond_destroy(PlatformCond* c);
void platform_cond_wait(PlatformCond* c, PlatformMutex* m);
void platform_cond_signal(PlatformCond* c);
void platform_cond_broadcast(PlatformCond* c);

// =====================
// Thread API
// =====================

typedef void* (*PlatformThreadFn)(void* arg);

PlatformThread* platform_thread_create(PlatformThreadFn func, void* arg);
void platform_thread_join(PlatformThread* t);

// =====================
// Time / Sleep
// =====================

void platform_sleep_ms(int ms);
long long platform_time_ms(void);  // monotonic if possible

// =====================
// String
// =====================

int platform_stricmp(const char* s1, const char* s2);

// =====================
// Socket
// =====================

// 初始化網路程式庫 (Windows 需要, Linux 為 no-op)
int platform_socket_init(void);

// 清理 (Windows 需要, Linux 為 no-op)
void platform_socket_cleanup(void);

// 關閉 socket
void platform_socket_close(platform_socket_t s);

// 取得錯誤碼 (回傳平台相關錯誤)
int platform_socket_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
