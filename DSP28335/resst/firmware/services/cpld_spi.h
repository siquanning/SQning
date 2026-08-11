#ifndef CPLD_SPI_H
#define CPLD_SPI_H

#include <stdint.h>

/*
 * CPLD register write via SPI-A.
 *
 * Protocol:  0x02 | ADDR | DATA_H | DATA_L
 * Modulation format: signed int16, unit permill (‰), range ±980.
 *
 * Write to addr 0/1/2 for mA/mB/mC (pending registers),
 * then call CPLD_Commit() to latch all three atomically.
 */

void CPLD_Init(void);

/*
 * Blocking 4-byte SPI write: CMD=0x02 + addr + int16 value (big-endian).
 * Only for foreground protocol verification — do NOT call from ISR.
 */
void CPLD_WriteReg16(uint16_t addr, int16_t value);

/*
 * Commit pending modulation registers (addr 3 ← 0x0001).
 * CPLD latches pending→active at next PWM carrier zero.
 */
void CPLD_Commit(void);

#endif
