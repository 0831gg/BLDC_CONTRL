# Session Memory - 2026-03-12

## Current repo state

- Git repo: `D:\Electrical Design\MOTOR\My_Motor_Control_Project\my_control\MDK-ARM`
- Active branch: `main`
- Recent commits in this session:
  - `be5b36b` - `Phase 1 complete: PWM/SD self-test, 10kHz timing, protection baseline`
  - `7a7ec2f` - `Phase 1 debug: disable break for PWM verification`
  - `289e25b` - `Phase 1 debug: restore low-active break protection`

## Clock and PWM conclusions

- System clock is confirmed at `168 MHz`
- TIM1 PWM target is confirmed at `10 kHz`
- Current TIM1 base settings:
  - `PSC = 0`
  - `CounterMode = center-aligned`
  - `ARR = 8399`
- PWM frequency formula used:
  - `Fpwm = 168000000 / (2 * (8399 + 1)) = 10000 Hz`

## Phase 1 current structure

- Main entry is already switched from `test_phase0` to `test_phase1`
- Relevant files:
  - `Core/Src/main.c`
  - `User/Test/Src/test_phase1.c`
  - `User/Bsp/Src/bsp_ctrl_sd.c`
  - `User/Bsp/Src/bsp_pwm.c`
- `test_phase1` step logic currently is:
  - `Step0`: SD disable, PWM off, lower off
  - `Step1`: SD enable only
  - `Step2`: CH1/2/3 50% PWM
  - `Step3`: U phase PWM only
  - `Step4`: lower bridge UN/WN on
  - `Step5`: all outputs off

## Important hardware/software findings

- `PWM_Enable` / `PC13` logic is confirmed:
  - `PC13 = 0` -> driver disabled
  - `PC13 = 1` -> driver enabled
- Hall inputs `PA0/PA1/PA2` are configured with internal pull-up
- ADC2 injected trigger is configured to `TIM1_CC4` rising edge
- TIM1 master trigger is configured to `OC4REF`

## Break protection conclusion

- Break logic is confirmed to be the main reason `Step2` previously had no PWM
- Current intended Break logic is:
  - `PB12 -> TIM1_BKIN`
  - low level active fault
  - when `PB12` is low, TIM1 Break triggers and PWM is shut down
- Proof from testing:
  - after temporarily disabling Break, `Step2` PWM waveform became visible
  - therefore PWM path itself is functional
  - root cause is in BKIN/Break chain, not the PWM start code

## Current Break configuration

- Break is now restored and enabled again
- Current code intent:
  - `TIM_BREAKINPUTSOURCE_ENABLE`
  - `TIM_BREAK_ENABLE`
  - `TIM_BREAKINPUTSOURCE_POLARITY_LOW`
  - `TIM_BREAKPOLARITY_LOW`
- `PB12` currently uses pull-up in code to try to keep non-fault idle level high

## Break callback behavior

- In `User/Bsp/Src/bsp_pwm.c`, `HAL_TIMEx_BreakCallback()` does:
  - set software fault flag
  - lower bridge all off
  - PWM all close
  - SD disable
- `test_phase1` main loop polls the break fault flag and prints:
  - `[P1] Break fault detected`

## Current diagnostics added

- `test_phase1` prints startup message showing Break status
- After each step change, it prints:
  - current step
  - SD state
  - duty
  - break flag

## What still needs checking next time

- Measure actual `PB12` level during idle and during Step2
- Check hardware schematic for BKIN fault source:
  - whether fault output is open-drain or push-pull
  - whether low level active is correct on the real board
  - whether external pull-up already exists
  - whether there is inverter/comparator/opto in the fault path
- Confirm why enabling Break causes no PWM:
  - `PB12` really low
  - floating/noisy BKIN
  - external fault asserted by driver/protection circuit

## Expected LED behavior in test_phase1

- `Step0`: LED0 off, LED1 off
- `Step1`: LED0 on, LED1 off
- `Step2`: LED0 off, LED1 on
- `Step3`: LED0 on, LED1 on
- `Step4`: LED0 toggles, LED1 off
- `Step5`: LED0 off, LED1 toggles

## Suggested restart point for tomorrow

1. Open and read this file first: `session_memory_2026-03-12.md`
2. Check `PB12/BKIN` actual hardware level
3. Re-test `Step2`
4. If Break still kills PWM, focus only on BKIN hardware path and polarity
