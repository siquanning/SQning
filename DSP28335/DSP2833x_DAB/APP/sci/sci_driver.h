/*
 * sci_driver.h
 *
 *  Created on: 2026年5月9日
 *      Author: 32485
 */

#ifndef APP_SCI_SCI_DRIVER_H_
#define APP_SCI_SCI_DRIVER_H_



#include "DSP2833x_Device.h"

#define LSPCLK_FREQ     37500000L   // 根据你的实际时钟调整
#define SCI_BAUDRATE    9600L
#define SCI_BRR_VALUE    ((LSPCLK_FREQ / (SCI_BAUDRATE * 8)) - 1)

void Init_Scia(void);
void Scia_SendChar(char data);
void Scia_SendString(char *str);
void Scia_Receive_Handler(void);    // 在 ISR 中调用

// 注意：必须加上 volatile
extern volatile char Scia_Received_Data;
extern volatile Uint16 Scia_Data_Received_Flag;


#endif /* APP_SCI_SCI_DRIVER_H_ */
