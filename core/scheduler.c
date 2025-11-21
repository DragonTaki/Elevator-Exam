/* ----- ----- ----- ----- */
// scheduler.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#include <stdlib.h>
#include <stdio.h>
#include "scheduler.h"

int assign_elevator(int request_floor, Elevator elevators[], int elevator_count) {
    int best_index = -1;
    int best_dist  = INT_MAX;

    for (int i = 0; i < elevator_count; ++i) {
        int dist = abs(elevators[i].current_floor - request_floor);
        if (dist < best_dist) {
            best_dist  = dist;
            best_index = i;
        }
    }

    if (best_index >= 0) {
        Elevator_move(&elevators[best_index], request_floor);
        printf("Assigned request to Elevator %d\n",
            elevators[best_index].id);
    }

    return best_index;
}
