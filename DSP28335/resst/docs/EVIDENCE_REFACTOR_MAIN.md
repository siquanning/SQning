# EVIDENCE — main.c 职责收敛重构

**Date:** 2026-08-10
**Project:** F28335_RTControl_Platform
**Scope:** main.c 职责拆分，不改变已验证功能

## 1. 改动原因

重构前 `main.c`（173行）同时承担 7 类职责：初始化编排、SCI→SPI队列处理、SPI服务、参数提交、PWM disable消费、状态机调度、诊断汇总与故障检测。这违反了单一职责原则，且 main.c 直接跨模块访问 `StateMachine.pwm_disable_requested`、`ParamManager.commit_requested`、`ParamManager.active.*`、`Diagnostics.*` 等内部字段。

## 2. 服务职责图

```
main.c (10行)
  └─ App_Init(&g_app)              → app.c App_Init()
  └─ App_RunForever(&g_app)        → app.c App_RunForever()
       ├─ foreground (每次迭代)      → App_ServiceForeground()  SCI队列 + SPI bridge + Indicator
       ├─ 1ms  (Scheduler gate)     → App_Service1ms()         Param_ServicePendingCommit + PWM disable消费
       ├─ 10ms (Scheduler gate)     → App_Service10ms()         StateMachine_Service
       └─ 100ms(Scheduler gate)     → App_Service100ms()        诊断汇总 + 故障检测
```

## 3. API 变更摘要

### 新增模块

| 文件 | 说明 |
|------|------|
| `firmware/app/app.h` | App_Init, App_RunForever, 4个 Service 函数声明 |
| `firmware/app/app.c` | 全部实现 + 内部 static Scheduler（唯一写入者：App层） |

### 新增 API

| 模块 | API | 语义 |
|------|-----|------|
| StateMachine | `StateMachine_ConsumePwmDisableRequest()` | 临界区保护的单次 read-and-clear，替代 IsPending+Clear 两步 |
| StateMachine | `StateMachine_GetDiagSnapshot()` | 读取 state/first_fault/fault_tick |
| ParamManager | `Param_ServicePendingCommit()` | 内部检查 commit_requested → 校验 → 提交 → 更新诊断，App 层不接触内部字段 |
| ParamManager | `Param_GetDiagSnapshot()` | 读取 commit_count/reject_count/last_reject_reason |
| Diagnostics | `Diagnostics_SetSchedulerStats()` 等 6 个 batch setter | 替代主循环中逐字段写 Diagnostics |

### init_pass = 0UL 语义修正

- 原注释写 `/* init_pass = */ 0UL`，但参数名实际为 `diag_flags`，且 0 语义为"无自检失败=自检通过"，与 `init_pass=0` 字面含义相反
- 新增 `tests/host/test_init_diag.c` T1-T2 覆盖：diag_flags MSB 清除→INIT→STANDBY；MSB 置位→INIT 阻塞

## 4. 并发处理说明

### PWM disable request（StateMachine_ConsumePwmDisableRequest）

- System_EnterFault()（ISR 或 main 可调用）写入 `pwm_disable_requested = 1`
- Consume 在 C2000 上使用 `__disable_interrupts()` / `__enable_interrupts()` 临界区保护 read-and-clear
- Host 测试为单线程，无竞争条件
- 即使临界区保护失败（double-consume 空转），最坏情况是 PWM disable 延迟至下一个 1ms tick，不会永久丢失

### commit_requested（Param_ServicePendingCommit）

- `commit_requested` 仅由 App 层（background）写入和读取
- ISR 不操作 ParamManager 字段
- 不存在并发的 TOCTOU 问题

## 5. 不变项确认

- UART→SPI 协议、SCI 队列行为、Scheduler 1/10/100ms 周期：未改动
- PWM 安全锁（AQCSFRC force LOW, TBCLKSYNC=0）：未改动
- `BOARD_PWM_ADC_HW_CONFIRMED=0U`：未改动
- ADC/PWM/Trip Zone 硬件配置：未改动
- 四种构建配置（Prototype_RAM, Prototype_Flash, Industrial_RAM, Industrial_Flash）：全部通过

## 6. 测试结果

### Host 测试（9/9 PASS）

```
test_sci_rx_queue     PASS
test_uart_frame       PASS
test_spi_request      PASS
test_spi_bridge       PASS
test_step3_control    PASS
test_step3_state      PASS
test_step3_params     PASS
test_step3_telemetry  PASS
test_init_diag        PASS  ← 新增
```

### C2000 构建（4/4 PASS, 0 error 0 warning）

```
Prototype_RAM_Debug        SUCCESS
Prototype_Flash_Demo       SUCCESS
Industrial_RAM_Debug       SUCCESS
Industrial_Flash_Release   SUCCESS
```

## 7. 最终 main.c

```c
#include "firmware/app/app.h"

static AppContext g_app;

void main(void)
{
    App_Init(&g_app);
    App_RunForever(&g_app);
}
```
