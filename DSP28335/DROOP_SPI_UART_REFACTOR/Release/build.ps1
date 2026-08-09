# Release (Flash) build for DROOP_SPI_UART_REFACTOR
$ErrorActionPreference = "Stop"

$CC = "E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000"
$CFLAGS = "-v28 -ml -mt --float_support=fpu32 --define=FLASH --diag_warning=225 --display_error_number --diag_wrap=off"
$INCLUDES = @(
    "--include_path=E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR",
    "--include_path=E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include",
    "--include_path=E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR/INCLUDE"
) -join " "
$BLD = "E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR/Release"
$SRC = "E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR"

function Compile-C2000($srcFile, $objDir) {
    $name = Split-Path $srcFile -Leaf
    $base = [IO.Path]::GetFileNameWithoutExtension($name)
    $depFile = "$objDir/$base.d_raw"
    Write-Host "  Compiling $srcFile ..."
    $args = @(
        "-v28", "-ml", "-mt", "--float_support=fpu32", "--define=FLASH",
        "--diag_warning=225", "--display_error_number", "--diag_wrap=off",
        "--include_path=E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR",
        "--include_path=E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include",
        "--include_path=E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR/INCLUDE",
        "--preproc_with_compile",
        "--preproc_dependency=$depFile",
        "--obj_directory=$objDir",
        $srcFile
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $CC
    $psi.Arguments = $args -join " "
    $psi.UseShellExecute = $false
    $psi.RedirectStandardError = $true
    $psi.RedirectStandardOutput = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $stderr = $p.StandardError.ReadToEnd()
    $stdout = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    if ($stderr) { Write-Host $stderr }
    if ($stdout) { Write-Host $stdout }
    if ($p.ExitCode -ne 0) {
        Write-Host "ERROR: Compilation failed with exit code $($p.ExitCode)"
        exit 1
    }
    if ($stderr -match "error|Error") {
        Write-Host "ERROR: Compilation had errors"
        exit 1
    }
}

Write-Host "=== Release (Flash) Build ==="

Write-Host "[1/6] SRC files..."
Compile-C2000 "$SRC/SRC/DSP2833x_CodeStartBranch.asm" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_CpuTimers.c" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_DefaultIsr.c" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_GlobalVariableDefs.c" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_MemCopy.c" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_PieCtrl.c" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_PieVect.c" "$BLD/SRC"
Compile-C2000 "$SRC/SRC/DSP2833x_usDelay.asm" "$BLD/SRC"

Write-Host "[2/6] firmware/app..."
Compile-C2000 "$SRC/firmware/app/app_context.c" "$BLD/firmware/app"
Compile-C2000 "$SRC/firmware/app/diagnostics.c" "$BLD/firmware/app"
Compile-C2000 "$SRC/firmware/app/isr.c" "$BLD/firmware/app"
Compile-C2000 "$SRC/firmware/app/main.c" "$BLD/firmware/app"
Compile-C2000 "$SRC/firmware/app/scheduler.c" "$BLD/firmware/app"
Compile-C2000 "$SRC/firmware/app/sci_rx_queue.c" "$BLD/firmware/app"

Write-Host "[3/6] firmware/bsp..."
Compile-C2000 "$SRC/firmware/bsp/board.c" "$BLD/firmware/bsp"

Write-Host "[4/6] firmware/drivers..."
Compile-C2000 "$SRC/firmware/drivers/drv_gpio.c" "$BLD/firmware/drivers"
Compile-C2000 "$SRC/firmware/drivers/drv_interrupt.c" "$BLD/firmware/drivers"
Compile-C2000 "$SRC/firmware/drivers/drv_sci.c" "$BLD/firmware/drivers"
Compile-C2000 "$SRC/firmware/drivers/drv_spi.c" "$BLD/firmware/drivers"
Compile-C2000 "$SRC/firmware/drivers/drv_sysctrl.c" "$BLD/firmware/drivers"
Compile-C2000 "$SRC/firmware/drivers/drv_timer.c" "$BLD/firmware/drivers"

Write-Host "[5/6] firmware/services..."
Compile-C2000 "$SRC/firmware/services/indicator.c" "$BLD/firmware/services"
Compile-C2000 "$SRC/firmware/services/spi_bridge.c" "$BLD/firmware/services"
Compile-C2000 "$SRC/firmware/services/spi_request.c" "$BLD/firmware/services"
Compile-C2000 "$SRC/firmware/services/uart_frame.c" "$BLD/firmware/services"

Write-Host "[6/6] Linking (Flash)..."
$objList = @(
    "$BLD/SRC/DSP2833x_CodeStartBranch.obj",
    "$BLD/SRC/DSP2833x_CpuTimers.obj",
    "$BLD/SRC/DSP2833x_DefaultIsr.obj",
    "$BLD/SRC/DSP2833x_GlobalVariableDefs.obj",
    "$BLD/SRC/DSP2833x_MemCopy.obj",
    "$BLD/SRC/DSP2833x_PieCtrl.obj",
    "$BLD/SRC/DSP2833x_PieVect.obj",
    "$BLD/SRC/DSP2833x_usDelay.obj",
    "$BLD/firmware/app/app_context.obj",
    "$BLD/firmware/app/diagnostics.obj",
    "$BLD/firmware/app/isr.obj",
    "$BLD/firmware/app/main.obj",
    "$BLD/firmware/app/scheduler.obj",
    "$BLD/firmware/app/sci_rx_queue.obj",
    "$BLD/firmware/bsp/board.obj",
    "$BLD/firmware/drivers/drv_gpio.obj",
    "$BLD/firmware/drivers/drv_interrupt.obj",
    "$BLD/firmware/drivers/drv_sci.obj",
    "$BLD/firmware/drivers/drv_spi.obj",
    "$BLD/firmware/drivers/drv_sysctrl.obj",
    "$BLD/firmware/drivers/drv_timer.obj",
    "$BLD/firmware/services/indicator.obj",
    "$BLD/firmware/services/spi_bridge.obj",
    "$BLD/firmware/services/spi_request.obj",
    "$BLD/firmware/services/uart_frame.obj",
    "$SRC/CMD/DSP2833x_Headers_nonBIOS.cmd",
    "$SRC/linker/f28335_flash.cmd"
)

$linkArgs = @(
    "-v28", "-ml", "-mt", "--float_support=fpu32",
    "--diag_warning=225", "--display_error_number", "--diag_wrap=off",
    "-z",
    "-m$BLD/DROOP_SPI_UART_REFACTOR.map",
    "--stack_size=0x300",
    "--warn_sections",
    "--retain=code_start",
    "--retain=DSP28x_usDelay",
    "-iE:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/lib",
    "-iE:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include",
    "--reread_libs",
    "--display_error_number",
    "--diag_wrap=off",
    "--xml_link_info=$BLD/DROOP_SPI_UART_REFACTOR_linkInfo.xml",
    "--rom_model",
    "-o", "$BLD/DROOP_SPI_UART_REFACTOR.out"
) + $objList + @("-llibc.a")

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $CC
$psi.Arguments = $linkArgs -join " "
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$psi.RedirectStandardOutput = $true
$p = [System.Diagnostics.Process]::Start($psi)
$stderr = $p.StandardError.ReadToEnd()
$stdout = $p.StandardOutput.ReadToEnd()
$p.WaitForExit()
if ($stderr) { Write-Host $stderr }
if ($stdout) { Write-Host $stdout }
if ($p.ExitCode -ne 0) {
    Write-Host "LINK ERROR: exit code $($p.ExitCode)"
    exit 1
}
if ($stderr -match "error #") {
    Write-Host "LINK ERROR: errors detected"
    exit 1
}

Write-Host ""
Write-Host "=== SUCCESS: Release (Flash) build complete ==="
Write-Host "Output: $BLD/DROOP_SPI_UART_REFACTOR.out"
Write-Host "Map:    $BLD/DROOP_SPI_UART_REFACTOR.map"
