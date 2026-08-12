#ifndef DRV_SYSCTRL_H
#define DRV_SYSCTRL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint16_t pll_div;
    uint16_t divsel;
    uint16_t hispcp;
    uint16_t lospcp_div;
} SysClockConfig;

bool DrvSysCtrl_Init(const SysClockConfig *config);

/*
 * Flash wait-state and pipeline configuration.
 * MUST execute from RAM — placed in ramfuncs via linker.
 * Call once after PLL is stable, before any Flash code/data access.
 */
void DrvFlash_Init(void);

#endif
