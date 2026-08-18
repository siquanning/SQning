/* Created by Siquanning */
#ifndef AC_PROTECT_H
#define AC_PROTECT_H

#include "firmware/app/state_machine.h"

/*
 * 瞬时过压/过流比较（纯函数，不写 GPIO/PWM）。
 * 调用方在 20kHz ISR 中根据返回码执行 PWM_BlockOutput + System_EnterFault；
 * 继电器由 FAULT 路径按 PWM → GPIO44 → GPIO42 断开。
 *
 * 优先级：交流过压 > 交流过流 > 直流过压。
 */
SystemFault AcProtect_Check(const float vline[3],
                            const float iac[3],
                            const float vdc[6]);

#endif
