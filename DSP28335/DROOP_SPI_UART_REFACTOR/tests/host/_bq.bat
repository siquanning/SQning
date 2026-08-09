@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d "E:\repos\DSP28335\DROOP_SPI_UART_REFACTOR"
if not exist "tests\host\host_build" mkdir "tests\host\host_build"
cl.exe /nologo /W3 /wd4100 /Fo:tests\host\host_build\ /Fe:tests\host\host_build\test_sci_rx_queue.exe tests\host\test_sci_rx_queue.c firmware\app\sci_rx_queue.c /I.
if errorlevel 1 exit /b 1
tests\host\host_build\test_sci_rx_queue.exe
