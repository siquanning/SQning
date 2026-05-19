/*
 * DPS Modulation Algorithm — 来自 PLECS 仿真 dab_test.plecs
 * 来源: D:\plecs_model\dab_test.plecs, 行号 2412-2463, OutputFcn
 *
 * 功能: DAB 双移相 (DPS) 调制，最小化 RMS 电流
 * 输入: p0 — 归一化功率指令 [0, 1]
 *       k  — 电压转换比 = V1 / (n * V2)
 * 输出: D1 — 桥间移相比 [0, 1]
 *       D2 — 桥内移相比 [0, 1]
 */

#include <math.h>

// 1. 读取输入端口：端口0是 p0, 端口1是 k
double p0 = InputSignal(0, 0);
double k  = InputSignal(1, 0);

// 初始化输出
double D1 = 0.0;
double D2 = 0.0;

// 极小值保护
double tol = 1e-6;

// 2. 强制限制 p0 和 k 的物理范围 (使用 C99 的 fmax 和 fmin)
p0 = fmax(0.0, fmin(1.0, p0));
k  = fmax(tol, k);

// 3. 计算公共判断条件项
double sqrt_term_inner = fmax(0.0, 4.0 - 6.0 * p0);
double sqrt_term = sqrt(sqrt_term_inner);

// 计算 k 的四个边界值
double bound1_L = (1.0 - sqrt_term) / 3.0;
double bound1_H = (1.0 + sqrt_term) / 3.0;
double bound2_L = 3.0 / fmax((1.0 + sqrt_term), tol);
double bound2_H = 3.0 / fmax((1.0 - sqrt_term), tol);

// ================= 核心分支判断 =================

if (p0 >= (2.0 / 3.0) && p0 <= 1.0) {
    // 【区域 1】高功率区间
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
    // 【区域 2 & 3】低功率区间 (p0 < 2/3)
    if (k >= bound1_L && k < bound1_H) {
        // 【区域 2】k < 1 时的中间区域
        double term1 = p0 * fmax(0.0, 1.0 - k) / fmax(tol, 2.0 * (1.0 + 3.0 * k));
        double term2_denom = fmax(tol, sqrt(fmax(0.0, 2.0 * p0 * (1.0 - k) * (1.0 + 3.0 * k))));
        double term2 = 2.0 * k * p0 / term2_denom;

        D1 = 1.0 - sqrt(term1) - term2;
        D2 = sqrt(term1);

    } else if (k >= bound2_L && k < bound2_H) {
        // 【区域 3】k >= 1 时的中间区域
        double term1 = p0 * fmax(0.0, k - 1.0) / fmax(tol, 2.0 * (k + 3.0));
        double term2_denom = fmax(tol, sqrt(fmax(0.0, 2.0 * p0 * (k*k + 2.0*k - 3.0))));
        double term2 = 2.0 * p0 / term2_denom;

        D1 = 1.0 - sqrt(term1) - term2;
        D2 = sqrt(term1);

    } else {
        // 【兜底】跳出中间区域，回退到高功率计算逻辑
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

// 4. 最终输出限幅
D1 = fmax(0.0, fmin(1.0, D1));
D2 = fmax(0.0, fmin(1.0, D2));

// 5. 赋值给输出端口：端口0是 D1, 端口1是 D2
OutputSignal(0, 0) = D1;
OutputSignal(1, 0) = D2;
