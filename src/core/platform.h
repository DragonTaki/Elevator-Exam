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

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
