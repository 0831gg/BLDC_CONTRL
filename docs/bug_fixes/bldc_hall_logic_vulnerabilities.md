# BLDC Hall 六步换向控制 — 调用逻辑漏洞报告

> 文件路径：`docs/bug_fixes/bldc_hall_logic_vulnerabilities.md`
> 日期：2026-03-17
> 涉及文件：`motor_ctrl.c` / `bsp_adc_motor.c` / `bsp_hall.c`

---

## 漏洞总览

| # | 严重程度 | 文件 | 问题描述 | 状态 |
|---|---------|------|---------|------|
| 1 | 高危 | `motor_ctrl.c:390` | `s_motor_running=1` 早于 `sd_enable()`，TIM1中断提前换相 | 待修复 |
| 2 | 高危 | `bsp_adc_motor.c:229` | 三相电流非原子写入，主循环可读到撕裂数据 | 待修复 |
| 3 | 中危 | `motor_ctrl.c:380` | `motor_start_simple()` 缺少启动前 `sd_disable`+`lower_all_off` 保护 | 待修复 |
| 4 | 中危 | `bsp_hall.c:385` | 物理旋转方向与换相方向无交叉校验 | 待修复 |
| 5 | 中危 | `bsp_hall.c:211` | `bsp_hall_get_speed()` 主循环读与中断写无互斥 | 待修复 |
| 6 | 低危 | `motor_ctrl.c:384` | `duty` 参数无上限边界检查 | 待修复 |
| 7 | 低危 | `bsp_hall.c:311` | `bsp_hall_clear_stats()` 主循环调用时与TIM1中断存在竞态窗口 | 待修复 |

---

## 漏洞详情

### 漏洞1（高危）— `s_motor_running=1` 早于 `sd_enable()`

**位置：** [motor_ctrl.c:390](../../User/Motor/Src/motor_ctrl.c#L390)

**问题：**
```c
s_motor_running = 1U;        // L390: TIM1中断从此刻起开始执行换相
motor_ctrl_set_startup_trace(...);
motor_sensor_mode_phase();
// ...
bsp_ctrl_sd_enable();        // L404: 驱动芯片才在这里使能
```
TIM1更新中断（10kHz）在 `s_motor_running=1` 之后、`bsp_ctrl_sd_enable()` 之前，
会调用 `bsp_hall_poll_and_commutate()` → `motor_sensor_mode_phase()`，
向PWM写入换相动作，但驱动芯片SD脚还未使能，首次换相指令丢失或时序错乱。

**修复方案：** 将 `s_motor_running=1` 移到 `bsp_ctrl_sd_enable()` 之后，
或在置位前先屏蔽TIM1更新中断。

---

### 漏洞2（高危）— 三相电流非原子写入

**位置：** [bsp_adc_motor.c:229](../../User/Bsp/Src/bsp_adc_motor.c#L229)

**问题：**
```c
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC2) {
        s_phase_u_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1); // 写U
        s_phase_v_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2); // 写V
        s_phase_w_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3); // 写W
        s_injected_update_count++;
    }
}
```
主循环调用 `bsp_adc_get_phase_current()` 时，可能读到U已更新、V/W还是旧值的撕裂状态，
影响过流保护判断的准确性。

**修复方案：** 引入本地快照缓冲区，在回调中一次性写入，主循环读快照。

---

### 漏洞3（中危）— `motor_start_simple()` 缺少启动前保护

**位置：** [motor_ctrl.c:380](../../User/Motor/Src/motor_ctrl.c#L380)

**问题：**
`motor_start_simple()` 相比 `motor_ctrl_prepare_start()`（被 `motor_start()` 使用）
跳过了以下两步：
```c
bsp_ctrl_sd_disable();    // 缺失
bsp_pwm_lower_all_off();  // 缺失
```
电机停止后再调用 `motor_start_simple()`，下桥PWM输出状态可能不干净，
导致启动瞬间出现非预期的相电流路径。

**修复方案：** 在 `motor_start_simple()` 开头补充这两个调用。

---

### 漏洞4（中危）— 物理方向与换相方向无交叉校验

**位置：** [bsp_hall.c:385](../../User/Bsp/Src/bsp_hall.c#L385)

**问题：**
`bsp_hall_poll_and_commutate()` 检测到的物理旋转方向存入 `s_direction`，
但 `motor_sensor_mode_phase()` 查换相表用的是 `s_motor_direction`（上层设置）。
两者没有交叉校验：电机实际反转时，换相表仍用错误方向，持续输出错误换相而不报错。

**修复方案：** 在 `bsp_hall_poll_and_commutate()` 检测到稳定方向后，
与 `s_motor_direction` 比对，不一致时设置 `MOTOR_FAULT_HALL_INVALID` 或停机。

---

### 漏洞5（中危）— `bsp_hall_get_speed()` 读写竞态

**位置：** [bsp_hall.c:211](../../User/Bsp/Src/bsp_hall.c#L211)

**问题：**
```c
count = s_period_buffer_count;          // 主循环读count
for (i = 0U; i < count; i++) {
    sum += s_period_buffer[i];          // 主循环读buffer
}
```
读取 `count` 之后、读取 `buffer` 之前，TIM1中断可能已更新
`s_period_buffer_index` 和 `s_period_buffer_count`，
导致读到部分更新的数据，计算出错误的RPM。

**修复方案：** 读取前关闭TIM1更新中断，读完后恢复；或使用局部快照拷贝。

---

### 漏洞6（低危）— `duty` 参数无上限边界检查

**位置：** [motor_ctrl.c:384](../../User/Motor/Src/motor_ctrl.c#L384)

**问题：**
```c
s_motor_duty = duty;  // 无边界检查，直接赋值
bsp_pwm_duty_set(s_motor_duty);
```
若调用方传入超过 `BSP_PWM_PERIOD` 的值，行为取决于 `bsp_pwm_duty_set()` 内部保护，
存在PWM占空比溢出风险。

**修复方案：** 加入 `if (duty > BSP_PWM_PERIOD) duty = BSP_PWM_PERIOD;`。

---

### 漏洞7（低危）— `bsp_hall_clear_stats()` 竞态窗口

**位置：** [bsp_hall.c:311](../../User/Bsp/Src/bsp_hall.c#L311)

**问题：**
`bsp_hall_clear_stats()` 在主循环（`motor_start_simple()`）中调用，
逐条清零 `s_valid_transition_count`、`s_period_buffer[]` 等变量。
TIM1中断可能在清零序列中途写入这些变量，导致清零不完整或状态不一致。

**修复方案：** 清零前关闭TIM1更新中断，清零后恢复。

---

## 修复进度

- [x] 漏洞1：`s_motor_running` 时序 — `motor_ctrl.c` `motor_start_simple()`
- [x] 漏洞2：三相电流原子读写 — `bsp_adc_motor.c` 引入 `phase_snapshot_t`
- [x] 漏洞3：启动前保护 — `motor_ctrl.c` `motor_start_simple()` 补充 `sd_disable`+`lower_all_off`
- [x] 漏洞4：方向交叉校验 — `bsp_hall.c` `bsp_hall_poll_and_commutate()` 加入方向比对
- [x] 漏洞5：RPM读写互斥 — `bsp_hall.c` `bsp_hall_get_speed()` 加入 NVIC 临界区保护
- [x] 漏洞6：duty边界检查 — `motor_ctrl.c` `motor_start_simple()` 加入上限截断
- [x] 漏洞7：clear_stats中断保护 — `bsp_hall.c` `bsp_hall_clear_stats()` 加入 NVIC 临界区保护
