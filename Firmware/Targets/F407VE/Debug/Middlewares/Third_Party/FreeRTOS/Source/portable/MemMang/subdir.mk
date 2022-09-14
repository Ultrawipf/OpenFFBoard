################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c 

C_DEPS += \
./Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.d 

OBJS += \
./Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.o 


# Each subdirectory must supply rules for building sources it contributes
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/%.o Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/%.su: ../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/%.c Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DSTM32_THREAD_SAFE_STRATEGY=4 -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Core/ThreadSafe -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/Inc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/audio" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/cdc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/hid" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/common" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/device" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/osal" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/portable/st/stm32_fsdev" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/UserExtensions/Inc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/midi" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=standard_c_nano_cpp.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Middlewares-2f-Third_Party-2f-FreeRTOS-2f-Source-2f-portable-2f-MemMang

clean-Middlewares-2f-Third_Party-2f-FreeRTOS-2f-Source-2f-portable-2f-MemMang:
	-$(RM) ./Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.d ./Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.o ./Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.su

.PHONY: clean-Middlewares-2f-Third_Party-2f-FreeRTOS-2f-Source-2f-portable-2f-MemMang

