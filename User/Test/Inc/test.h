/**
 * @file    test.h
 * @brief   Unified test entry selection
 */

#ifndef __TEST_H
#define __TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define TEST_PHASE_0_ID   (0U)
#define TEST_PHASE_1_ID   (1U)
#define TEST_PHASE_2_ID   (2U)
#define TEST_PHASE_3_ID   (3U)
#define TEST_PHASE_4_ID   (4U)
#define TEST_PHASE_5_ID   (5U)

/*
 * Select the active test phase here.
 * Use Phase1 for disconnected-board output checks.
 * Use Phase4 for disconnected-board pseudo Hall verification.
 * Use Phase5 for low-voltage integrated board-level bring-up.
 */
#define TEST_ACTIVE_PHASE  TEST_PHASE_5_ID

void Test_Init(void);
void Test_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __TEST_H */
