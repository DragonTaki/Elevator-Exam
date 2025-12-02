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
#include "../core/status.h"
#include "../core/tick.h"
#include "protocol.h"

#pragma comment(lib, "ws2_32.lib")

#define SIM_TICK_MS 300
#define LISTEN_BACKLOG 16
#define MAX_LINE_LEN 512

typedef enum {
    CLIENT_UNKNOWN = 0,
    CLIENT_BUTTON,
    CLIENT_GUARD
} ClientType;

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
static RequestQueue reqq;
/* 新增：每台電梯被 assign 但尚未處理的 drop floor（-1 表示無） */
int g_assigned_drop[MAX_ELEVATORS];

/* small helper to send a CRLF-terminated line */
static void send_line(SOCKET s, const char* line) {
    if (s == INVALID_SOCKET) return;
    char buf[MAX_LINE_LEN];
    int n = snprintf(buf, sizeof(buf), "%s\r\n", line);
    send(s, buf, n, 0);
}

/* broadcast status to guards (only watchers if only_watchers=1) */
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

static void accept_new_client(SOCKET listen_sock) {
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    SOCKET c = accept(listen_sock, (struct sockaddr*)&addr, &addrlen);
    if (c == INVALID_SOCKET) {
        printf("[SERVER] accept failed: %d\n", WSAGetLastError());
        return;
    }
    if (client_count >= MAX_CLIENTS) {
        send_line(c, "SERVER_BUSY");
        closesocket(c);
        return;
    }
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

static void remove_client(int idx) {
    if (idx < 0 || idx >= client_count) return;
    closesocket(clients[idx].sock);
    printf("[SERVER] Client %d disconnected\n", clients[idx].id);
    for (int j = idx; j + 1 < client_count; ++j) clients[j] = clients[j+1];
    client_count--;
}
/* parse and handle a single textual command line from client idx */
static void handle_client_command(int idx, const char* line, Elevator elevators[], int elevator_count) {
    ClientInfo* c = &clients[idx];
    char cmd[64];
    if (sscanf(line, "%63s", cmd) != 1) return;

    if (c->type == CLIENT_UNKNOWN) {
        if (_stricmp(cmd, "ROLE") == 0) {
            char role[64];
            if (sscanf(line, "ROLE %63s", role) == 1) {
                if (_stricmp(role, "GUARD") == 0) {
                    c->type = CLIENT_GUARD;
                    c->watching = 0;
                    send_line(c->sock, "ROLE_OK GUARD");
                    printf("[SERVER] Client %d set ROLE GUARD\n", c->id);
                } else if (_stricmp(role, "BUTTON") == 0) {
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

        /* If client is unknown but sends CALL -> treat as BUTTON CALL */
        if (_stricmp(cmd, "CALL") == 0 || _stricmp(cmd, "INSIDE") == 0) {
            /* fall through to common handling below (as if CLIENT_BUTTON) */
            /* mark as BUTTON for future messages (optional) */
            c->type = CLIENT_BUTTON;
            /* do NOT return here; let processing continue to BUTTON branch */
        } else {
            /* other commands are not allowed until role declared */
            send_line(c->sock, "Please declare role: ROLE GUARD  OR  ROLE BUTTON <floor>");
            return;
        }
    }

    /* Known client */
    if (c->type == CLIENT_BUTTON) {
        if (_stricmp(cmd, "CALL") == 0) {
            int from, to;
            char dirstr[16] = {0};

            /* numeric form: CALL <from> <to>  (we translate to PendingRequest with to_floor hint) */
            if (sscanf(line, "CALL %d %d", &from, &to) == 2) {
                if (from < 0 || from >= MAX_FLOORS || to < 0 || to >= MAX_FLOORS) {
                    send_line(c->sock, "CALL_BAD floor out of range");
                } else if (from == to) {
                    send_line(c->sock, "CALL_BAD from==to");
                } else {
                    PendingRequest p;
                    p.floor = from;
                    p.to_floor = to; /* hint for scheduler/assigned drop */
                    p.source_id = c->id;
                    p.type = (to > from) ? REQ_CALL_UP : REQ_CALL_DOWN;

                    if (rq_push(&reqq, p) == 0) {
                        send_line(c->sock, "CALL_OK");
                        printf("[SERVER] Request queued from client %d: %d -> %d\n", c->id, from, to);
                    } else {
                        send_line(c->sock, "CALL_REJECT queue_full");
                    }
                }
            }
            /* try directional form: CALL <from> UP/DOWN */
            else if (sscanf(line, "CALL %d %15s", &from, dirstr) == 2) {
                if (from < 0 || from >= MAX_FLOORS) {
                    send_line(c->sock, "CALL_BAD floor out of range");
                    return;
                }
                PendingRequest p;
                p.floor = from;
                p.to_floor = -1;
                p.source_id = c->id;
                if (_stricmp(dirstr, "UP") == 0) p.type = REQ_CALL_UP;
                else if (_stricmp(dirstr, "DOWN") == 0) p.type = REQ_CALL_DOWN;
                else {
                    send_line(c->sock, "CALL_BAD usage: CALL <from> <to>|UP|DOWN");
                    return;
                }
                if (rq_push(&reqq, p) == 0) {
                    send_line(c->sock, "CALL_OK");
                    printf("[SERVER] Directional CALL queued from client %d: %d %s\n",
                        c->id, from, dirstr);
                } else {
                    send_line(c->sock, "CALL_REJECT queue_full");
                }
            } else {
                send_line(c->sock, "CALL_BAD usage: CALL <from> <to>  or  CALL <from> UP|DOWN");
            }
        } else if (_stricmp(cmd, "INSIDE") == 0) {
            int eid, dest;
            if (sscanf(line, "INSIDE %d %d", &eid, &dest) == 2) {
                /* validate elevator id */
                if (eid >= 0 && eid < elevator_count) {
                    int rc = Elevator_push_inside_request(&elevators[eid], dest, c->id /*client*/);
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
    } else if (c->type == CLIENT_GUARD) {
        if (_stricmp(cmd, "STATUS") == 0) {
            char buf[256];
            for (int i = 0; i < elevator_count; ++i) {
                Elevator_status_line(&elevators[i], buf, sizeof(buf));
                send_line(c->sock, buf);
            }
        } else if (_stricmp(cmd, "WATCH") == 0) {
            c->watching = 1;
            send_line(c->sock, "WATCH_OK");
        } else if (_stricmp(cmd, "UNWATCH") == 0) {
            c->watching = 0;
            send_line(c->sock, "UNWATCH_OK");
        } else if (_stricmp(cmd, "CALL") == 0) {
            int from, to;
            char dirstr[16] = {0};
            if (sscanf(line, "CALL %d %d", &from, &to) == 2) {
                if (from < 0 || from >= MAX_FLOORS || to < 0 || to >= MAX_FLOORS || from == to) {
                    send_line(c->sock, "CALL_BAD");
                } else {
                    PendingRequest p;
                    p.floor = from;
                    p.to_floor = to;
                    p.source_id = c->id;
                    p.type = (to > from) ? REQ_CALL_UP : REQ_CALL_DOWN;
                    if (rq_push(&reqq, p) == 0) {
                        send_line(c->sock, "CALL_OK");
                        printf("[SERVER] Guard client %d queued CALL %d->%d\n", c->id, from, to);
                    } else {
                        send_line(c->sock, "CALL_REJECT queue_full");
                    }
                }
            } else if (sscanf(line, "CALL %d %15s", &from, dirstr) == 2) {
                if (from < 0 || from >= MAX_FLOORS) { send_line(c->sock, "CALL_BAD"); return; }
                PendingRequest p;
                p.floor = from;
                p.to_floor = -1;
                p.source_id = c->id;
                if (_stricmp(dirstr, "UP") == 0) p.type = REQ_CALL_UP;
                else if (_stricmp(dirstr, "DOWN") == 0) p.type = REQ_CALL_DOWN;
                else { send_line(c->sock, "CALL_BAD usage: CALL <from> <to>|UP|DOWN"); return; }

                if (rq_push(&reqq, p) == 0) {
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

/* Public function */
void run_remote_server(Elevator elevators[], int elevator_count, int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("[SERVER] WSAStartup failed: %d\n", WSAGetLastError());
        return;
    }

    rq_init(&reqq);
    /* 初始化 assigned drop table */
    for (int i = 0; i < MAX_ELEVATORS; ++i) g_assigned_drop[i] = -1;
    remote_tick_init(elevators, elevator_count, &reqq, SIM_TICK_MS, g_assigned_drop);
    /* optionally force print every tick: */
    REMOTE_TICK_ALWAYS_PRINT_STATUS = 0; /* 1 to always print */

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        printf("[SERVER] socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
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

    DWORD last_tick = GetTickCount();

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_sock, &readfds);
        SOCKET maxfd = listen_sock;
        for (int i = 0; i < client_count; ++i) {
            FD_SET(clients[i].sock, &readfds);
            if (clients[i].sock > maxfd) maxfd = clients[i].sock;
        }

        DWORD now = GetTickCount();
        int elapsed = (int)(now - last_tick);
        int wait_ms = SIM_TICK_MS - (elapsed > 0 ? elapsed : 0);
        if (wait_ms < 0) wait_ms = 0;

        struct timeval tv;
        tv.tv_sec = wait_ms / 1000;
        tv.tv_usec = (wait_ms % 1000) * 1000;

        int ready = select((int)maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ready == SOCKET_ERROR) {
            printf("[SERVER] select failed: %d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(listen_sock, &readfds)) {
            accept_new_client(listen_sock);
        }

        /* handle client sockets */
        for (int i = 0; i < client_count; ++i) {
            if (FD_ISSET(clients[i].sock, &readfds)) {
                char buf[512];
                int len = recv(clients[i].sock, buf, sizeof(buf)-1, 0);
                if (len <= 0) {
                    remove_client(i);
                    i--;
                    continue;
                }
                buf[len] = '\0';
                /* concatenate to inbuf (handle partial lines) */
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
                char* start = clients[i].inbuf;
                char* eol;
                while ((eol = strstr(start, "\r\n")) != NULL || (eol = strchr(start, '\n')) != NULL) {
                    size_t linelen = eol - start;
                    char line[512];
                    if (linelen >= sizeof(line)) linelen = sizeof(line)-1;
                    memcpy(line, start, linelen);
                    line[linelen] = '\0';
                    handle_client_command(i, line, elevators, elevator_count);
                    /* advance start past line ending */
                    if (*eol == '\r' && *(eol+1) == '\n') start = eol + 2;
                    else start = eol + 1;
                }
                /* move remaining partial data to front */
                int rem = strlen(start);
                memmove(clients[i].inbuf, start, rem);
                clients[i].inbuf_len = rem;
                clients[i].inbuf[rem] = '\0';
            }
        }

        now = GetTickCount();
        remote_tick_try_do(now);
    }

    remote_tick_shutdown();
    closesocket(listen_sock);
    WSACleanup();
}
