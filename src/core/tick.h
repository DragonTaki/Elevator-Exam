/* ----- ----- ----- ----- */
// tick.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/12/01
// Update Date: 2025/12/01
// Version: v1.0
/* ----- ----- ----- ----- */

#ifndef TICK_H
#define TICK_H

#include <windows.h>
#include "elevator.h"
#include "request_queue.h"

/* Initialize tick module.
 * - elevators: pointer to elevator array (not copied)
 * - elevator_count: number of elevators
 * - pending: pointer to RequestQueue (not copied)
 * - tick_ms: tick interval in milliseconds (e.g. SIM_TICK_MS)
 *
 * Returns 0 on success.
 */
int remote_tick_init(Elevator elevators[], int elevator_count, RequestQueue* pending, int tick_ms, int assigned_drop_array[]);

/* Try to perform tick processing if now_ms has advanced enough.
 * - now_ms: current time in ms (GetTickCount on Windows). If <=0, function will call GetTickCount().
 * Returns 1 if a tick was executed, 0 if not (too early), -1 on error.
 */
int remote_tick_try_do(DWORD now_ms);

/* Force a single tick regardless of timing (for testing). */
int remote_tick_do_now(void);

/* Shutdown/cleanup the tick module (free state). */
void remote_tick_shutdown(void);

/* Controls: if set to 1, tick prints elevator status each tick even if unchanged.
 * By default prints only when changed (to reduce spam).
 */
extern int REMOTE_TICK_ALWAYS_PRINT_STATUS;

#endif /* TICK_H */
