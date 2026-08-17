@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d "%~dp0..\.."
set "HOST_BUILD_DIR=%TEMP%\resst_host_build"
if not exist "%HOST_BUILD_DIR%" mkdir "%HOST_BUILD_DIR%"

echo ===== 1/17 test_sci_rx_queue =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_sci_rx_queue.exe" tests\host\test_sci_rx_queue.c firmware\app\sci_rx_queue.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_sci_rx_queue.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 2/17 test_uart_frame =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_uart_frame.exe" tests\host\test_uart_frame.c firmware\services\uart_frame.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_uart_frame.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 3/17 test_spi_request =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_spi_request.exe" tests\host\test_spi_request.c firmware\services\spi_request.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_spi_request.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 4/17 test_spi_bridge =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_spi_bridge.exe" tests\host\test_spi_bridge.c firmware\services\spi_bridge.c firmware\services\uart_frame.c firmware\services\spi_request.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_spi_bridge.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 5/17 test_step3_control =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_control.exe" tests\host\test_step3_control.c firmware\control\control_faststep.c firmware\control\safe_openloop.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_control.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 6/17 test_step3_state =====
cl.exe /nologo /W3 /wd4100 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_state.exe" tests\host\test_step3_state.c firmware\app\state_machine.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_state.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 7/17 test_step3_params =====
cl.exe /nologo /W3 /wd4100 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_params.exe" tests\host\test_step3_params.c firmware\app\param_manager.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_params.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 8/17 test_step3_telemetry =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_step3_telemetry.exe" tests\host\test_step3_telemetry.c firmware\app\telemetry.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_step3_telemetry.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 9/17 test_init_diag =====
cl.exe /nologo /W3 /wd4100 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_init_diag.exe" tests\host\test_init_diag.c firmware\app\state_machine.c firmware\app\param_manager.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_init_diag.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 10/17 test_measurement_offset =====
cl.exe /nologo /W3 /wd4100 /utf-8 /D__interrupt= /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_measurement_offset.exe" tests\host\test_measurement_offset.c firmware\services\measurement.c /I.
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_measurement_offset.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 11/17 test_pll =====
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_pll.exe" tests\host\test_pll.c firmware\control\control_pll.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_pll.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 12/17 test_run_control =====
cl.exe /nologo /W3 /wd4100 /utf-8 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_run_control.exe" tests\host\test_run_control.c firmware\app\run_control.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_run_control.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 13/17 test_run_supervisor =====
cl.exe /nologo /W3 /wd4100 /utf-8 /DPLATFORM_PROFILE_PROTOTYPE /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_run_supervisor.exe" tests\host\test_run_supervisor.c firmware\app\run_supervisor.c firmware\app\state_machine.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_run_supervisor.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 14/17 test_pwm_tz =====
cl.exe /nologo /W3 /wd4100 /utf-8 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_pwm_tz.exe" tests\host\test_pwm_tz.c firmware\drivers\drv_epwm.c /Itests\host\fake_ti /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_pwm_tz.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 15/17 test_closedloop =====
cl.exe /nologo /W3 /wd4100 /utf-8 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_closedloop.exe" tests\host\test_closedloop.c firmware\control\control_closedloop.c firmware\control\control_qsg.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_closedloop.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 16/17 test_pll_host_protocol =====
cl.exe /nologo /W3 /wd4100 /utf-8 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_pll_host_protocol.exe" tests\host\test_pll_host_protocol.c firmware\services\pll_host_protocol.c firmware\control\control_pll.c firmware\control\control_closedloop.c firmware\control\control_qsg.c firmware\app\sci_rx_queue.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_pll_host_protocol.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== 17/17 test_justfloat =====
cl.exe /nologo /W3 /wd4100 /utf-8 /D__interrupt= /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_justfloat.exe" tests\host\test_justfloat.c firmware\services\justfloat.c /Itests\host\fake_ti /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
"%HOST_BUILD_DIR%\test_justfloat.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== ALL HOST TESTS PASSED =====
