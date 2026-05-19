################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
DSP2833x_Libraries/%.obj: ../DSP2833x_Libraries/%.asm $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --float_support=fpu32 --include_path="E:/DSP28335/DSP2833x_EPWM/APP/gpio" --include_path="E:/DSP28335/DSP2833x_EPWM" --include_path="E:/DSP28335/C2000/C2000Ware_5_04_00_00/device_support/f2833x/common/include" --include_path="E:/DSP28335/DSP2833x_EPWM/APP/sci" --include_path="E:/DSP28335/DSP2833x_EPWM/APP/leds" --include_path="E:/DSP28335/C2000/C2000Ware_5_04_00_00/device_support/f2833x/headers/include" --include_path="E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --include_path="E:/DSP28335/DSP2833x_EPWM/APP/epwm" -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --preproc_with_compile --preproc_dependency="DSP2833x_Libraries/$(basename $(<F)).d_raw" --obj_directory="DSP2833x_Libraries" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

DSP2833x_Libraries/%.obj: ../DSP2833x_Libraries/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --float_support=fpu32 --include_path="E:/DSP28335/DSP2833x_EPWM/APP/gpio" --include_path="E:/DSP28335/DSP2833x_EPWM" --include_path="E:/DSP28335/C2000/C2000Ware_5_04_00_00/device_support/f2833x/common/include" --include_path="E:/DSP28335/DSP2833x_EPWM/APP/sci" --include_path="E:/DSP28335/DSP2833x_EPWM/APP/leds" --include_path="E:/DSP28335/C2000/C2000Ware_5_04_00_00/device_support/f2833x/headers/include" --include_path="E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --include_path="E:/DSP28335/DSP2833x_EPWM/APP/epwm" -g --diag_warning=225 --diag_wrap=off --display_error_number --abi=coffabi --preproc_with_compile --preproc_dependency="DSP2833x_Libraries/$(basename $(<F)).d_raw" --obj_directory="DSP2833x_Libraries" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


