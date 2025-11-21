/* ----- ----- ----- ----- */
// guard_client.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define RECV_BUF_SIZE 512

// 選單提示
static void print_menu(void) {
    printf("\n=== Elevator Guard Client ===\n");
    printf("1) Show elevator status\n");
    printf("2) Call elevator (current floor -> target floor)\n");
    printf("3) Quit\n");
    printf("Select: ");
}

// 接收一次資料並印出
static int recv_and_print(SOCKET sock) {
    char buf[RECV_BUF_SIZE];
    int len = recv(sock, buf, RECV_BUF_SIZE - 1, 0);
    if (len > 0) {
        buf[len] = '\0';
        printf("%s", buf);
    }
    return len;
}

// 接收 server 回傳的資料
static void recv_response(SOCKET sock) {
    char buf[RECV_BUF_SIZE];

    // 阻塞式接收一次
    int len = recv(sock, buf, RECV_BUF_SIZE - 1, 0);
    if (len > 0) {
        buf[len] = '\0';
        printf("%s", buf);
    } else if (len == 0) {
        printf("[CLIENT] Server closed connection.\n");
    } else {
        printf("[CLIENT] recv() failed: %d\n", WSAGetLastError());
    }
}

// 連線到遠端 server
static SOCKET connect_to_server(const char *host, int port) {
    WSADATA wsa;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server_addr;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("[CLIENT] WSAStartup failed: %d\n", WSAGetLastError());
        return INVALID_SOCKET;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("[CLIENT] socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return INVALID_SOCKET;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons((u_short)port);
    server_addr.sin_addr.s_addr = inet_addr(host);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("[CLIENT] connect() failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return INVALID_SOCKET;
    }

    printf("[CLIENT] Connected to %s:%d\n", host, port);

    return sock;
}

int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int port = 5555;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = atoi(argv[2]);
    }

    SOCKET sock = connect_to_server(host, port);
    if (sock == INVALID_SOCKET) {
        return 1;
    }

    while (1) {
        int choice;
        print_menu();

        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exit.\n");
            break;
        }

        if (choice == 1) {
            // STATUS
            const char *cmd = "STATUS\r\n";
            send(sock, cmd, (int)strlen(cmd), 0);
            recv_response(sock);

        } else if (choice == 2) {
            // CALL
            int current, target;
            printf("Enter current floor: ");
            if (scanf("%d", &current) != 1) {
                printf("Invalid input.\n");
                break;
            }
            printf("Enter target floor: ");
            if (scanf("%d", &target) != 1) {
                printf("Invalid input.\n");
                break;
            }

            char cmd[64];
            snprintf(cmd, sizeof(cmd), "CALL %d %d\r\n", current, target);
            send(sock, cmd, (int)strlen(cmd), 0);
            recv_response(sock);

        } else if (choice == 3) {
            // QUIT
            const char *cmd = "QUIT\r\n";
            send(sock, cmd, (int)strlen(cmd), 0);
            recv_response(sock);
            break;

        } else {
            printf("Unknown selection.\n");
        }
    }

    closesocket(sock);
    WSACleanup();
    printf("[CLIENT] Disconnected.\n");
    return 0;
}
