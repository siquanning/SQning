################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
firmware/services/%.obj: ../firmware/services/%.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'C2000 Compiler - building file: "$<"'
	"E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/bin/cl2000" -v28 -ml -mt --float_support=fpu32 -O2 --include_path="E:/repos/DSP28335/resst" --include_path="E:/ti/ccs2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS/include" --include_path="E:/repos/DSP28335/resst/INCLUDE" --define=FLASH --define=PLATFORM_PROFILE_INDUSTRIAL --diag_warning=225 --display_error_number --diag_wrap=off --preproc_with_compile --preproc_dependency="firmware/services/$(basename $(<F)).d_raw" --obj_directory="firmware/services" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


