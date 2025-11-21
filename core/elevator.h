/* ----- ----- ----- ----- */
// elevator.h
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#pragma once

// 題目要求： Define a class called "Elevator"that has the following attributes and methods
// 但 C 中沒有 class，所以用 struct + 一組函式來模擬 OOP 的 class
typedef struct {
    int id;
    int current_floor;  // 題目要求的欄位："current_floor": an integer representing the current floor of the elevator
    int target_floor;
} Elevator;

void Elevator_init(Elevator *e, int id, int start_floor);
void Elevator_move(Elevator *e, int floor);
void Elevator_step(Elevator *e);

void Elevator_display(const Elevator *e);

// 如果改成 C++
/*
class Elevator {
private:
    int id;
    int currentFloor;
    int targetFloor;

public:
    Elevator(int id, int startFloor) {
        this->id = id;
        this->currentFloor = startFloor;
        this->targetFloor = startFloor;
    }

    void move(int floor) {...}
    void step() {...}

    void display() const {...}
};
*/
