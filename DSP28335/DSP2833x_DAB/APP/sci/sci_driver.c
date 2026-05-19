/*
 * sci_driver.c
 *
 *  Created on: 2026年5月9日
 *      Author: 32485
 */

#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "sci_driver.h"
#include "leds.h"
__interrupt void Scia_Rx_ISR(void);   // 告诉编译器：这个函数后面会定义
// 全局变量，用于中断接收
volatile char Scia_Received_Data = 0;
volatile Uint16 Scia_Data_Received_Flag = 0;

void Init_Scia(void)
{
    // 清除可能残留的中断标志
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    PieCtrlRegs.PIEACK.all = 0xFFFF;

    // 1. 通信参数配置
    SciaRegs.SCICCR.all = 0x0007;

    // 2. 初始控制（SWRESET=0）
    SciaRegs.SCICTL1.all = 0x0003;

    // 3. 使能接收中断总开关
    SciaRegs.SCICTL2.bit.RXBKINTENA = 1;

    // 4. 波特率
    SciaRegs.SCIHBAUD = (SCI_BRR_VALUE >> 8) & 0xFF;
    SciaRegs.SCILBAUD = SCI_BRR_VALUE & 0xFF;

    // 5. 先使能 FIFO，并复位接收/发送 FIFO
    SciaRegs.SCIFFTX.all = 0xE040;   // 使能 FIFO，SCIRST=1，清除发送 FIFO
    // 6. 再配置接收 FIFO
    SciaRegs.SCIFFRX.all = 0x2041;          // 先清标志，设触发深度=1
    SciaRegs.SCIFFRX.bit.RXFFIENA = 1;      // 再单独打开 FIFO 接收中断

    SciaRegs.SCIFFCT.all = 0x0;

    // 7. 退出复位，启动 SCI 模块
    SciaRegs.SCICTL1.all = 0x0027;   // bit2 = 1, 退出复位

    // 8. 中断向量与 PIE
    EALLOW;
    PieVectTable.SCIRXINTA = &Scia_Rx_ISR;
    EDIS;

    IER |= M_INT9;                      // 打开 CPU INT9
    PieCtrlRegs.PIEIER9.bit.INTx1 = 1;

    EINT;
    ERTM;
}

// 发送一个字符 (查询方式)
void Scia_SendChar(char data)
{
    while (SciaRegs.SCIFFTX.bit.TXFFST >= 16); // 等待FIFO有空间
    SciaRegs.SCITXBUF = data;
}

// 发送一个字符串
void Scia_SendString(char *str)
{
    while (*str != '\0')
    {
        Scia_SendChar(*str++);
    }
}

// 接收数据处理函数 (在ISR中调用)
void Scia_Receive_Handler(void)
{
    // 读取接收到的数据，标志位自动清除
    Scia_Received_Data = (char)(SciaRegs.SCIRXBUF.all & 0xFF);
    Scia_Data_Received_Flag = 1;
}

// SCI-A 接收中断服务函数
__interrupt void Scia_Rx_ISR(void)
{
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;
    // 检查接收FIFO中断标志是否置位
    if (SciaRegs.SCIFFRX.bit.RXFFINT)
    {
        LED2_TOGGLE;
        // 循环读取FIFO中所有数据
        while (SciaRegs.SCIFFRX.bit.RXFFST > 0)
        {
            Scia_Receive_Handler();
        }
    }

    // 清除PIE中断应答位，以便下次进入中断
    PieCtrlRegs.PIEACK.bit.ACK9 = 1;
}


