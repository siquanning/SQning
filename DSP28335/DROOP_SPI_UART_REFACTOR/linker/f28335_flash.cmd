/*
// TI File based on 28335_RAM_lnk.cmd and F28335.cmd
//
// FILE:    f28335_flash.cmd
//
// TITLE:   Linker Command File for DROOP_SPI_UART_REFACTOR Flash Release
//
//          Flash boot: codestart at BEGIN (0x33FFF6), code in FLASHB,
//          ramfuncs copied to RAML03 at init.
//
//          EABI section names: .text, .bss, .const, .data, .cinit, .switch
//
//          F28335 SARAM is uniform — no overlapping PAGE 0 / PAGE 1 regions.
*/

/* Prevent unused-section elimination from discarding assembly-only
 * symbols that are entry points (code_start) or referenced exclusively
 * by macros (DSP28x_usDelay). These have no C-level callers so the
 * linker's default garbage collection would drop them.
 */
--retain=code_start
--retain=DSP28x_usDelay

MEMORY
{
PAGE 0 :
   /* RAM blocks for ramfuncs execution */
   RAML03     : origin = 0x008000, length = 0x004000     /* Combined RAML0-3 */

   /* Flash sectors */
   FLASHB     : origin = 0x330000, length = 0x008000     /* 32K — primary code */
   FLASHC     : origin = 0x328000, length = 0x008000
   FLASHD     : origin = 0x320000, length = 0x008000
   FLASHE     : origin = 0x318000, length = 0x008000
   FLASHF     : origin = 0x310000, length = 0x008000
   FLASHG     : origin = 0x308000, length = 0x008000
   FLASHH     : origin = 0x300000, length = 0x008000

   /* CSM reserved and password — must NOT contain code */
   CSM_RSVD   : origin = 0x33FF80, length = 0x000076
   CSM_PWL    : origin = 0x33FFF8, length = 0x000008

   /* Flash boot entry point */
   BEGIN      : origin = 0x33FFF6, length = 0x000002

   /* Boot ROM tables (NOLOAD — use ROM copies) */
   ADC_CAL    : origin = 0x380080, length = 0x000009
   IQTABLES   : origin = 0x3FE000, length = 0x000b50
   IQTABLES2  : origin = 0x3FEB50, length = 0x00008c
   FPUTABLES  : origin = 0x3FEBDC, length = 0x0006A0
   BOOTROM    : origin = 0x3FF27C, length = 0x000D44
   RESET      : origin = 0x3FFFC0, length = 0x000002

PAGE 1 :
   BOOT_RSVD  : origin = 0x000002, length = 0x00004E     /* Boot ROM stack */
   RAMM1      : origin = 0x000400, length = 0x000400     /* Stack */
   RAML4      : origin = 0x00C000, length = 0x001000     /* .bss, comm_buffer */
   RAML5      : origin = 0x00D000, length = 0x001000     /* .const */
   RAML6      : origin = 0x00E000, length = 0x001000     /* Reserved */
   RAML7      : origin = 0x00F000, length = 0x001000     /* Reserved */
}


SECTIONS
{
   /* ---- Flash boot entry ---- */
   codestart        : > BEGIN,     PAGE = 0

   /* ---- Code in Flash ---- */
   .text            : > FLASHB,    PAGE = 0
   .cinit           : > FLASHB,    PAGE = 0
   .pinit           : > FLASHB,    PAGE = 0
   .switch          : > FLASHB,    PAGE = 0
   .init_array      : > FLASHB,    PAGE = 0

   /* ---- RAM data ---- */
   .stack           : > RAMM1,     PAGE = 1
   .bss             : > RAML4,     PAGE = 1
   .data            : > RAML4,     PAGE = 1
   .const           : > RAML5,     PAGE = 1
   .sysmem          : > RAMM1,     PAGE = 1

   /* ---- Semantic sections ---- */
   comm_buffer      : > RAML4,     PAGE = 1
   diagnostics      : > RAML4,     PAGE = 1

   /* ---- IQ / FPU tables (use Boot ROM copies) ---- */
   IQmath           : > FLASHB,    PAGE = 0
   IQmathTables     : > IQTABLES,  PAGE = 0, TYPE = NOLOAD
   FPUmathTables    : > FPUTABLES, PAGE = 0, TYPE = NOLOAD

   /*
    * ramfuncs: time-critical functions that MUST run from RAM.
    * Loaded in Flash, copied to RAML03 by Board_Init() via MemCopy().
    * DrvFlash_Init is the primary occupant — it reconfigures Flash
    * wait states and must not execute from Flash during that operation.
    */
   ramfuncs         : LOAD = FLASHB,   PAGE = 0
                      RUN  = RAML03,   PAGE = 0
                      LOAD_START(RamfuncsLoadStart),
                      LOAD_END(RamfuncsLoadEnd),
                      RUN_START(RamfuncsRunStart)

   /* ---- CSM and security (DSECT — not used, CSM unlocked) ---- */
   .reset           : > RESET,     PAGE = 0, TYPE = DSECT
   csm_rsvd         : > CSM_RSVD,  PAGE = 0, TYPE = DSECT
   csmpasswds       : > CSM_PWL,   PAGE = 0, TYPE = DSECT

   /* ---- ADC calibration (factory, NOLOAD) ---- */
   .adc_cal         : load = ADC_CAL,  PAGE = 0, TYPE = NOLOAD

}


/*
//===========================================================================
// End of file.
//===========================================================================
*/
