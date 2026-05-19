/*
 * types.h — Shared type definitions for the project
 *
 * TI DSP2833x headers define Uint16 and Uint32 but NOT Uint8.
 * This header fills that gap and serves as the single source of truth
 * for project-wide type aliases, breaking the circular dependency
 * between sci_driver.h and modbus_slave.h.
 */

#ifndef APP_COMMON_TYPES_H_
#define APP_COMMON_TYPES_H_

typedef unsigned char Uint8;

#endif /* APP_COMMON_TYPES_H_ */
