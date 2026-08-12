#include "DSP2833x_Device.h"
#include "firmware/services/modbus_vdc.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/app/isr.h"

/* CRC-16/MODBUS lookup table (polynomial 0xA001) */
static const uint16_t crc_table[256] = {
    0x0000U, 0xC0C1U, 0xC181U, 0x0140U, 0xC301U, 0x03C0U, 0x0280U, 0xC241U,
    0xC601U, 0x06C0U, 0x0780U, 0xC741U, 0x0500U, 0xC5C1U, 0xC481U, 0x0440U,
    0xCC01U, 0x0CC0U, 0x0D80U, 0xCD41U, 0x0F00U, 0xCFC1U, 0xCE81U, 0x0E40U,
    0x0A00U, 0xCAC1U, 0xCB81U, 0x0B40U, 0xC901U, 0x09C0U, 0x0880U, 0xC841U,
    0xD801U, 0x18C0U, 0x1980U, 0xD941U, 0x1B00U, 0xDBC1U, 0xDA81U, 0x1A40U,
    0x1E00U, 0xDEC1U, 0xDF81U, 0x1F40U, 0xDD01U, 0x1DC0U, 0x1C80U, 0xDC41U,
    0x1400U, 0xD4C1U, 0xD581U, 0x1540U, 0xD701U, 0x17C0U, 0x1680U, 0xD641U,
    0xD201U, 0x12C0U, 0x1380U, 0xD341U, 0x1100U, 0xD1C1U, 0xD081U, 0x1040U,
    0xF001U, 0x30C0U, 0x3180U, 0xF141U, 0x3300U, 0xF3C1U, 0xF281U, 0x3240U,
    0x3600U, 0xF6C1U, 0xF781U, 0x3740U, 0xF501U, 0x35C0U, 0x3480U, 0xF441U,
    0x3C00U, 0xFCC1U, 0xFD81U, 0x3D40U, 0xFF01U, 0x3FC0U, 0x3E80U, 0xFE41U,
    0xFA01U, 0x3AC0U, 0x3B80U, 0xFB41U, 0x3900U, 0xF9C1U, 0xF881U, 0x3840U,
    0x2800U, 0xE8C1U, 0xE981U, 0x2940U, 0xEB01U, 0x2BC0U, 0x2A80U, 0xEA41U,
    0xEE01U, 0x2EC0U, 0x2F80U, 0xEF41U, 0x2D00U, 0xEDC1U, 0xEC81U, 0x2C40U,
    0xE401U, 0x24C0U, 0x2580U, 0xE541U, 0x2700U, 0xE7C1U, 0xE681U, 0x2640U,
    0x2200U, 0xE2C1U, 0xE381U, 0x2340U, 0xE101U, 0x21C0U, 0x2080U, 0xE041U,
    0xA001U, 0x60C0U, 0x6180U, 0xA141U, 0x6300U, 0xA3C1U, 0xA281U, 0x6240U,
    0x6600U, 0xA6C1U, 0xA781U, 0x6740U, 0xA501U, 0x65C0U, 0x6480U, 0xA441U,
    0x6C00U, 0xACC1U, 0xAD81U, 0x6D40U, 0xAF01U, 0x6FC0U, 0x6E80U, 0xAE41U,
    0xAA01U, 0x6AC0U, 0x6B80U, 0xAB41U, 0x6900U, 0xA9C1U, 0xA881U, 0x6840U,
    0x7800U, 0xB8C1U, 0xB981U, 0x7940U, 0xBB01U, 0x7BC0U, 0x7A80U, 0xBA41U,
    0xBE01U, 0x7EC0U, 0x7F80U, 0xBF41U, 0x7D00U, 0xBDC1U, 0xBC81U, 0x7C40U,
    0xB401U, 0x74C0U, 0x7580U, 0xB541U, 0x7700U, 0xB7C1U, 0xB681U, 0x7640U,
    0x7200U, 0xB2C1U, 0xB381U, 0x7340U, 0xB101U, 0x71C0U, 0x7080U, 0xB041U,
    0x5000U, 0x90C1U, 0x9181U, 0x5140U, 0x9301U, 0x53C0U, 0x5280U, 0x9241U,
    0x9601U, 0x56C0U, 0x5780U, 0x9741U, 0x5500U, 0x95C1U, 0x9481U, 0x5440U,
    0x9C01U, 0x5CC0U, 0x5D80U, 0x9D41U, 0x5F00U, 0x9FC1U, 0x9E81U, 0x5E40U,
    0x5A00U, 0x9AC1U, 0x9B81U, 0x5B40U, 0x9901U, 0x59C0U, 0x5880U, 0x9841U,
    0x8801U, 0x48C0U, 0x4980U, 0x8941U, 0x4B00U, 0x8BC1U, 0x8A81U, 0x4A40U,
    0x4E00U, 0x8EC1U, 0x8F81U, 0x4F40U, 0x8D01U, 0x4DC0U, 0x4C80U, 0x8C41U,
    0x4400U, 0x84C1U, 0x8581U, 0x4540U, 0x8701U, 0x47C0U, 0x4680U, 0x8641U,
    0x8201U, 0x42C0U, 0x4380U, 0x8341U, 0x4100U, 0x81C1U, 0x8081U, 0x4040U
};

static uint16_t ModbusVdc_CRC16(const uint16_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;
    uint16_t i;
    for (i = 0U; i < len; ++i) {
        uint16_t idx = (crc ^ data[i]) & 0xFFU;
        crc = (uint16_t)((crc >> 8) ^ crc_table[idx]);
    }
    return crc;
}

/* ---- Static state ---- */
static uint16_t rx_buf[MODBUS_VDC_MAX_FRAME];
static uint16_t rx_len;
static uint32_t last_byte_tick;
static uint32_t crc_error_count;
static uint32_t request_count;

void ModbusVdc_Init(void)
{
    rx_len          = 0U;
    last_byte_tick  = 0UL;
    crc_error_count = 0UL;
    request_count   = 0UL;
}

/* ---- ADC snapshot with interrupt gate ---- */
static void ModbusVdc_TakeSnapshot(ModbusVdcSnapshot *snap)
{
    DINT;
    snap->vdc_raw[0] = g_vdc_raw[0];
    snap->vdc_raw[1] = g_vdc_raw[1];
    snap->vdc_raw[2] = g_vdc_raw[2];
    snap->vdc_raw[3] = g_vdc_raw[3];
    snap->vdc_raw[4] = g_vdc_raw[4];
    snap->vdc_raw[5] = g_vdc_raw[5];
    snap->vac_raw[0] = g_vac_raw[0];
    snap->vac_raw[1] = g_vac_raw[1];
    snap->vac_raw[2] = g_vac_raw[2];
    snap->iac_raw[0] = g_iac_raw[0];
    snap->iac_raw[1] = g_iac_raw[1];
    snap->iac_raw[2] = g_iac_raw[2];
    snap->adc_frame_count = g_adc_frame_count;
    EINT;
}

/* ---- Register readout from snapshot ---- */
static uint16_t ModbusVdc_ReadReg(uint16_t addr, const ModbusVdcSnapshot *snap)
{
    switch (addr) {
    case 0U: return snap->vdc_raw[0];
    case 1U: return snap->vdc_raw[1];
    case 2U: return snap->vdc_raw[2];
    case 3U: return snap->vdc_raw[3];
    case 4U: return snap->vdc_raw[4];
    case 5U: return snap->vdc_raw[5];
    case 6U: return snap->vac_raw[0];
    case 7U: return snap->vac_raw[1];
    case 8U:  return snap->vac_raw[2];
    case 9U:  return snap->iac_raw[0];
    case 10U: return snap->iac_raw[1];
    case 11U: return snap->iac_raw[2];
    default: return 0x0000U;
    }
}

/* ---- FC03 response: Read Holding Registers ---- */
static void ModbusVdc_SendReadRegisters(uint16_t start, uint16_t qty)
{
    ModbusVdcSnapshot snap;
    uint16_t resp[64];
    uint16_t len, crc, i;

    ModbusVdc_TakeSnapshot(&snap);

    resp[0] = MODBUS_VDC_SLAVE_ID;
    resp[1] = 0x03U;
    resp[2] = qty * 2U;
    len = 3U;

    for (i = 0U; i < qty; ++i) {
        uint16_t reg = ModbusVdc_ReadReg((uint16_t)(start + i), &snap);
        resp[len++] = (reg >> 8) & 0xFFU;
        resp[len++] = reg & 0xFFU;
    }

    crc = ModbusVdc_CRC16(resp, len);
    resp[len++] = crc & 0xFFU;
    resp[len++] = (crc >> 8) & 0xFFU;

    DrvSci_SendBytes(resp, len);
}

/* ---- Frame processing ---- */
static void ModbusVdc_ProcessFrame(const uint16_t *frame, uint16_t len)
{
    uint16_t crc;
    uint16_t start_addr;
    uint16_t quantity;

    if (len != 8U) {
        return;
    }

    if (frame[0] != MODBUS_VDC_SLAVE_ID) {
        return;
    }

    crc = ModbusVdc_CRC16(frame, (uint16_t)(len - 2U));
    if (crc != (uint16_t)((frame[len - 1] << 8) | frame[len - 2])) {
        crc_error_count++;
        return;
    }

    if (frame[1] != 0x03U) {
        return;
    }

    start_addr = (frame[2] << 8) | frame[3];
    quantity   = (frame[4] << 8) | frame[5];

    if ((uint32_t)start_addr + (uint32_t)quantity >
        (uint32_t)MODBUS_ADC_REG_COUNT) {
        return;
    }
    if ((quantity == 0U) ||
        (quantity > MODBUS_ADC_REG_COUNT)) {
        return;
    }

    ModbusVdc_SendReadRegisters(start_addr, quantity);
    request_count++;
}

/* ---- Main poll (call from foreground loop) ---- */
void ModbusVdc_Poll(SciRxQueue *queue, uint32_t now)
{
    SciRxItem item;

    while (SciRxQueue_Pop(queue, &item)) {
        if (item.error_flags != 0U) {
            rx_len = 0U;
            continue;
        }

        {
            uint16_t b = item.data & 0xFFU;

            if (rx_len == 0U) {
                rx_buf[0] = b;
                rx_len    = 1U;
            } else {
                int32_t gap = (int32_t)(item.tick - last_byte_tick);
                if (gap >= (int32_t)MODBUS_VDC_GAP_TICKS) {
                    ModbusVdc_ProcessFrame(rx_buf, rx_len);
                    rx_buf[0] = b;
                    rx_len    = 1U;
                } else if (rx_len < MODBUS_VDC_MAX_FRAME) {
                    rx_buf[rx_len] = b;
                    rx_len++;
                } else {
                    rx_len = 0U;
                }
            }
            last_byte_tick = item.tick;
        }
    }

    if (rx_len > 0U) {
        int32_t gap = (int32_t)(now - last_byte_tick);
        if (gap >= (int32_t)MODBUS_VDC_GAP_TICKS) {
            ModbusVdc_ProcessFrame(rx_buf, rx_len);
            rx_len = 0U;
        }
    }
}
