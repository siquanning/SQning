/*
 * modbus_slave.h — Modbus RTU slave protocol stack
 *
 * Dependencies: types.h (common module) for Uint8; TI DSP2833x headers for Uint16/Uint32.
 * Does NOT depend on sci_driver.h (send functions are called from .c only).
 */

#ifndef APP_MODBUS_MODBUS_SLAVE_H_
#define APP_MODBUS_MODBUS_SLAVE_H_

#include "DSP2833x_Device.h"
#include "types.h"

// ---- 从站地址 ----------------------------------
#define MODBUS_ADDR   1

// ---- 功能码 ------------------------------------
#define FUNC_READ_HOLDING   0x03
#define FUNC_READ_INPUT     0x04
#define FUNC_WRITE_SINGLE   0x06

// ---- 异常码 ------------------------------------
#define EXC_ILLEGAL_FUNC    0x01
#define EXC_ILLEGAL_DATA    0x02
#define EXC_ILLEGAL_VALUE   0x03

// ---- 寄存器数量 --------------------------------
#define HOLDING_REG_COUNT   2
#define INPUT_REG_COUNT     2

extern Uint16 HoldingRegs[HOLDING_REG_COUNT];
extern Uint16 InputRegs[INPUT_REG_COUNT];

extern volatile Uint32 ms_counter;

// ---- 接口函数 ----------------------------------
void   MB_Init(void);
void   MB_FeedByte(Uint8 data);
void   MB_Poll(void);
Uint16 MB_CRC16(const Uint8 *buf, Uint16 len);

#endif /* APP_MODBUS_MODBUS_SLAVE_H_ */
