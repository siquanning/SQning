@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d "%~dp0..\.."
set "HOST_BUILD_DIR=%TEMP%\resst_host_build"
if not exist "%HOST_BUILD_DIR%" mkdir "%HOST_BUILD_DIR%"
cl.exe /nologo /W3 /wd4100 /Fo:"%HOST_BUILD_DIR%\\" /Fe:"%HOST_BUILD_DIR%\test_sci_rx_queue.exe" tests\host\test_sci_rx_queue.c firmware\app\sci_rx_queue.c /I.
if errorlevel 1 exit /b 1
"%HOST_BUILD_DIR%\test_sci_rx_queue.exe"
