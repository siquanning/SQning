#ifndef DRV_SCI_H
#define DRV_SCI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t lspclk_hz;
    uint32_t baud_rate;
} DrvSciConfig;

void     DrvSci_Init(const DrvSciConfig *config);
uint16_t DrvSci_GetRxFifoCount(void);
uint16_t DrvSci_GetStatus(void);
uint16_t DrvSci_ReadByte(void);
bool     DrvSci_HasError(uint16_t *error_flags);
bool     DrvSci_StatusHasError(uint16_t status);
uint16_t DrvSci_GetErrorFromStatus(uint16_t status);
void     DrvSci_RecoverRx(void);
void     DrvSci_ClearRxInterrupt(void);

/*
 * Blocking byte/bytes send via SCI TX.
 * DrvSci_SendByte waits for TXRDY before writing SCITXBUF.
 * DrvSci_SendBytes sends len bytes from data, one at a time.
 * Call from foreground only (not ISR).
 */
void     DrvSci_SendByte(uint16_t byte);
void     DrvSci_SendBytes(const uint16_t *data, uint16_t len);

/*
 * Non-blocking TX FIFO primitives (SCI-C, PIE 8.6 TX FIFO interrupt).
 * TXFFIL=8: interrupt when TXFFST<=8. Caller must use GetTxFifoFree() and
 * never wait; never assume the FIFO is empty or stuff a fixed 16 bytes.
 */
uint16_t DrvSci_GetTxFifoFree(void);       /* 16 - TXFFST */
void     DrvSci_TxPutByte(uint16_t byte);  /* write SCITXBUF (no wait) */
void     DrvSci_TxIntEnable(uint16_t en);  /* 1 = arm TX FIFO interrupt (TXFFIENA) */
void     DrvSci_ClearTxIntFlag(void);      /* TXFFINTCLR = 1 */

#endif
