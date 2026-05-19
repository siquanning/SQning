/*
 * sci_driver.h — SCI-A (UART) driver for RS-485 Modbus communication
 */

#ifndef APP_SCI_SCI_DRIVER_H_
#define APP_SCI_SCI_DRIVER_H_

#include "DSP2833x_Device.h"
#include "types.h"

// ---- 波特率配置 --------------------------------
#define LSPCLK_FREQ     37500000L
#define SCI_BAUDRATE    9600L
#define SCI_BRR_VALUE   ((LSPCLK_FREQ / (SCI_BAUDRATE * 8)) - 1)

void Init_Scia(void);
void Scia_SendChar(char data);
void Scia_SendString(char *str);

extern volatile char   Scia_Received_Data;
extern volatile Uint16 Scia_Data_Received_Flag;

__interrupt void Scia_Rx_ISR(void);

#endif /* APP_SCI_SCI_DRIVER_H_ */
