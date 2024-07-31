################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/subdir.mk
-include FFBoard/UserExtensions/Src/subdir.mk
-include FFBoard/USB/portable/st/synopsys/subdir.mk
-include FFBoard/USB/portable/st/stm32_fsdev/subdir.mk
-include FFBoard/USB/host/subdir.mk
-include FFBoard/USB/device/subdir.mk
-include FFBoard/USB/common/subdir.mk
-include FFBoard/USB/class/video/subdir.mk
-include FFBoard/USB/class/vendor/subdir.mk
-include FFBoard/USB/class/usbtmc/subdir.mk
-include FFBoard/USB/class/net/subdir.mk
-include FFBoard/USB/class/msc/subdir.mk
-include FFBoard/USB/class/midi/subdir.mk
-include FFBoard/USB/class/hid/subdir.mk
-include FFBoard/USB/class/dfu/subdir.mk
-include FFBoard/USB/class/cdc/subdir.mk
-include FFBoard/USB/class/bth/subdir.mk
-include FFBoard/USB/class/audio/subdir.mk
-include FFBoard/USB/subdir.mk
-include FFBoard/Src/subdir.mk
-include Drivers/STM32F4xx_HAL_Driver/Src/subdir.mk
-include Core/ThreadSafe/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := OpenFFBoard_F411CE
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
OpenFFBoard_F411CE.elf \

MAP_FILES += \
OpenFFBoard_F411CE.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
OpenFFBoard_F411CE.list \

OBJCOPY_HEX += \
OpenFFBoard_F411CE.hex \


# All Target
all: main-build

# Main-build Target
main-build: OpenFFBoard_F411CE.elf secondary-outputs

# Tool invocations
OpenFFBoard_F411CE.elf OpenFFBoard_F411CE.map: $(OBJS) $(USER_OBJS) C:\Users\Niki\Desktop\Openffboard\OpenFFBoard-1.15.0\Firmware\Targets\F411CE\STM32F411CEUX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-g++ -o "OpenFFBoard_F411CE.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m4 -T"C:\Users\Niki\Desktop\Openffboard\OpenFFBoard-1.15.0\Firmware\Targets\F411CE\STM32F411CEUX_FLASH.ld" --specs=nosys.specs -Wl,-Map="OpenFFBoard_F411CE.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -u _printf_float -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

OpenFFBoard_F411CE.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "OpenFFBoard_F411CE.list"
	@echo 'Finished building: $@'
	@echo ' '

OpenFFBoard_F411CE.hex: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objcopy  -O ihex $(EXECUTABLES) "OpenFFBoard_F411CE.hex"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) OpenFFBoard_F411CE.elf OpenFFBoard_F411CE.hex OpenFFBoard_F411CE.list OpenFFBoard_F411CE.map default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(OBJCOPY_HEX)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
