/* Created by Siquanning */
#ifndef MODBUS_VDC_H
#define MODBUS_VDC_H

#include <stdint.h>
#include "firmware/app/sci_rx_queue.h"

#define MODBUS_VDC_SLAVE_ID     1U
#define MODBUS_VDC_MAX_FRAME    32U

/* 4 ms silent gap at 100 us scheduler tick */
#define MODBUS_VDC_GAP_TICKS    40U

/* HR0..HR11 = 12 ADC raw registers (Vdc×6 + Vac×3 + Iac×3) */
#define MODBUS_ADC_REG_COUNT    12U

typedef struct {
    uint16_t vdc_raw[6];
    uint16_t vac_raw[3];
    uint16_t iac_raw[3];
    uint32_t adc_frame_count;
} ModbusVdcSnapshot;

void ModbusVdc_Init(void);
void ModbusVdc_Poll(SciRxQueue *queue, uint32_t now);

#endif
