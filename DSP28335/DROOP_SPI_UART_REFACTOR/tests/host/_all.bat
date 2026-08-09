@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d "E:\repos\DSP28335\DROOP_SPI_UART_REFACTOR"
if not exist "tests\host\host_build" mkdir "tests\host\host_build"

echo ===== 1/4 test_sci_rx_queue =====
cl.exe /nologo /W3 /wd4100 /Fo:tests\host\host_build\ /Fe:tests\host\host_build\test_sci_rx_queue.exe tests\host\test_sci_rx_queue.c firmware\app\sci_rx_queue.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
tests\host\host_build\test_sci_rx_queue.exe
if errorlevel 1 exit /b 1

echo.
echo ===== 2/4 test_uart_frame =====
cl.exe /nologo /W3 /wd4100 /Fo:tests\host\host_build\ /Fe:tests\host\host_build\test_uart_frame.exe tests\host\test_uart_frame.c firmware\services\uart_frame.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
tests\host\host_build\test_uart_frame.exe
if errorlevel 1 exit /b 1

echo.
echo ===== 3/4 test_spi_request =====
cl.exe /nologo /W3 /wd4100 /Fo:tests\host\host_build\ /Fe:tests\host\host_build\test_spi_request.exe tests\host\test_spi_request.c firmware\services\spi_request.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
tests\host\host_build\test_spi_request.exe
if errorlevel 1 exit /b 1

echo.
echo ===== 4/4 test_spi_bridge =====
cl.exe /nologo /W3 /wd4100 /Fo:tests\host\host_build\ /Fe:tests\host\host_build\test_spi_bridge.exe tests\host\test_spi_bridge.c firmware\services\spi_bridge.c firmware\services\uart_frame.c firmware\services\spi_request.c /I. > nul 2>&1
if errorlevel 1 (echo BUILD FAILED & exit /b 1)
tests\host\host_build\test_spi_bridge.exe
if errorlevel 1 exit /b 1

echo.
echo ===== ALL HOST TESTS PASSED =====
