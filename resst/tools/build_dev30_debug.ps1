# =====================================================================
# build_dev30_debug.ps1 — DEV_30MHZ (150MHz) Debug 构建入口
#
# 用途：为 30MHz 开发板生成明确命名的 DEV30 固件产物，避免与
#       TARGET_20MHZ (100MHz) 的 resst.out 混淆（防烧错固件）。
#
# 生成产物：
#   <proj>\Debug\resst_dev30_150mhz_debug.out          (普通 Debug)
#   <proj>\Debug\resst_dev30_150mhz_clock_bringup.out  (-BringUp 时)
#   及同名 .txt manifest（PROFILE/OSCCLK/SYSCLK/PLLCR/DIVSEL/...）
#
# 用法：
#   powershell -File tools\build_dev30_debug.ps1             # 普通 DEV30 Debug
#   powershell -File tools\build_dev30_debug.ps1 -BringUp    # Clock Bring-up 模式
#
# 依赖：
#   - CCS 20.5.1 (E:\ti\ccs2051) 与 ti-cgt-c2000_25.11.0.LTS
#   - 工程 Debug 配置已由 CCS 生成 managed makefile（Debug\makefile）
#   - 通过环境变量 GEN_OPTS__FLAG + gmake -e 注入宏
#     （makefile 内 GEN_OPTS__FLAG := 会覆盖命令行变量，故用环境变量 + -e）
# =====================================================================
$ErrorActionPreference = "Stop"

param(
    [switch]$BringUp
)

$PROJ   = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$GMAKE  = "E:\ti\ccs2051\ccs\utils\bin\gmake.exe"
$DEBUG  = Join-Path $PROJ "Debug"

if (-not (Test-Path (Join-Path $DEBUG "makefile"))) {
    Write-Error "Debug\makefile not found. Open the project in CCS once to generate the managed build."
    exit 1
}

# ---- 宏注入（环境变量 + gmake -e） ----
if ($BringUp) {
    $env:GEN_OPTS__FLAG = "-DBOARD_CLOCK_PROFILE_DEV_30MHZ -DBOARD_CLOCK_BRINGUP_ONLY"
    $suffix = "dev30_150mhz_clock_bringup"
} else {
    $env:GEN_OPTS__FLAG = "-DBOARD_CLOCK_PROFILE_DEV_30MHZ"
    $suffix = "dev30_150mhz_debug"
}

Write-Host "==== Building DEV_30MHZ ($(if($BringUp){'CLOCK_BRINGUP_ONLY'}else{'Debug'})) ===="
Write-Host "GEN_OPTS__FLAG = $env:GEN_OPTS__FLAG"

# 强制完整重建（-B），确保宏对全部源文件生效
& $GMAKE -k -e -B all -r "GEN_OPTS__FLAG=$env:GEN_OPTS__FLAG" 2>&1 | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) {
    Write-Error "DEV30 build FAILED (gmake exit $LASTEXITCODE)"
    exit 1
}

# ---- 复制为明确命名产物 + manifest ----
$outSrc  = Join-Path $DEBUG "resst.out"
$outDst  = Join-Path $DEBUG ("resst_" + $suffix + ".out")
$mapDst  = Join-Path $DEBUG ("resst_" + $suffix + ".map")
$manDst  = Join-Path $DEBUG ("resst_" + $suffix + ".txt")

Copy-Item $outSrc $outDst -Force
if (Test-Path (Join-Path $DEBUG "resst.map")) {
    Copy-Item (Join-Path $DEBUG "resst.map") $mapDst -Force
}

# manifest 内容（与 board_clock_profile.h 根参数一致）
$manifest = @"
PROFILE=DEV_30MHZ
OSCCLK=30000000
SYSCLK=150000000
PLLCR=10
DIVSEL=2
PWM=20000
CONTROL_TS_US=50
BUILD_CONFIG=Debug
BRINGUP_ONLY=$([int]$BringUp)
BUILD_TIME=$(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
"@
Set-Content -Path $manDst -Value $manifest -Encoding ASCII

Write-Host ""
Write-Host "==== SUCCESS ===="
Write-Host "OUT     : $outDst"
Write-Host "MAP     : $mapDst"
Write-Host "MANIFEST: $manDst"
Write-Host "---- manifest ----"
Get-Content $manDst
