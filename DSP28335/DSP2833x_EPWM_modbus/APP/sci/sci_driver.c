/*
 * sci_driver.c — SCI-A driver with FIFO interrupt reception
 *
 * Receive path: ISR → MB_FeedByte() → Modbus state machine
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "sci_driver.h"
#include "modbus_slave.h"

volatile char   Scia_Received_Data = 0;
volatile Uint16 Scia_Data_Received_Flag = 0;

// ---- SCI-A 初始化 --------------------------------------------------------

void Init_Scia(void)
{
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    PieCtrlRegs.PIEACK.all = 0xFFFF;

    // 1 停止位, 无校验, 8 数据位, 空闲线模式
    SciaRegs.SCICCR.all = 0x0007;

    // 初始控制（SWRESET=0, TXENA=1, RXENA=1）
    SciaRegs.SCICTL1.all = 0x0003;

    // 波特率
    SciaRegs.SCIHBAUD = (SCI_BRR_VALUE >> 8) & 0xFF;
    SciaRegs.SCILBAUD =  SCI_BRR_VALUE & 0xFF;

    // 使能 TX FIFO，复位 TX/RX FIFO
    SciaRegs.SCIFFTX.all = 0xE040;

    // RX FIFO: 触发深度=1, 使能 RX FIFO 中断
    SciaRegs.SCIFFRX.all = 0x2041;
    SciaRegs.SCIFFRX.bit.RXFFIENA = 1;

    SciaRegs.SCIFFCT.all = 0x0;

    // 退出复位，启动 SCI（SWRESET=1, TXENA=1, RXENA=1）
    SciaRegs.SCICTL1.all = 0x0023;

    // 使能 SCI-A RX 中断（IER/PIEIER 级别，全局 EINT 由 main.c 最后统一开启）
    IER |= M_INT9;
    PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
}

// ---- 发送（查询方式）------------------------------------------------------

void Scia_SendChar(char data)
{
    while (SciaRegs.SCIFFTX.bit.TXFFST >= 16);
    SciaRegs.SCITXBUF = data;
}

void Scia_SendString(char *str)
{
    while (*str != '\0')
        Scia_SendChar(*str++);
}

// ---- SCI-A 接收中断 ISR ---------------------------------------------------

__interrupt void Scia_Rx_ISR(void)
{
    while (SciaRegs.SCIFFRX.bit.RXFFST > 0)
    {
        char data = (char)(SciaRegs.SCIRXBUF.all & 0xFF);
        MB_FeedByte((Uint8)data);
    }
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}
