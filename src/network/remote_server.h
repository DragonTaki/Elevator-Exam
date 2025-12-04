/* ----- ----- ----- ----- */
// remote_server.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#ifndef REMOTE_SERVER_H
#define REMOTE_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/elevator.h" /* Elevator types and RequestQueue APIs */

/* Default port if caller passes 0 or negative. */
#define REMOTE_SERVER_DEFAULT_PORT 5555

/* Simulation tick in milliseconds — determines elevator step frequency.
 * (Server may choose a different tick, but public API documents expected tick behavior.)
 */
#define REMOTE_SERVER_DEFAULT_TICK_MS 300

void run_remote_server(int port);

#ifdef __cplusplus
}
#endif

#endif /* REMOTE_SERVER_H */
