/*
 * DAB 完整 PWM 调制链路 — 从 D1/D2 到 8 路 PWM 门极信号
 *
 * 来源: PLECS 仿真 dab_test.plecs + 用户整理扩展
 * 参考: 2P_DAB/assets/reference/dps_cscript_algorithm.c (原始 PLECS C-Script)
 *
 * 调制链路:
 *   p0,k → [Part 1] → D1,D2 → [Part 2] → SD1,SD2,ST → [Part 3/4] → raw PWM → [Part 5] → 8路门极信号
 *
 * 关键参数:
 *   - 开关频率: 10kHz (Tsw = 100us)
 *   - 载波: 对称三角波, 范围 [0, 1], 递增-递减模式 (TB_COUNT_UPDOWN)
 *   - 死区: 上升沿延迟 Td_on, 下降沿立即关断
 *   - k: 电压转换比 = V1 / (n * V2)
 */

#include <math.h>
#include <string.h>

// ==================== 参数定义 ====================
#define F_SW      10000.0   // 开关频率 10kHz
#define DEAD_TIME 0.0       // 死区时间 (PLECS中由BlankingTime模块处理)

// ==================== 第一部分: D1, D2 计算 (C-Script OutputFcn) ====================
// 输入: p0 (功率标幺值), k (电压转换比 = V1/(n*V2))
// 输出: D1, D2 (两个移相比, 范围 [0, 1])

void compute_D1_D2(double p0, double k, double *D1_out, double *D2_out)
{
    double tol = 1e-6;

    // 限幅
    p0 = fmax(0.0, fmin(1.0, p0));
    k  = fmax(tol, k);

    // 公共项
    double sqrt_term_inner = fmax(0.0, 4.0 - 6.0 * p0);
    double sqrt_term = sqrt(sqrt_term_inner);

    // k 的四个边界值
    double bound1_L = (1.0 - sqrt_term) / 3.0;
    double bound1_H = (1.0 + sqrt_term) / 3.0;
    double bound2_L = 3.0 / fmax((1.0 + sqrt_term), tol);
    double bound2_H = 3.0 / fmax((1.0 - sqrt_term), tol);

    double D1 = 0.0, D2 = 0.0;

    if (p0 >= (2.0 / 3.0) && p0 <= 1.0) {
        // 区域1: 高功率
        if (k >= 1.0) {
            double denom = fmax(tol, 2.0 * (k*k - 2.0*k + 3.0));
            double A = sqrt(fmax(0.0, 1.0 - p0) / denom);
            D1 = (k - 1.0) * A;
            D2 = 0.5 - A;
        } else if (k >= 0.0 && k < 1.0) {
            double denom = fmax(tol, 2.0 * (3.0*k*k - 2.0*k + 1.0));
            double A = sqrt(fmax(0.0, 1.0 - p0) / denom);
            D1 = (1.0 - k) * A;
            D2 = 0.5 - k * A;
        }
    } else {
        // 区域2 & 3: 低功率 (p0 < 2/3)
        if (k >= bound1_L && k < bound1_H) {
            // 区域2: k < 1 中间区域
            double term1 = p0 * fmax(0.0, 1.0 - k)
                           / fmax(tol, 2.0 * (1.0 + 3.0 * k));
            double term2_denom = fmax(tol, sqrt(fmax(0.0,
                2.0 * p0 * (1.0 - k) * (1.0 + 3.0 * k))));
            double term2 = 2.0 * k * p0 / term2_denom;
            D1 = 1.0 - sqrt(term1) - term2;
            D2 = sqrt(term1);
        } else if (k >= bound2_L && k < bound2_H) {
            // 区域3: k >= 1 中间区域
            double term1 = p0 * fmax(0.0, k - 1.0)
                           / fmax(tol, 2.0 * (k + 3.0));
            double term2_denom = fmax(tol, sqrt(fmax(0.0,
                2.0 * p0 * (k*k + 2.0*k - 3.0))));
            double term2 = 2.0 * p0 / term2_denom;
            D1 = 1.0 - sqrt(term1) - term2;
            D2 = sqrt(term1);
        } else {
            // 兜底: 回退到高功率逻辑
            if (k >= 1.0) {
                double denom = fmax(tol, 2.0 * (k*k - 2.0*k + 3.0));
                double A = sqrt(fmax(0.0, 1.0 - p0) / denom);
                D1 = (k - 1.0) * A;
                D2 = 0.5 - A;
            } else if (k >= 0.0 && k < 1.0) {
                double denom = fmax(tol, 2.0 * (3.0*k*k - 2.0*k + 1.0));
                double A = sqrt(fmax(0.0, 1.0 - p0) / denom);
                D1 = (1.0 - k) * A;
                D2 = 0.5 - k * A;
            }
        }
    }

    // 输出限幅
    *D1_out = fmax(0.0, fmin(1.0, D1));
    *D2_out = fmax(0.0, fmin(1.0, D2));
}

// ==================== 第二部分: D1,D2 → 调制波 SD1,SD2,ST ====================
// SD1 = 0.5 * (1.0 + D1)     → 范围 [0.5, 1.0]
// SD2 = 0.5 * D2             → 范围 [0.0, 0.5]
// ST  = SD1 + SD2            → 范围 [0.5, 1.5]

void compute_modulation_signals(double D1, double D2,
                                 double *SD1, double *SD2, double *ST)
{
    *SD1 = 0.5 * (1.0 + D1);
    *SD2 = 0.5 * D2;
    *ST  = *SD1 + *SD2;
}

// ==================== 第三部分: 三角载波生成 ====================
// PLECS TriangleGenerator: Min=0, Max=1, f=10kHz, DutyCycle=1 (对称三角波)
// 载波范围 [0, 1], 周期 Tsw = 1/10000 = 100us

double carrier_wave(double t, double Tsw)
{
    double t_mod = fmod(t, Tsw);           // 当前周期内的时间
    double half_T = Tsw / 2.0;

    if (t_mod < half_T)
        return (t_mod / half_T);           // 上升段: 0 → 1
    else
        return (2.0 - t_mod / half_T);     // 下降段: 1 → 0  (因其DutyCycle=1)
}

// ==================== 第四部分: PWM 比较 → 原始门极信号 ====================
// 四个 RelationalOperator (类型3 = ">=") 比较后经 BlankingTime (死区) 输出
//
// 注意: PLECS中BlankingTime模块的作用是:
//   - 上升沿延迟 Td_on (插入死区)
//   - 下降沿立即关断
// 下面的代码用简化方式表示死区

void compute_pwm_raw(double carrier, double SD1, double SD2, double ST,
                     int *s1_raw, int *s2_raw,
                     int *s3_raw, int *s4_raw,
                     int *q1_raw, int *q2_raw,
                     int *q3_raw, int *q4_raw)
{
    // === 原边 H 桥 (S1~S4) ===

    // Relational Operator4: carrier >= 0.5 → 基础方波 (50%占空比, 互补)
    int pwm_50 = (carrier >= 0.5) ? 1 : 0;  // → S1
    int pwm_50_n = 1 - pwm_50;              // → S2 (经LogicalOperator4 NOT)

    *s1_raw = pwm_50;
    *s2_raw = pwm_50_n;

    // Relational Operator5: |SD1 - carrier| >= 0.5 → 移相 PWM
    int pwm_sd1 = (fabs(SD1 - carrier) >= 0.5) ? 1 : 0;  // → S3
    int pwm_sd1_n = 1 - pwm_sd1;                          // → S4 (经NOT)

    *s3_raw = pwm_sd1;
    *s4_raw = pwm_sd1_n;

    // === 副边 H 桥 (Q1~Q4) ===

    // Relational Operator6: |SD2 - carrier| >= 0.5 → 移相 PWM
    int pwm_sd2 = (fabs(SD2 - carrier) >= 0.5) ? 1 : 0;  // → Q1
    int pwm_sd2_n = 1 - pwm_sd2;                          // → Q2 (经NOT)

    *q1_raw = pwm_sd2;
    *q2_raw = pwm_sd2_n;

    // Relational Operator7: |ST - carrier| >= 0.5 → 移相 PWM
    int pwm_st = (fabs(ST - carrier) >= 0.5) ? 1 : 0;    // → Q3
    int pwm_st_n = 1 - pwm_st;                            // → Q4 (经NOT)

    *q3_raw = pwm_st;
    *q4_raw = pwm_st_n;
}

// ==================== 第五部分: 死区插入 (BlankingTime) ====================
// PLECS中4个BlankingTime模块分别处理4路主PWM信号:
//   BlankingTime3: S1(S2互补)  ← RelationalOperator4
//   BlankingTime:  S3(S4互补)  ← RelationalOperator5
//   BlankingTime1: Q1(Q2互补)  ← RelationalOperator6
//   BlankingTime2: Q3(Q4互补)  ← RelationalOperator7
//
// 死区逻辑: 上升沿延迟 td, 下降沿不延迟
// 互补信号由 LogicalOperator (NOT) 自动生成

typedef struct {
    double td_on;       // 开通延迟 (死区时间)
    double td_off;      // 关断延迟 (通常为0)
    int last_state;     // 上一个状态
    double timer_on;    // 开通计时器
} deadtime_state;

void blanking_time(int pwm_in, double dt, deadtime_state *st,
                   int *pwm_out)
{
    if (pwm_in == 1 && st->last_state == 0) {
        // 上升沿: 延迟开通
        st->timer_on += dt;
        if (st->timer_on >= st->td_on) {
            *pwm_out = 1;
            st->timer_on = 0.0;
        } else {
            *pwm_out = 0;
        }
    } else if (pwm_in == 0 && st->last_state == 1) {
        // 下降沿: 立即关断
        *pwm_out = 0;
    } else {
        *pwm_out = pwm_in;
    }
    st->last_state = pwm_in;
}

// ==================== 第六部分: 整体 PWM 生成函数 ====================

typedef struct {
    // 死区状态
    deadtime_state dt_s1, dt_s3, dt_q1, dt_q3;
    double Tsw;
} dab_pwm_state;

void dab_pwm_step(double t, double dt, double D1, double D2,
                  dab_pwm_state *state,
                  int *S1, int *S2, int *S3, int *S4,
                  int *Q1, int *Q2, int *Q3, int *Q4)
{
    double Tsw = state->Tsw;

    // Step 1: 计算调制波
    double SD1, SD2, ST;
    compute_modulation_signals(D1, D2, &SD1, &SD2, &ST);

    // Step 2: 获取当前载波值
    double carrier = carrier_wave(t, Tsw);

    // Step 3: PWM 比较
    int s1_raw, s2_raw, s3_raw, s4_raw;
    int q1_raw, q2_raw, q3_raw, q4_raw;
    compute_pwm_raw(carrier, SD1, SD2, ST,
                    &s1_raw, &s2_raw, &s3_raw, &s4_raw,
                    &q1_raw, &q2_raw, &q3_raw, &q4_raw);

    // Step 4: 插入死区 (仅对主开关, 互补由NOT生成)
    blanking_time(s1_raw, dt, &state->dt_s1, S1);   // S1 → S2=NOT(S1)
    blanking_time(s3_raw, dt, &state->dt_s3, S3);   // S3 → S4=NOT(S3)
    blanking_time(q1_raw, dt, &state->dt_q1, Q1);   // Q1 → Q2=NOT(Q1)
    blanking_time(q3_raw, dt, &state->dt_q3, Q3);   // Q3 → Q4=NOT(Q3)

    // 互补信号
    *S2 = 1 - *S1;
    *S4 = 1 - *S3;
    *Q2 = 1 - *Q1;
    *Q4 = 1 - *Q3;
}

// ==================== 初始化 ====================
void dab_pwm_init(dab_pwm_state *state, double td_deadtime)
{
    memset(state, 0, sizeof(*state));
    state->Tsw = 1.0 / F_SW;  // 100us

    state->dt_s1.td_on  = td_deadtime;
    state->dt_s3.td_on  = td_deadtime;
    state->dt_q1.td_on  = td_deadtime;
    state->dt_q3.td_on  = td_deadtime;
}
