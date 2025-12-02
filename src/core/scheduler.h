/* ----- ----- ----- ----- */
// scheduler.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/29
// Version: v1.1
/* ----- ----- ----- ----- */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "elevator.h"
#include "request_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

void Scheduler_Process(Elevator elevators[], int elevator_count, RequestQueue* pending);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
