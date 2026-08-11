# BUILD_AND_FLASH — F28335_RTControl_Platform

## 1. Build Configurations (Four Identities)

| # | Configuration | Profile | Memory | Define Flags | Linker | CCS Build Config | Output |
|---|--------------|---------|--------|-------------|--------|-----------------|--------|
| 1 | **Prototype_RAM_Debug** | Prototype | RAM | `PLATFORM_PROFILE_PROTOTYPE` | `linker/28335_RAM_lnk.cmd` | Debug | `Debug/F28335_RTControl_Platform.out` |
| 2 | **Prototype_Flash_Demo** | Prototype | Flash | `FLASH PLATFORM_PROFILE_PROTOTYPE` | `linker/f28335_flash.cmd` | Flash_Demo | `Flash_Demo/F28335_RTControl_Platform.out` |
| 3 | **Industrial_RAM_Debug** | Industrial | RAM | `PLATFORM_PROFILE_INDUSTRIAL` | `linker/28335_RAM_lnk.cmd` | Industrial_RAM | `Industrial_RAM/F28335_RTControl_Platform.out` |
| 4 | **Industrial_Flash_Release** | Industrial | Flash | `FLASH PLATFORM_PROFILE_INDUSTRIAL` | `linker/f28335_flash.cmd` | Flash_Release | `Flash_Release/F28335_RTControl_Platform.out` |

All configurations produce EABI (ELF) output via the TI C2000 CGT 25.11.0.LTS compiler.

## 2. CCS IDE Build (All Configurations)

### 2.1 Prerequisites

- Code Composer Studio (CCS) 20.5.1 or later
- TI C2000 CGT 25.11.0.LTS compiler
- XDS100v3 JTAG debug probe
- Project: `F28335_RTControl_Platform` imported into CCS workspace

### 2.2 Build Steps

1. Open CCS workspace at `E:\repos\DSP28335`
2. Select project `F28335_RTControl_Platform`
3. Right-click project → **Build Configurations** → **Set Active** → choose one of:
   - `Debug` (Prototype_RAM_Debug)
   - `Flash_Demo` (Prototype_Flash_Demo)
   - `Industrial_RAM` (Industrial_RAM_Debug)
   - `Flash_Release` (Industrial_Flash_Release)
4. Build: **Project → Build Project** (Ctrl+B)

Expected output for all configurations:
```
**** Build Finished ****
0 errors, 0 warnings
```

### 2.3 JTAG Download

1. Connect XDS100v3 to target board
2. **Run → Debug** (F11)
3. CCS will: connect target → erase → program → reset → halt at `main()`
4. **Run → Resume** (F8) to start execution

### 2.4 Post-Load Verification (RAM Debug)

After loading, verify via CCS Debug perspective:
- `Timebase_Now()` increments (100 μs tick)
- SCI-A registers: `SCICCR=0x0007`, `SCIHBAUD=0x0001`, `SCILBAUD=0x00E7` (9600 bps)
- SPI-A registers: `SPICCR=0x0087`, `SPIBRR=0x007F` (~293 kHz)

## 3. Command-Line Build (Flash Configurations)

### 3.1 Prerequisites

- PowerShell 5.1 (Windows built-in)
- TI C2000 CGT 25.11.0.LTS at `E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS`
- Project source at `E:/repos/DSP28335/F28335_RTControl_Platform`

### 3.2 Prototype Flash Demo

```powershell
cd E:\repos\DSP28335\F28335_RTControl_Platform\Release
.\build.ps1 -Profile Prototype
```

Output:
```
=== F28335_RTControl_Platform — Prototype_Flash_Demo ===
...
Output: .../Flash_Demo/F28335_RTControl_Platform.out
Map:    .../Flash_Demo/F28335_RTControl_Platform.map
```

### 3.3 Industrial Flash Release

```powershell
cd E:\repos\DSP28335\F28335_RTControl_Platform\Release
.\build.ps1 -Profile Industrial
```

Output:
```
=== F28335_RTControl_Platform — Industrial_Flash_Release ===
...
Output: .../Flash_Release/F28335_RTControl_Platform.out
Map:    .../Flash_Release/F28335_RTControl_Platform.map
```

### 3.4 Flash Build Artifacts

| File | Description |
|------|-------------|
| `{Config}/F28335_RTControl_Platform.out` | ELF executable (Flash loadable) |
| `{Config}/F28335_RTControl_Platform.map` | Linker map (section placement, symbols) |
| `{Config}/F28335_RTControl_Platform_linkInfo.xml` | Detailed link information (XML) |
| `{Config}/F28335_RTControl_Platform.hex` | Intel HEX (if hex converter enabled) |

### 3.5 Compiler/Linker Flags

**Compile** (`cl2000 -v28`):
```
-v28 -ml -mt --float_support=fpu32
-D<PROFILE_MACRO> [-DFLASH]
--diag_warning=225 --display_error_number --diag_wrap=off
--preproc_with_compile --preproc_dependency=<file>.d_raw
--obj_directory=<dir>
```

**Link** (`cl2000 -v28 -z`):
```
-v28 -ml -mt --float_support=fpu32
--stack_size=0x300 --warn_sections
--retain=code_start --retain=DSP28x_usDelay
--reread_libs --rom_model
-o <output>.out --map=<output>.map
--xml_link_info=<output>_linkInfo.xml
-l libc.a
```

The `--retain` directives are **critical**: they prevent the linker from garbage-collecting assembly-only entry points (`code_start`, `DSP28x_usDelay`) that have no C-level callers.

## 4. Flash Programming

### 4.1 Via CCS (Recommended for Development)

1. Open CCS workspace
2. Set active build configuration to `Flash_Demo` or `Flash_Release`
3. **Run → Debug** (F11) — CCS will program Flash via XDS100v3
4. **Run → Resume** (F8) to boot
5. To verify standalone boot: **Run → Terminate** (disconnect JTAG), then power-cycle the board — the firmware should boot from Flash independently.

### 4.2 Via C2Prog (Serial Bootloader, Production)

The F28335 supports serial Flash programming via SCI-A (GPIO35/36) boot mode:

1. Set boot mode pins for SCI-A boot (GPIO87=0, GPIO86=0, GPIO85=1, GPIO84=0)
2. Use C2Prog or serial flash programmer tool
3. Select the `.out` or `.hex` file from the Release build
4. Follow C2Prog prompts to program and verify

### 4.3 Flash Entry Point Verification

After programming, verify via CCS memory browser (or `.map` file):
- `0x33FFF6-0x33FFF7`: codestart section (2 bytes — `LB _c_int00` branch)
- `0x33FFF8-0x33FFFF`: CSM passwords (all 0xFFFF = unlocked)

If codestart is not placed at 0x33FFF6, check that `--retain=code_start` is present in linker arguments.

## 5. Profile-Specific Behavior

| Feature | Prototype | Industrial |
|---------|-----------|------------|
| Extended diagnostics | Enabled | Disabled |
| Safe open-loop testing | Allowed | Blocked |
| Algorithm bypass | Allowed | Blocked |
| Fault quick reset | Enabled | Disabled |
| Debugger halt | Allowed | Blocked |
| Dev defaults | Active | Inactive |
| Production mode | No | Yes |

Profile capabilities are controlled by `firmware/platform_profile.h`. See that file for the full capability matrix.

## 6. Boot Mode Selection

F28335 boot mode is determined by 4 GPIO pins at reset:

| GPIO87 | GPIO86 | GPIO85 | GPIO84 | Mode |
|--------|--------|--------|--------|------|
| 1 | 1 | 1 | 1 | **Flash** (jump to 0x33FFF6) |
| 0 | 0 | 1 | 0 | SCI-A boot (serial programming) |
| 1 | 1 | 0 | 1 | EMU (JTAG) |

For normal standalone operation, all four boot pins must be high (Flash boot).

## 7. Troubleshooting

### 7.1 "codestart not placed" / "BEGIN memory region unused" in map

**Cause**: `--unused_section_elimination=on` (default) garbage-collected the codestart section.

**Fix**: Add `--retain=code_start` to linker arguments (already present in `build.ps1` and `f28335_flash.cmd`).

### 7.2 "ramfuncs not copied" / "Flash init hangs"

**Cause**: `MemCopy()` was not called before `DrvFlash_Init()`, or `ramfuncs` LOAD/RUN directive is missing from the linker command file.

**Fix**: Verify `MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart)` executes before `DrvFlash_Init()` in `Board_Init()` (guarded by `#ifdef FLASH`).

### 7.3 "Device locked" / "CSM error" on connect

**Cause**: Code Security Module (CSM) passwords in Flash are non-0xFFFF.

**Fix**: The build uses `TYPE = DSECT` for CSM regions. An unprogrammed Flash defaults to 0xFFFF (unlocked). If previously secured, use CCS **Tools → On-Chip Flash → Unlock** or perform a full erase.

### 7.4 "Build succeeds but device doesn't boot standalone"

Checklist:
1. Boot pins: all four (GPIO84-87) must be high for Flash boot
2. `codestart` at 0x33FFF6: check map file → `codestart    0   0033fff6   00000002`
3. `code_start` in module list: check map for `DSP2833x_CodeStartBranch.obj`
4. Power-cycle (not just reset): Flash boot is only on power-on reset

### 7.5 Compilation errors

Common causes:
- Missing include path (verify `$INCLUDES` paths in build script)
- Source file renamed or moved (verify path in `Compile-C2000` call)
- Compiler installation path changed (verify `$CC` path)

## 8. Memory Footprint Reference

| Section | Debug (RAM) | Release (Flash) |
|---------|------------|-----------------|
| .text | ~3.3K words (RAML03) | ~2.5K words (FLASHB) |
| .bss + .data | ~0.7K words (RAML4) | ~0.7K words (RAML4) |
| .const | ~0.3K words (RAML5) | ~0.3K words (RAML5) |
| .stack | 0x300 bytes (RAMM1) | 0x300 bytes (RAMM1) |
| ramfuncs | N/A | ~0.1K words (FLASHB→RAML03) |
| Flash used | N/A | ~3.3K words / 256K (1.3%) |

See [MEMORY_LAYOUT.md](MEMORY_LAYOUT.md) for the complete memory map.

## 9. Historical Note

Build artifacts and test records predating the rename to `F28335_RTControl_Platform` use the name `DROOP_SPI_UART_REFACTOR`. These are preserved in `tests/hardware/baseline_artifacts/` and `tests/hardware/BASELINE_TEST_RECORD.md` as historical baselines. All current build scripts, CCS project metadata, and current documentation use `F28335_RTControl_Platform`.
