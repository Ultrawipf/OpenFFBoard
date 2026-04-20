################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Users/Niki/Desktop/Openffboard/OpenFFBoard-1.15.0/Firmware/FFBoard/USB/portable/st/synopsys/dcd_synopsys.c 

C_DEPS += \
./FFBoard/USB/portable/st/synopsys/dcd_synopsys.d 

OBJS += \
./FFBoard/USB/portable/st/synopsys/dcd_synopsys.o 


# Each subdirectory must supply rules for building sources it contributes
FFBoard/USB/portable/st/synopsys/dcd_synopsys.o: C:/Users/Niki/Desktop/Openffboard/OpenFFBoard-1.15.0/Firmware/FFBoard/USB/portable/st/synopsys/dcd_synopsys.c FFBoard/USB/portable/st/synopsys/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32F411xE -DDEBUG -DSTM32_THREAD_SAFE_STRATEGY=4 -c -I../Core/Inc -I../../../FFBoard/UserExtensions/Inc -I../../../FFBoard/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../FFBoard/USB -I../../../FFBoard/USB/class/cdc -I../../../FFBoard/USB/class -I../../../FFBoard/USB/class/hid -I../../../FFBoard/USB/common -I../../../FFBoard/USB/device -I../../../FFBoard/USB/osal -I../../../FFBoard/USB/class/midi -I../../../FFBoard/USB/class/audio -I../Core/ThreadSafe -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-FFBoard-2f-USB-2f-portable-2f-st-2f-synopsys

clean-FFBoard-2f-USB-2f-portable-2f-st-2f-synopsys:
	-$(RM) ./FFBoard/USB/portable/st/synopsys/dcd_synopsys.d ./FFBoard/USB/portable/st/synopsys/dcd_synopsys.o ./FFBoard/USB/portable/st/synopsys/dcd_synopsys.su

.PHONY: clean-FFBoard-2f-USB-2f-portable-2f-st-2f-synopsys

