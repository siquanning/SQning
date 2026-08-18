# F28335_RTControl_Platform — Quality Gate Script
# Usage: powershell -ExecutionPolicy Bypass -File tools/quality_gate.ps1
$ErrorActionPreference = "Continue"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjRoot = (Resolve-Path "$ScriptDir\..").Path

$AllPassed = $true
$GateResults = @{}

function Write-Stage {
    param([string]$Message)
    Write-Host "`n=== $Message ===" -ForegroundColor Cyan
}

function Report-Result {
    param([string]$Name, [string]$Status, [string]$Detail = "")
    $GateResults[$Name] = @{ Status = $Status; Detail = $Detail }
    $color = if ($Status -eq "PASS") { "Green" }
             elseif ($Status -eq "SKIP") { "Yellow" }
             elseif ($Status -eq "WARN") { "Yellow" }
             else { "Red" }
    Write-Host "  [$Status] $Name" -ForegroundColor $color
    if ($Detail) { Write-Host "         $Detail" -ForegroundColor $color }
    if ($Status -eq "FAIL") { $script:AllPassed = $false }
}

# =========================================================================
# Stage 1: Host Tests
# =========================================================================
Write-Stage "Stage 1: Host Tests"
$HostTestDir = "$ProjRoot\tests\host"
$HostBuildDir = Join-Path $env:TEMP "resst_host_build"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$msvcAvailable = $false
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -property installationPath 2>$null
    if ($vsPath) {
        $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvars) { $msvcAvailable = $true }
    }
}

if (-not $msvcAvailable) {
    Report-Result "Host Tests" "SKIP" "MSVC 2022 not found"
} else {
    if (Test-Path $HostBuildDir) { Remove-Item -Recurse -Force $HostBuildDir }
    New-Item -ItemType Directory -Force -Path $HostBuildDir | Out-Null

    $allPassed = $true
    $testSpecs = @(
        @{Name="sci_rx_queue"; Src="test_sci_rx_queue.c"; Deps="firmware\app\sci_rx_queue.c"; Defs=""},
        @{Name="uart_frame"; Src="test_uart_frame.c"; Deps="firmware\services\uart_frame.c"; Defs=""},
        @{Name="spi_request"; Src="test_spi_request.c"; Deps="firmware\services\spi_request.c"; Defs=""},
        @{Name="spi_bridge"; Src="test_spi_bridge.c"; Deps="firmware\services\spi_bridge.c firmware\services\uart_frame.c firmware\services\spi_request.c"; Defs=""},
        @{Name="step3_control"; Src="test_step3_control.c"; Deps="firmware\control\control_faststep.c firmware\control\safe_openloop.c"; Defs=""},
        @{Name="step3_state"; Src="test_step3_state.c"; Deps="firmware\app\state_machine.c"; Defs="/DPLATFORM_PROFILE_PROTOTYPE"},
        @{Name="step3_params"; Src="test_step3_params.c"; Deps="firmware\app\param_manager.c"; Defs="/DPLATFORM_PROFILE_PROTOTYPE"},
        @{Name="step3_telemetry"; Src="test_step3_telemetry.c"; Deps="firmware\app\telemetry.c"; Defs=""},
        @{Name="init_diag"; Src="test_init_diag.c"; Deps="firmware\app\state_machine.c firmware\app\param_manager.c"; Defs="/DPLATFORM_PROFILE_PROTOTYPE"},
        @{Name="measurement_offset"; Src="test_measurement_offset.c"; Deps="firmware\services\measurement.c"; Defs="/utf-8 /D__interrupt="},
        @{Name="pll"; Src="test_pll.c"; Deps="firmware\control\control_pll.c"; Defs=""},
        @{Name="pll_host_protocol"; Src="test_pll_host_protocol.c"; Deps="firmware\services\pll_host_protocol.c firmware\control\control_pll.c firmware\app\sci_rx_queue.c"; Defs="/utf-8"},
        @{Name="run_control"; Src="test_run_control.c"; Deps="firmware\app\run_control.c"; Defs="/utf-8"},
        @{Name="run_supervisor"; Src="test_run_supervisor.c"; Deps="firmware\app\run_supervisor.c firmware\app\state_machine.c"; Defs="/utf-8 /DPLATFORM_PROFILE_PROTOTYPE"},
        @{Name="pwm_tz"; Src="test_pwm_tz.c"; Deps="firmware\drivers\drv_epwm.c"; Defs="/utf-8 /Itests\host\fake_ti"},
        @{Name="closedloop"; Src="test_closedloop.c"; Deps="firmware\control\control_closedloop.c"; Defs="/utf-8"},
        @{Name="justfloat"; Src="test_justfloat.c"; Deps="firmware\services\justfloat.c"; Defs="/utf-8 /D__interrupt= /Itests\host\fake_ti"},
        @{Name="ac_protect"; Src="test_ac_protect.c"; Deps="firmware\app\ac_protect.c"; Defs="/utf-8"}
    )

    foreach ($spec in $testSpecs) {
        $name = $spec.Name
        $src = "$HostTestDir\$($spec.Src)"
        $depsAbs = ($spec.Deps -split ' ' | ForEach-Object { Join-Path $ProjRoot $_ })
        $depsStr = ($depsAbs | ForEach-Object { "`"$_`"" }) -join ' '
        $exe = "$HostBuildDir\test_$name.exe"
        $defs = $spec.Defs
        $compileArgs = "/nologo /W3 /wd4100 $defs /Fo`"$HostBuildDir\\`" /Fe`"$exe`" `"$src`" $depsStr /I`"$ProjRoot`""

        $compileResult = cmd /c "call `"$vcvars`" > nul 2>&1 && cl.exe $compileArgs 2>&1"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  COMPILE FAILED: $compileResult"
            $allPassed = $false
        } else {
            $testResult = & $exe 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host "  TEST FAILED: $testResult"
                $allPassed = $false
            } else {
                Write-Host "  PASS: test_$name"
            }
        }
    }

    if ($allPassed) {
        Report-Result "Host Tests ($($testSpecs.Count) suites)" "PASS"
    } else {
        Report-Result "Host Tests" "FAIL" "One or more test suites failed"
    }
}

# =========================================================================
# Stage 2: Static Boundary Checks
# =========================================================================
Write-Stage "Stage 2: Static Boundary Checks"

# 2a: Direct register access outside drivers/ and BSP
Write-Host "  Checking direct register access outside drivers/ and bsp/ ..."
$nonDriverFiles = Get-ChildItem -Path "$ProjRoot\firmware" -Recurse -Include *.c,*.h |
    Where-Object { $_.FullName -notmatch '\\drivers\\' -and $_.FullName -notmatch '\\bsp\\' -and $_.FullName -notmatch '\\reference\\' }
$regAccessViolations = @()
foreach ($f in $nonDriverFiles) {
    $content = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    # Check for direct peripheral register struct access patterns
    if ($content -match 'Regs\.') {
        $matches = Select-String -Path $f.FullName -Pattern 'Regs\.' -AllMatches
        foreach ($m in $matches) {
            $line = $m.Line.Trim()
            if ($line -notmatch '^\s*#include' -and $line -notmatch '^\s*//' -and $line -notmatch '^\s*/\*') {
                $relPath = $f.FullName -replace [regex]::Escape($ProjRoot), ''
                $regAccessViolations += "$relPath : $line"
            }
        }
    }
}
if ($regAccessViolations.Count -eq 0) {
    Report-Result "Register access boundary (app/services/control/algorithm)" "PASS" "No direct Regs. access outside drivers/ and bsp/"
} else {
    Report-Result "Register access boundary" "FAIL" "$($regAccessViolations.Count) violation(s) found"
    foreach ($v in $regAccessViolations) { Write-Host "    $v" }
}

# 2b: Check platform_profile.h guards
Write-Host "  Checking platform_profile.h definition guards ..."
$profileHeader = "$ProjRoot\firmware\platform_profile.h"
if (Test-Path $profileHeader) {
    $content = Get-Content $profileHeader -Raw
    $hasGuard = ($content -match '#error.*both.*defined' -and $content -match '#error.*no profile')
    if ($hasGuard) {
        Report-Result "platform_profile.h compile guards" "PASS"
    } else {
        Report-Result "platform_profile.h compile guards" "FAIL" "Missing #error guard"
    }
} else {
    Report-Result "platform_profile.h" "FAIL" "File not found"
}

# 2c: Check no legacy code in active build paths
Write-Host "  Checking legacy code in active build paths ..."
$legacyInActive = Get-ChildItem -Path "$ProjRoot\firmware" -Directory |
    Where-Object { $_.Name -in @('board_legacy', 'comm_legacy') }
if ($legacyInActive.Count -eq 0) {
    Report-Result "Legacy code in active paths" "PASS"
} else {
    Report-Result "Legacy code in active paths" "FAIL" "Legacy directories present in firmware/"
}

# 2d: Check for DSP2833x_Device.h includes outside drivers/ and bsp/
Write-Host "  Checking DSP2833x_Device.h includes outside drivers/ and bsp/ ..."
$dspIncludeViolations = @()
foreach ($f in $nonDriverFiles) {
    $content = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    if ($content -match '#include\s+"DSP2833x_Device\.h"') {
        $relPath = $f.FullName -replace [regex]::Escape($ProjRoot), ''
        $dspIncludeViolations += $relPath
    }
}
if ($dspIncludeViolations.Count -eq 0) {
    Report-Result "DSP2833x_Device.h isolation (drivers/bsp only)" "PASS"
} else {
    Report-Result "DSP2833x_Device.h isolation" "FAIL" "$($dspIncludeViolations.Count) violation(s): $($dspIncludeViolations -join ', ')"
}

# =========================================================================
# Stage 3: RAM Debug Build (Prototype_RAM_Debug)
# =========================================================================
Write-Stage "Stage 3: RAM Debug Build (Prototype_RAM_Debug)"

$CompilerPath = "E:\ti\ccs2051\ccs\tools\compiler\ti-cgt-c2000_25.11.0.LTS\bin\cl2000.exe"
if (-not (Test-Path $CompilerPath)) {
    Report-Result "RAM Debug Build (Prototype)" "SKIP" "C2000 25.11.0.LTS compiler not found at expected location"
    Report-Result "Flash Release Build (Industrial)" "SKIP" "Compiler unavailable"
} else {
    $CC = $CompilerPath
    $CG_LIB = "E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/lib"
    $CG_INC = "E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include"

    function Invoke-C2000Build {
        param([string]$Label, [string]$BuildDir, [string]$Defines, [string]$LinkerCmd, [string]$OutputName)

        Write-Host "  Building $Label ..."
        $outDir = "$ProjRoot\$BuildDir\gate_build"
        if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
        New-Item -ItemType Directory -Force -Path $outDir | Out-Null

        $allPassed = $true
        $baseFlags = @("-v28", "-ml", "-mt", "--float_support=fpu32",
            "--diag_warning=225", "--display_error_number", "--diag_wrap=off",
            "--include_path=$ProjRoot",
            "--include_path=$CG_INC",
            "--include_path=$ProjRoot/INCLUDE")

        # Add profile/platform defines
        foreach ($d in ($Defines -split ' ')) {
            if ($d) { $baseFlags += "--define=$d" }
        }

        # Compile all sources
        $objDir = "$outDir\obj"
        New-Item -ItemType Directory -Force -Path $objDir | Out-Null
        $objFiles = @()

        $srcFiles = @(
            "$ProjRoot\SRC\DSP2833x_CodeStartBranch.asm",
            "$ProjRoot\SRC\DSP2833x_CpuTimers.c",
            "$ProjRoot\SRC\DSP2833x_DefaultIsr.c",
            "$ProjRoot\SRC\DSP2833x_GlobalVariableDefs.c",
            "$ProjRoot\SRC\DSP2833x_MemCopy.c",
            "$ProjRoot\SRC\DSP2833x_PieCtrl.c",
            "$ProjRoot\SRC\DSP2833x_PieVect.c",
            "$ProjRoot\SRC\DSP2833x_usDelay.asm",
            "$ProjRoot\firmware\app\app_context.c",
            "$ProjRoot\firmware\app\app.c",
            "$ProjRoot\firmware\app\diagnostics.c",
            "$ProjRoot\firmware\app\isr.c",
            "$ProjRoot\firmware\app\ac_protect.c",
            "$ProjRoot\firmware\app\main.c",
            "$ProjRoot\firmware\app\param_manager.c",
            "$ProjRoot\firmware\app\run_control.c",
            "$ProjRoot\firmware\app\run_supervisor.c",
            "$ProjRoot\firmware\app\scheduler.c",
            "$ProjRoot\firmware\app\sci_rx_queue.c",
            "$ProjRoot\firmware\app\state_machine.c",
            "$ProjRoot\firmware\app\telemetry.c",
            "$ProjRoot\firmware\bsp\board.c",
            "$ProjRoot\firmware\control\control_faststep.c",
            "$ProjRoot\firmware\control\control_closedloop.c",
            "$ProjRoot\firmware\control\control_global.c",
            "$ProjRoot\firmware\control\control_openloop.c",
            "$ProjRoot\firmware\control\control_pll.c",
            "$ProjRoot\firmware\control\safe_openloop.c",
            "$ProjRoot\firmware\drivers\drv_adc.c",
            "$ProjRoot\firmware\drivers\drv_epwm.c",
            "$ProjRoot\firmware\drivers\drv_gpio.c",
            "$ProjRoot\firmware\drivers\drv_interrupt.c",
            "$ProjRoot\firmware\drivers\drv_sci.c",
            "$ProjRoot\firmware\drivers\drv_spi.c",
            "$ProjRoot\firmware\drivers\drv_sysctrl.c",
            "$ProjRoot\firmware\drivers\drv_timer.c",
            "$ProjRoot\firmware\services\cpld_spi.c",
            "$ProjRoot\firmware\services\indicator.c",
            "$ProjRoot\firmware\services\justfloat.c",
            "$ProjRoot\firmware\services\measurement.c",
            "$ProjRoot\firmware\services\pll_host_protocol.c",
            "$ProjRoot\firmware\services\modbus_vdc.c",
            "$ProjRoot\firmware\services\spi_bridge.c",
            "$ProjRoot\firmware\services\spi_request.c",
            "$ProjRoot\firmware\services\uart_frame.c"
        )

        foreach ($src in $srcFiles) {
            $name = Split-Path $src -Leaf
            $base = [IO.Path]::GetFileNameWithoutExtension($name)
            $depFile = "$objDir/$base.d_raw"
            $args = $baseFlags + @("--preproc_with_compile",
                "--preproc_dependency=$depFile",
                "--obj_directory=$objDir",
                $src)
            $psi = New-Object System.Diagnostics.ProcessStartInfo
            $psi.FileName = $CC
            $psi.Arguments = $args -join " "
            $psi.UseShellExecute = $false
            $psi.RedirectStandardError = $true
            $psi.RedirectStandardOutput = $true
            $p = [System.Diagnostics.Process]::Start($psi)
            $stderr = $p.StandardError.ReadToEnd()
            $p.WaitForExit()
            if ($p.ExitCode -ne 0) {
                Write-Host "    COMPILE ERROR ($name): $stderr"
                $allPassed = $false
            }
            $objFiles += "$objDir/$base.obj"
        }

        if (-not $allPassed) {
            Report-Result $Label "FAIL" "Compilation errors"
            return
        }

        # Link
        $linkArgs = @("-v28", "-ml", "-mt", "--float_support=fpu32",
            "--diag_warning=225", "--display_error_number", "--diag_wrap=off",
            "-z",
            "-m$outDir/$OutputName.map",
            "--stack_size=0x300",
            "--warn_sections",
            "--retain=code_start",
            "--retain=DSP28x_usDelay",
            "-i$CG_LIB",
            "-i$CG_INC",
            "--reread_libs",
            "--display_error_number",
            "--diag_wrap=off",
            "--xml_link_info=$outDir/${OutputName}_linkInfo.xml",
            "--rom_model",
            "-o", "$outDir/$OutputName.out"
        ) + $objFiles + @("$ProjRoot\CMD\DSP2833x_Headers_nonBIOS.cmd", "$ProjRoot\$LinkerCmd") + @("-llibc.a")

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $CC
        $psi.Arguments = $linkArgs -join " "
        $psi.UseShellExecute = $false
        $psi.RedirectStandardError = $true
        $psi.RedirectStandardOutput = $true
        $p = [System.Diagnostics.Process]::Start($psi)
        $stderr = $p.StandardError.ReadToEnd()
        $p.WaitForExit()
        if ($p.ExitCode -ne 0) {
            Write-Host "    LINK ERROR: $stderr"
            $allPassed = $false
        }
        if ($stderr -match "error #") {
            Write-Host "    LINK ERROR: $stderr"
            $allPassed = $false
        }
        if ($stderr -match "warning") {
            Write-Host "    LINK WARNING: $stderr"
        }

        if ($allPassed) {
            Report-Result $Label "PASS" "Output: $outDir/$OutputName.out, Map: $outDir/$OutputName.map"
        } else {
            Report-Result $Label "FAIL" "Link errors"
        }
    }

    # --- Build Prototype_RAM_Debug ---
    Invoke-C2000Build -Label "Build: Prototype_RAM_Debug" `
        -BuildDir "Debug" -Defines "PLATFORM_PROFILE_PROTOTYPE" `
        -LinkerCmd "linker/28335_RAM_lnk.cmd" `
        -OutputName "F28335_RTControl_Platform"

    # --- Build Industrial_Flash_Release ---
    Invoke-C2000Build -Label "Build: Industrial_Flash_Release" `
        -BuildDir "Flash_Release" -Defines "FLASH PLATFORM_PROFILE_INDUSTRIAL" `
        -LinkerCmd "linker/f28335_flash.cmd" `
        -OutputName "F28335_RTControl_Platform"
}

# =========================================================================
# Stage 4: Source File Sanity
# =========================================================================
Write-Stage "Stage 4: Source File Checks"

$issues = @()
$sourceFiles = Get-ChildItem -Path "$ProjRoot\firmware" -Recurse -Include *.c,*.h |
    Where-Object { $_.FullName -notmatch '\\reference\\' }

foreach ($f in $sourceFiles) {
    $content = Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue
    if (-not $content) { continue }
    $relPath = $f.FullName -replace [regex]::Escape($ProjRoot), ''

    if ($f.Extension -eq '.c' -and $content.Trim().Length -eq 0) {
        $issues += "$relPath : empty source file"
    }
}
if ($issues.Count -eq 0) {
    Report-Result "Source file sanity check" "PASS"
} else {
    Report-Result "Source file sanity check" "WARN" "$($issues.Count) issue(s) found"
    foreach ($i in $issues) { Write-Host "    $i" }
}

# =========================================================================
# Summary
# =========================================================================
Write-Stage "Quality Gate Summary"
Write-Host ""
$passCount = ($GateResults.Values | Where-Object { $_.Status -eq "PASS" }).Count
$skipCount = ($GateResults.Values | Where-Object { $_.Status -eq "SKIP" }).Count
$warnCount = ($GateResults.Values | Where-Object { $_.Status -eq "WARN" }).Count
$failCount = ($GateResults.Values | Where-Object { $_.Status -eq "FAIL" }).Count

Write-Host "  PASS: $passCount  SKIP: $skipCount  WARN: $warnCount  FAIL: $failCount" -ForegroundColor Cyan
Write-Host ""

foreach ($key in $GateResults.Keys) {
    $r = $GateResults[$key]
    $color = if ($r.Status -eq "PASS") { "Green" }
             elseif ($r.Status -eq "SKIP") { "Yellow" }
             elseif ($r.Status -eq "WARN") { "Yellow" }
             else { "Red" }
    Write-Host "  [$($r.Status)] $key" -ForegroundColor $color
    if ($r.Detail) { Write-Host "           $($r.Detail)" }
}

Write-Host ""
if ($failCount -gt 0) {
    Write-Host "QUALITY GATE: FAILED ($failCount failure(s))" -ForegroundColor Red
    exit 1
} elseif ($warnCount -gt 0) {
    Write-Host "QUALITY GATE: PASSED WITH WARNINGS ($warnCount warning(s))" -ForegroundColor Yellow
    exit 0
} else {
    Write-Host "QUALITY GATE: PASSED" -ForegroundColor Green
    exit 0
}
