@echo off
REM Release (Flash) build for DROOP_SPI_UART_REFACTOR
REM Uses --define=FLASH and linker/f28335_flash.cmd

set "CG_TOOL_ROOT=E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS"
set "CC=%CG_TOOL_ROOT%/bin/cl2000"
set "PROJ_ROOT=E:/repos/DSP28335/DROOP_SPI_UART_REFACTOR"
set "BLD=%PROJ_ROOT%/Release"

cd /d "%PROJ_ROOT%"

REM Common compiler flags (same as Debug, minus -g, plus --define=FLASH)
set "CFLAGS=-v28 -ml -mt --float_support=fpu32 --define=FLASH --diag_warning=225 --display_error_number --diag_wrap=off"
set "INCLUDES=--include_path=\"%PROJ_ROOT%\" --include_path=\"%CG_TOOL_ROOT%/include\" --include_path=\"%PROJ_ROOT%/INCLUDE\""

echo === Release (Flash) Build for DROOP_SPI_UART_REFACTOR ===
echo.

REM ---- Compile C sources ----
echo [1/7] Compiling SRC files...
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_CodeStartBranch.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_CodeStartBranch.asm"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_CpuTimers.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_CpuTimers.c"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_DefaultIsr.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_DefaultIsr.c"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_GlobalVariableDefs.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_GlobalVariableDefs.c"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_MemCopy.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_MemCopy.c"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_PieCtrl.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_PieCtrl.c"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_PieVect.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_PieVect.c"
if %ERRORLEVEL% neq 0 goto :error

"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/SRC/DSP2833x_usDelay.d_raw" --obj_directory="%BLD%/SRC" "%PROJ_ROOT%/SRC/DSP2833x_usDelay.asm"
if %ERRORLEVEL% neq 0 goto :error

echo [2/7] Compiling firmware/app...
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/app/app_context.d_raw" --obj_directory="%BLD%/firmware/app" "%PROJ_ROOT%/firmware/app/app_context.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/app/isr.d_raw" --obj_directory="%BLD%/firmware/app" "%PROJ_ROOT%/firmware/app/isr.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/app/main.d_raw" --obj_directory="%BLD%/firmware/app" "%PROJ_ROOT%/firmware/app/main.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/app/scheduler.d_raw" --obj_directory="%BLD%/firmware/app" "%PROJ_ROOT%/firmware/app/scheduler.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/app/sci_rx_queue.d_raw" --obj_directory="%BLD%/firmware/app" "%PROJ_ROOT%/firmware/app/sci_rx_queue.c"
if %ERRORLEVEL% neq 0 goto :error

echo [3/7] Compiling firmware/bsp...
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/bsp/board.d_raw" --obj_directory="%BLD%/firmware/bsp" "%PROJ_ROOT%/firmware/bsp/board.c"
if %ERRORLEVEL% neq 0 goto :error

echo [4/7] Compiling firmware/drivers...
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/drivers/drv_gpio.d_raw" --obj_directory="%BLD%/firmware/drivers" "%PROJ_ROOT%/firmware/drivers/drv_gpio.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/drivers/drv_interrupt.d_raw" --obj_directory="%BLD%/firmware/drivers" "%PROJ_ROOT%/firmware/drivers/drv_interrupt.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/drivers/drv_sci.d_raw" --obj_directory="%BLD%/firmware/drivers" "%PROJ_ROOT%/firmware/drivers/drv_sci.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/drivers/drv_spi.d_raw" --obj_directory="%BLD%/firmware/drivers" "%PROJ_ROOT%/firmware/drivers/drv_spi.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/drivers/drv_sysctrl.d_raw" --obj_directory="%BLD%/firmware/drivers" "%PROJ_ROOT%/firmware/drivers/drv_sysctrl.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/drivers/drv_timer.d_raw" --obj_directory="%BLD%/firmware/drivers" "%PROJ_ROOT%/firmware/drivers/drv_timer.c"
if %ERRORLEVEL% neq 0 goto :error

echo [5/7] Compiling firmware/services...
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/services/indicator.d_raw" --obj_directory="%BLD%/firmware/services" "%PROJ_ROOT%/firmware/services/indicator.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/services/spi_bridge.d_raw" --obj_directory="%BLD%/firmware/services" "%PROJ_ROOT%/firmware/services/spi_bridge.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/services/spi_request.d_raw" --obj_directory="%BLD%/firmware/services" "%PROJ_ROOT%/firmware/services/spi_request.c"
if %ERRORLEVEL% neq 0 goto :error
"%CC%" %CFLAGS% %INCLUDES% --preproc_with_compile --preproc_dependency="%BLD%/firmware/services/uart_frame.d_raw" --obj_directory="%BLD%/firmware/services" "%PROJ_ROOT%/firmware/services/uart_frame.c"
if %ERRORLEVEL% neq 0 goto :error

echo [6/7] Linking with Flash linker...
"%CC%" -v28 -ml -mt --float_support=fpu32 --diag_warning=225 --display_error_number --diag_wrap=off -z -m"%BLD%/DROOP_SPI_UART_REFACTOR.map" --stack_size=0x300 --warn_sections -i"%CG_TOOL_ROOT%/lib" -i"%CG_TOOL_ROOT%/include" --reread_libs --display_error_number --diag_wrap=off --xml_link_info="%BLD%/DROOP_SPI_UART_REFACTOR_linkInfo.xml" --rom_model -o "%BLD%/DROOP_SPI_UART_REFACTOR.out" "%BLD%/SRC/DSP2833x_CodeStartBranch.obj" "%BLD%/SRC/DSP2833x_CpuTimers.obj" "%BLD%/SRC/DSP2833x_DefaultIsr.obj" "%BLD%/SRC/DSP2833x_GlobalVariableDefs.obj" "%BLD%/SRC/DSP2833x_MemCopy.obj" "%BLD%/SRC/DSP2833x_PieCtrl.obj" "%BLD%/SRC/DSP2833x_PieVect.obj" "%BLD%/SRC/DSP2833x_usDelay.obj" "%BLD%/firmware/app/app_context.obj" "%BLD%/firmware/app/isr.obj" "%BLD%/firmware/app/main.obj" "%BLD%/firmware/app/scheduler.obj" "%BLD%/firmware/app/sci_rx_queue.obj" "%BLD%/firmware/bsp/board.obj" "%BLD%/firmware/drivers/drv_gpio.obj" "%BLD%/firmware/drivers/drv_interrupt.obj" "%BLD%/firmware/drivers/drv_sci.obj" "%BLD%/firmware/drivers/drv_spi.obj" "%BLD%/firmware/drivers/drv_sysctrl.obj" "%BLD%/firmware/drivers/drv_timer.obj" "%BLD%/firmware/services/indicator.obj" "%BLD%/firmware/services/spi_bridge.obj" "%BLD%/firmware/services/spi_request.obj" "%BLD%/firmware/services/uart_frame.obj" "%PROJ_ROOT%/CMD/DSP2833x_Headers_nonBIOS.cmd" "%PROJ_ROOT%/linker/f28335_flash.cmd" -llibc.a
if %ERRORLEVEL% neq 0 goto :error

echo.
echo === SUCCESS: Release (Flash) build complete ===
echo Output: %BLD%/DROOP_SPI_UART_REFACTOR.out
echo Map:    %BLD%/DROOP_SPI_UART_REFACTOR.map
goto :eof

:error
echo.
echo === BUILD FAILED ===
exit /b 1
