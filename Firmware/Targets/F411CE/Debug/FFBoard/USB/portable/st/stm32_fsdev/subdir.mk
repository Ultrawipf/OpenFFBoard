################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Niki/Desktop/Openffboard/OpenFFBoard-1.15.0/Firmware/FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.c 

C_DEPS += \
./FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.d 

OBJS += \
./FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.o 


# Each subdirectory must supply rules for building sources it contributes
FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.o: C:/Users/Niki/Desktop/Openffboard/OpenFFBoard-1.15.0/Firmware/FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.c FFBoard/USB/portable/st/stm32_fsdev/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32F411xE -DDEBUG -DSTM32_THREAD_SAFE_STRATEGY=4 -c -I../Core/Inc -I../../../FFBoard/UserExtensions/Inc -I../../../FFBoard/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../FFBoard/USB -I../../../FFBoard/USB/class/cdc -I../../../FFBoard/USB/class -I../../../FFBoard/USB/class/hid -I../../../FFBoard/USB/common -I../../../FFBoard/USB/device -I../../../FFBoard/USB/osal -I../../../FFBoard/USB/class/midi -I../../../FFBoard/USB/class/audio -I../Core/ThreadSafe -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FFBoard-2f-USB-2f-portable-2f-st-2f-stm32_fsdev

clean-FFBoard-2f-USB-2f-portable-2f-st-2f-stm32_fsdev:
	-$(RM) ./FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.d ./FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.o ./FFBoard/USB/portable/st/stm32_fsdev/dcd_stm32_fsdev.su

.PHONY: clean-FFBoard-2f-USB-2f-portable-2f-st-2f-stm32_fsdev

