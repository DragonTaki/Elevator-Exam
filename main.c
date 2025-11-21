/* ----- ----- ----- ----- */
// main.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#include <stdio.h>
#include <stdlib.h>
#include "core/elevator.h"
#include "core/scheduler.h"
#include "core/simulator.h"

#define ELEVATOR_COUNT 2

#define MAX_FLOOR 10
#define MIN_FLOOR 1

#define PORT 5555

static void run_server_mode(Elevator elevators[], int count) {
    printf("=== Elevator Simulator (Server Mode) ===\n");
    run_remote_server(elevators, count, PORT);
}

static void run_cli_mode(Elevator elevators[], int count) {
    printf("=== Elevator Simulator (CLI Mode) ===\n");

    while (1) {
        int current_floor;
        int target_floor;

        // CURRENT
        printf("\nEnter your CURRENT floor (%d-%d), or 0 to exit: ", MIN_FLOOR, MAX_FLOOR);

        if (scanf("%d", &current_floor) != 1) {
            printf("Invalid input.\n");
            break;
        }

        if (current_floor == 0) {
            printf("Exiting simulator...\n");
            break;
        }

        if (current_floor < MIN_FLOOR || current_floor > MAX_FLOOR) {
            printf("Invalid current number.\n");
            continue;
        }

        // TARGET
        printf("Enter your TARGET floor (%d-%d): ", MIN_FLOOR, MAX_FLOOR);

        if (scanf("%d", &target_floor) != 1) {
            printf("Invalid input.\n");
            break;
        }

        if (target_floor < MIN_FLOOR || target_floor > MAX_FLOOR) {
            printf("Invalid target floor.\n");
            continue;
        }

        /*if (target_floor == current_floor) {
            printf("Target floor is the same as current floor. Nothing to do.\n");
            continue;
        }*/

        // 1. 選電梯 -> 移動至 current_floor
        int idx = assign_elevator(current_floor, elevators, ELEVATOR_COUNT);
        if (idx < 0) {
            printf("No elevator available.\n");
            continue;
        }

        // 2. 模擬電梯移動（移動至 current_floor）
        printf("\n--- Moving elevator to pick you up at floor %d ---\n", current_floor);
        simulate(elevators, ELEVATOR_COUNT);

        // 3. 模擬電梯移動（將同一台移動至 target_floor）
        if (current_floor == target_floor) {
            printf("\n--- Elevator arrived at floor %d, you are already there ---\n",
                current_floor);
        } else {
            printf("\n--- Moving you from floor %d to floor %d ---\n",
                current_floor, target_floor);
            Elevator_move(&elevators[idx], target_floor);
            simulate(elevators, ELEVATOR_COUNT);
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    Elevator elevators[ELEVATOR_COUNT];

    Elevator_init(&elevators[0], 1, 1);
    Elevator_init(&elevators[1], 2, 10);

    if (argc > 1 && strcmp(argv[1], "--server") == 0) {
        run_server_mode(elevators, ELEVATOR_COUNT);
    } else {
        run_cli_mode(elevators, ELEVATOR_COUNT);
    }

    return 0;
}
