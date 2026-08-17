/* Created by Siquanning */
#ifndef UART_FRAME_H
#define UART_FRAME_H

#include <stdint.h>
#include "config/comm_config.h"

#define FRAME_IDLE       0U
#define FRAME_RECEIVING  1U
#define FRAME_READY      2U
#define FRAME_TOO_LONG   3U

typedef struct
{
    uint16_t buffer[UART_FRAME_CAPACITY];
    uint16_t length;
    uint16_t state;
    uint32_t last_rx_tick;

    /* Diagnostics — privately owned by this module */
    uint32_t rx_bytes;
    uint32_t ready_frames;
    uint32_t too_long_frames;
    uint32_t busy_drops;
    uint32_t uart_errors;
    uint16_t last_error;
} UartFrameContext;

typedef struct
{
    uint32_t rx_bytes;
    uint32_t ready_frames;
    uint32_t too_long_frames;
    uint32_t busy_drops;
    uint32_t uart_errors;
    uint16_t last_error;
} UartFrameDiagnostics;

void UartFrame_Init(UartFrameContext *context);

void UartFrame_OnByte(UartFrameContext *context,
                      uint16_t data,
                      uint32_t tick);

void UartFrame_OnError(UartFrameContext *context,
                       uint16_t error_flags,
                       uint32_t tick);

int  UartFrame_Service(UartFrameContext *context,
                       uint32_t tick);

int  UartFrame_IsReady(const UartFrameContext *context);

int  UartFrame_GetReadyData(const UartFrameContext *context,
                            const uint16_t **data,
                            uint16_t *length);

void UartFrame_Consume(UartFrameContext *context);

void UartFrame_GetDiagnostics(const UartFrameContext *context,
                              UartFrameDiagnostics *snapshot);

#endif
