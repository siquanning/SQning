#ifndef SPI_REQUEST_H
#define SPI_REQUEST_H

#include <stdint.h>

/* ---- State codes returned by SpiRequest_Service ---- */
#define SPI_REQ_IDLE      0
#define SPI_REQ_WAIT_GAP  1
#define SPI_REQ_WAIT_DONE 2
#define SPI_REQ_DONE      3
#define SPI_REQ_TIMEOUT   4

/* ---- Callback types ---- */
typedef int (*SpiStartFn)(uint16_t txByte);
typedef int (*SpiCompleteFn)(uint16_t *rxByte);

/* ---- Context ---- */
typedef struct
{
    const uint16_t *data;
    uint16_t length;
    uint16_t index;
    uint16_t state;
    uint32_t next_action_tick;
    uint32_t byte_started_tick;

    /* Diagnostics */
    uint32_t req_frames;
    uint32_t req_bytes;
    uint32_t miso_idle_ff;
    uint32_t miso_unexpected;
    uint32_t start_failures;
    uint32_t timeouts;
    uint16_t last_error;
} SpiRequestContext;

/* ---- Diagnostics snapshot ---- */
typedef struct
{
    uint32_t req_frames;
    uint32_t req_bytes;
    uint32_t miso_idle_ff;
    uint32_t miso_unexpected;
    uint32_t start_failures;
    uint32_t timeouts;
    uint16_t last_error;
} SpiRequestDiagnostics;

#define SPI_ERR_NONE       0U
#define SPI_ERR_START_FAIL 1U
#define SPI_ERR_TIMEOUT    2U

/* ---- Public API ---- */
void SpiRequest_Init(SpiRequestContext *context);

int SpiRequest_IsIdle(const SpiRequestContext *context);

void SpiRequest_Start(SpiRequestContext *context,
                      const uint16_t *data,
                      uint16_t length,
                      uint32_t tick);

int SpiRequest_Service(SpiRequestContext *context,
                       uint32_t tick,
                       SpiStartFn startFn,
                       SpiCompleteFn completeFn);

void SpiRequest_Finish(SpiRequestContext *context);

void SpiRequest_GetDiagnostics(const SpiRequestContext *context,
                               SpiRequestDiagnostics *snapshot);

#endif
