/* ----- ----- ----- ----- */
// platform_posix.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#ifndef _WIN32

#include "platform.h"
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

// =====================
// Mutex API
// =====================

struct PlatformMutex {
    pthread_mutex_t m;
};

PlatformMutex* platform_mutex_create(void){
    PlatformMutex* x = malloc(sizeof(PlatformMutex));
    pthread_mutex_init(&x->m, NULL);
    return x;
}

void platform_mutex_destroy(PlatformMutex* x){
    pthread_mutex_destroy(&x->m);
    free(x);
}

void platform_mutex_lock(PlatformMutex* x){
    pthread_mutex_lock(&x->m);
}

void platform_mutex_unlock(PlatformMutex* x){
    pthread_mutex_unlock(&x->m);
}

// =====================
// Condition Variable API
// =====================

struct PlatformCond {
    pthread_cond_t c;
};

PlatformCond* platform_cond_create(void){
    PlatformCond* x = malloc(sizeof(PlatformCond));
    pthread_cond_init(&x->c, NULL);
    return x;
}

void platform_cond_destroy(PlatformCond* x){
    pthread_cond_destroy(&x->c);
    free(x);
}

void platform_cond_wait(PlatformCond* c, PlatformMutex* m){
    pthread_cond_wait(&c->c, &m->m);
}

void platform_cond_signal(PlatformCond* c){
    pthread_cond_signal(&c->c);
}

void platform_cond_broadcast(PlatformCond* c){
    pthread_cond_broadcast(&c->c);
}

// =====================
// Thread API
// =====================

struct PlatformThread {
    pthread_t th;
};

PlatformThread* platform_thread_create(PlatformThreadFn fn, void* arg){
    PlatformThread* t = malloc(sizeof(PlatformThread));
    pthread_create(&t->th, NULL, fn, arg);
    return t;
}

void platform_thread_join(PlatformThread* t){
    pthread_join(t->th, NULL);
    free(t);
}

// =====================
// Time / Sleep
// =====================

void platform_sleep_ms(int ms){
    usleep(ms * 1000);
}

long long platform_time_ms(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL +
           (long long)ts.tv_nsec / 1000000LL;
}

// =====================
// String
// =====================

int platform_stricmp(const char* s1, const char* s2) {
    return strcasecmp(s1, s2);
}

// =====================
// Socket
// =====================

int platform_socket_init(void) {
    // POSIX 不需要特別初始化
    return 0;
}

void platform_socket_cleanup(void) {
    // POSIX 不需要特別清理
}

void platform_socket_close(platform_socket_t s) {
    if (s >= 0) {
        close(s);
    }
}

int platform_socket_last_error(void) {
    return errno;
}

#endif
