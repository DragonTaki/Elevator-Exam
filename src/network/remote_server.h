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

/**
 * @file remote_server.h
 * @brief Networked elevator server interface.
 *
 * This header exposes the public entrypoint for running the networked elevator server.
 * The server implements a tick-driven simulation loop, a request queue, and a text-based
 * protocol for clients (ROLE declaration, CALL, STATUS, WATCH, etc).
 *
 * 中文說明：
 * 這個 header 提供執行遠端 server 的公開介面。實作細節（例如 client 管理、select 事件迴圈、
 * request queue）都在 remote_server.c，呼叫端只需要呼叫 run_remote_server() 即可啟動整個服務。
 */

/* Default port if caller passes 0 or negative. */
#define REMOTE_SERVER_DEFAULT_PORT 5555

/* Simulation tick in milliseconds — determines elevator step frequency.
 * (Server may choose a different tick, but public API documents expected tick behavior.)
 */
#define REMOTE_SERVER_DEFAULT_TICK_MS 300

/**
 * run_remote_server
 *
 * Start the networked elevator server and block until the server stops (process exit).
 *
 * Parameters:
 *   elevators      - pointer to an array of Elevator structures that the server will manage.
 *                    The caller retains ownership of this array; the server will read and
 *                    modify Elevator entries during runtime.
 *   elevator_count - number of elevators in the elevators array (must be >0 and <= MAX_ELEVATORS).
 *   port           - TCP port to listen on. If <=0, REMOTE_SERVER_DEFAULT_PORT is used.
 *
 * Notes:
 *  - This function is blocking: it runs the server loop using select() and tick-driven simulation.
 *  - The server expects the elevators[] array to be initialized (e.g., via Elevator_init()) before calling.
 *  - The server uses the RequestQueue APIs (rq_init / rq_push / rq_pop) implemented in core/elevator.c.
 *
 * Return:
 *   This function does not return under normal operation (it runs until the process exits).
 *   If initialization fails it may return immediately; in that case a non-zero return value
 *   will indicate failure.
 *
 * 中文說明：
 * 啟動遠端 server 的主函式；此函式會阻塞執行並處理連線、指令、以及以 tick 為驅動的模擬。
 * 呼叫者需在呼叫前初始化 elevators（例如使用 Elevator_init()）。port 若 <= 0 則使用預設 5555。
 */
int run_remote_server(Elevator elevators[], int elevator_count, int port);

#ifdef __cplusplus
}
#endif

#endif /* REMOTE_SERVER_H */
