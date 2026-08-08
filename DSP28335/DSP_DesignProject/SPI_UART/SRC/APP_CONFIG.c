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
 * 约定:
 *   - GPIO 方向: GpioCtrlRegs.GPxDIR.bit.GPIOx = 1(输出) / 0(输入)
 *   - GPIO 复用: GpioCtrlRegs.GPxMUXy.bit.GPIOx = 0(GPIO) / 其他值(外设功能)
 *   - 输入限定: GpioCtrlRegs.GPxQSELy.bit.GPIOx 用于去抖动
 */
void AppConfig_InitGpio(void)
{
    EALLOW;

    /* TODO: 在此配置 GPIO 引脚
     *
     * 示例 — 配置 GPIO5 为输出:
     *   GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 0;   // GPIO 功能
     *   GpioCtrlRegs.GPADIR.bit.GPIO5  = 1;   // 输出
     *
     * 示例 — 配置 GPIO6 为输入:
     *   GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 0;   // GPIO 功能
     *   GpioCtrlRegs.GPADIR.bit.GPIO6  = 0;   // 输入
     *   GpioCtrlRegs.GPAQSEL1.bit.GPIO6 = 0;  // 与 SYSCLKOUT 同步
     *
     * 示例 — 配置 LED 指示:
     *   GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 0;
     *   GpioCtrlRegs.GPADIR.bit.GPIO5  = 1;
     */

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
