################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS

GEN_OPTS__FLAG := 
GEN_CMDS__FLAG := 

ORDERED_OBJS += \
"./SRC/DSP2833x_CodeStartBranch.obj" \
"./SRC/DSP2833x_CpuTimers.obj" \
"./SRC/DSP2833x_DefaultIsr.obj" \
"./SRC/DSP2833x_GlobalVariableDefs.obj" \
"./SRC/DSP2833x_MemCopy.obj" \
"./SRC/DSP2833x_PieCtrl.obj" \
"./SRC/DSP2833x_PieVect.obj" \
"./SRC/DSP2833x_usDelay.obj" \
"./firmware/app/app.obj" \
"./firmware/app/app_context.obj" \
"./firmware/app/diagnostics.obj" \
"./firmware/app/isr.obj" \
"./firmware/app/main.obj" \
"./firmware/app/param_manager.obj" \
"./firmware/app/scheduler.obj" \
"./firmware/app/sci_rx_queue.obj" \
"./firmware/app/state_machine.obj" \
"./firmware/app/telemetry.obj" \
"./firmware/bsp/board.obj" \
"./firmware/control/control_faststep.obj" \
"./firmware/control/control_openloop.obj" \
"./firmware/control/safe_openloop.obj" \
"./firmware/drivers/drv_adc.obj" \
"./firmware/drivers/drv_epwm.obj" \
"./firmware/drivers/drv_gpio.obj" \
"./firmware/drivers/drv_interrupt.obj" \
"./firmware/drivers/drv_sci.obj" \
"./firmware/drivers/drv_spi.obj" \
"./firmware/drivers/drv_sysctrl.obj" \
"./firmware/drivers/drv_timer.obj" \
"./firmware/services/cpld_spi.obj" \
"./firmware/services/indicator.obj" \
"./firmware/services/measurement.obj" \
"./firmware/services/modbus_vdc.obj" \
"./firmware/services/spi_bridge.obj" \
"./firmware/services/spi_request.obj" \
"./firmware/services/uart_frame.obj" \
"../CMD/DSP2833x_Headers_nonBIOS.cmd" \
"../linker/f28335_flash.cmd" \
$(GEN_CMDS__FLAG) \
-llibc.a \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include CMD/subdir_vars.mk
-include SRC/subdir_vars.mk
-include firmware/app/subdir_vars.mk
-include firmware/bsp/subdir_vars.mk
-include firmware/control/subdir_vars.mk
-include firmware/drivers/subdir_vars.mk
-include firmware/services/subdir_vars.mk
-include linker/subdir_vars.mk
-include CMD/subdir_rules.mk
-include SRC/subdir_rules.mk
-include firmware/app/subdir_rules.mk
-include firmware/bsp/subdir_rules.mk
-include firmware/control/subdir_rules.mk
-include firmware/drivers/subdir_rules.mk
-include firmware/services/subdir_rules.mk
-include linker/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(SV7A_DEPS)),)
-include $(SV7A_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
EXE_OUTPUTS += \
resst.out 

EXE_OUTPUTS__QUOTED += \
"resst.out" 


# All Target
all: resst.out

# Tool invocations
resst.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'C2000 Linker - building target: "$@"'
	"E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --float_support=fpu32 -O2 --define=FLASH --define=PLATFORM_PROFILE_INDUSTRIAL --diag_warning=225 --display_error_number --diag_wrap=off -z -m"resst.map" --stack_size=0x300 --warn_sections -i"E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/lib" -i"E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --reread_libs --display_error_number --diag_wrap=off --xml_link_info="resst_linkInfo.xml" --rom_model -o "resst.out" $(ORDERED_OBJS)
	@echo 'Finished building target: "$@"'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXE_OUTPUTS__QUOTED)
	-$(RM) "SRC\DSP2833x_CodeStartBranch.obj" "SRC\DSP2833x_CpuTimers.obj" "SRC\DSP2833x_DefaultIsr.obj" "SRC\DSP2833x_GlobalVariableDefs.obj" "SRC\DSP2833x_MemCopy.obj" "SRC\DSP2833x_PieCtrl.obj" "SRC\DSP2833x_PieVect.obj" "SRC\DSP2833x_usDelay.obj" "firmware\app\app.obj" "firmware\app\app_context.obj" "firmware\app\diagnostics.obj" "firmware\app\isr.obj" "firmware\app\main.obj" "firmware\app\param_manager.obj" "firmware\app\scheduler.obj" "firmware\app\sci_rx_queue.obj" "firmware\app\state_machine.obj" "firmware\app\telemetry.obj" "firmware\bsp\board.obj" "firmware\control\control_faststep.obj" "firmware\control\control_openloop.obj" "firmware\control\safe_openloop.obj" "firmware\drivers\drv_adc.obj" "firmware\drivers\drv_epwm.obj" "firmware\drivers\drv_gpio.obj" "firmware\drivers\drv_interrupt.obj" "firmware\drivers\drv_sci.obj" "firmware\drivers\drv_spi.obj" "firmware\drivers\drv_sysctrl.obj" "firmware\drivers\drv_timer.obj" "firmware\services\cpld_spi.obj" "firmware\services\indicator.obj" "firmware\services\measurement.obj" 
	-$(RM) "firmware\services\modbus_vdc.obj" "firmware\services\spi_bridge.obj" "firmware\services\spi_request.obj" "firmware\services\uart_frame.obj" 
	-$(RM) "SRC\DSP2833x_CpuTimers.d" "SRC\DSP2833x_DefaultIsr.d" "SRC\DSP2833x_GlobalVariableDefs.d" "SRC\DSP2833x_MemCopy.d" "SRC\DSP2833x_PieCtrl.d" "SRC\DSP2833x_PieVect.d" "firmware\app\app.d" "firmware\app\app_context.d" "firmware\app\diagnostics.d" "firmware\app\isr.d" "firmware\app\main.d" "firmware\app\param_manager.d" "firmware\app\scheduler.d" "firmware\app\sci_rx_queue.d" "firmware\app\state_machine.d" "firmware\app\telemetry.d" "firmware\bsp\board.d" "firmware\control\control_faststep.d" "firmware\control\control_openloop.d" "firmware\control\safe_openloop.d" "firmware\drivers\drv_adc.d" "firmware\drivers\drv_epwm.d" "firmware\drivers\drv_gpio.d" "firmware\drivers\drv_interrupt.d" "firmware\drivers\drv_sci.d" "firmware\drivers\drv_spi.d" "firmware\drivers\drv_sysctrl.d" "firmware\drivers\drv_timer.d" "firmware\services\cpld_spi.d" "firmware\services\indicator.d" "firmware\services\measurement.d" "firmware\services\modbus_vdc.d" "firmware\services\spi_bridge.d" "firmware\services\spi_request.d" 
	-$(RM) "firmware\services\uart_frame.d" 
	-$(RM) "SRC\DSP2833x_CodeStartBranch.d" "SRC\DSP2833x_usDelay.d" 
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

