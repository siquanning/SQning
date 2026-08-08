/**
 * @file    APP_CONFIG.c
 * @brief   应用层配置 — GPIO, 外设初始化, 硬件引脚定义
 *
 * 接口约定:
 *   - AppConfig_Init() 在 main() 的步骤4(外设初始化)中调用
 *   - 每个 GPIO 配置函数内部用 EALLOW/EDIS 保护
 *   - 外设初始化函数负责:
 *       1) 调用 InitXxxGpio() 配置 GPIO 复用
 *       2) 配置外设寄存器参数
 *       3) 配置中断/Trip Zone/触发源
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "APP_CONFIG.h"

/* ================================================================
 * GPIO 管理
 * ================================================================ */

/**
 * @brief 初始化所有应用层 GPIO
 *
 * 引脚分配 (方案A — 单从机，CS 永久拉低):
 *   GPIO16 = SPISIMOA (MUX=3), 输出
 *   GPIO17 = SPISOMIA (MUX=3), 输入
 *   GPIO18 = SPICLKA  (MUX=3), 输出
 *   GPIO19 = GPIO 输出低 (永久 CS, 单从机)
 *   GPIO35 = SCITXDA  (MUX=2), 输出
 *   GPIO36 = SCIRXDA  (MUX=2), 输入
 *   GPIO67 = LED TX (GPIO 输出高灭)
 *   GPIO68 = LED RX (GPIO 输出高灭)
 *
 * 约定:
 *   - GPIO 方向: GpioCtrlRegs.GPxDIR.bit.GPIOx = 1(输出) / 0(输入)
 *   - GPIO 复用: GpioCtrlRegs.GPxMUXy.bit.GPIOx = 0(GPIO) / 其他值(外设功能)
 */
void AppConfig_InitGpio(void)
{
    EALLOW;

    /* ---- SPI-A 引脚 (GPIO16-18) ---- */
    /* GPIO16 = SPISIMOA (MOSI → CPLD SOMI), 输出方向 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 3;
    GpioCtrlRegs.GPADIR.bit.GPIO16  = 1;

    /* GPIO17 = SPISOMIA (MISO ← CPLD SIMO), 输入方向 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 3;
    GpioCtrlRegs.GPADIR.bit.GPIO17  = 0;

    /* GPIO18 = SPICLKA (时钟 → CPLD), 输出方向 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 3;
    GpioCtrlRegs.GPADIR.bit.GPIO18  = 1;

    /* GPIO19 = 普通 GPIO 输出, 拉低 → CPLD 片选永久选中 (方案A: 单从机) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO19  = 1;
    GpioDataRegs.GPACLEAR.bit.GPIO19 = 1;

    /* ---- SCI-A 引脚 (GPIO35-36, MUX=2 备选位置) ---- */
    /* GPIO35 = SCITXDA (TX → PC RX), 输出方向 */
    GpioCtrlRegs.GPBMUX1.bit.GPIO35 = 2;
    GpioCtrlRegs.GPBDIR.bit.GPIO35  = 1;

    /* GPIO36 = SCIRXDA (RX ← PC TX), 输入方向 */
    GpioCtrlRegs.GPBMUX1.bit.GPIO36 = 2;
    GpioCtrlRegs.GPBDIR.bit.GPIO36  = 0;

    /* ---- LED 引脚 (GPIO67-68, GPIOC 端口, 低电平点亮) ---- */
    /* GPIO67 = TX LED, 初始拉高 (灭) */
    GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO67  = 1;
    GpioDataRegs.GPCSET.bit.GPIO67   = 1;

    /* GPIO68 = RX LED, 初始拉高 (灭) */
    GpioCtrlRegs.GPCMUX1.bit.GPIO68 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO68  = 1;
    GpioDataRegs.GPCSET.bit.GPIO68   = 1;

    EDIS;
}

/* ================================================================
 * 外设初始化
 * ================================================================ */

/**
 * @brief ePWM 外设初始化
 *
 * 模板参数:
 *   - PWM_CLK   (Hz):  开关频率
 *   - SP:              时基周期 = CPU_CLK / (2 * PWM_CLK)
 *   - TBCTLVAL:        时基控制字
 *   - DBRED/DBFED:     死区上升/下降延时
 */
#if 0  /* TODO: 取消此 #if 0 并实现 */
void AppConfig_InitEPwm(void)
{
    /* 1. 配置 GPIO 复用为 ePWM 功能 */
    InitEPwm1Gpio();   /* GPIO0=PWM1A, GPIO1=PWM1B */
    InitEPwm2Gpio();   /* GPIO2=PWM2A, GPIO3=PWM2B */
    /* TODO: 按需添加 InitEPwm3Gpio() ~ InitEPwm6Gpio() */

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;  /* 停止所有 TB 时钟 */

    /* 2. 配置 ePWM1 时基 */
    EPwm1Regs.TBPRD = SP;                     /* 周期 */
    EPwm1Regs.TBPHS.half.TBPHS = 0;           /* 相位 = 0 */
    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UPDOWN;
    EPwm1Regs.TBCTL.bit.HSPCLKDIV = TB_DIV1;
    EPwm1Regs.TBCTL.bit.CLKDIV    = TB_DIV1;

    /* 3. 配置比较值 / 动作限定器 / 死区 */

    /* 4. 配置 Trip-Zone */

    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;   /* 同步启动所有 TB */
    EDIS;
}
#endif

/* TODO: 添加其他外设初始化函数
 *
 * void AppConfig_InitADC(void)    — 配置 ADC (级联/双序列, 采样窗, SOC 触发源)
 * void AppConfig_InitSCI(void)    — 配置 SCI 串口 (波特率, FIFO, 中断)
 * void AppConfig_InitSPI(void)    — 配置 SPI (模式, 速率, FIFO)
 * void AppConfig_InitECan(void)   — 配置 eCAN (波特率, 邮箱, 中断)
 * void AppConfig_InitXINTF(void)  — 配置外部存储器接口时序
 */

/* ================================================================
 * 应用层初始化入口
 * ================================================================ */

/**
 * @brief 应用层总初始化 — 在 main() 外设初始化阶段调用
 *
 * 调用顺序:
 *   1. AppConfig_InitGpio()
 *   2. AppConfig_InitEPwm()  (或其他外设初始化函数)
 */
void AppConfig_Init(void)
{
    AppConfig_InitGpio();

    /* TODO: 按需调用外设初始化函数 */
    /* AppConfig_InitEPwm(); */
    /* AppConfig_InitADC();  */
    /* AppConfig_InitSCI();  */
}
