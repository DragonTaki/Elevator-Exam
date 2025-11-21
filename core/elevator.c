/* ----- ----- ----- ----- */
// elevator.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#include <stdio.h>
#include "elevator.h"

void Elevator_init(Elevator *e, int id, int start_floor) {
    if (!e) return;

    e->id = id;
    e->current_floor = start_floor;
    e->target_floor = start_floor;
}

void Elevator_move(Elevator *e, int floor) {
    if (!e) return;

    e->target_floor = floor;
}

void Elevator_step(Elevator *e) {
    if (!e) return;

    if (e->current_floor < e->target_floor) {
        e->current_floor++;
    } else if (e->current_floor > e->target_floor) {
        e->current_floor--;
    }
}

void Elevator_display(const Elevator *e) {
    if (!e) return;

    printf("[Elevator %d] Current floor: %d (target: %d)\n",
        e->id, e->current_floor, e->target_floor);
}
