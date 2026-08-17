/* Created by Siquanning */
#include "firmware/control/control_qsg.h"

#define QSG_K_DEFAULT   1.414213562f   /* √2 阻尼 */

void Qsg_Init(QsgSogi *q)
{
    if (q == ((QsgSogi *)0)) return;
    q->x1 = 0.0f;
    q->x2 = 0.0f;
    q->k = QSG_K_DEFAULT;
    q->omega = 0.0f;
}

void Qsg_Reset(QsgSogi *q)
{
    if (q == ((QsgSogi *)0)) return;
    q->x1 = 0.0f;
    q->x2 = 0.0f;
    /* omega 保留（频率跟随由调用方每拍传入，复位不影响） */
}

void Qsg_Run(QsgSogi *q, float u, float omega, float ts)
{
    float x1, x2, k, w;

    if (q == ((QsgSogi *)0)) return;
    if (ts <= 0.0f) return;

    /* omega<=0（PLL 未就绪/异常）时保持状态不积分 */
    if (omega <= 0.0f) return;

    x1 = q->x1;
    x2 = q->x2;
    k  = q->k;
    w  = omega;

    /* SOGI-QSG 前向欧拉一步 */
    x1 += ts * (k * w * (u - x1) - w * x2);
    x2 += ts * (w * x1);

    q->x1 = x1;
    q->x2 = x2;
    q->omega = w;
}
