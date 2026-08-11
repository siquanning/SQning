#include "firmware/services/indicator.h"
#include "firmware/drivers/drv_gpio.h"
#include "firmware/bsp/board_pins.h"
#include "config/comm_config.h"

static uint32_t g_rxLedOffTick;
static uint32_t g_txLedOffTick;
static uint16_t g_rxLedActive;
static uint16_t g_txLedActive;

void Indicator_Init(void)
{
    g_rxLedOffTick = 0UL;
    g_txLedOffTick = 0UL;
    g_rxLedActive  = 0U;
    g_txLedActive  = 0U;

    DrvGpio_InitOutput(BOARD_PIN_LED_TX, 1);
    DrvGpio_InitOutput(BOARD_PIN_LED_RX, 1);
}

void Indicator_TriggerRx(uint32_t now)
{
    DrvGpio_Clear(BOARD_PIN_LED_RX);
    g_rxLedOffTick = now + LED_DURATION_TICKS;
    g_rxLedActive  = 1U;
}

void Indicator_TriggerTx(uint32_t now)
{
    DrvGpio_Clear(BOARD_PIN_LED_TX);
    g_txLedOffTick = now + LED_DURATION_TICKS;
    g_txLedActive  = 1U;
}

void Indicator_Service(uint32_t now)
{
    if (g_rxLedActive && (int32_t)(now - g_rxLedOffTick) >= 0)
    {
        DrvGpio_Set(BOARD_PIN_LED_RX);
        g_rxLedActive = 0U;
    }

    if (g_txLedActive && (int32_t)(now - g_txLedOffTick) >= 0)
    {
        DrvGpio_Set(BOARD_PIN_LED_TX);
        g_txLedActive = 0U;
    }
}
