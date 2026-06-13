/*
 * modbus_slave.c — Modbus RTU 从站协议实现
 *
 * 基于帧的协议：字节通过 SCI ISR 逐一喂入 MB_FeedByte()。
 * MB_Poll() 检测 5ms 字节间隔超时（约 3.5 字符 @ 9600），
 * 超时后处理完整帧。收到有效帧时返回 1，供调用者驱动 LED。
 */

#include "DSP2833x_Device.h"
#include "modbus_slave.h"
#include "sci_driver.h"
#include "app_modbus.h"

#define RX_BUF_SIZE    64
#define MAX_RESP_LEN  256

static Uint8  rx_buf[RX_BUF_SIZE];
static Uint16 rx_cnt = 0;
static Uint32 last_rx_time = 0;

Uint16 HoldingRegs[HOLDING_REG_COUNT];
Uint16 InputRegs[INPUT_REG_COUNT];

void MB_Init(void)
{
    rx_cnt = 0;
    last_rx_time = 0;
}

// ---- 内部辅助函数 ------------------------------------------------------------

static void SendResponse(const Uint8 *data, Uint16 len)
{
    Uint16 i;
    for (i = 0; i < len; i++)
        Scia_SendChar(data[i]);
}

static void SendException(Uint8 func, Uint8 excode)
{
    Uint8 resp[5];
    resp[0] = MODBUS_ADDR;
    resp[1] = func | 0x80;
    resp[2] = excode;
    Uint16 crc = MB_CRC16(resp, 3);
    resp[3] = crc & 0xFF;
    resp[4] = crc >> 8;
    SendResponse(resp, 5);
}

// ---- 功能码 0x03：读保持寄存器 ------------------------------------------------

static void HandleReadHolding(void)
{
    Uint16 start_addr = ((Uint16)rx_buf[2] << 8) | rx_buf[3];
    Uint16 quantity   = ((Uint16)rx_buf[4] << 8) | rx_buf[5];
    Uint8  resp[MAX_RESP_LEN];

    if (quantity < 1 || quantity > 125)
    {
        SendException(FUNC_READ_HOLDING, EXC_ILLEGAL_VALUE);
        return;
    }
    if (start_addr + quantity > HOLDING_REG_COUNT)
    {
        SendException(FUNC_READ_HOLDING, EXC_ILLEGAL_DATA);
        return;
    }

    resp[0] = MODBUS_ADDR;
    resp[1] = FUNC_READ_HOLDING;
    resp[2] = (Uint8)(quantity * 2);
    Uint16 i;
    for (i = 0; i < quantity; i++)
    {
        Uint16 reg_val = HoldingRegs[start_addr + i];
        resp[3 + i * 2] = (Uint8)(reg_val >> 8);
        resp[4 + i * 2] = (Uint8)(reg_val & 0xFF);
    }
    Uint16 len = 3 + quantity * 2;
    Uint16 crc = MB_CRC16(resp, len);
    resp[len]     = (Uint8)(crc & 0xFF);
    resp[len + 1] = (Uint8)(crc >> 8);
    SendResponse(resp, len + 2);
}

// ---- 功能码 0x04：读输入寄存器 ------------------------------------------------

static void HandleReadInput(void)
{
    Uint16 start_addr = ((Uint16)rx_buf[2] << 8) | rx_buf[3];
    Uint16 quantity   = ((Uint16)rx_buf[4] << 8) | rx_buf[5];
    Uint8  resp[MAX_RESP_LEN];

    if (quantity < 1 || quantity > 125)
    {
        SendException(FUNC_READ_INPUT, EXC_ILLEGAL_VALUE);
        return;
    }
    if (start_addr + quantity > INPUT_REG_COUNT)
    {
        SendException(FUNC_READ_INPUT, EXC_ILLEGAL_DATA);
        return;
    }

    MB_ReadInputRegs();

    resp[0] = MODBUS_ADDR;
    resp[1] = FUNC_READ_INPUT;
    resp[2] = (Uint8)(quantity * 2);
    Uint16 i;
    for (i = 0; i < quantity; i++)
    {
        Uint16 reg_val = InputRegs[start_addr + i];
        resp[3 + i * 2] = (Uint8)(reg_val >> 8);
        resp[4 + i * 2] = (Uint8)(reg_val & 0xFF);
    }
    Uint16 len = 3 + quantity * 2;
    Uint16 crc = MB_CRC16(resp, len);
    resp[len]     = (Uint8)(crc & 0xFF);
    resp[len + 1] = (Uint8)(crc >> 8);
    SendResponse(resp, len + 2);
}

// ---- 功能码 0x06：写单个寄存器 ------------------------------------------------

static void HandleWriteSingle(void)
{
    Uint16 reg_addr = ((Uint16)rx_buf[2] << 8) | rx_buf[3];
    Uint16 value    = ((Uint16)rx_buf[4] << 8) | rx_buf[5];

    if (reg_addr >= HOLDING_REG_COUNT)
    {
        SendException(FUNC_WRITE_SINGLE, EXC_ILLEGAL_DATA);
        return;
    }

    HoldingRegs[reg_addr] = value;
    if (!MB_ApplyRegChanges(reg_addr))
    {
        SendException(FUNC_WRITE_SINGLE, EXC_ILLEGAL_VALUE);
        return;
    }
    SendResponse(rx_buf, 8);
}

// ---- 帧处理 ------------------------------------------------------------------

static Uint16 ProcessFrame(void)
{
    if (rx_cnt < 4) return 0;
    if (rx_buf[0] != MODBUS_ADDR) return 0;

    Uint16 crc_recv = ((Uint16)rx_buf[rx_cnt - 1] << 8) | rx_buf[rx_cnt - 2];
    if (crc_recv != MB_CRC16(rx_buf, rx_cnt - 2))
        return 0;

    switch (rx_buf[1])
    {
        case FUNC_READ_HOLDING:  HandleReadHolding();  break;
        case FUNC_READ_INPUT:    HandleReadInput();    break;
        case FUNC_WRITE_SINGLE:  HandleWriteSingle();  break;
        default:                 SendException(rx_buf[1], EXC_ILLEGAL_FUNC); break;
    }
    return 1;
}

// ---- 公开接口 ----------------------------------------------------------------

void MB_FeedByte(Uint8 data)
{
    if (rx_cnt < RX_BUF_SIZE)
    {
        rx_buf[rx_cnt++] = data;
        last_rx_time = ms_counter;
    }
}

Uint16 MB_Poll(void)
{
    if (rx_cnt > 0 && (ms_counter - last_rx_time) >= 5)
    {
        SciaRegs.SCIFFRX.bit.RXFFIENA = 0;
        Uint16 result = ProcessFrame();
        rx_cnt = 0;
        SciaRegs.SCIFFRX.bit.RXFFIENA = 1;
        return result;
    }
    return 0;
}

Uint16 MB_CRC16(const Uint8 *buf, Uint16 len)
{
    Uint16 crc = 0xFFFF;
    Uint16 i, j;
    for (i = 0; i < len; i++)
    {
        crc ^= buf[i];
        for (j = 0; j < 8; j++)
        {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}
