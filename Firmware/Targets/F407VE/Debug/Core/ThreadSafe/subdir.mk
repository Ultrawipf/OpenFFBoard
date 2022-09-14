################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/ThreadSafe/newlib_lock_glue.c 

C_DEPS += \
./Core/ThreadSafe/newlib_lock_glue.d 

OBJS += \
./Core/ThreadSafe/newlib_lock_glue.o 


# Each subdirectory must supply rules for building sources it contributes
Core/ThreadSafe/%.o Core/ThreadSafe/%.su: ../Core/ThreadSafe/%.c Core/ThreadSafe/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -DSTM32_THREAD_SAFE_STRATEGY=4 -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Core/ThreadSafe -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/Inc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/audio" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/cdc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/hid" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/common" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/device" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/osal" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/portable/st/stm32_fsdev" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/UserExtensions/Inc" -I"C:/Users/sampa/Documents/GitHub/OpenFFBoard/Firmware/FFBoard/USB/class/midi" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=standard_c_nano_cpp.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-ThreadSafe

clean-Core-2f-ThreadSafe:
	-$(RM) ./Core/ThreadSafe/newlib_lock_glue.d ./Core/ThreadSafe/newlib_lock_glue.o ./Core/ThreadSafe/newlib_lock_glue.su

.PHONY: clean-Core-2f-ThreadSafe

