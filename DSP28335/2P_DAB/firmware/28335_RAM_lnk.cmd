/*
 * 28335_RAM_lnk.cmd — TMS320F28335 RAM Linker Command File
 *
 * Memory layout for RAM-based debugging (load to RAM, run from RAM).
 * For Flash-based release builds, use a separate _FLASH.cmd file.
 *
 * F28335 memory map:
 *   M0/M1:   1Kw each (0x0000-0x07FF)
 *   L0-L3:   4Kw each (0x8000-0x8FFF), total 16Kw
 *   L4-L7:   4Kw each (0xC000-0xFFFF), total 16Kw
 *   Flash:   256Kw (0x300000-0x33FFFF)
 *   BootROM: 8Kw (0x3FE000-0x3FFFFF)
 */

MEMORY
{
PAGE 0 :  /* Program Memory */
   BEGIN      : origin = 0x000000, length = 0x000002  /* Boot-to-RAM entry */
   RAMM0      : origin = 0x000050, length = 0x0003B0  /* M0 after codestart */
   RAML0_L3   : origin = 0x008000, length = 0x004000  /* L0-L3: 16Kw */
   RAML4_L7   : origin = 0x00C000, length = 0x004000  /* L4-L7: 16Kw */
   RESET      : origin = 0x3FFFC0, length = 0x000002  /* Reset vector (boot ROM) */
   CSM_RSVD   : origin = 0x33FF80, length = 0x000076  /* Reserved for CSM */
   CSM_PWL    : origin = 0x33FFF8, length = 0x000008  /* CSM password locations */

PAGE 1 :  /* Data Memory */
   RAMM1      : origin = 0x000400, length = 0x000400  /* M1: 1Kw */
   RAML0_L3_D : origin = 0x008000, length = 0x004000  /* L0-L3 data mirror */
   RAML4_L7_D : origin = 0x00C000, length = 0x004000  /* L4-L7 data mirror */
}

SECTIONS
{
   /* Allocate to PAGE 0 (program) */
   .text       : > RAML0_L3,    PAGE = 0
   .cinit      : > RAML0_L3,    PAGE = 0
   .pinit      : > RAML0_L3,    PAGE = 0
   .switch     : > RAML0_L3,    PAGE = 0
   .econst     : > RAML4_L7,    PAGE = 0
   .reset      : > RESET,       PAGE = 0, TYPE = DSECT  /* Not used, keep for reference */
   codestart   : > BEGIN,       PAGE = 0

   /* Allocate to PAGE 1 (data) */
   .stack      : > RAMM1,       PAGE = 1
   .ebss       : > RAML4_L7_D,  PAGE = 1
   .esysmem    : > RAMM1,       PAGE = 1
   .cio        : > RAML0_L3_D,  PAGE = 1

   /* FPU support */
   .TI.ramfunc : {} LOAD = RAML0_L3,
                         RUN  = RAML0_L3,
                         LOAD_START(_RamfuncsLoadStart),
                         LOAD_SIZE(_RamfuncsLoadSize),
                         LOAD_END(_RamfuncsLoadEnd),
                         RUN_START(_RamfuncsRunStart),
                         RUN_SIZE(_RamfuncsRunSize),
                         RUN_END(_RamfuncsRunEnd),
                         PAGE = 0
}

/* Stack and heap sizes */
__STACK_SIZE = 0x200;   /* 512 words */
__HEAP_SIZE  = 0x100;   /* 256 words */
