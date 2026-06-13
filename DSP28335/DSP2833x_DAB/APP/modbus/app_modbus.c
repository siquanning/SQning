/*
 * app_modbus.c — DAB 应用寄存器桥接
 *
 * 桥接 Modbus 寄存器数组与 DAB 系统变量。
 * 其他模块（控制、保护）写入 g_* 全局变量；本模块在读取时
 * 将其复制到 InputRegs[]，并在写入时验证/应用 HoldingRegs[]。
 */

#include "app_modbus.h"

// ---- 寄存器索引常量 ----------------------------------------------------------
#define REG_V2_REF        0
#define REG_SOFTSTART     1
#define REG_PID_KP        2
#define REG_PID_KI        3
#define REG_PID_KD        4
#define REG_COMMAND       5
#define REG_OVP_THR       6
#define REG_OCP_THR       7

// ---- 应用层全局变量定义 ------------------------------------------------------
Uint16 g_V2_actual  = 0;
Uint16 g_Power      = 0;
Uint16 g_D1         = 0;
Uint16 g_D2         = 0;
Uint16 g_State      = 0;
Uint16 g_FaultCode  = 0;

// ---- 默认值（PRD §9.2）-------------------------------------------------------
#define DEFAULT_V2_REF          0
#define DEFAULT_SOFTSTART_TIME  100
#define DEFAULT_KP              1000
#define DEFAULT_KI              100
#define DEFAULT_KD              1000
#define DEFAULT_COMMAND         0
#define DEFAULT_OVP_THR         1200
#define DEFAULT_OCP_THR         150

// ---- 有效范围 ----------------------------------------------------------------
#define V2_REF_MAX      2000
#define SOFTSTART_MIN   10
#define SOFTSTART_MAX   5000
#define KP_MAX          10000
#define KI_MAX          10000
#define KD_MAX          10000
#define COMMAND_MAX     2
#define OVP_THR_MAX     3000
#define OCP_THR_MAX     500

void MB_InitRegs(void)
{
    HoldingRegs[REG_V2_REF]    = DEFAULT_V2_REF;
    HoldingRegs[REG_SOFTSTART] = DEFAULT_SOFTSTART_TIME;
    HoldingRegs[REG_PID_KP]    = DEFAULT_KP;
    HoldingRegs[REG_PID_KI]    = DEFAULT_KI;
    HoldingRegs[REG_PID_KD]    = DEFAULT_KD;
    HoldingRegs[REG_COMMAND]   = DEFAULT_COMMAND;
    HoldingRegs[REG_OVP_THR]   = DEFAULT_OVP_THR;
    HoldingRegs[REG_OCP_THR]   = DEFAULT_OCP_THR;
}

Uint16 MB_ApplyRegChanges(Uint16 reg_addr)
{
    switch (reg_addr)
    {
        case REG_V2_REF:
            if (HoldingRegs[REG_V2_REF] > V2_REF_MAX)
                { HoldingRegs[REG_V2_REF] = DEFAULT_V2_REF; return 0; }
            break;

        case REG_SOFTSTART:
            if (HoldingRegs[REG_SOFTSTART] < SOFTSTART_MIN ||
                HoldingRegs[REG_SOFTSTART] > SOFTSTART_MAX)
                { HoldingRegs[REG_SOFTSTART] = DEFAULT_SOFTSTART_TIME; return 0; }
            break;

        case REG_PID_KP:
            if (HoldingRegs[REG_PID_KP] > KP_MAX)
                { HoldingRegs[REG_PID_KP] = DEFAULT_KP; return 0; }
            break;

        case REG_PID_KI:
            if (HoldingRegs[REG_PID_KI] > KI_MAX)
                { HoldingRegs[REG_PID_KI] = DEFAULT_KI; return 0; }
            break;

        case REG_PID_KD:
            if (HoldingRegs[REG_PID_KD] > KD_MAX)
                { HoldingRegs[REG_PID_KD] = DEFAULT_KD; return 0; }
            break;

        case REG_COMMAND:
            if (HoldingRegs[REG_COMMAND] > COMMAND_MAX)
                { HoldingRegs[REG_COMMAND] = DEFAULT_COMMAND; return 0; }
            break;

        case REG_OVP_THR:
            if (HoldingRegs[REG_OVP_THR] > OVP_THR_MAX)
                { HoldingRegs[REG_OVP_THR] = DEFAULT_OVP_THR; return 0; }
            break;

        case REG_OCP_THR:
            if (HoldingRegs[REG_OCP_THR] > OCP_THR_MAX)
                { HoldingRegs[REG_OCP_THR] = DEFAULT_OCP_THR; return 0; }
            break;

        default:
            return 0;
    }
    return 1;
}

void MB_ReadInputRegs(void)
{
    InputRegs[0] = g_V2_actual;
    InputRegs[1] = g_Power;
    InputRegs[2] = g_D1;
    InputRegs[3] = g_D2;
    InputRegs[4] = g_State;
    InputRegs[5] = g_FaultCode;
}
