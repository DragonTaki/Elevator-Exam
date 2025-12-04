/* ----- ----- ----- ----- */
// main.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/core/elevator.h"
#include "src/core/platform.h"

/* Forward: implemented in remote_server.c */
void run_remote_server(int port);

int main(int argc, char* argv[]) {
    /* configure elevator set here */
    const int elevator_count = 2;

    /* choose port (optional argument) */
    int port = 5555;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0) port = 5555;
    }

    server_core_init(elevator_count);

    /* 設定狀態回呼（未實作） */
    // server_core_set_status_callback(my_status_handler);

    /* 啟動核心執行緒（開始自動跑排程與電梯移動） */
    server_core_start();

    printf("=== Elevator Simulator (Server Mode) ===\n");
    printf("[MAIN] Starting server on port %d...\n", port);

    /* 啟動網路伺服器 => 負責接收指令 */
    run_remote_server(port);

    /* 結束 => 關閉核心 */
    server_core_stop();
    server_core_join();

    printf("[MAIN] Server stopped. Exiting.\n");
    return 0;
}
