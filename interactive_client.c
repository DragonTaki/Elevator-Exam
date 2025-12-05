/* ----- ----- ----- ----- */
// interactive_client.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/30
// Update Date: 2025/11/30
// Version: v1.0
/* ----- ----- ----- ----- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib,"ws2_32.lib")
  typedef SOCKET sock_t;
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  typedef int sock_t;
  #define INVALID_SOCKET (-1)
  #define closesocket close
#endif

/* send all bytes */
static int send_all(sock_t s, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = (int)send(s, buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return sent;
}

#ifdef _WIN32
#include <conio.h>  // for _kbhit()
#endif

/* 監看模式：已經送出 WATCH 之後呼叫，持續收狀態，直到使用者按鍵 */
static void watch_mode_loop(sock_t s) {
    printf("=== WATCH mode: server status streaming ===\n");
    printf("Press ENTER to send UNWATCH, or type any command then ENTER to exit watch.\n");

    char buf[4096];

    for (;;) {
        /* 1) 用 select 等待 socket 有資料（最多 200ms） */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000; /* 200 ms */

        int rv = select((int)(s + 1), &rfds, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(s, &rfds)) {
            int n = (int)recv(s, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                printf("\n[WATCH] Connection closed by server.\n");
                return;
            }
            buf[n] = '\0';
            printf("SERVER> %s", buf);
            fflush(stdout);
        }

        /* 2) 檢查有沒有鍵盤輸入（Windows 用 _kbhit） */
#ifdef _WIN32
        if (_kbhit()) {
            char line[512];
            if (!fgets(line, sizeof(line), stdin)) {
                printf("\n[WATCH] stdin closed, exiting watch.\n");
                return;
            }
            /* 修剪換行 */
            size_t L = strlen(line);
            while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';

            if (L == 0) {
                /* 空行 => 自動送 UNWATCH */
                const char* cmd = "UNWATCH\r\n";
                send_all(s, cmd, (int)strlen(cmd));
                printf("[WATCH] Sent UNWATCH, leaving watch mode.\n");
            } else {
                /* 有輸入內容 => 原樣送出（可以是 UNWATCH 或其他指令） */
                char out[600];
                int outlen = snprintf(out, sizeof(out), "%s\r\n", line);
                if (outlen > 0 && outlen < (int)sizeof(out)) {
                    send_all(s, out, outlen);
                    printf("[WATCH] Sent command: %s\n", line);
                }
            }
            /* 不管送了什麼指令，都離開 watch mode 回到一般互動 */
            return;
        }
#else
        /* 非 Windows 平台：沒 _kbhit，就先不要卡在這裡，直接離開 */
        /* 你之後若要支援，可以用 select 監 stdin。 */
        // return;
#endif
    }
}

/* try recv with small timeout (non-blocking friendly) */
static void try_read_and_print(sock_t s) {
    char buf[4096];
    int n = (int)recv(s, buf, sizeof(buf)-1, MSG_DONTWAIT);
    if (n > 0) {
        buf[n] = '\0';
        printf("SERVER> %s", buf);
        fflush(stdout);
    }
}

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    const char* port = "5555";

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = argv[2];

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 2;
    }

    sock_t s = INVALID_SOCKET;
    for (struct addrinfo* p = res; p; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == INVALID_SOCKET) continue;
        if (connect(s, p->ai_addr, (int)p->ai_addrlen) == 0) break;
        closesocket(s);
        s = INVALID_SOCKET;
    }
    freeaddrinfo(res);
    if (s == INVALID_SOCKET) {
        fprintf(stderr, "connect failed\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 3;
    }

    /* print initial server welcome if any */
    char buf[4096];
    int n = (int)recv(s, buf, sizeof(buf)-1, 0);
    if (n > 0) {
        buf[n] = '\0';
        printf("SERVER> %s", buf);
    }

    printf("Connected to %s:%s. Type commands (CTRL+D or CTRL+C to quit).\n", host, port);

    /* interactive loop: read stdin line, send with CRLF, try read server reply */
    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* trim newline at end */
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) { line[--L] = '\0'; }

        if (L == 0) { try_read_and_print(s); continue; }

        /* 檢查是否為 WATCH 指令（簡單比對，大寫） */
        if (strcmp(line, "WATCH") == 0) {
            /* 一樣先送給 server */
            char out[600];
            int outlen = snprintf(out, sizeof(out), "%s\r\n", line);
            if (outlen > 0 && outlen < (int)sizeof(out)) {
                if (send_all(s, out, outlen) < 0) {
                    fprintf(stderr, "send failed or connection closed\n");
                    break;
                }
            }
            /* 進入監看模式：只收資料，直到你按鍵 */
            watch_mode_loop(s);
            /* 回到主迴圈，讓你繼續輸入其他指令 */
            continue;
        }

        /* 一般指令流程：append CRLF 然後送出 */
        char out[600];
        int outlen = snprintf(out, sizeof(out), "%s\r\n", line);
        if (outlen <= 0 || outlen >= (int)sizeof(out)) { fprintf(stderr, "line too long\n"); continue; }

        if (send_all(s, out, outlen) < 0) {
            fprintf(stderr, "send failed or connection closed\n");
            break;
        }

        /* after send_all(s, out, outlen) succeeded */

        /* wait up to 200 ms for replies, then read all available data */
        fd_set readfds;
        struct timeval tv;
        int rv;
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000; /* 200 ms */

        rv = select((int)(s + 1), &readfds, NULL, NULL, &tv);
        if (rv > 0 && FD_ISSET(s, &readfds)) {
            while (1) {
                int n = (int)recv(s, buf, sizeof(buf)-1, 0);
                if (n <= 0) break;
                buf[n] = '\0';
                printf("SERVER> %s", buf);
                /* short non-blocking probe to see if more data is pending */
                /* use select with zero timeout */
                FD_ZERO(&readfds);
                FD_SET(s, &readfds);
                tv.tv_sec = 0;
                tv.tv_usec = 0;
                rv = select((int)(s + 1), &readfds, NULL, NULL, &tv);
                if (!(rv > 0 && FD_ISSET(s, &readfds))) break;
            }
        }
    }

    printf("Exiting.\n");
    closesocket(s);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
