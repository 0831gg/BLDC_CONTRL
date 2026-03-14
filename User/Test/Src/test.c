#include "test.h"

#include "test_phase0.h"
#include "test_phase1.h"
#include "test_phase2.h"
#include "test_phase3.h"
#include "test_phase4.h"
#include "test_phase5.h"

void Test_Init(void)
{
#if (TEST_ACTIVE_PHASE == TEST_PHASE_0_ID)
    Test_Phase0_Init();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_1_ID)
    Test_Phase1_Init();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_2_ID)
    Test_Phase2_Init();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_3_ID)
    Test_Phase3_Init();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_4_ID)
    Test_Phase4_Init();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_5_ID)
    Test_Phase5_Init();
#else
#error "Unsupported TEST_ACTIVE_PHASE"
#endif
}

void Test_Loop(void)
{
#if (TEST_ACTIVE_PHASE == TEST_PHASE_0_ID)
    Test_Phase0_Loop();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_1_ID)
    Test_Phase1_Loop();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_2_ID)
    Test_Phase2_Loop();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_3_ID)
    Test_Phase3_Loop();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_4_ID)
    Test_Phase4_Loop();
#elif (TEST_ACTIVE_PHASE == TEST_PHASE_5_ID)
    Test_Phase5_Loop();
#else
#error "Unsupported TEST_ACTIVE_PHASE"
#endif
}
