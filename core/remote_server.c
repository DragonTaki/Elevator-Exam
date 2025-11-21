/* ----- ----- ----- ----- */
// remote_server.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#include "remote_server.h"
#include "scheduler.h"
#include "simulator.h"

#define RECV_BUF_SIZE 256

static void send_all(SOCKET client, const char* msg) {
    send(client, msg, (int)strlen(msg), 0);
}

static void send_status(SOCKET client, Elevator elevators[], int count) {
    char buf[1024];
    int offset = 0;

    for (int i = 0; i < count; ++i) {
        int n = snprintf(buf + offset, sizeof(buf) - offset,
            "[Elevator %d] Current floor: %d (target: %d)\r\n",
            elevators[i].id,
            elevators[i].current_floor,
            elevators[i].target_floor);

        if (n < 0 || n >= (int)(sizeof(buf) - offset)) {
            // 避免溢位，直接送出目前累積部分
            break;
        }
        offset += n;
    }

    if (offset > 0) {
        send(client, buf, offset, 0);
    }
}

void run_remote_server(Elevator elevators[], int elevator_count, int port) {
    WSADATA wsa;
    SOCKET listen_sock = INVALID_SOCKET;
    SOCKET client_sock = INVALID_SOCKET;
    struct sockaddr_in server_addr, client_addr;
    int client_len;

    printf("[SERVER] Initializing Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("[SERVER] WSAStartup failed: %d\n", WSAGetLastError());
        return;
    }

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        printf("[SERVER] socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons((u_short)port);

    if (bind(listen_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("[SERVER] bind() failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return;
    }

    if (listen(listen_sock, 1) == SOCKET_ERROR) {
        printf("[SERVER] listen() failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return;
    }

    printf("[SERVER] Listening on port %d...\n", port);

    while (1) {
        client_len = sizeof(client_addr);
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            printf("[SERVER] accept() failed: %d\n", WSAGetLastError());
            break;
        }

        printf("[SERVER] Client connected.\n");

        char recv_buf[RECV_BUF_SIZE];

        while (1) {
            int recv_len = recv(client_sock, recv_buf, RECV_BUF_SIZE - 1, 0);
            if (recv_len <= 0) {
                printf("[SERVER] Client disconnected.\n");
                break;
            }

            recv_buf[recv_len] = '\0';

            char cmd[16];
            int cur, tgt;

            if (sscanf(recv_buf, "%15s", cmd) != 1) {
                send_all(client_sock, "Invalid command\r\n");
                continue;
            }

            if (_stricmp(cmd, "STATUS") == 0) {
                // STATUS：顯示電梯訊息
                send_status(client_sock, elevators, elevator_count);
            } else if (_stricmp(cmd, "CALL") == 0) {
                // CALL：呼叫電梯
                // 目前實作為等待電梯跑完，再一次送所有訊息
                // client 發送一次指令 => 接收一次回饋
                if (sscanf(recv_buf, "%*s %d %d", &cur, &tgt) != 2) {
                    send_all(client_sock, "Usage: CALL <current> <target>\r\n");
                    continue;
                }

                // 把整個回應組在同一個 buffer 裡，一次送完
                char resp[512];
                int offset = 0;

                // 1. 基本訊息
                offset += snprintf(resp + offset, sizeof(resp) - offset,
                    "Calling elevator: current=%d, target=%d\r\n", cur, tgt);

                // 2. 指派電梯
                int idx = assign_elevator(cur, elevators, elevator_count);
                if (idx < 0) {
                    offset += snprintf(resp + offset, sizeof(resp) - offset, "No elevator available\r\n");

                    // 沒電梯可用 => 直接送出目前累積的內容
                    send(client_sock, resp, offset, 0);
                    continue;
                }

                Elevator *e = &elevators[idx];

                offset += snprintf(resp + offset, sizeof(resp) - offset, "Assigned to Elevator %d\r\n", e->id);

                // 3. 模擬電梯移動
                simulate(elevators, elevator_count);

                if (cur != tgt) {
                    Elevator_move(e, tgt);
                    simulate(elevators, elevator_count);
                } else {
                    send_all(client_sock, "Pickup floor == target floor, no extra movement.\r\n");
                }

                // 4. 完成
                offset += snprintf(resp + offset, sizeof(resp) - offset,
                    "CALL_DONE elevator=%d pickup=%d drop=%d final_floor=%d\r\n",
                    e->id, cur, tgt, e->current_floor);

                // 5. 一次送出全部回應
                send(client_sock, resp, offset, 0);
            } else if (_stricmp(cmd, "QUIT") == 0) {
                // QUIT：離開
                send_all(client_sock, "Bye\r\n");
                printf("[SERVER] Client requested QUIT.\n");
                break;
            } else {
                // 其他未知指令
                send_all(client_sock, "Unknown command\r\n");
            }
        }

        closesocket(client_sock);
        client_sock = INVALID_SOCKET;
    }

    closesocket(listen_sock);
    WSACleanup();
}
