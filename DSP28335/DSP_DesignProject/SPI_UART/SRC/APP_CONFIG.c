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

    /* ---- SPI-A 引脚 (GPIO16=SIMO, GPIO17=SOMI, GPIO18=CLK, GPIO19=CS) ---- */
    /* GPIO16: SPI-A SIMO (MOSI), MUX=3 (SPI 主功能), 推挽输出 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO16 = 3;
    GpioCtrlRegs.GPADIR.bit.GPIO16 = 1;

    /* GPIO17: SPI-A SOMI (MISO), MUX=3 (SPI 主功能), QSEL=3 (异步输入), PUD=0 (上拉) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO17 = 3;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO17 = 3;
    GpioCtrlRegs.GPAPUD.bit.GPIO17 = 0;

    /* GPIO18: SPI-A CLK, MUX=3 (SPI 主功能), 推挽输出 */
    GpioCtrlRegs.GPAMUX2.bit.GPIO18 = 3;
    GpioCtrlRegs.GPADIR.bit.GPIO18 = 1;

    /* GPIO19: 手动 NSS (CS), MUX=0 (普通 GPIO), DIR=1 (输出), 初始高电平 (未选中) */
    GpioCtrlRegs.GPAMUX2.bit.GPIO19 = 0;
    GpioCtrlRegs.GPADIR.bit.GPIO19 = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO19 = 0;
    SPI_CS_HIGH;    /* 初始化阶段拉高, AppConfig_InitSpi() 完成后拉低选中 */

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

    /* 8. 释放模块复位 (SWRESET=1), SLEEP=0 (接收所有字节, 非仅地址帧) */
    SciaRegs.SCICTL1.all = 0x0023;

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
 * SPI 字节级收发封装
 *
 * SPI 是全双工总线——主机每发一字节同时也会收一字节。
 * SpiSendByte  发一字节, 丢弃同时收到的数据 (仅发不收场景用)
 * SpiReceiveByte 发哑字节 0xFF 产生时钟, 返回 CPLD 发来的数据
 * ================================================================ */

/**
 * @brief 通过 SPI-A 发送一字节（查询方式, 阻塞等待传输完成）
 *
 * SPI 为主机全双工模式, 发送的同时会从 CPLD 收一字节存入 SPIRXBUF。
 * 本函数读取 SPIRXBUF 以清除 INT_FLAG, 但丢弃收到的数据。
 * 若需同时收发, 发完后再调 SpiReceiveByte 取回 CPLD 发来的数据。
 *
 * 传输流程:
 *   1. 等待 SPITXBUF 为空 (BUFFULL_FLAG == 0)
 *   2. 写入待发送字节到 SPITXBUF, 启动 SPI 移位
 *   3. 等待移位完成 (INT_FLAG == 1, 收/发均完成)
 *   4. 读 SPIRXBUF 清除 INT_FLAG
 *
 * @param data 待发送字节 (仅低 8 位有效)
 */
void SpiSendByte(Uint16 data)
{
    /* 等待 TX 缓冲空——上一次写入的数据已转移到移位寄存器 */
    while (SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1)
    {
    }

    /* 写入待发字节, 硬件自动启动 SPI 移位传输 */
    SpiaRegs.SPITXBUF = (data & 0xFF);

    /* 等待一字节传输完成 (INT_FLAG 置位 = SPIRXBUF 有新数据) */
    while (SpiaRegs.SPISTS.bit.INT_FLAG == 0)
    {
    }

    /* 读接收缓冲以清除 INT_FLAG——此字节是 CPLD 同时发来的数据 */
    SpiaRegs.SPIRXBUF;
}

/**
 * @brief 从 SPI-A 接收一字节（查询方式, 发 0xFF 哑字节产生时钟）
 *
 * DSP 是 SPI 主机, 必须主动发时钟才能从 CPLD 收数据。
 * 发送哑字节 0xFF (MOSI 持续高电平, 对 CPLD 无意义) 产生 8 个时钟,
 * 同时锁存 MISO 上的数据。
 *
 * 传输流程:
 *   1. 等待 SPITXBUF 为空
 *   2. 写哑字节 0xFF 到 SPITXBUF (生成时钟, CPLD 侧视为 NOP)
 *   3. 等待移位完成
 *   4. 返回 SPIRXBUF 中 CPLD 发来的数据
 *
 * @return CPLD 发来的字节 (仅低 8 位有效)
 */
Uint16 SpiReceiveByte(void)
{
    /* 等待 TX 缓冲空 */
    while (SpiaRegs.SPISTS.bit.BUFFULL_FLAG == 1)
    {
    }

    /* 发哑字节产生 8 个 SPI 时钟, 同时从 MISO 线采样 CPLD 数据 */
    SpiaRegs.SPITXBUF = 0xFF;

    /* 等待传输完成 */
    while (SpiaRegs.SPISTS.bit.INT_FLAG == 0)
    {
    }

    /* 返回 CPLD 发来的数据 (仅低 8 位有效) */
    return (SpiaRegs.SPIRXBUF & 0xFF);
}

/* ================================================================
 * 应用层初始化入口
 * ================================================================ */

/**
 * @brief SPI-A 主机模式初始化 — 8-bit, SPI Mode 0, ~293 kHz
 *
 * 按 F28335 SPI 标准初始化序列配置:
 *   1. SPISWRESET=0 (模块复位, 配置期间暂停)
 *   2. SPICCR=0x0007 (8-bit 字符, 空闲低电平, 非回环)
 *   3. SPICTL=0x0006 (主机模式, 使能发送, CLK_PHASE=0 下降沿输出/上升沿锁存, 无中断)
 *   4. SPIBRR=127 (最慢波特率: LSPCLK/128 ≈ 293 kHz)
 *   5. SPIPRI=0x0010 (自由仿真模式, 调试断点不停止 SPI)
 *   6. SPISWRESET=1 (释放复位, 启动 SPI 模块)
 *   7. SPI_CS_LOW (GPIO19 拉低, 永久选中 CPLD 单从机)
 *
 * SPI Mode 0 说明:
 *   - CLKPOLARITY=0: 时钟空闲低电平
 *   - CLK_PHASE=0:  上升沿锁存数据 (CPLD→DSP), 下降沿输出数据 (DSP→CPLD)
 *   - 这是最常见的 SPI 模式, 与标准 SPI Flash/SD 卡一致
 *
 * 波特率说明:
 *   - SPIBRR=127 → SPI 时钟 = 37500000/(127+1) ≈ 292.97 kHz
 *   - SPI 时钟远高于 UART 9600 bps, 但有效吞吐量由主循环轮询周期决定
 *   - CPLD 作为 SPI 从机可轻松处理 293 kHz 时钟
 *
 * 注意: 不使用 SPI FIFO 增强模式 (标准 SPI 模式, 单缓冲 TX/RX),
 *        收发通过 SpiSendByte/SpiReceiveByte 查询方式操作。
 */
void AppConfig_InitSpi(void)
{
    /* 1. 模块复位 (配置期间暂停 SPI 逻辑) */
    SpiaRegs.SPICCR.bit.SPISWRESET = 0;

    /* 2. 通信格式: 8 位字符, 时钟空闲低电平, 非回环模式 */
    SpiaRegs.SPICCR.all = 0x0007;      /* SPICHAR=7 (8-bit), CLKPOLARITY=0, SPILBK=0 */

    /* 3. 操作控制: 主机模式, 使能发送, 时钟相位=0, 无 SPI 中断 (轮询收发) */
    SpiaRegs.SPICTL.all = 0x0006;      /* CLK_PHASE=0, MASTER_SLAVE=1, TALK=1, 无中断 */

    /* 4. 波特率除数: 设到最慢 (SPIBRR 最大=127, 7 位限制) */
    SpiaRegs.SPIBRR = SPI_BRR_VALUE;

    /* 5. 优先级控制: 仿真挂起时继续运行 (避免调试断点导致 SPI 总线卡死) */
    SpiaRegs.SPIPRI.all = 0x0010;      /* FREE=1, SOFT=0 */

    /* 6. 延时等待, 确保 CPLD 侧已就绪 (293 kHz 下 1 字节 ≈ 27μs, 无实质影响) */
    DELAY_US(100);

    /* 7. 释放复位, 启动 SPI 模块 */
    SpiaRegs.SPICCR.bit.SPISWRESET = 1;

    /* 8. 拉低 CS 选中 CPLD (单从机, 不切换) */
    SPI_CS_LOW;
}

/* ================================================================
 * 应用层初始化入口
 * ================================================================ */

/**
 * @brief 应用层总初始化 — 在 main() 外设初始化阶段调用
 *
 * 调用顺序:
 *   1. AppConfig_InitGpio()
 *   2. AppConfig_InitSci()   (或其他外设初始化函数)
 */
void AppConfig_Init(void)
{
    AppConfig_InitGpio();
    AppConfig_InitSci();
    AppConfig_InitSpi();

    /* TODO: 按需调用其他外设初始化函数 */
    /* AppConfig_InitEPwm(); */
    /* AppConfig_InitADC();  */
    /* AppConfig_InitCpuTimer0(); */
}
