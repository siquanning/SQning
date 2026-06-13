/*
 * modbus_slave.h — Modbus RTU 从站协议栈
 *
 * 依赖：types.h（Uint8）、TI DSP2833x 头文件（Uint16/Uint32）
 */

#ifndef APP_MODBUS_MODBUS_SLAVE_H_
#define APP_MODBUS_MODBUS_SLAVE_H_

#include "DSP2833x_Device.h"
#include "types.h"

// ---- 从站地址 ------------------------------------
#define MODBUS_ADDR   1

// ---- 功能码 --------------------------------------
#define FUNC_READ_HOLDING   0x03
#define FUNC_READ_INPUT     0x04
#define FUNC_WRITE_SINGLE   0x06

// ---- 异常码 --------------------------------------
#define EXC_ILLEGAL_FUNC    0x01
#define EXC_ILLEGAL_DATA    0x02
#define EXC_ILLEGAL_VALUE   0x03

// ---- 寄存器数量（PRD §9） ------------------------
#define HOLDING_REG_COUNT   8
#define INPUT_REG_COUNT     6

extern Uint16 HoldingRegs[HOLDING_REG_COUNT];
extern Uint16 InputRegs[INPUT_REG_COUNT];

extern volatile Uint32 ms_counter;

// ---- 公开接口 ------------------------------------
void   MB_Init(void);
void   MB_FeedByte(Uint8 data);
Uint16 MB_Poll(void);
Uint16 MB_CRC16(const Uint8 *buf, Uint16 len);

#endif /* APP_MODBUS_MODBUS_SLAVE_H_ */
