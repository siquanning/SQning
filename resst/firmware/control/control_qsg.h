/* Created by Siquanning */
#ifndef CONTROL_QSG_H
#define CONTROL_QSG_H

#include <stdint.h>

/*
 * CONTROL_QSG_H — 单相虚拟正交轴（SOGI-QSG）
 *
 * 用途: 单相 dq 电流内环需要 Iα/Iβ 两相分量；实际只有当前测试相一路电流，
 *       用 SOGI(Second Order Generalized Integrator)-QSG 生成与输入正交约 90° 的 β 轴。
 *
 * 算法（前向欧拉离散，20kHz 调用）:
 *   x1 += ts·( k·ω·(u − x1) − ω·x2 )
 *   x2 += ts·( ω·x1 )
 *   vα = x1
 *   vβ = x2          // β 滞后 α 90°（u=cos(ωt) 时稳态 vα=cos, vβ=sin）
 *
 * 中心角频率 ω 每拍由调用方传入（PLL omega），跟随电网频率（45~55Hz）。
 * ω<=0 时本拍不积分（状态保持），避免 PLL 未就绪时发散。
 *
 * 纯控制层模块：不访问任何全局/外设，只依赖传入参数，host 可测。
 */

typedef struct
{
    float x1;      /* α 状态（vα） */
    float x2;      /* β 状态（vβ，滞后 α 90°） */
    float k;       /* SOGI 阻尼系数（√2 ≈ 1.414） */
    float omega;   /* 中心角频率 [rad/s]（跟随 PLL） */
} QsgSogi;

/* 初始化：k=√2，状态清零 */
void Qsg_Init(QsgSogi *q);

/* 状态清零（相切换 / disable 时使用） */
void Qsg_Reset(QsgSogi *q);

/*
 * 单拍 SOGI 更新。
 *   u     : 输入（当前测试相 Iac [A]）
 *   omega : 中心角频率 [rad/s]（= 2π·PLL freq）
 *   ts    : 控制周期 [s]（20kHz → 50e-6）
 * 输出经 q->x1 / q->x2 读取（vα / vβ）。
 */
void Qsg_Run(QsgSogi *q, float u, float omega, float ts);

#endif /* CONTROL_QSG_H */
