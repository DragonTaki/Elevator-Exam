/* ----- ----- ----- ----- */
// server_core.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/29
// Update Date: 2025/11/29
// Version: v1.0
/* ----- ----- ----- ----- */

#ifndef SERVER_CORE_H
#define SERVER_CORE_H

#include "elevator.h"
#include "platform.h"
#include "request_queue.h"

/* Tick default used by internal core (same as in server_core.c). */
#define SERVER_CORE_DEFAULT_TICK_SECONDS 0.1

#ifdef __cplusplus
extern "C" {
#endif

/* Status snapshot callback signature. If set, called each publish tick with a
 * multi-line textual snapshot (null-terminated). The callback must not block
 * for long; it will be executed from the core thread.
 */
typedef void (*server_core_status_cb_t)(const char* snapshot);

/* Initialize server core. Must be called before start.
 * - elevator_count: number of elevators to initialize (clamped 1..MAX_ELEVATORS).
 * Returns 0 on success, non-zero on error.
 */
int server_core_init(int elevator_count);

/* Set optional status callback (may be NULL to clear). If set while the core
 * thread is running, the callback pointer is updated atomically (but the callback
 * itself must be thread-safe).
 */
void server_core_set_status_callback(server_core_status_cb_t cb);

/* Start core loop in a background thread. Returns 0 on success, non-zero on error.
 * server_core_init must have been called first.
 */
int server_core_start(void);

/* Stop core loop and join the thread. Safe to call multiple times. */
void server_core_stop(void);

/* Blocking join (if a caller wants to wait on thread termination). */
void server_core_join(void);

/* Request server core shutdown (fast-stop). This triggers server events shutdown
 * and causes the core loop to exit gracefully.
 */
void server_core_shutdown(void);

/* Access helpers (read-only pointers). The pointers refer to internal data and
 * are valid until server_core_shutdown is called. Concurrent reads are fine, but
 * callers must *not* mutate the returned data. If the caller needs consistent
 * snapshots across multiple values, implement external synchronization.
 */
RequestQueue* server_core_get_pending_queue(void);
Elevator* server_core_get_elevators(void);      /* pointer to array of elevators */
int server_core_get_elevator_count(void);

#ifdef __cplusplus
}
#endif

#endif /* SERVER_CORE_H */
