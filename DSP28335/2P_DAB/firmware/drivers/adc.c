#include "drivers/adc.h"
#include "drivers/epwm.h"

// 全局 ADC 数据实例（调试器直接观察）
adc_data_t g_adc;

// ADC 上电延时 (HSPCLK=150MHz 下约 5ms)
#define ADC_POWER_DELAY 750000L

// ============================================================================
// 内部辅助：ADC 上电 + 校准（对标 TI InitAdc）
// ============================================================================
static void adc_power_cal(void)
{
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.ADCENCLK = 1;
    ADC_cal();
    EDIS;

    // 上电顺序：bandgap → reference → ADC core
    // ADCTRL3.all = 0x00E0: ADCBGRFDN=1, ADCPWDN=1, ADCCLKPS=3
    AdcRegs.ADCTRL3.all = 0x00E0 | (ADC_CLKPS << 0);

    // 模拟电路稳定延时 (~5ms)
    volatile Uint32 i;
    for (i = 0; i < ADC_POWER_DELAY; i++) {}
}

// ============================================================================
// 内部辅助：ePWM1 SOCA 事件触发配置 (CTR=0 时触发，避开开关噪声)
// ============================================================================
static void adc_epwm_soc_init(void)
{
    // SOCA: CTR=0 时触发（PWM 周期起点/中点，远离开关时刻）
    EPwm1Regs.ETSEL.bit.SOCASEL = 1;    // CTR=0
    EPwm1Regs.ETSEL.bit.SOCAEN  = 1;    // 使能 SOCA
    EPwm1Regs.ETPS.bit.SOCAPRD  = 1;    // 每个事件都触发（不分频）
}

// ============================================================================
// 公开接口
// ============================================================================

void adc_init(void)
{
    // 1. ADC 上电 + 校准
    adc_power_cal();

    // 2. 配置 SEQ1：单通道级联模式，1 个转换 (CONV00)
    AdcRegs.ADCTRL1.bit.SEQ_CASC = 0;   // 双序列模式
    AdcRegs.ADCTRL1.bit.CONT_RUN = 0;   // 单次触发
    AdcRegs.ADCTRL1.bit.CPS      = ADC_CPS;
    AdcRegs.ADCTRL1.bit.ACQ_PS   = ADC_ACQ_PS;

    AdcRegs.ADCMAXCONV.bit.MAX_CONV1 = 0;  // 1 个转换 (CONV00 only)

    // ADCINA0 → SEQ1 CONV00
    AdcRegs.ADCCHSELSEQ1.bit.CONV00 = 0;

    // 3. ePWM1 SOCA 触发 SEQ1
    adc_epwm_soc_init();
    AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1;

    // 4. 使能 SEQ1 中断（每次 EOS 触发）
    AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1  = 1;
    AdcRegs.ADCTRL2.bit.INT_MOD_SEQ1  = 0;  // 每个 SEQ1 结束都中断

    // 5. 初始化 IIR 滤波器
    iir1_init(&g_adc.iir, IIR_ALPHA);

    // zero_offset = 0xFFFF 表示"未校准"，ISR 会在前 N 次触发中自动采集
    g_adc.zero_offset = 0xFFFF;
    g_adc.raw         = 0;
    g_adc.v2_raw      = 0.0f;
    g_adc.v2_filtered = 0.0f;
    g_adc.k           = 0.0f;
}

__interrupt void adc_isr(void)
{
    Uint16 adc_val = AdcRegs.ADCRESULT0 >> 4;

    // 零点校准：前 ZERO_CAL_SAMPLES 次 ISR 触发自动采集偏移量
    // 用 ePWM 触发（而非软件触发），确保与正常运行采样条件一致
    static Uint16 cal_count = 0;
    static Uint32 cal_sum  = 0;

    if (g_adc.zero_offset == 0xFFFF) {
        cal_sum += adc_val;
        cal_count++;
        if (cal_count >= ZERO_CAL_SAMPLES) {
            g_adc.zero_offset = (Uint16)(cal_sum / ZERO_CAL_SAMPLES);
        }
        g_adc.raw = 0;  // 校准期间输出 0
    } else {
        // 减去零点偏移（防下溢）
        Uint16 raw = (adc_val > g_adc.zero_offset) ? (adc_val - g_adc.zero_offset) : 0;
        g_adc.raw = raw;

        // 量纲转换：ADC 原始值 → 引脚电压 → 实际 V2
        float v_adc = (float)raw * (ADC_VREF / ADC_RESOLUTION);
        g_adc.v2_raw = v_adc * ADC_V2_SCALE;

        // 一阶 IIR 低通滤波
        g_adc.v2_filtered = iir1_step(&g_adc.iir, g_adc.v2_raw);

        // k = V1/(n×V2), clamped to [0.5, 3.0] to avoid D1=0 deadlock
        // at startup (k=1→D1=0) and numerical blowup (k→∞)
        if (g_adc.v2_filtered > 0.5f) {
            float k_raw = V1_FIXED / (TRANSFORMER_RATIO * g_adc.v2_filtered);
            g_adc.k = (k_raw < 0.5f) ? 0.5f : ((k_raw > 3.0f) ? 3.0f : k_raw);
        } else {
            g_adc.k = 3.0f;  // V2≈0 → max k clamp, ensures D1 > 0 at startup
        }
    }

    // 清理 SEQ1 中断标志，重置序列到初始状态
    AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1;
    AdcRegs.ADCST.bit.INT_SEQ1_CLR = 1;
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
