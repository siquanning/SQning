#ifndef DRIVERS_ADC_H
#define DRIVERS_ADC_H

#include "include/common.h"
#include "control/iir.h"

// -----------------------------------------------------------------------------
// ADC 时钟参数 (SYSCLKOUT=150MHz, HSPCLK=150MHz)
// ADCCLK = HSPCLK / (2 × ADCCLKPS) = 150MHz / 6 = 25MHz (max spec)
// -----------------------------------------------------------------------------
#define ADC_CLKPS       3       // ADCTRL3.ADCCLKPS, 0=bypass, N>0 → ÷(2N)
#define ADC_CPS         0       // ADCTRL1.CPS, 0=÷1, 1=÷2
#define ADC_ACQ_PS      6       // S/H window = (6+1) × 25ns = 175ns
#define ADC_SHCLK       25.0f   // effective ADCCLK in MHz

// -----------------------------------------------------------------------------
// ADC 量纲转换 — 校准参数，需对照实际硬件测量标定
// V2_max = 200V → ADC 输入 3V → 分压比 ≈ 66.67
// -----------------------------------------------------------------------------
#define ADC_V2_SCALE        66.67f
#define ADC_VREF            3.0f
#define ADC_RESOLUTION      4096.0f

// 系统固定参数
#define V1_FIXED            400.0f
#define TRANSFORMER_RATIO   4.0f     // n = Np/Ns

// -----------------------------------------------------------------------------
// IIR 滤波器系数: fc ≈ 100Hz @ fs = 10kHz
// alpha = 1 - exp(-2π × fc / fs) ≈ 0.0609
// -----------------------------------------------------------------------------
#define IIR_ALPHA           0.061f

// 零点校准 — 功率级不上电时软件触发采样取平均，消除输入端偏置
#define ZERO_CAL_SAMPLES   32

// -----------------------------------------------------------------------------
// 全局 ADC 数据结构（调试器直接观察）
// -----------------------------------------------------------------------------
typedef struct {
    Uint16 raw;           // ADC 原始值 [0, 4095]
    Uint16 zero_offset;   // 零点偏移量（校准值）
    float  v2_raw;        // 一次侧电压 (V)，滤波前
    float  v2_filtered;   // 一次侧电压 (V)，IIR 滤波后
    float  k;             // 电压变比 = V1/(n×V2)，DPS 算法输入之一
    iir1_t iir;           // IIR 滤波器状态
} adc_data_t;

extern adc_data_t g_adc;

// -----------------------------------------------------------------------------
// 公开接口
// -----------------------------------------------------------------------------

// 初始化 ADC 模块：上电、校准、通道选择、ePWM1 SOCA 触发
void adc_init(void);

// ADC 中断服务例程（ePWM1 SOCA → SEQ1 EOC）
__interrupt void adc_isr(void);

#endif
