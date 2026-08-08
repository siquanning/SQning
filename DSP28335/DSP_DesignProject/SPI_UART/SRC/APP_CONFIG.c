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
 *   - GPIO 复用: GpioCtrlRegs.GPxMUXy.bit.GPIOx = 0(GPIO) / 1(SCI) / 3(SPI)
 *   - 输入限定: GpioCtrlRegs.GPxQSELy.bit.GPIOx: 3=异步 (通信外设)
 */
void AppConfig_InitGpio(void)
{
    EALLOW;

    /* ---- SCI-A 引脚 (GPIO35=TX, GPIO36=RX) ---- */
    /* GPIO35: SCI-A TX, MUX=1 (SCI 主功能), QSEL=3 (异步), PUD=0 (上拉) */
    GpioCtrlRegs.GPBMUX1.bit.GPIO35 = 1;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO35 = 0;

    /* GPIO36: SCI-A RX, MUX=1 (SCI 主功能), QSEL=3 (异步), PUD=0 (上拉) */
    GpioCtrlRegs.GPBMUX1.bit.GPIO36 = 1;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;
    GpioCtrlRegs.GPBPUD.bit.GPIO36 = 0;

    /* ---- LED 引脚 (GPIO67=TX_LED, GPIO68=RX_LED) ---- */
    /* GPIO67: GPIO 输出, 初始高电平 (LED 灭) */
    GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO67 = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO67 = 0;
    SET_TXLED;      /* 初始高电平 → LED 灭 */

    /* GPIO68: GPIO 输出, 初始高电平 (LED 灭) */
    GpioCtrlRegs.GPCMUX1.bit.GPIO68 = 0;
    GpioCtrlRegs.GPCDIR.bit.GPIO68 = 1;
    GpioCtrlRegs.GPCPUD.bit.GPIO68 = 0;
    SET_RXLED;      /* 初始高电平 → LED 灭 */

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

/**
 * @brief SCI-A 初始化 — 9600bps 8N1, FIFO 增强模式, RX 中断
 *
 * 按 PRD §5.5 寄存器初始化序列配置:
 *   1. SCICCR=0x0007 (8 数据位/1 停止位/无校验/空闲线模式)
 *   2. SCICTL1=0x0003 (RXENA+TXENA, SWRESET=0 保持复位)
 *   3. SCICTL2.RXBKINTENA=1 (使能接收中断)
 *   4. 波特率 BRR=487=0x1E7 (9600bps @ LSPCLK=37.5MHz)
 *   5. SCIFFTX=0xE040 (FIFO 使能, SCIRST=1, TX FIFO 复位)
 *   6. SCIFFRX=0x2041 + RXFFIENA=1 (RX FIFO 复位, 触发深度=1)
 *   7. SCIFFCT=0x0000 (无自动波特率, 无 TX 延迟)
 *   8. SCICTL1=0x0027 (SWRESET=1, 模块释放复位开始运行)
 *   9. 注册 SCIRXINTA ISR 到 PIE 向量表
 *   10. 使能 PIE Group 9.1 (SCI RX) + CPU INT9
 *
 * 注意: 本函数不调用 EINT/ERTM, 由 main() 在所有外设初始化完成后统一开启
 */
void AppConfig_InitSci(void)
{
    /* 清除可能残留的 SCI 中断标志, 避免上电误触发 */
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    PieCtrlRegs.PIEACK.all = 0xFFFF;

    /* 1. 通信参数: 8 数据位, 1 停止位, 无校验, 空闲线模式 */
    SciaRegs.SCICCR.all = 0x0007;

    /* 2. 初始控制: RXENA=1, TXENA=1, SWRESET=0 (模块保持复位, 写完所有寄存器再释放) */
    SciaRegs.SCICTL1.all = 0x0003;

    /* 3. 使能接收中断总开关 */
    SciaRegs.SCICTL2.bit.RXBKINTENA = 1;

    /* 4. 波特率: BRR = LSPCLK/(Baud×8)−1 = 37500000/76800−1 = 487 = 0x1E7 */
    SciaRegs.SCIHBAUD = (SCI_BRR_VALUE >> 8) & 0xFF;  /* 高字节 0x01 */
    SciaRegs.SCILBAUD = SCI_BRR_VALUE & 0xFF;         /* 低字节 0xE7 */

    /* 5. FIFO 发送配置: SCIFFENA=1 (增强模式), SCIRST=1, TXFIFO 复位 */
    SciaRegs.SCIFFTX.all = 0xE040;

    /* 6. FIFO 接收配置: RXFIFO 复位, 触发深度=1 字节, 使能 RX FIFO 中断 */
    SciaRegs.SCIFFRX.all = 0x2041;
    SciaRegs.SCIFFRX.bit.RXFFIENA = 1;

    /* 7. FIFO 控制: 无自动波特率检测, 无 TX 发送延迟 */
    SciaRegs.SCIFFCT.all = 0x0000;

    /* 8. 释放模块复位 (SWRESET=1), SCI 开始工作 */
    SciaRegs.SCICTL1.all = 0x0027;

    /* 9. 注册 SCI RX 中断向量到 PIE 向量表 (受 EALLOW 保护) */
    EALLOW;
    PieVectTable.SCIRXINTA = &ISRSciRx;
    EDIS;

    /* 10. 使能 PIE Group 9 通道 1 (SCIRXINTA) + CPU INT9 */
    PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
    IER |= M_INT9;
}

/* ================================================================
 * SCI 字节级收发封装
 * ================================================================ */

/**
 * @brief 通过 SCI-A 发送一字节（阻塞等待 TX FIFO 非满）
 *
 * 在 8 位数据模式下, SCITXBUF 低 8 位为待发送数据,
 * 高 8 位为 SCIFFTX 控制位（0 即可）。
 *
 * @param data 待发送字节 (仅低 8 位有效)
 */
void SciSendByte(Uint16 data)
{
    /* 等待 TX FIFO 有空位（TXFFST < 16 即可写入） */
    while (SciaRegs.SCIFFTX.bit.TXFFST >= 16)
    {
        /* FIFO 满则原地等待, 由 SCI 硬件自动移出数据 */
    }

    SciaRegs.SCITXBUF = (data & 0xFF);
}

/**
 * @brief 从 SCI-A 读取一字节（非阻塞，调用前应确认 FIFO 非空）
 *
 * 在 8 位数据模式下, SCIRXBUF 低 8 位为接收数据。
 * 调用方在 ISR 中通过 RXFFST 判断 FIFO 中是否有数据再调用本函数。
 *
 * @return 接收到的字节 (仅低 8 位有效)
 */
Uint16 SciReceiveByte(void)
{
    return (SciaRegs.SCIRXBUF.all & 0xFF);
}

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
    AppConfig_InitSci();

    /* TODO: 按需调用其他外设初始化函数 */
    /* AppConfig_InitEPwm(); */
    /* AppConfig_InitADC();  */
}
