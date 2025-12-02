/* ----- ----- ----- ----- */
// test.c
// Do not distribute or modify
// Author: DragonTaki (https://github.com/DragonTaki)
// Create Date: 2025/11/22
// Update Date: 2025/11/22
// Version: v1.0
/* ----- ----- ----- ----- */

#include <stdio.h>
#include "core/elevator.h"
#include "core/scheduler.h"

static int g_failed = 0;
static int g_run    = 0;

#define EXPECT_EQ_INT(expected, actual) \
    do { \
        g_run++; \
        if ((expected) != (actual)) { \
            g_failed++; \
            printf("[FAIL] %s:%d: expected %d, got %d\n", \
                   __FILE__, __LINE__, (expected), (actual)); \
        } \
    } while (0)

/* ----- ----- ----- ----- */

// 測試初始化
void test_elevator_init(void) {
    Elevator e;
    Elevator_init(&e, 1, 3);

    EXPECT_EQ_INT(1, e.id);
    EXPECT_EQ_INT(3, e.current_floor);
    EXPECT_EQ_INT(3, e.target_floor);
}

// 測試移動
void test_elevator_move(void) {
    Elevator e;
    Elevator_init(&e, 1, 3);

    Elevator_move(&e, 7);

    EXPECT_EQ_INT(3, e.current_floor);
    EXPECT_EQ_INT(7, e.target_floor);
}

// 測試往上
void test_elevator_step_up(void) {
    Elevator e;
    Elevator_init(&e, 1, 1);
    Elevator_move(&e, 4);

    Elevator_step(&e);
    EXPECT_EQ_INT(2, e.current_floor);
    Elevator_step(&e);
    EXPECT_EQ_INT(3, e.current_floor);
    Elevator_step(&e);
    EXPECT_EQ_INT(4, e.current_floor);
    Elevator_step(&e);  // 不應超過目標
    EXPECT_EQ_INT(4, e.current_floor);
}

// 測試往下
void test_elevator_step_down(void) {
    Elevator e;
    Elevator_init(&e, 1, 5);
    Elevator_move(&e, 2);

    Elevator_step(&e);
    EXPECT_EQ_INT(4, e.current_floor);
    Elevator_step(&e);
    EXPECT_EQ_INT(3, e.current_floor);
    Elevator_step(&e);
    EXPECT_EQ_INT(2, e.current_floor);
    Elevator_step(&e);  // 不應低於目標
    EXPECT_EQ_INT(2, e.current_floor);
}

// 測試選電梯
void test_scheduler_pick_nearest() {
    Elevator e[2];
    Elevator_init(&e[0], 1, 1);   // 在 1 樓
    Elevator_init(&e[1], 2, 10);  // 在 10 樓

    int idx = assign_elevator(3, e, 2);

    EXPECT_EQ_INT(0, idx);            // 應該選 elevator 1
    EXPECT_EQ_INT(3, e[0].target_floor);
    EXPECT_EQ_INT(10, e[1].target_floor); // 另一台不變
}

// 測試選電梯（兩邊一樣選 ID 小）
void test_scheduler_tie_breaker() {
    Elevator e[2];
    Elevator_init(&e[0], 1, 2);  // 離 5 差 3
    Elevator_init(&e[1], 2, 8);  // 離 5 差 3

    int idx = assign_elevator(5, e, 2);

    EXPECT_EQ_INT(0, idx);  // 設計成 tie 時選第一個
}

// 測試多個電梯
void test_scheduler_multiple_elevators(void) {
    Elevator es[3];
    Elevator_init(&es[0], 1, 1);
    Elevator_init(&es[1], 2, 4);
    Elevator_init(&es[2], 3, 9);

    int idx = assign_elevator(6, es, 3);

    EXPECT_EQ_INT(1, idx);   // id=2, 在4樓，距離6樓最近
    EXPECT_EQ_INT(6, es[idx].target_floor);
}

int main(void) {
    printf("=== Elevator Unit Tests ===\n");

    test_elevator_init();
    test_elevator_move();
    test_elevator_step_up();
    test_elevator_step_down();
    test_scheduler_pick_nearest();
    test_scheduler_tie_breaker();
    test_scheduler_multiple_elevators();

    printf("\nRun %d checks, %d failed.\n", g_run, g_failed);
    return g_failed ? 1 : 0;
}
