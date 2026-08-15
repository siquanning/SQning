#ifndef FAKE_DSP2833X_DEVICE_H
#define FAKE_DSP2833X_DEVICE_H

/*
 * Host-test fake of TI's DSP2833x_Device.h.
 * Provides a minimal register model covering exactly what drv_epwm.c
 * references at compile time. TZ latch/interrupt semantics are NOT
 * emulated here (plain storage) — the test models them explicitly.
 */

#include <stdint.h>

typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int16_t  Int16;
typedef int32_t  Int32;

#define EALLOW
#define EDIS
#define DINT
#define EINT

struct EPWM_REGS
{
    Uint16 TBPRD;
    Uint16 TBCTR;
    union { Uint16 all; struct { Uint16 TBPHS:16; } half; } TBPHS;
    union {
        Uint16 all;
        struct {
            Uint16 CTRMODE:2; Uint16 PHSEN:1; Uint16 PRDLD:1;
            Uint16 SYNCOSEL:2; Uint16 HSPCLKDIV:3; Uint16 CLKDIV:3;
            Uint16 FREE_SOFT:2; Uint16 rsvd:2;
        } bit;
    } TBCTL;
    Uint16 rsvd1;
    union { Uint16 all; struct { Uint16 CMPA:16; } half; } CMPA;
    Uint16 CMPB;
    union {
        Uint16 all;
        struct {
            Uint16 LOADAMODE:1; Uint16 LOADBMODE:1;
            Uint16 SHDWAMODE:1; Uint16 SHDWBMODE:1; Uint16 rsvd:12;
        } bit;
    } CMPCTL;
    union {
        Uint16 all;
        struct {
            Uint16 ZRO:2; Uint16 PRD:2; Uint16 CAU:2; Uint16 CAD:2;
            Uint16 CBU:2; Uint16 CBD:2; Uint16 rsvd:4;
        } bit;
    } AQCTLA;
    union {
        Uint16 all;
        struct {
            Uint16 ZRO:2; Uint16 PRD:2; Uint16 CAU:2; Uint16 CAD:2;
            Uint16 CBU:2; Uint16 CBD:2; Uint16 rsvd:4;
        } bit;
    } AQCTLB;
    union { Uint16 all; struct { Uint16 rsvd:16; } bit; } AQSFRC;
    union {
        Uint16 all;
        struct { Uint16 CSFA:2; Uint16 CSFB:2; Uint16 RLDCSF:2; Uint16 rsvd:10; } bit;
    } AQCSFRC;
    union {
        Uint16 all;
        struct { Uint16 OUT_MODE:2; Uint16 POLSEL:2; Uint16 IN_MODE:2; Uint16 rsvd:10; } bit;
    } DBCTL;
    Uint16 DBRED;
    Uint16 DBFED;
    union { Uint16 all; struct { Uint16 rsvd:16; } bit; } TZSEL;
    union {
        Uint16 all;
        struct { Uint16 TZA:2; Uint16 TZB:2; Uint16 rsvd:12; } bit;
    } TZCTL;
    union {
        Uint16 all;
        struct { Uint16 INT:1; Uint16 CBC:1; Uint16 OST:1; Uint16 rsvd:13; } bit;
    } TZEINT;
    union {
        Uint16 all;
        struct { Uint16 INT:1; Uint16 CBC:1; Uint16 OST:1; Uint16 rsvd:13; } bit;
    } TZCLR;
    union {
        Uint16 all;
        struct { Uint16 INT:1; Uint16 CBC:1; Uint16 OST:1; Uint16 rsvd:13; } bit;
    } TZFRC;
    union {
        Uint16 all;
        struct { Uint16 INT:1; Uint16 CBC:1; Uint16 OST:1; Uint16 rsvd:13; } bit;
    } TZFLG;
    union {
        Uint16 all;
        struct {
            Uint16 INTEN:1; Uint16 INTSEL:3; Uint16 SOCASEL:3;
            Uint16 SOCBSEL:3; Uint16 SOCAEN:1; Uint16 SOCBEN:1; Uint16 rsvd:4;
        } bit;
    } ETSEL;
    union {
        Uint16 all;
        struct { Uint16 INTPRD:2; Uint16 SOCAPRD:2; Uint16 SOCBPRD:2; Uint16 rsvd:10; } bit;
    } ETPS;
    union {
        Uint16 all;
        struct { Uint16 INT:1; Uint16 SOCA:1; Uint16 SOCB:1; Uint16 rsvd:13; } bit;
    } ETCLR;
    union { Uint16 all; struct { Uint16 rsvd:16; } bit; } PCCTL;
};

union PCLKCR0_REG { Uint16 all; struct { Uint16 TBCLKSYNC:1; Uint16 rsvd:15; } bit; };
union PCLKCR1_REG {
    Uint16 all;
    struct {
        Uint16 EPWM1ENCLK:1; Uint16 EPWM2ENCLK:1; Uint16 EPWM3ENCLK:1;
        Uint16 EPWM4ENCLK:1; Uint16 EPWM5ENCLK:1; Uint16 EPWM6ENCLK:1;
        Uint16 rsvd:10;
    } bit;
};

struct SYS_CTRL_REGS
{
    Uint16 rsvd0;
    union PCLKCR0_REG PCLKCR0;
    union PCLKCR1_REG PCLKCR1;
};

struct GPIO_CTRL_REGS
{
    union {
        Uint32 all;
        struct {
            Uint32 GPIO0:2;  Uint32 GPIO1:2;  Uint32 GPIO2:2;  Uint32 GPIO3:2;
            Uint32 GPIO4:2;  Uint32 GPIO5:2;  Uint32 GPIO6:2;  Uint32 GPIO7:2;
            Uint32 GPIO8:2;  Uint32 GPIO9:2;  Uint32 GPIO10:2; Uint32 GPIO11:2;
            Uint32 GPIO12:2; Uint32 GPIO13:2; Uint32 GPIO14:2; Uint32 GPIO15:2;
        } bit;
    } GPAMUX1;
};

extern volatile struct EPWM_REGS    EPwm1Regs;
extern volatile struct EPWM_REGS    EPwm2Regs;
extern volatile struct EPWM_REGS    EPwm3Regs;
extern volatile struct EPWM_REGS    EPwm4Regs;
extern volatile struct EPWM_REGS    EPwm5Regs;
extern volatile struct EPWM_REGS    EPwm6Regs;
extern volatile struct SYS_CTRL_REGS  SysCtrlRegs;
extern volatile struct GPIO_CTRL_REGS GpioCtrlRegs;

#endif /* FAKE_DSP2833X_DEVICE_H */
