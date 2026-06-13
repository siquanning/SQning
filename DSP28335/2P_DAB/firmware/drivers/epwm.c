#include "drivers/epwm.h"

static void epwm_tz_init(void);

// =============================================================================
// 内部辅助函数
// =============================================================================

// GPIO 引脚复用到 ePWM 功能（GPIO00~07 → ePWM1A/B ~ ePWM4A/B）
static void epwm_gpio_init(void)
{
    EALLOW;

    // ePWM1A/B: GPIO00/01 → S1/S2（原边左半桥）
    GpioCtrlRegs.GPAPUD.bit.GPIO0 = 0;      // 使能内部上拉
    GpioCtrlRegs.GPAPUD.bit.GPIO1 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;      // MUX=1 → ePWM 功能
    GpioCtrlRegs.GPAMUX1.bit.GPIO1 = 1;

    // ePWM2A/B: GPIO02/03 → S3/S4（原边右半桥）
    GpioCtrlRegs.GPAPUD.bit.GPIO2 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO3 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO2 = 1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO3 = 1;

    // ePWM3A/B: GPIO04/05 → Q1/Q2（副边左半桥）
    GpioCtrlRegs.GPAPUD.bit.GPIO4 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO5 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1;

    // ePWM4A/B: GPIO06/07 → Q3/Q4（副边右半桥）
    GpioCtrlRegs.GPAPUD.bit.GPIO6 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO7 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1;
    GpioCtrlRegs.GPAMUX1.bit.GPIO7 = 1;

    EDIS;
}

// 单个 ePWM 模块初始化：时基 + 比较器 + 动作限定 + 死区
static void epwm_module_init(volatile struct EPWM_REGS *epwm,
                             Uint16 syncosel, Uint16 phsen)
{
    // ----- 时基：递增-递减模式，10kHz -----
    epwm->TBCTL.bit.CTRMODE   = TB_COUNT_UPDOWN;
    epwm->TBCTL.bit.HSPCLKDIV = TB_DIV1;     // TBCLK = SYSCLKOUT / 1 = 150MHz
    epwm->TBCTL.bit.CLKDIV    = TB_DIV1;
    epwm->TBCTL.bit.PHSEN     = phsen;       // 相移加载使能
    epwm->TBCTL.bit.SYNCOSEL  = syncosel;    // 同步输出源选择
    epwm->TBPRD               = PWM_TBPRD;
    epwm->TBPHS.half.TBPHS   = 0;           // 初始相移 = 0
    epwm->TBCTR               = 0;           // 计数器清零

    // ----- 比较器：影子寄存器模式，CTR=0 时装载 -----
    epwm->CMPCTL.bit.SHDWAMODE = CC_SHADOW;
    epwm->CMPCTL.bit.SHDWBMODE = CC_SHADOW;
    epwm->CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;
    epwm->CMPCTL.bit.LOADBMODE = CC_CTR_ZERO;

    // 初始 50% 占空比，方便示波器验证
    epwm->CMPA.half.CMPA = PWM_TBPRD / 2;
    epwm->CMPB             = PWM_TBPRD / 2;

    // ----- 动作限定（Active High Complementary 互补输出）-----
    // ePWMxA：上行到 CMPA 置高，下行到 CMPA 清低 → 50% 方波
    // ePWMxB：A 的反相 → 互补输出
    epwm->AQCTLA.bit.CAU = AQ_SET;
    epwm->AQCTLA.bit.CAD = AQ_CLEAR;
    epwm->AQCTLB.bit.CAU = AQ_CLEAR;
    epwm->AQCTLB.bit.CAD = AQ_SET;

    // ----- 死区：Active High Complementary，200ns -----
    // OUT_MODE=3: RED 作用于 A 通道，FED 作用于 B 通道
    // POLSEL=2: A 高有效，B 高有效互补（AHC 模式）
    // IN_MODE=0: 两个通道均以 ePWMxA 为源
    epwm->DBCTL.bit.OUT_MODE = DB_FULL_ENABLE;
    epwm->DBCTL.bit.POLSEL   = DB_ACTV_HIC;
    epwm->DBCTL.bit.IN_MODE  = DBA_ALL;
    epwm->DBRED = PWM_DB_TICKS;              // 上升沿延迟 = 200ns
    epwm->DBFED = PWM_DB_TICKS;              // 下降沿延迟 = 200ns
}

// =============================================================================
// 公开接口
// =============================================================================

void epwm_init(void)
{
    // 1. 使能 ePWM1~4 外设时钟
    EALLOW;
    SysCtrlRegs.PCLKCR1.bit.EPWM1ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EPWM2ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EPWM3ENCLK = 1;
    SysCtrlRegs.PCLKCR1.bit.EPWM4ENCLK = 1;
    EDIS;

    // 2. GPIO 引脚复用
    epwm_gpio_init();

    // 3. 暂停时基同步，避免配置过程中各模块不同步
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    // 4. 逐个模块初始化
    // ePWM1：主模块，CTR=0 时产生同步脉冲
    epwm_module_init(&EPwm1Regs, TB_CTR_ZERO, TB_DISABLE);

    // ePWM2~4：从模块，透传同步脉冲，使能相移加载
    epwm_module_init(&EPwm2Regs, TB_SYNC_IN, TB_ENABLE);
    epwm_module_init(&EPwm3Regs, TB_SYNC_IN, TB_ENABLE);
    epwm_module_init(&EPwm4Regs, TB_SYNC_IN, TB_ENABLE);

    // 5. 恢复时基同步 → 所有模块同步启动，相位对齐
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;

    // 6. TZ (Trip Zone) 配置：TZ1 单次关断，强制输出低电平
    epwm_tz_init();
}

static void epwm_tz_init(void)
{
    volatile struct EPWM_REGS *epwms[] = {&EPwm1Regs, &EPwm2Regs, &EPwm3Regs, &EPwm4Regs};
    int i;
    for (i = 0; i < 4; i++) {
        EALLOW;
        epwms[i]->TZSEL.bit.OSHT1 = TZ_ENABLE;   // TZ1 as one-shot trip source
        epwms[i]->TZCTL.bit.TZA   = TZ_FORCE_LO;  // ePWMxA → forced low on trip
        epwms[i]->TZCTL.bit.TZB   = TZ_FORCE_LO;  // ePWMxB → forced low on trip
        EDIS;
    }
}

void epwm_set_duty(Uint16 module, float duty)
{
    // 限幅
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    Uint16 cmp = (Uint16)((float)PWM_TBPRD * duty);

    // 定位 ePWM 模块寄存器
    volatile struct EPWM_REGS *epwm;
    switch (module)
    {
    case EPWM_MODULE_S1S2: epwm = &EPwm1Regs; break;
    case EPWM_MODULE_S3S4: epwm = &EPwm2Regs; break;
    case EPWM_MODULE_Q1Q2: epwm = &EPwm3Regs; break;
    case EPWM_MODULE_Q3Q4: epwm = &EPwm4Regs; break;
    default:               return;
    }

    // 写入影子寄存器，CTR=0 时自动装载
    epwm->CMPA.half.CMPA = cmp;
    epwm->CMPB             = cmp;
}

void epwm_set_phase(Uint16 module, float phase)
{
    // 限幅
    if (phase < 0.0f) phase = 0.0f;
    if (phase > 1.0f) phase = 1.0f;

    // 相移 [0, 1] → TBPHS 计数值 [0, TBPRD]
    Uint16 phs = (Uint16)((float)PWM_TBPRD * phase);

    // 定位 ePWM 模块寄存器
    volatile struct EPWM_REGS *epwm;
    switch (module)
    {
    case EPWM_MODULE_S1S2: epwm = &EPwm1Regs; break;
    case EPWM_MODULE_S3S4: epwm = &EPwm2Regs; break;
    case EPWM_MODULE_Q1Q2: epwm = &EPwm3Regs; break;
    case EPWM_MODULE_Q3Q4: epwm = &EPwm4Regs; break;
    default:               return;
    }

    // 写入 TBPHS，下次同步事件时装载到 TBCTR
    epwm->TBPHS.half.TBPHS = phs;
}
