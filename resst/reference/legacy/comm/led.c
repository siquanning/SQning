#include "led.h"
#include "config/comm_config.h"
#include "DSP2833x_Device.h"

#define LED_TX_GPIO 67U
#define LED_RX_GPIO 68U

static uint32_t g_rxLedOffTick;
static uint32_t g_txLedOffTick;
static uint16_t g_rxLedActive;
static uint16_t g_txLedActive;

void Led_Init(void)
{
    g_rxLedOffTick = 0UL;
    g_txLedOffTick = 0UL;
    g_rxLedActive  = 0U;
    g_txLedActive  = 0U;

    EALLOW;

    GpioDataRegs.GPCSET.bit.GPIO67 = 1;
    GpioDataRegs.GPCSET.bit.GPIO68 = 1;

    GpioCtrlRegs.GPCMUX1.bit.GPIO67 = 0;
    GpioCtrlRegs.GPCMUX1.bit.GPIO68 = 0;

    GpioCtrlRegs.GPCDIR.bit.GPIO67 = 1;
    GpioCtrlRegs.GPCDIR.bit.GPIO68 = 1;

    EDIS;
}

void Led_TriggerRx(uint32_t nowTick)
{
    GpioDataRegs.GPCCLEAR.bit.GPIO68 = 1;
    g_rxLedOffTick = nowTick + LED_DURATION_TICKS;
    g_rxLedActive  = 1U;
}

void Led_TriggerTx(uint32_t nowTick)
{
    GpioDataRegs.GPCCLEAR.bit.GPIO67 = 1;
    g_txLedOffTick = nowTick + LED_DURATION_TICKS;
    g_txLedActive  = 1U;
}

void Led_Service(uint32_t nowTick)
{
    if (g_rxLedActive && (int32_t)(nowTick - g_rxLedOffTick) >= 0)
    {
        GpioDataRegs.GPCSET.bit.GPIO68 = 1;
        g_rxLedActive = 0U;
    }

    if (g_txLedActive && (int32_t)(nowTick - g_txLedOffTick) >= 0)
    {
        GpioDataRegs.GPCSET.bit.GPIO67 = 1;
        g_txLedActive = 0U;
    }
}
