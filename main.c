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
void run_remote_server(Elevator elevators[], int elevator_count, int port);

/* simple delay abstraction used by simulator */
void sim_delay_one_floor(void) {
    /* keep same pace as server tick (300ms) */
    platform_sleep_ms(300);
}

int main(int argc, char* argv[]) {
    /* configure elevator set here */
    const int elevator_count = 2;
    static Elevator elevators[MAX_ELEVATORS];

    /* initialize elevators (IDs and start floors) */
    Elevator_init(&elevators[0], 0, 1);
    Elevator_init(&elevators[1], 1, 10);

    /* choose port (optional argument) */
    int port = 5555;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0) port = 5555;
    }

    printf("=== Elevator Simulator (Server Mode) ===\n");
    printf("[MAIN] Starting server on port %d...\n", port);

    /* run the networked server — this call blocks until process exits */
    run_remote_server(elevators, elevator_count, port);

    printf("[MAIN] Server stopped. Exiting.\n");
    return 0;
}
