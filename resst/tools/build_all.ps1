# Build all 4 configurations for F28335_RTControl_Platform
$ErrorActionPreference = "Stop"

$PROJ = "E:\repos\DSP28335\F28335_RTControl_Platform"
$CC = "E:\ti\ccs2051\ccs\tools\compiler\ti-cgt-c2000_25.11.0.LTS\bin\cl2000"
$CG_INC = "E:\ti\ccs2051\ccs\tools\compiler\ti-cgt-c2000_25.11.0.LTS\include"
$CG_LIB = "E:\ti\ccs2051\ccs\tools\compiler\ti-cgt-c2000_25.11.0.LTS\lib"

$COMMON = @(
    "-v28", "-ml", "-mt", "--float_support=fpu32",
    "--diag_warning=225", "--display_error_number", "--diag_wrap=off",
    "--include_path=`"$PROJ`"",
    "--include_path=`"$CG_INC`"",
    "--include_path=`"$PROJ\INCLUDE`""
)

$SRC_FILES = @(
    "$PROJ\SRC\DSP2833x_CodeStartBranch.asm",
    "$PROJ\SRC\DSP2833x_CpuTimers.c",
    "$PROJ\SRC\DSP2833x_DefaultIsr.c",
    "$PROJ\SRC\DSP2833x_GlobalVariableDefs.c",
    "$PROJ\SRC\DSP2833x_MemCopy.c",
    "$PROJ\SRC\DSP2833x_PieCtrl.c",
    "$PROJ\SRC\DSP2833x_PieVect.c",
    "$PROJ\SRC\DSP2833x_usDelay.asm",
    "$PROJ\firmware\app\app_context.c",
    "$PROJ\firmware\app\app.c",
    "$PROJ\firmware\app\diagnostics.c",
    "$PROJ\firmware\app\isr.c",
    "$PROJ\firmware\app\main.c",
    "$PROJ\firmware\app\param_manager.c",
    "$PROJ\firmware\app\scheduler.c",
    "$PROJ\firmware\app\sci_rx_queue.c",
    "$PROJ\firmware\app\state_machine.c",
    "$PROJ\firmware\app\telemetry.c",
    "$PROJ\firmware\control\control_faststep.c",
    "$PROJ\firmware\control\safe_openloop.c",
    "$PROJ\firmware\bsp\board.c",
    "$PROJ\firmware\drivers\drv_adc.c",
    "$PROJ\firmware\drivers\drv_epwm.c",
    "$PROJ\firmware\drivers\drv_gpio.c",
    "$PROJ\firmware\drivers\drv_interrupt.c",
    "$PROJ\firmware\drivers\drv_sci.c",
    "$PROJ\firmware\drivers\drv_spi.c",
    "$PROJ\firmware\drivers\drv_sysctrl.c",
    "$PROJ\firmware\drivers\drv_timer.c",
    "$PROJ\firmware\services\indicator.c",
    "$PROJ\firmware\services\spi_bridge.c",
    "$PROJ\firmware\services\spi_request.c",
    "$PROJ\firmware\services\uart_frame.c"
)

function Build-One {
    param($Name, $Defines, $LinkerCmd, $ExtraCompiler, $OutDir)

    $out = "$PROJ\$OutDir"
    New-Item -ItemType Directory -Force -Path $out | Out-Null

    Write-Host "==== Building $Name ($OutDir) ===="

    $objs = @()
    foreach ($src in $SRC_FILES) {
        $rel = $src.Replace("$PROJ\", "")
        $objDir = "$out\" + (Split-Path -Parent $rel)
        New-Item -ItemType Directory -Force -Path $objDir | Out-Null
        $obj = "$out\$rel" -replace '\.(c|asm)$', '.obj'
        $objs += $obj

        $defArgs = ($Defines | ForEach-Object { "--define=$_" })
        $cmdArgs = @($CC, "--compile_only") + $COMMON + $ExtraCompiler + $defArgs +
                   @("--obj_directory=`"$objDir`"", "`"$src`"")

        $cmd = $cmdArgs -join " "
        Write-Host "  Compiling: $rel"
        $result = cmd /c $cmd 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  ERROR: $result"
            exit 1
        }
    }

    # Link
    $objList = ($objs | ForEach-Object { "`"$_`"" }) -join " "
    $linkArgs = @(
        $CC, "-v28", "-ml", "-mt", "--float_support=fpu32"
    ) + $ExtraCompiler + ($Defines | ForEach-Object { "--define=$_" }) + @(
        "--diag_warning=225", "--display_error_number", "--diag_wrap=off",
        "-z", "-m`"$out\$($Name).map`"",
        "--stack_size=0x300", "--warn_sections",
        "-i`"$CG_LIB`"", "-i`"$CG_INC`"",
        "--reread_libs", "--display_error_number", "--diag_wrap=off",
        "--xml_link_info=`"$out\$($Name)_linkInfo.xml`"",
        "--rom_model",
        "-o `"$out\$($Name).out`"",
        $objList,
        "`"$PROJ\CMD\DSP2833x_Headers_nonBIOS.cmd`"",
        "`"$PROJ\$LinkerCmd`"",
        "-llibc.a"
    )

    $linkCmd = $linkArgs -join " "
    Write-Host "  Linking: $($Name).out"
    $result = cmd /c $linkCmd 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  LINK ERROR: $result"
        exit 1
    }
    Write-Host "  SUCCESS: $($Name).out"
    Write-Host ""
}

# 1. Debug (Prototype_RAM_Debug)
Build-One -Name "F28335_RTControl_Platform" `
    -Defines @("PLATFORM_PROFILE_PROTOTYPE") `
    -LinkerCmd "linker\28335_RAM_lnk.cmd" `
    -ExtraCompiler @("-g") `
    -OutDir "Debug"

# 2. Release (Prototype_Flash_Demo)
Build-One -Name "F28335_RTControl_Platform" `
    -Defines @("FLASH", "PLATFORM_PROFILE_PROTOTYPE") `
    -LinkerCmd "linker\f28335_flash.cmd" `
    -ExtraCompiler @() `
    -OutDir "Release"

# 3. Industrial_RAM (Industrial_RAM_Debug)
Build-One -Name "F28335_RTControl_Platform" `
    -Defines @("PLATFORM_PROFILE_INDUSTRIAL") `
    -LinkerCmd "linker\28335_RAM_lnk.cmd" `
    -ExtraCompiler @("-g") `
    -OutDir "Industrial_RAM"

# 4. Flash_Release (Industrial_Flash_Release)
Build-One -Name "F28335_RTControl_Platform" `
    -Defines @("FLASH", "PLATFORM_PROFILE_INDUSTRIAL") `
    -LinkerCmd "linker\f28335_flash.cmd" `
    -ExtraCompiler @() `
    -OutDir "Flash_Release"

Write-Host "==== All 4 configurations built successfully ===="
