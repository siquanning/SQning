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
 *   GPIO16 = SPISIMOA (MUX=1), 输出, 异步, 内部上拉
 *   GPIO17 = SPISOMIA (MUX=1), 输入, 异步, 内部上拉
 *   GPIO18 = SPICLKA  (MUX=1), 输出, 异步, 内部上拉
 *   GPIO19 = GPIO 输出低 (永久 CS, 单从机)
 *   GPIO35 = SCITXDA  (MUX=1), 输出, 异步, 内部上拉
 *   GPIO36 = SCIRXDA  (MUX=1), 输入, 异步, 内部上拉
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

    /* ---- SPI-A 引脚 (GPIO16-18, MUX=1 = SPI 功能) ---- */
    /* GPIO16 = SPISIMOA (MOSI → CPLD SOMI), 输出方向, 异步, 内部上拉 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO16  = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO16   = 1;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO16 = 3;
    GpioCtrlRegs.GPAPUD.bit.GPIO16   = 0;

    /* GPIO17 = SPISOMIA (MISO ← CPLD SIMO), 输入方向, 异步, 内部上拉 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO17  = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO17   = 0;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3;
    GpioCtrlRegs.GPAPUD.bit.GPIO17   = 0;

    /* GPIO18 = SPICLKA (时钟 → CPLD), 输出方向, 异步, 内部上拉 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO18  = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO18   = 1;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO18 = 3;
    GpioCtrlRegs.GPAPUD.bit.GPIO18   = 0;

    /* GPIO19 = 普通 GPIO 输出, 拉低 → CPLD 片选永久选中 (方案A: 单从机) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO19  = 1;
    GpioDataRegs.GPACLEAR.bit.GPIO19 = 1;

    /* ---- SCI-A 引脚 (GPIO35-36, MUX=1, 参照 DSP2833x_DAB 已验证) ---- */
    /* GPIO35 = SCITXDA (TX → PC RX), 输出方向, 异步模式 */
    GpioCtrlRegs.GPBMUX1.bit.GPIO35  = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO35   = 1;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO35 = 3;   /* 异步 (旁路同步触发器)       */
    GpioCtrlRegs.GPBPUD.bit.GPIO35   = 0;   /* 内部上拉使能               */

    /* GPIO36 = SCIRXDA (RX ← PC TX), 输入方向, 异步模式 */
    GpioCtrlRegs.GPBMUX1.bit.GPIO36  = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO36   = 0;
    GpioCtrlRegs.GPBQSEL1.bit.GPIO36 = 3;   /* 异步 (旁路同步触发器)       */
    GpioCtrlRegs.GPBPUD.bit.GPIO36   = 0;   /* 内部上拉使能               */

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

/* ================================================================
 * SCI (UART) 初始化
 * ================================================================ */

/**
 * @brief 初始化 SCI-A — 9600bps / 8N1 / 无硬件流控 / FIFO 8 字节
 *
 * 接口约定:
 *   - GPIO35/36 的 MUX=1 (SCITXDA/SCIRXDA) 已在 AppConfig_InitGpio() 中配置
 *   - 本函数不使能 SCI 中断 — 收发由 ISR 中轮询 RXFFST/TXFFST 完成
 *   - 波特率公式: BRR = LSPCLK / (Baud × 8) - 1
 *   - SWRESET=0 时所有配置写完后, 最后 SWRESET=1 退出复位启动
 *
 * SCI-A 寄存器配置摘要:
 *   SCICCR  = 0x0007  (1 停止位 / 无校验 / 8 数据位 / 空闲线模式)
 *   SCICTL1 = 0x0023  (TX/RX 使能, SWRESET=1)
 *   BRR     = 487     (实际速率 = 9605.5bps, 误差 +0.06%)
 */
void AppConfig_InitSci(void)
{
    /* 清除可能残留的 FIFO 中断标志 (上电后寄存器值不确定) */
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    SciaRegs.SCIFFTX.bit.TXFFINTCLR = 1;

    /* 通信控制寄存器: 1 停止位, 无校验, 8 位字符, 空闲线多处理器模式 */
    SciaRegs.SCICCR.all = 0x0007;

    /* 控制寄存器 1: 使能 TX 和 RX (SWRESET=0 时仅"预置", 退出复位后才生效) */
    SciaRegs.SCICTL1.all = 0x0003;

    /* 波特率 = LSPCLK / [(BRR+1) × 8]
     * BRR = 37,500,000 / (9600×8) - 1 = 487 → 实际 9605.5 bps (±0.06%) */
    SciaRegs.SCIHBAUD = (SCI_BRR_VALUE >> 8) & 0xFF;
    SciaRegs.SCILBAUD =  SCI_BRR_VALUE & 0xFF;

    /* TX FIFO: 使能 SCI FIFO 模式, 复位 TX/RX FIFO, 清除 TX 中断标志
     * SCIFFENA=1(使能FIFO), TXFIFORESET=1(复位), TXFFINTCLR=1(清中断) */
    SciaRegs.SCIFFTX.all = 0xE040;

    /* RX FIFO: 不复位 (由 TX 寄存器统一复位), 接收触发深度=1, 无 FIFO 中断
     * RXFIFORESET=0(不复位), RXFFIL=1(触发不起作用：中断未使能, 仅 ISR 轮询) */
    SciaRegs.SCIFFRX.all = 0x0001;

    /* FIFO 控制: 不使能自动波特率检测 */
    SciaRegs.SCIFFCT.all = 0x0000;

    /* SWRESET=1 退出复位, 启动 SCI (SLEEP=0 不休眠, TXWAKE=0) */
    SciaRegs.SCICTL1.all = 0x0023;
}

/* ================================================================
 * SPI 初始化
 * ================================================================ */

/**
 * @brief 初始化 SPI-A — 主机模式 / 8-bit / 无相位滞后 / 下降沿输出
 *
 * 接口约定:
 *   - GPIO16-18 的 MUX=1 (SPI 功能) 已在 AppConfig_InitGpio() 中配置
 *   - 本函数不使能 SPI 中断 — 收发由 ISR 中轮询 SPISTS 完成
 *   - SPI 波特率 = LSPCLK / (SPIBRR + 1)
 *   - SWRESET=0 时冻结配置, 所有寄存器写完后 SWRESET=1 启动
 *
 * SPI-A 寄存器配置摘要:
 *   SPICCR  = 0x0007  (8-bit 字符, SWRESET=0 → 配完后 0x0087 SWRESET=1)
 *   SPICTL  = 0x0006  (主机模式, TALK=1, CLK_PHASE=0 无相位滞后, 无中断)
 *   SPIBRR  = 127     (SPI CLK ≈ 293 kHz, 有效吞吐由 1ms ISR 软件控制)
 */
void AppConfig_InitSpi(void)
{
    /* 使能 SPI-A 外设时钟 (EALLOW 保护) */
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.SPIAENCLK = 1;
    EDIS;

    /* 通过 FIFO 复位 SPI (SPIRST=1 将整个 SPI 回归空闲态) */
    SpiaRegs.SPIFFTX.all = 0x8000;

    /* 通信控制: 8-bit 字符长度, SPISWRESET=0 (配置冻结中) */
    SpiaRegs.SPICCR.all = 0x0007;

    /* 控制寄存器: CLK_PHASE=0 (无相位滞后/下降沿输出),
     * MASTER_SLAVE=1 (主机), TALK=1 (使能发送), 无中断 */
    SpiaRegs.SPICTL.all = 0x0006;

    /* 波特率 = LSPCLK / (SPI_BRR + 1) = 37.5MHz / 128 ≈ 293 kHz
     * 这是 SPI 硬件时钟, 有效数据吞吐由 1ms ISR 轮询节奏决定 */
    SpiaRegs.SPIBRR = SPI_BRR;

    /* SWRESET=1 退出复位, 启动 SPI (CLK 开始输出) */
    SpiaRegs.SPICCR.all = 0x0087;
}

/* ================================================================
 * CPU Timer0 初始化
 * ================================================================ */

/**
 * @brief 初始化 CPU Timer0 — 1ms 周期中断
 *
 * 接口约定:
 *   - InitCpuTimers() 必须在 main() 中先于本函数调用 (初始化定时器硬件)
 *   - 本函数仅配置周期参数和使能中断, ISR 向量注册在 main() 中完成
 *
 * 参数说明:
 *   - ConfigCpuTimer(&CpuTimer0, 150, 1000):
 *     Freq=150 (MHz, CPU主频), Period=1000 (µs, 1ms 中断周期)
 *
 * 中断链路: Timer0(C28x INT1) → PIE Group1 Channel7 → CPU INT1
 */
void AppConfig_InitCpuTimer0(void)
{
    /* 配置定时器周期: 150MHz CPU → 1ms = 1000µs 中断间隔 */
    ConfigCpuTimer(&CpuTimer0, 150.0, 1000.0);

    /* 使能 PIE Group1 Channel7 (TINT0) 中断 */
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
}

/* ================================================================
 * 应用层初始化入口
 * ================================================================ */

/**
 * @brief 应用层总初始化 — 在 main() 外设初始化阶段调用
 *
 * 调用顺序:
 *   1. AppConfig_InitGpio()  — GPIO 复用 (SPI/SCI/LED)
 *   2. AppConfig_InitSci()   — SCI-A UART 初始化
 *   3. AppConfig_InitSpi()   — SPI-A 主机初始化 (Step 2.2)
 *   4. AppConfig_InitCpuTimer0() — 1ms 时基 (Step 2.3)
 */
void AppConfig_Init(void)
{
    AppConfig_InitGpio();
    AppConfig_InitSci();

    AppConfig_InitSpi();
    AppConfig_InitCpuTimer0();
}
