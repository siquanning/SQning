#include "firmware/bsp/board.h"
#include "firmware/bsp/board_config.h"
#include "firmware/bsp/board_pins.h"
#include "DSP2833x_Device.h"
#include "firmware/drivers/drv_sysctrl.h"
#include "firmware/drivers/drv_interrupt.h"
#include "firmware/drivers/drv_timer.h"
#include "firmware/drivers/drv_sci.h"
#include "firmware/drivers/drv_spi.h"
#if BOARD_ADC_ENABLED
#include "firmware/drivers/drv_adc.h"
#endif
#include "firmware/drivers/drv_epwm.h"
#include "firmware/drivers/drv_gpio.h"
#include "firmware/app/isr.h"

/* 6-module half-bridge PWM: EPWM1–EPWM6 */
static const uint32_t pwm_modules[BOARD_EPWM_MODULE_COUNT] = { 1U, 2U, 3U, 4U, 5U, 6U };

void Board_Init(void)
{
    /*
     * PLL: 20 MHz × 7 / 1 = 140 MHz SYSCLKOUT
     * HISPCP=0x0001 → HSPCLK = 70 MHz
     * LOSPCP=0x0002 → LSPCLK = 35 MHz
     */
    const SysClockConfig clk = { 7U, 3U, 0x0001U, 0x0002U };

#ifdef FLASH
    {
        extern uint16_t RamfuncsLoadStart;
        extern uint16_t RamfuncsLoadEnd;
        extern uint16_t RamfuncsRunStart;
        extern void MemCopy(uint16_t *src, uint16_t *end, uint16_t *dst);
        MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);
    }
#endif

    if (!DrvSysCtrl_Init(&clk))
    {
        for (;;) { }
    }

#ifdef FLASH
    DrvFlash_Init();
#endif

    DrvInterrupt_Init();

    DrvTimer0_Init(BOARD_SYSCLK_MHZ, BOARD_TIMER0_PERIOD_US);
    DrvTimer2_CycleInit();  /* WCET cycle counter running before any ISR fires */
    DrvInterrupt_BindTimer0(&App_Timer0Isr);
    DrvInterrupt_EnableTimer0();

    {
        const DrvSciConfig sciCfg = { BOARD_LSPCLK_HZ, BOARD_SCI_BAUD };
        DrvSci_Init(&sciCfg);
    }
    DrvInterrupt_BindScicRx(&App_ScicRxIsr);
    DrvInterrupt_EnableScicRx();

    {
        const DrvSpiConfig spiCfg = { BOARD_SPIA_BRR };
        DrvSpi_Init(&spiCfg);
    }

    /*==================================================================
     * 6-Module ePWM / ADC / Trip Zone initialization
     *
     * Safety sequence:
     *  1. Halt all ePWM time-base clocks (TBCLKSYNC=0) — safety gate
     *  2. Init each ePWM module to safe-disabled state (AQCSFRC force LOW)
     *  3. Init ADC (power-up, configure, trigger disabled)
     *  4. GPIO mux for all ePWM modules + TZ pins
     *  5. Bind ADC ISR in PIE (framework ready; trigger stays OFF)
     *  6. Write CMPA = TBPRD/2 (50% duty) — shadow load at CTR=ZERO
     *
     * All PWM outputs remain forced LOW after init.
     * Call PWM_Enable() to release outputs; PWM_Disable() to block.
     *==================================================================*/

    /* Step 1: Halt all ePWM time-base clocks (safety gate) */
    DrvEpwm_HaltTimebase();

    /* Step 2: Init each ePWM module to safe-disabled state */
    {
        const DrvEpwmConfig epwmCfg = {
            (uint32_t)(BOARD_SYSCLK_MHZ * 1000000UL),
            BOARD_PWM_FREQ_HZ,
            BOARD_PWM_COUNT_MODE,
            BOARD_PWM_DB_RED,
            BOARD_PWM_DB_FED,
            BOARD_TZ_OSHT_SOURCES | BOARD_TZ_CBC_SOURCES,
            0U,
            0U
        };

        uint32_t i;
        for (i = 0U; i < BOARD_EPWM_MODULE_COUNT; i++)
        {
            DrvEpwm_Init(pwm_modules[i], &epwmCfg);
        }
    }

#if BOARD_ADC_ENABLED
    /* Step 3: Init ADC (powered up, channels configured, triggers disabled) */
    {
        const DrvAdcConfig adcCfg = {
            BOARD_ADC_ADCCLKPS,
            BOARD_ADC_ACQ_PS,
            BOARD_ADC_CPS,
            BOARD_ADC_CH_COUNT,
            { BOARD_ADC_CONV00, BOARD_ADC_CONV01, BOARD_ADC_CONV02,
              BOARD_ADC_CONV03, BOARD_ADC_CONV04, BOARD_ADC_CONV05,
              BOARD_ADC_CONV06, BOARD_ADC_CONV07, BOARD_ADC_CONV08,
              BOARD_ADC_CONV09, BOARD_ADC_CONV10, BOARD_ADC_CONV11 }
        };
        DrvAdc_Init(&adcCfg);
    }
#endif

    /* Step 4: GPIO mux for all ePWM modules + TZ */
    {
        uint32_t i;
        for (i = 0U; i < BOARD_EPWM_MODULE_COUNT; i++)
        {
            DrvEpwm_ConfigGpio(pwm_modules[i]);
        }
    }

#if BOARD_ADC_ENABLED
    /* Step 5: Bind ADC ISR + enable trigger chain (SOCA → ADC → interrupt) */
    DrvInterrupt_BindAdcSeq1(&App_AdcIsr);
    DrvInterrupt_EnableAdcSeq1();
    DrvEpwm_EnableAdcSocA(BOARD_EPWM_MODULE);
    DrvAdc_EnableTrigger();
#endif

    /* Step 6: Set initial CMPA on all 6 modules (CMPB is not in H1 modulation chain) */
    {
        const uint16_t init_cmp = (uint16_t)(((uint32_t)BOARD_PWM_TBPRD
                                   * (uint32_t)BOARD_PWM_FIXED_DUTY_PERMILL) / 1000U);
        uint32_t i;
        for (i = 0U; i < BOARD_EPWM_MODULE_COUNT; i++)
        {
            DrvEpwm_SetCompareA(pwm_modules[i], init_cmp,
                                BOARD_MODULATION_DUTY_MAX_PERMILL);
        }
    }

    /* Step 7: CPLD control signals — all LOW (safe: gates blocked) */
    DrvGpio_InitFaultGate();
    DrvGpio_InitCpldLed();
    DrvGpio_InitUniPolarity();

    DrvInterrupt_EnableGlobal();
    DrvTimer0_Start();
}

void PWM_StartTimebase(void)
{
#if BOARD_PWM_ADC_HW_CONFIRMED == 1U
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1U;  /* Start all ePWM counters */
    EDIS;
#endif
}

void PWM_BlockOutput(void)
{
    uint32_t i;

#if BOARD_PWM_ADC_HW_CONFIRMED == 1U
    DrvGpio_WriteFaultGate(0U);   /* FAULT_GATE=0 FIRST — CPLD blocks gates */
    for (i = 0U; i < BOARD_EPWM_MODULE_COUNT; i++)
    {
        DrvEpwm_ForceTrip(pwm_modules[i]);  /* TZ OST force → outputs LOW */
    }
#endif
}

void PWM_ReleaseOutput(void)
{
    uint32_t i;

#if BOARD_PWM_ADC_HW_CONFIRMED == 1U
    for (i = 0U; i < BOARD_EPWM_MODULE_COUNT; i++)
    {
        DrvEpwm_ClearOstTrip(pwm_modules[i]);  /* Clear TZ latch */
    }
    DrvGpio_WriteFaultGate(1U);   /* FAULT_GATE=1 LAST — CPLD enables gates */
#endif
}

void PWM_Disable(void)
{
    PWM_BlockOutput();
}

uint16_t PWM_AreTripInputsClear(void)
{
#if BOARD_PWM_ADC_HW_CONFIRMED == 1U
    /*
     * Read the real-time GPIO input buffer — NOT the latched TZFLG.
     * Active level is defined by BOARD_TZ1_ACTIVE_LEVEL / BOARD_TZ2_ACTIVE_LEVEL
     * in board_config.h (verified against schematic).
     */
    uint16_t tz1_ok = (GpioDataRegs.GPADAT.bit.GPIO12 != BOARD_TZ1_ACTIVE_LEVEL);
    uint16_t tz2_ok = (GpioDataRegs.GPADAT.bit.GPIO13 != BOARD_TZ2_ACTIVE_LEVEL);
    return (tz1_ok && tz2_ok) ? 1U : 0U;
#else
    return 1U;
#endif
}
