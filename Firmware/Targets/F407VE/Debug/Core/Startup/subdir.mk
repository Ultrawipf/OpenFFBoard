################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Core/Startup/startup_stm32f407vetx.s 

S_DEPS += \
./Core/Startup/startup_stm32f407vetx.d 

OBJS += \
./Core/Startup/startup_stm32f407vetx.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Startup/%.o: ../Core/Startup/%.s Core/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -DDEBUG -c -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/Inc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/audio" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/cdc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/hid" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/common" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/device" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/osal" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/portable/st/stm32_fsdev" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/UserExtensions/Inc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/midi" -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=standard_c_nano_cpp.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

clean: clean-Core-2f-Startup

clean-Core-2f-Startup:
	-$(RM) ./Core/Startup/startup_stm32f407vetx.d ./Core/Startup/startup_stm32f407vetx.o

.PHONY: clean-Core-2f-Startup

