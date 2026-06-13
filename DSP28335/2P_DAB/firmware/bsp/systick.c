#include "include/common.h"
#include "bsp/systick.h"

static systick_callback_t systick_cb = ((void*)0);

void systick_init(void)
{
    // CPU Timer 0 direct register config for 1ms at 150MHz SYSCLKOUT.
    // Period = (TDDRH:TDDR + 1) * (PRDH:PRD + 1) / 150MHz
    //        = (149 + 1) * (999 + 1) / 150e6 = 1ms
    CpuTimer0Regs.TCR.bit.TSS = 1;       // stop timer during config

    CpuTimer0Regs.TPR.all  = 149;        // TDDR=149 (low byte), TDDRH=0 (high byte)
    CpuTimer0Regs.TPRH.all = 0;
    CpuTimer0Regs.PRD.all  = 999;        // 32-bit period register
    CpuTimer0Regs.TCR.bit.TRB = 1;       // reload timer with period values

    // Register ISR in PIE vector table
    EALLOW;
    PieVectTable.TINT0 = &systick_isr;
    EDIS;

    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;  // TINT0 = PIE group 1, channel 7
    IER |= M_INT1;

    CpuTimer0Regs.TCR.bit.TIE = 1;       // enable timer interrupt
    CpuTimer0Regs.TCR.bit.TSS = 0;       // start timer
}

void systick_register_callback(systick_callback_t cb)
{
    systick_cb = cb;
}

__interrupt void systick_isr(void)
{
    if (systick_cb != ((void*)0)) {
        systick_cb();
    }

    CpuTimer0Regs.TCR.bit.TIF = 1;           // clear flag
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}
