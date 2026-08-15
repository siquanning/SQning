#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/* ---- Communication pins (SCI-C, verified) ---- */
#define BOARD_PIN_SCI_TX        63U   /* GPIO63 = SCITXDC */
#define BOARD_PIN_SCI_RX        62U   /* GPIO62 = SCIRXDC */
#define BOARD_PIN_SPIA_MOSI     16U
#define BOARD_PIN_SPIA_MISO     17U
#define BOARD_PIN_SPIA_CLK      18U
#define BOARD_PIN_LED_TX        67U
#define BOARD_PIN_LED_RX        68U

/* ---- Step 2: ADC / ePWM / Trip Zone pins (6-module half-bridge PWM) ---- */
#define BOARD_PIN_EPWM1A         0U    /* GPIO0  — EPWM1A, Phase A left upper  */
#define BOARD_PIN_EPWM1B         1U    /* GPIO1  — EPWM1B, Phase A left lower  */
#define BOARD_PIN_EPWM2A         2U    /* GPIO2  — EPWM2A, Phase A right upper */
#define BOARD_PIN_EPWM2B         3U    /* GPIO3  — EPWM2B, Phase A right lower */
#define BOARD_PIN_EPWM3A         4U    /* GPIO4  — EPWM3A, Phase B left upper  */
#define BOARD_PIN_EPWM3B         5U    /* GPIO5  — EPWM3B, Phase B left lower  */
#define BOARD_PIN_EPWM4A         6U    /* GPIO6  — EPWM4A, Phase B right upper */
#define BOARD_PIN_EPWM4B         7U    /* GPIO7  — EPWM4B, Phase B right lower */
#define BOARD_PIN_EPWM5A         8U    /* GPIO8  — EPWM5A, Phase C left upper  */
#define BOARD_PIN_EPWM5B         9U    /* GPIO9  — EPWM5B, Phase C left lower  */
#define BOARD_PIN_EPWM6A        10U    /* GPIO10 — EPWM6A, Phase C right upper */
#define BOARD_PIN_EPWM6B        11U    /* GPIO11 — EPWM6B, Phase C right lower */
#define BOARD_PIN_TZ1           12U    /* GPIO12 — TZ1 */
#define BOARD_PIN_TZ2           13U    /* GPIO13 — TZ2 */

/* ---- PWM_ENABLE / FAULT_GATE to CPLD (Port A, GPIO30) ---- */
#define BOARD_PIN_FAULT_GATE     30U    /* GPIO30 → CPLD G8: 1=RUN, 0=ALL GATES LOW */

/* ---- Run/Stop button input + run state indicator (Port A) ---- */
#define BOARD_PIN_RUN_BTN       21U    /* GPIO21 — 启停按钮输入 (高有效, CPLD 驱动) */
#define BOARD_PIN_RUN_STATE     20U    /* GPIO20 — 运行状态 LED (高有效) */
#define BOARD_PIN_GRID_SWITCH   22U    /* GPIO22 — S1/S2/S3 三相输入开关 (高有效) */
#define BOARD_PIN_PRECHARGE_BYPASS 23U /* GPIO23 — S4/S5/S6 预充旁路开关 (高有效) */

/* ---- CPLD LED heartbeat (Port A, GPIO26) ---- */
#define BOARD_PIN_CPLD_LED      26U    /* GPIO26 → CPLD GPIOK4 / PIN_111 → LED */

/* ---- UNI polarity status to CPLD (Port A, GPIO27–29) ---- */
#define BOARD_PIN_UNI_A_POS     27U    /* GPIO27 → CPLD GPIOK3 / PIN_110 */
#define BOARD_PIN_UNI_B_POS     28U    /* GPIO28 → CPLD GPIOK1 / PIN_108 */
#define BOARD_PIN_UNI_C_POS     29U    /* GPIO29 → CPLD GPIOJ5 / PIN_104 */

#endif
