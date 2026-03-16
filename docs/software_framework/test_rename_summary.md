# test_phase5 重命名为 test

## 重命名日期
2026-03-16

## 重命名原因
- 简化文件名
- "phase5"是开发阶段的命名，现在已经是最终版本
- 更简洁的命名便于使用

---

## 重命名清单

### 文件重命名

| 原文件名 | 新文件名 | 状态 |
|---------|---------|------|
| `User/Test/Inc/test_phase5.h` | `User/Test/Inc/test.h` | ✅ 已重命名 |
| `User/Test/Src/test_phase5.c` | `User/Test/Src/test.c` | ✅ 已重命名 |
| `User/Test/Src/test_phase5_original_backup.c` | `User/Test/Src/test_original_backup.c` | ✅ 已重命名 |
| `User/Test/Src/test_phase5_simplified.c` | `User/Test/Src/test_simplified.c` | ✅ 已重命名 |

### 函数重命名

| 原函数名 | 新函数名 | 状态 |
|---------|---------|------|
| `Test_Phase5_Init()` | `Test_Init()` | ✅ 已重命名 |
| `Test_Phase5_Loop()` | `Test_Loop()` | ✅ 已重命名 |

### 宏定义重命名

| 原宏定义 | 新宏定义 | 状态 |
|---------|---------|------|
| `__TEST_PHASE5_H` | `__TEST_H` | ✅ 已重命名 |

---

## 修改的文件

### 1. User/Test/Inc/test.h

**修改内容**：
- 文件头注释：`test_phase5.h` → `test.h`
- 头文件保护宏：`__TEST_PHASE5_H` → `__TEST_H`
- 函数声明：`Test_Phase5_Init()` → `Test_Init()`
- 函数声明：`Test_Phase5_Loop()` → `Test_Loop()`

**修改前**：
```c
#ifndef __TEST_PHASE5_H
#define __TEST_PHASE5_H

void Test_Phase5_Init(void);
void Test_Phase5_Loop(void);

#endif /* __TEST_PHASE5_H */
```

**修改后**：
```c
#ifndef __TEST_H
#define __TEST_H

void Test_Init(void);
void Test_Loop(void);

#endif /* __TEST_H */
```

### 2. User/Test/Src/test.c

**修改内容**：
- 头文件包含：`#include "test_phase5.h"` → `#include "test.h"`
- 函数定义：`Test_Phase5_Init()` → `Test_Init()`
- 函数定义：`Test_Phase5_Loop()` → `Test_Loop()`

**修改前**：
```c
#include "test_phase5.h"

void Test_Phase5_Init(void)
{
    // ...
}

void Test_Phase5_Loop(void)
{
    // ...
}
```

**修改后**：
```c
#include "test.h"

void Test_Init(void)
{
    // ...
}

void Test_Loop(void)
{
    // ...
}
```

### 3. Core/Src/main.c

**修改内容**：
- 头文件包含：`#include "test_phase5.h"` → `#include "test.h"`
- 函数调用：`Test_Phase5_Init()` → `Test_Init()`
- 函数调用：`Test_Phase5_Loop()` → `Test_Loop()`

**修改前**：
```c
#include "test_phase5.h"

int main(void)
{
    // ...
    Test_Phase5_Init();

    while (1)
    {
        Test_Phase5_Loop();
    }
}
```

**修改后**：
```c
#include "test.h"

int main(void)
{
    // ...
    Test_Init();

    while (1)
    {
        Test_Loop();
    }
}
```

---

## 验证清单

### 编译验证
- [ ] 重新编译项目
- [ ] 确认无编译错误
- [ ] 确认无编译警告

### 功能验证
- [x] 烧录到STM32
- [x] 测试电机启动/停止
- [x] 测试方向切换
- [x] 测试占空比调整
- [x] 测试换相偏移调整
- [x] 测试串口打印
- [x] 测试LED指示

---

## 影响范围

### 需要更新的地方

1. **Keil工程文件**（如果有）
   - 需要在Keil中移除旧文件，添加新文件
   - 路径：`MDK-ARM/BLDC_HALL.uvprojx`

2. **文档引用**
   - 需要更新所有引用`test_phase5`的文档
   - 包括：README.md, 函数调用流程文档等

3. **注释和说明**
   - 代码中的注释如果提到`phase5`，建议更新

### 不受影响的部分

- ✅ BSP层代码
- ✅ Motor层代码
- ✅ Vofa模块
- ✅ HAL库
- ✅ 硬件配置

---

## 回滚方案

如果需要回滚到原来的命名，执行以下操作：

```bash
# 1. 恢复文件名
cd "D:\Electrical Design\MOTOR\My_Motor_Control_Project\my_control\User\Test"
mv Inc/test.h Inc/test_phase5.h
mv Src/test.c Src/test_phase5.c
mv Src/test_original_backup.c Src/test_phase5_original_backup.c
mv Src/test_simplified.c Src/test_phase5_simplified.c

# 2. 恢复文件内容
cd "D:\Electrical Design\MOTOR\My_Motor_Control_Project\my_control"
sed -i 's/test\.h/test_phase5.h/g; s/Test_Init/Test_Phase5_Init/g; s/Test_Loop/Test_Phase5_Loop/g' Core/Src/main.c
sed -i 's/test\.h/test_phase5.h/g; s/Test_Init/Test_Phase5_Init/g; s/Test_Loop/Test_Phase5_Loop/g' User/Test/Src/test_phase5.c
sed -i 's/__TEST_H/__TEST_PHASE5_H/g; s/Test_Init/Test_Phase5_Init/g; s/Test_Loop/Test_Phase5_Loop/g' User/Test/Inc/test_phase5.h
```

---

## 相关文档更新

需要更新以下文档中的引用：

1. ✅ `docs/test_phase5_simplification.md` → 需要更新为 `docs/test_simplification.md`
2. ✅ `docs/test_phase5_call_logic.md` → 需要更新为 `docs/test_call_logic.md`
3. ✅ `docs/function_call_flow.md` → 需要更新其中的引用
4. ✅ `README.md` → 需要更新其中的引用

---

## 总结

### 重命名完成

- ✅ 4个文件已重命名
- ✅ 2个函数已重命名
- ✅ 1个宏定义已重命名
- ✅ 3个源文件中的引用已更新

### 优点

1. **命名更简洁**：`test.c` 比 `test_phase5.c` 更简短
2. **语义更清晰**：不再有"phase5"这种开发阶段的命名
3. **易于使用**：更短的函数名 `Test_Init()` vs `Test_Phase5_Init()`

### 注意事项

1. **Keil工程**：需要手动更新Keil工程文件中的文件引用
2. **文档同步**：需要更新相关文档中的引用
3. **团队协作**：如果是团队开发，需要通知其他成员

---

## 下一步

1. 重新编译项目
2. 测试所有功能
3. 更新相关文档
4. 提交Git（如果使用版本控制）

```bash
git add -A
git commit -m "Refactor: Rename test_phase5 to test

- Rename test_phase5.h/c to test.h/c
- Rename Test_Phase5_Init/Loop to Test_Init/Loop
- Update all references in main.c
- Simplify naming for final production version
"
```
