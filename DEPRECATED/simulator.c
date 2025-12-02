/* ----- ----- ----- ----- */
// simulator.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#include <stdio.h>
#include "sim_time.h"
#include "simulator.h"

void simulate(Elevator elevators[], int elevator_count) {
    int all_idle = 1;
    
    // 起始狀態
    for (int i = 0; i < elevator_count; ++i) {
        Elevator_display(&elevators[i]);
        if (elevators[i].current_floor != elevators[i].target_floor) {
            all_idle = 0;
        }
    }

    // 如果全部都已到達目標，直接跳出
    if (all_idle) {
        printf("All elevators are idle.\n");
        return;
    }

    // 開始移動
    sim_delay_one_floor();
    while (1) {
        int all_idle = 1;

        for (int i = 0; i < elevator_count; ++i) {
            Elevator_step(&elevators[i]);
            Elevator_display(&elevators[i]);

            if (elevators[i].current_floor != elevators[i].target_floor) {
                all_idle = 0;
            }
        }

        if (all_idle) {
            printf("All elevators are idle.\n");
            break;
        }

        sim_delay_one_floor();
    }
}
