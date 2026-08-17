/* Created by Siquanning */
#ifndef DEBUG_SNAPSHOT_H
#define DEBUG_SNAPSHOT_H

#include <stdint.h>

/*
 * DEBUG_SNAPSHOT_H — 统一调试观测快照（纯观测层，不参与任何控制/安全路径）
 *
 * 生产者: App_Epwm1Isr (20kHz) 每个控制周期末尾调用 DebugSnapshot_Update()
 *         （实现于 isr.c，本头文件只定义结构与共享变量）
 * 消费者: Lite 下 JustFloat_OnSnapshot() 在同一 1kHz 分频点入队；
 *         GetChannels() 仅供前台/测试在 DINT 下拷贝快照。
 *
 * 设计原则:
 *   - 快照只存放「已有计算的结果」或「由已有结果按同一公式派生的观测值」，
 *     不重复维护任何控制状态，不新增控制算法。
 *   - 20kHz ISR 不写 SCITXBUF；Lite 只入队，发送在 SCI-C TX ISR。
 *   - 关闭 JustFloat / 切 VIEW / 串口异常都不影响控制与安全状态。
 */

typedef struct
{
    /* ---- 采样物理量（20kHz 同一控制时刻换算） ---- */
    float    vac[3];        /* 三相 Vac [V] —— PLL 本拍实际输入 (g_pll_input_vabc) */
    float    vline[3];      /* 线电压 Vab/Vbc/Vca [V] = vac[i]−vac[i+1]（同一参考点相电压相减） */
    float    iac[3];        /* 三相 Iac [A] —— Measurement_ConvertIac 换算 */
    float    vdc[6];        /* 六路 Vdc [V] —— Measurement_ConvertVdc 换算 */
    float    vac_raw[3];    /* 三相 Vac ADC raw counts */
    float    iac_raw[3];    /* 三相 Iac ADC raw counts */
    float    vac_offset[3]; /* 三相 Vac 零偏 counts (g_vac_vx_offset_counts) */
    float    iac_offset[3]; /* 三相 Iac 零偏 counts (g_iac_ix_offset_counts) */

    /* ---- PLL ---- */
    float    pll_freq;      /* g_pll.freq [Hz] */
    float    pll_vd;        /* g_pll.vd [V] */
    float    pll_vq;        /* g_pll.vq [V] */
    float    pll_vmag;      /* g_pll.vmag [V] */
    float    pll_theta;     /* g_pll.theta [rad] */
    uint16_t pll_lock;      /* g_pll_switch_req: 1=已锁定并切到 PLL */

    /* ---- 当前观测相双闭环（来源 g_phase_ctrl[obs_idx]；未运行/未激活则为 0） ---- */
    float    vdc_avg;       /* 0.5×(vdc[2i]+vdc[2i+1]) 观测值 [V] */
    float    vdc_balance;   /* |vdc[2i]-vdc[2i+1]| [V] */
    float    vdc_ref_ramp;  /* 电压外环参考斜坡 [V] */
    float    vdc_integral;  /* 电压外环积分项 [A] */
    float    vdc_err;       /* vdc_ref_ramp - vdc_avg [V]（派生观测） */
    float    iamp;          /* 电压外环输出电流幅值 [A]（= id_ref 幅值） */
    float    iamp_lim;      /* 1.0=撞 Iamp 限幅（iamp>=g_i_limit_a）（派生观测） */
    float    id_ref;        /* d 轴有功电流参考 [A] */
    float    iq_ref;        /* q 轴电流参考 [A]（固定 0） */
    float    id;            /* Park 后 d 轴电流 [A] */
    float    iq;            /* Park 后 q 轴电流 [A] */
    float    id_err;        /* id_ref - id [A] */
    float    iq_err;        /* iq_ref - iq [A] */
    float    id_integral;   /* d 轴积分器 [V] */
    float    iq_integral;   /* q 轴积分器 [V] */
    float    vd_ctrl;       /* d 轴 PI 输出（控制修正量）[V] */
    float    vq_ctrl;       /* q 轴 PI 输出（控制修正量）[V] */
    float    i_alpha;       /* SOGI α 轴电流 [A] */
    float    i_beta;        /* SOGI β 轴电流（滞后 α 90°）[A] */
    float    m_raw;         /* 钳位前 m（控制层拷贝） */
    float    m_final;       /* g_phase_ctrl.m —— 钳位后 m */
    float    theta_phase;   /* 当前相角 [rad] */
    uint16_t obs_idx;       /* 当前观测相索引 0..2（g_jf_phase/g_ctrl_test_phase 决定） */

    /* ---- PWM / 安全链 ---- */
    int16_t  mabc[3];       /* 三相最终调制度 per-mill（写入 CMPA 前的 m×1000） */
    uint16_t uni_polarity;  /* bit0=A bit1=B bit2=C：1=m>=0 左腿钳位（写入 CPLD 的 UNI） */
    uint16_t cmp_left;      /* 观测相左桥 CMPA（force_left=1 时无意义） */
    uint16_t force_left;    /* 观测相左桥 AQCSFRC 强制高标志 */
    uint16_t cmp_right;     /* 观测相右桥 CMPA */
    uint16_t force_right;   /* 观测相右桥 AQCSFRC 强制高标志 */

    /* ---- 系统状态 ---- */
    uint16_t run_request;   /* GPIO21 消抖稳定电平（app.c 10ms 更新） */
    uint16_t state;         /* SYSTEM_STATE_* */
    uint16_t active_phase;  /* 锁存 active phase（0=未运行/非法） */
    uint16_t active_mode;   /* 0=未运行 1=单相 2=三相 */
    uint16_t gpio30;        /* FAULT_GATE 电平 */
    uint16_t gpio42;        /* 输入开关电平 */
    uint16_t gpio44;        /* 预充旁路电平 */
    uint16_t tz_status;     /* EPWM1 TZFLG */
    uint16_t fault;         /* first_fault 码 */
} DebugSnapshot;

/* 快照实例：20kHz ISR 写；Lite 入队同 ISR 只读，GetChannels 前台 DINT 拷贝 */
extern DebugSnapshot g_dbg_snap;

/* GPIO21 消抖稳定电平的运行期镜像（app.c 定义/更新，供快照读取） */
extern volatile uint16_t g_run_request;

#endif /* DEBUG_SNAPSHOT_H */
