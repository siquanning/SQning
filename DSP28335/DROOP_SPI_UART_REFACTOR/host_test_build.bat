@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d "E:\repos\DSP28335\DROOP_SPI_UART_REFACTOR"

set "BUILD_DIR=tests\host\host_build"

REM === UART Frame test ===
set "EXE_PATH=%BUILD_DIR%\test_uart_frame.exe"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cl.exe /nologo /W3 /wd4100 /Fo%BUILD_DIR%\ /Fe%EXE_PATH% tests\host\test_uart_frame.c firmware\services\uart_frame.c /I.
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===== Running UART Frame tests =====
    "%EXE_PATH%"
) else (
    echo UART FRAME BUILD FAILED
    exit /b 1
)

REM === SPI Request test ===
set "EXE_PATH=%BUILD_DIR%\test_spi_request.exe"
cl.exe /nologo /W3 /wd4100 /Fo%BUILD_DIR%\ /Fe%EXE_PATH% tests\host\test_spi_request.c firmware\services\spi_request.c /I.
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ===== Running SPI Request tests =====
    "%EXE_PATH%"
) else (
    echo SPI REQUEST BUILD FAILED
    exit /b 1
)

echo.
echo ===== Cleaning up host build artifacts =====
rd /s /q "%BUILD_DIR%"
echo All host tests passed.
