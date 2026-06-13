/*
 * app_modbus.h — 应用层 Modbus 寄存器映射（PRD §9）
 *
 * 保持寄存器（40001–40008, R/W）：
 *   [0] V2_ref         — 输出电压给定（0~200.0V，单位 0.1V）
 *   [1] SoftStartTime  — 软启动斜坡时间（10~5000 ms）
 *   [2] PID_Kp         — 比例系数 ×1000
 *   [3] PID_Ki         — 积分系数 ×1000
 *   [4] PID_Kd         — 微分系数 ×1000
 *   [5] Command        — 0=停机, 1=启动, 2=清除故障（写后自动清零）
 *   [6] OVP_Threshold  — 过压保护阈值（0~300.0V，单位 0.1V）
 *   [7] OCP_Threshold  — 过流保护阈值（0~50.0A，单位 0.1A）
 *
 * 输入寄存器（30001–30006, R/O）：
 *   [0] V2_Actual      — 实际输出电压（0~200.0V，单位 0.1V）
 *   [1] Power          — 输出功率（W×10）
 *   [2] D1             — 当前 D1 移相角 ×1000
 *   [3] D2             — 当前 D2 移相角 ×1000
 *   [4] State          — 0=IDLE, 1=SOFT_START, 2=RUNNING, 3=FAULT
 *   [5] FaultCode      — 0=无故障, 1=过压, 2=过流
 */

#ifndef APP_MODBUS_APP_MODBUS_H_
#define APP_MODBUS_APP_MODBUS_H_

#include "modbus_slave.h"

// ---- 应用层全局变量（由控制/保护模块写入） -----------------------------------
extern Uint16 g_V2_actual;       // 单位 0.1V
extern Uint16 g_Power;           // W×10
extern Uint16 g_D1;              // ×1000
extern Uint16 g_D2;              // ×1000
extern Uint16 g_State;           // 0=IDLE, 1=SOFTSTART, 2=RUNNING, 3=FAULT
extern Uint16 g_FaultCode;       // 0=无, 1=OVP, 2=OCP

// ---- Modbus ↔ 应用桥接 -------------------------------------------------------
void   MB_InitRegs(void);
Uint16 MB_ApplyRegChanges(Uint16 reg_addr);
void   MB_ReadInputRegs(void);

#endif /* APP_MODBUS_APP_MODBUS_H_ */
