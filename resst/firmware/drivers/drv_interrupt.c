#include "DSP2833x_Device.h"
#include "DSP2833x_GlobalPrototypes.h"
#include "firmware/drivers/drv_interrupt.h"

void DrvInterrupt_Init(void)
{
    DINT;
    InitPieCtrl();
    IER = 0U;
    IFR = 0U;
    InitPieVectTable();
}

void DrvInterrupt_BindTimer0(void (*handler)(void))
{
    EALLOW;
    PieVectTable.TINT0 = handler;
    EDIS;
}

void DrvInterrupt_BindScicRx(void (*handler)(void))
{
    EALLOW;
    PieVectTable.SCIRXINTC = handler;
    EDIS;
}

void DrvInterrupt_BindAdcSeq1(void (*handler)(void))
{
    EALLOW;
    PieVectTable.SEQ1INT = handler;
    EDIS;
}

void DrvInterrupt_EnableTimer0(void)
{
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    IER |= M_INT1;
}

void DrvInterrupt_EnableScicRx(void)
{
    PieCtrlRegs.PIEIER8.bit.INTx5 = 1;
    IER |= M_INT8;
}

void DrvInterrupt_EnableAdcSeq1(void)
{
    PieCtrlRegs.PIEIER1.bit.INTx1 = 1;
    IER |= M_INT1;
}

void DrvInterrupt_BindEpwm1(void (*handler)(void))
{
    EALLOW;
    PieVectTable.EPWM1_INT = handler;
    EDIS;
}

void DrvInterrupt_EnableEpwm1(void)
{
    PieCtrlRegs.PIEIER3.bit.INTx1 = 1;   /* EPWM1 = PIE Group 3, INT1 */
    IER |= M_INT3;
}

void DrvInterrupt_BindEpwmTz(uint32_t module, void (*handler)(void))
{
    EALLOW;
    if (module == 1U) PieVectTable.EPWM1_TZINT = handler;
    else if (module == 2U) PieVectTable.EPWM2_TZINT = handler;
    else if (module == 3U) PieVectTable.EPWM3_TZINT = handler;
    else if (module == 4U) PieVectTable.EPWM4_TZINT = handler;
    else if (module == 5U) PieVectTable.EPWM5_TZINT = handler;
    else if (module == 6U) PieVectTable.EPWM6_TZINT = handler;
    EDIS;
}

void DrvInterrupt_EnableEpwmTz(uint32_t module)
{
    if (module == 1U) PieCtrlRegs.PIEIER2.bit.INTx1 = 1U;
    else if (module == 2U) PieCtrlRegs.PIEIER2.bit.INTx2 = 1U;
    else if (module == 3U) PieCtrlRegs.PIEIER2.bit.INTx3 = 1U;
    else if (module == 4U) PieCtrlRegs.PIEIER2.bit.INTx4 = 1U;
    else if (module == 5U) PieCtrlRegs.PIEIER2.bit.INTx5 = 1U;
    else if (module == 6U) PieCtrlRegs.PIEIER2.bit.INTx6 = 1U;
    else return;
    IER |= M_INT2;
}

void DrvInterrupt_AckGroup1(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

void DrvInterrupt_AckGroup2(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP2;
}

void DrvInterrupt_AckGroup3(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

void DrvInterrupt_AckGroup8(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP8;
}

void DrvInterrupt_EnableGlobal(void)
{
    EINT;
    ERTM;
}
