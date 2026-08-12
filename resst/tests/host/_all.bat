@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d "%~dp0..\.."
set "HOST_BUILD_DIR=%TEMP%\resst_host_build"
if not exist "%HOST_BUILD_DIR%" mkdir "%HOST_BUILD_DIR%"

echo ===== 1/9 test_sci_rx_queue =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_sci_rx_queue.exe" tests\host\test_sci_rx_queue.c firmware\app\sci_rx_queue.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_sci_rx_queue.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 2/9 test_uart_frame =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_uart_frame.exe" tests\host\test_uart_frame.c firmware\services\uart_frame.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_uart_frame.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 3/9 test_spi_request =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_spi_request.exe" tests\host\test_spi_request.c firmware\services\spi_request.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_spi_request.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 4/9 test_spi_bridge =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_spi_bridge.exe" tests\host\test_spi_bridge.c firmware\services\spi_bridge.c firmware\services\uart_frame.c firmware\services\spi_request.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_spi_bridge.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 5/9 test_step3_control =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_control.exe" tests\host\test_step3_control.c firmware\control\control_faststep.c firmware\control\safe_openloop.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_control.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 6/9 test_step3_state =====
cl.exe /nologo /W3 /wd4100 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_state.exe" tests\host\test_step3_state.c firmware\app\state_machine.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_state.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 7/9 test_step3_params =====
cl.exe /nologo /W3 /wd4100 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_params.exe" tests\host\test_step3_params.c firmware\app\param_manager.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_params.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 8/9 test_step3_telemetry =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_telemetry.exe" tests\host\test_step3_telemetry.c firmware\app\telemetry.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_telemetry.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 9/9 test_init_diag =====
cl.exe /nologo /W3 /wd4100 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_init_diag.exe" tests\host\test_init_diag.c firmware\app\state_machine.c firmware\app\param_manager.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_init_diag.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== ALL HOST TESTS PASSED =====
