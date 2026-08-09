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

void DrvInterrupt_BindSciaRx(void (*handler)(void))
{
    EALLOW;
    PieVectTable.SCIRXINTA = handler;
    EDIS;
}

void DrvInterrupt_EnableTimer0(void)
{
    PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
    IER |= M_INT1;
}

void DrvInterrupt_EnableSciaRx(void)
{
    PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
    IER |= M_INT9;
}

void DrvInterrupt_AckGroup1(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

void DrvInterrupt_AckGroup9(void)
{
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}

void DrvInterrupt_EnableGlobal(void)
{
    EINT;
    ERTM;
}
