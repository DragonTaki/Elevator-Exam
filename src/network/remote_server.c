/* ----- ----- ----- ----- */
// remote_server.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../core/elevator.h"
#include "../core/server_core.h"
#include "../core/server_events.h"
#include "../core/status.h"
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

#define SIM_TICK_MS 300    // 多久跑一次
#define LISTEN_BACKLOG 16  // 等待連線數量上限
#define MAX_LINE_LEN 512

/* 用戶端種類 */
typedef enum {
    CLIENT_UNKNOWN = 0,
    CLIENT_BUTTON,
    CLIENT_GUARD
} ClientType;

/* 用戶端資料 */
typedef struct {
    SOCKET sock;
    ClientType type;
    int floor;     // for BUTTON
    int watching;  // for GUARD
    int id;
    char inbuf[1024];
    int inbuf_len;
} ClientInfo;

static ClientInfo clients[MAX_CLIENTS];
static int client_count = 0;

static Elevator* g_elevators = NULL;
static int g_elevator_count = 0;

/* 發送字串 */
static void send_line(SOCKET s, const char* line) {
    if (s == INVALID_SOCKET) return;
    char buf[MAX_LINE_LEN];
    int n = snprintf(buf, sizeof(buf), "%s\r\n", line);
    send(s, buf, n, 0);
}

/* 將所有電梯狀態發送給所有警衛端 */
void broadcast_status_to_guards(Elevator elevators[], int elevator_count, int only_watchers) {
    char line[128];
    char big[2048];
    big[0] = '\0';
    for (int i = 0; i < elevator_count; ++i) {
        Elevator_status_line(&elevators[i], line, sizeof(line));
        strncat(big, line, sizeof(big) - strlen(big) - 1);
        strncat(big, "\r\n", sizeof(big) - strlen(big) - 1);
    }
    for (int i = 0; i < client_count; ++i) {
        if (clients[i].type == CLIENT_GUARD) {
            if (only_watchers && !clients[i].watching) continue;
            send(clients[i].sock, big, (int)strlen(big), 0);
        }
    }
}

/* 接受新用戶端連線 */
static void accept_new_client(SOCKET listen_sock) {
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    SOCKET c = accept(listen_sock, (struct sockaddr*)&addr, &addrlen);

    // 如果無效或失敗 => 顯示錯誤並跳出
    if (c == INVALID_SOCKET) {
        printf("[SERVER] accept failed: %d\n", WSAGetLastError());
        return;
    }
    if (client_count >= MAX_CLIENTS) {
        send_line(c, "SERVER_BUSY");
        closesocket(c);
        return;
    }

    // 成功 => 加入至 clients 陣列尾端
    clients[client_count].sock = c;
    clients[client_count].type = CLIENT_UNKNOWN;
    clients[client_count].floor = -1;
    clients[client_count].watching = 0;
    clients[client_count].id = client_count;
    clients[client_count].inbuf_len = 0;
    client_count++;
    printf("[SERVER] Client connected (id=%d)\n", client_count-1);
    send_line(c, "WELCOME");
    send_line(c, "Please declare role: ROLE GUARD  OR  ROLE BUTTON <floor>");
}

/* 移除已離線或失效的用戶端 */
static void remove_client(int idx) {
    if (idx < 0 || idx >= client_count) return;
    closesocket(clients[idx].sock);
    printf("[SERVER] Client %d disconnected\n", clients[idx].id);

    // 移動 clients 陣列（後面往前補）
    for (int j = idx; j + 1 < client_count; ++j) clients[j] = clients[j+1];
    client_count--;
}

/* 剖析並處理用戶端文字指令 */
static void handle_client_command(int idx, const char* line) {
    ClientInfo* c = &clients[idx];
    char cmd[64];  // 指令的第一個 token
    if (sscanf(line, "%63s", cmd) != 1) return;

    // 未知身分
    if (c->type == CLIENT_UNKNOWN) {
        // 先看是不是指定 ROLE
        if (platform_stricmp(cmd, "ROLE") == 0) {
            char role[64];
            if (sscanf(line, "ROLE %63s", role) == 1) {
                if (platform_stricmp(role, "GUARD") == 0) {
                    c->type = CLIENT_GUARD;
                    c->watching = 0;
                    send_line(c->sock, "ROLE_OK GUARD");
                    printf("[SERVER] Client %d set ROLE GUARD\n", c->id);
                } else if (platform_stricmp(role, "BUTTON") == 0) {
                    int floor = -1;
                    if (sscanf(line, "ROLE BUTTON %d", &floor) >= 1) {
                        c->type = CLIENT_BUTTON;
                        c->floor = floor;
                        send_line(c->sock, "ROLE_OK BUTTON");
                        printf("[SERVER] Client %d set ROLE BUTTON floor=%d\n", c->id, floor);
                    } else {
                        send_line(c->sock, "ROLE_BAD BUTTON usage: ROLE BUTTON <floor>");
                    }
                } else {
                    send_line(c->sock, "ROLE_BAD");
                }
            } else {
                send_line(c->sock, "ROLE_BAD");
            }
            return;
        }

        // 未知身分 & 沒指定 ROLE => 當作按鈕 & 讀入 CALL 或 INSIDE
        if (platform_stricmp(cmd, "CALL") == 0 || platform_stricmp(cmd, "INSIDE") == 0) {
            c->type = CLIENT_BUTTON;
        }
        // 未知身分 & 沒指定 ROLE & 未知指令 => 提示輸入
        else {
            send_line(c->sock, "Please declare role: ROLE GUARD  OR  ROLE BUTTON <floor>");
            return;
        }
    }

    // 已知身分
    if (c->type == CLIENT_BUTTON) {
        // CALL => 外部呼叫（電梯上／下樓按鈕）
        if (platform_stricmp(cmd, "CALL") == 0) {
            int from, to;
            char dirstr[16] = {0};

            // CALL <from> <to>
            if (sscanf(line, "CALL %d %d", &from, &to) == 2) {
                if (from < 0 || from >= MAX_FLOORS || to < 0 || to >= MAX_FLOORS) {
                    send_line(c->sock, "CALL_BAD floor out of range");
                } else if (from == to) {
                    send_line(c->sock, "CALL_BAD from==to");
                } else {
                    int dir = (to > from) ? DIR_UP : DIR_DOWN;
                    if (server_events_push_outside(from, dir, c->id) == 0) {
                        send_line(c->sock, "CALL_OK");
                        printf("[SERVER] Request queued from client %d: %d -> %d\n", c->id, from, to);
                    } else {
                        send_line(c->sock, "CALL_REJECT queue_full");
                    }
                }
            }
            // CALL <哪層樓按的> UP/DOWN
            else if (sscanf(line, "CALL %d %15s", &from, dirstr) == 2) {
                if (from < 0 || from >= MAX_FLOORS) {
                    send_line(c->sock, "CALL_BAD floor out of range");
                    return;
                }
                int dir;
                if (platform_stricmp(dirstr, "UP") == 0) dir = DIR_UP;
                else if (platform_stricmp(dirstr, "DOWN") == 0) dir = DIR_DOWN;
                else {
                    send_line(c->sock, "CALL_BAD usage: CALL <from> UP|DOWN");
                    return;
                }
                if (server_events_push_outside(from, dir, c->id) == 0) {
                    send_line(c->sock, "CALL_OK");
                    printf("[SERVER] Directional CALL queued from client %d: %d %s\n",
                        c->id, from, dirstr);
                } else {
                    send_line(c->sock, "CALL_REJECT queue_full");
                }
            } else {
                send_line(c->sock, "CALL_BAD usage: CALL <from> UP|DOWN");
            }
        }
        // INSIDE => 內部呼叫（電梯內部按樓層）
        else if (platform_stricmp(cmd, "INSIDE") == 0) {
            int eid, dest;
            // INSIDE <電梯 ID> <樓層>
            if (sscanf(line, "INSIDE %d %d", &eid, &dest) == 2) {
                /* validate elevator id */
                if (eid >= 0) {
                    int rc = server_events_push_inside(eid, dest, c->id);
                    if (rc == ELEV_OK) {
                        send_line(c->sock, "INSIDE_OK");
                    } else if (rc == ELEV_DUPLICATE) {
                        send_line(c->sock, "INSIDE_DUPLICATE");
                    } else {
                        send_line(c->sock, "INSIDE_REJECT");
                    }
                } else {
                    send_line(c->sock, "INSIDE_BAD elevator_id");
                }
            } else {
                send_line(c->sock, "INSIDE_BAD usage: INSIDE <elevator_id> <dest>");
            }
        } else {
            send_line(c->sock, "UNKNOWN_CMD (BUTTON allowed: CALL, INSIDE)");
        }
    }
    // 已知身分（警衛）
    else if (c->type == CLIENT_GUARD) {
        if (platform_stricmp(cmd, "STATUS") == 0) {
            char buf[256];
            for (int i = 0; i < g_elevator_count; ++i) {
                Elevator_status_line(&g_elevators[i], buf, sizeof(buf));
                send_line(c->sock, buf);
            }
        } else if (platform_stricmp(cmd, "WATCH") == 0) {
            c->watching = 1;
            send_line(c->sock, "WATCH_OK");
        } else if (platform_stricmp(cmd, "UNWATCH") == 0) {
            c->watching = 0;
            send_line(c->sock, "UNWATCH_OK");
        } else if (platform_stricmp(cmd, "CALL") == 0) {
            int from, to;
            char dirstr[16] = {0};
            if (sscanf(line, "CALL %d %d", &from, &to) == 2) {
                if (from < 0 || from >= MAX_FLOORS || to < 0 || to >= MAX_FLOORS || from == to) {
                    send_line(c->sock, "CALL_BAD");
                } else {
                    int dir = (to > from) ? DIR_UP : DIR_DOWN;
                    if (server_events_push_outside(from, dir, c->id) == 0) {
                        send_line(c->sock, "CALL_OK");
                        printf("[SERVER] Guard client %d queued CALL %d->%d\n", c->id, from, to);
                    } else {
                        send_line(c->sock, "CALL_REJECT queue_full");
                    }
                }
            } else if (sscanf(line, "CALL %d %15s", &from, dirstr) == 2) {
                if (from < 0 || from >= MAX_FLOORS) { send_line(c->sock, "CALL_BAD"); return; }
                int dir;
                if (platform_stricmp(dirstr, "UP") == 0) dir = DIR_UP;
                else if (platform_stricmp(dirstr, "DOWN") == 0) dir = DIR_DOWN;
                else { send_line(c->sock, "CALL_BAD usage: CALL <from> UP|DOWN"); return; }

                if (server_events_push_outside(from, dir, c->id) == 0) {
                    send_line(c->sock, "CALL_OK");
                    printf("[SERVER] Guard directional CALL queued %d %s\n", from, dirstr);
                } else {
                    send_line(c->sock, "CALL_REJECT queue_full");
                }
            } else {
                send_line(c->sock, "CALL_BAD usage: CALL <from> <to>");
            }
        } else {
            send_line(c->sock, "UNKNOWN_CMD (GUARD allowed: STATUS, WATCH, UNWATCH, CALL)");
        }
    }
}

/* 伺服器主進入點 */
void run_remote_server(int port) {
    g_elevators = server_core_get_elevators();
    g_elevator_count = server_core_get_elevator_count();

    if (platform_socket_init() != 0) {
        printf("[SERVER] platform_socket_init failed: %d\n", platform_socket_last_error());
        return;
    }

    platform_socket_t listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == (platform_socket_t)-1
    #ifdef _WIN32
        || listen_sock == INVALID_SOCKET
    #endif
    ) {
        printf("[SERVER] socket failed: %d\n", platform_socket_last_error());
        platform_socket_cleanup();
        return;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;  // IPv4
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons((u_short)port);

    if (bind(listen_sock, (struct sockaddr*)&serv, sizeof(serv)) == SOCKET_ERROR) {
        printf("[SERVER] bind failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return;
    }
    if (listen(listen_sock, LISTEN_BACKLOG) == SOCKET_ERROR) {
        printf("[SERVER] listen failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return;
    }

    printf("[SERVER] Listening on port %d...\n", port);

    while (1) {
        fd_set readfds;                 // 要監聽的 socket 集合
        FD_ZERO(&readfds);              // 清空
        FD_SET(listen_sock, &readfds);  // 把監聽 socket 加入，監控是否有新連線
        SOCKET maxfd = listen_sock;     // 最大 socket 值，供 select 使用
        for (int i = 0; i < client_count; ++i) {
            FD_SET(clients[i].sock, &readfds);
            if (clients[i].sock > maxfd) maxfd = clients[i].sock;
        }

        int wait_ms = SIM_TICK_MS;

        struct timeval tv;  // select 沉睡需要使用 timeval 為單位
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = (wait_ms % 1000) * 1000;

        int ready = select((int)maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ready == SOCKET_ERROR) {
            printf("[SERVER] select failed: %d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(listen_sock, &readfds)) {  // 有新連線
            accept_new_client(listen_sock);
        }

        /* handle client sockets */
        for (int i = 0; i < client_count; ++i) {
            if (FD_ISSET(clients[i].sock, &readfds)) {  // client[i] 有資料可讀
                char buf[512];
                int len = recv(clients[i].sock, buf, sizeof(buf)-1, 0);
                if (len <= 0) {  // 出錯或失效的 => 移除
                    remove_client(i);
                    i--;
                    continue;
                }
                buf[len] = '\0';
                // 把收到的字串附加到 clients[i].inbuf 上
                if (clients[i].inbuf_len + len < (int)sizeof(clients[i].inbuf) - 1) {
                    memcpy(clients[i].inbuf + clients[i].inbuf_len, buf, len);
                    clients[i].inbuf_len += len;
                    clients[i].inbuf[clients[i].inbuf_len] = '\0';
                } else {
                    /* overflow: reset buffer */
                    clients[i].inbuf_len = 0;
                    clients[i].inbuf[0] = '\0';
                }
                /* process full lines */
                char* start = clients[i].inbuf;  // 指向尚未處理的起點
                char* eol;                       // 指向指令結尾
                while ((eol = strstr(start, "\r\n")) != NULL || (eol = strchr(start, '\n')) != NULL) {  // 找指令結尾
                    size_t linelen = eol - start;
                    char line[512];  // 用於儲存指令
                    if (linelen >= sizeof(line)) linelen = sizeof(line)-1;
                    memcpy(line, start, linelen);
                    line[linelen] = '\0';
                    handle_client_command(i, line);
                    // 處理完 => 起點指標移動至 "\r\n" 或 '\n' 後面 => 下個 while 繼續處理
                    if (*eol == '\r' && *(eol+1) == '\n') start = eol + 2;
                    else start = eol + 1;
                }
                // while 處理完 => 將後面殘留的移到前面 => 繼續拼接
                int rem = strlen(start);
                memmove(clients[i].inbuf, start, rem);
                clients[i].inbuf_len = rem;
                clients[i].inbuf[rem] = '\0';
            }
        }
    }

    closesocket(listen_sock);
    WSACleanup();
}
