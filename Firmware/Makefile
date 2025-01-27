
# ------------------------------------------------
# Generic Makefile (based on gcc)
#
# ------------------------------------------------


######################################
# building variables
######################################
# debug build?
DEBUG = 0
# optimization
OPT = -O2 # -mno-unaligned-access
OPTCPP = -Os


#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

######################################
# source
######################################

# Choose your board mcu F407VG or F411RE
MCU_TARGET=F407VG
# The directory of mcu target
TARGET_DIR = Targets/$(MCU_TARGET)

######################################
# target
######################################
TARGET = OpenFFBoard_$(MCU_TARGET)

# Output directory for hex/bin files
OUTPUT_DIR = $(BUILD_DIR)

# C sources
C_SOURCES = $(wildcard $(TARGET_DIR)/Core/Src/*.c)
C_SOURCES += $(wildcard $(TARGET_DIR)/Core/ThreadSafe/*.c)
C_SOURCES += $(wildcard $(TARGET_DIR)/Drivers/STM32F4xx_HAL_Driver/Src/*.c)
C_SOURCES += $(wildcard $(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/*.c)
C_SOURCES += $(wildcard $(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/*.c)
C_SOURCES += $(wildcard $(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/*.c)
C_SOURCES += $(wildcard $(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/*.c)

C_SOURCES += $(wildcard FFBoard/Src/*.c)
C_SOURCES += $(wildcard FFBoard/USB/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/audio/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/bth/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/cdc/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/dfu/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/hid/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/midi/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/msc/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/net/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/usbtmc/*.c)
C_SOURCES += $(wildcard FFBoard/USB/class/vendor/*.c)
C_SOURCES += $(wildcard FFBoard/USB/common/*.c)
C_SOURCES += $(wildcard FFBoard/USB/device/*.c)
C_SOURCES += $(wildcard FFBoard/USB/host/*.c)
C_SOURCES += $(wildcard FFBoard/USB/portable/st/stm32_fsdev/*.c)
C_SOURCES += $(wildcard FFBoard/USB/portable/synopsys/dwc2/*.c)
C_SOURCES += $(wildcard FFBoard/USB/portable/template/*.c)
C_SOURCES += $(wildcard FFBoard/USB/portable/ehci/*.c)
C_SOURCES += $(wildcard FFBoard/USB/portable/ohci/*.c)
C_SOURCES += $(wildcard FFBoard/UserExtensions/Src/*.c)

CPP_SOURCES = $(wildcard FFBoard/Src/*.cpp)
CPP_SOURCES += $(wildcard $(TARGET_DIR)/Core/Src/*.cpp)
CPP_SOURCES += $(wildcard FFBoard/UserExtensions/Src/*.cpp)

# ASM sources
ifeq ($(MCU_TARGET),F411RE)
ASM_SOURCES = $(TARGET_DIR)/Core/Startup/startup_stm32f411retx.s
else
ASM_SOURCES = $(TARGET_DIR)/Core/Startup/startup_stm32f407vgtx.s
endif


# AS includes
AS_INCLUDES = 

# C includes
C_INCLUDES =  \
FFBoard/Inc \
FFBoard/USB \
FFBoard/UserExtensions/Inc \
FFBoard/USB/device \
FFBoard/USB/portable/synopsys/dwc2/ \
FFBoard/USB/class/cdc \
FFBoard/USB/class/midi \
FFBoard/USB/class/hid \
$(TARGET_DIR)/Core/Inc \
$(TARGET_DIR)/Core/Inc/ThreadSafe \
$(TARGET_DIR)/Drivers/STM32F4xx_HAL_Driver/Inc \
$(TARGET_DIR)/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
$(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
$(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/include \
$(TARGET_DIR)/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
$(TARGET_DIR)/Drivers/CMSIS/Device/ST/STM32F4xx/Include \
$(TARGET_DIR)/Drivers/CMSIS/Include 


C_INCLUDES := $(addprefix -I, $(C_INCLUDES))

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
CXX = $(GCC_PATH)/$(PREFIX)g++
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
# cpu
CPU = -mcpu=cortex-m4

# fpu
FPU = -mfpu=fpv4-sp-d16

# float-abi
FLOAT-ABI = -mfloat-abi=hard

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# macros for gcc
# AS defines
AS_DEFS = 

# C defines
C_DEFS =  \
-DSTM32_THREAD_SAFE_STRATEGY=4 \
-DUSE_HAL_DRIVER 
ifeq ($(MCU_TARGET),F411RE)
C_DEFS += -DSTM32F411xE
else
C_DEFS += -DSTM32F407xx
endif

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -std=gnu11

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif


# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

CXXFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPTCPP) -Wall -fdata-sections -ffunction-sections -fno-exceptions -fno-use-cxa-atexit -std=gnu++20 -fstack-usage -Wno-volatile

#######################################
# LDFLAGS
#######################################
# link script
ifeq ($(MCU_TARGET),F407VG_DISCO)
LDSCRIPT = $(TARGET_DIR)/STM32F407VGTX_FLASH.ld
else
LDSCRIPT = $(TARGET_DIR)/STM32$(MCU_TARGET)TX_FLASH.ld
endif

# libraries
LIBS = -lc -lm -lnosys 
LIBDIR = 
LDFLAGS = $(MCU) --specs=standard_c_nano_cpp.specs -u _printf_float -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections -static
# -mcpu=cortex-m4 -T"C:\Users\zhouli\Desktop\my-openffb\OpenFFBoard\Firmware\Targets\F407VG\STM32F407VGTX_FLASH.ld" --specs=nosys.specs -Wl,-Map="OpenFFBoard_F407VG.map" -Wl,--cref -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -u _printf_float -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(OUTPUT_DIR)/$(TARGET).hex $(OUTPUT_DIR)/$(TARGET).bin
hex: $(OUTPUT_DIR)/$(TARGET).hex
hexmetadata: $(OUTPUT_DIR)/$(TARGET).hex
	echo "#METADATA" >> $(OUTPUT_DIR)/$(TARGET).hex
	echo "#MCU_TARGET $(MCU_TARGET)" >> $(OUTPUT_DIR)/$(TARGET).hex
	sed -rn -e "s/^#define\s*(\w+)\s*([^\/\*]+)?.*$$/#\1 \2/p" -e "s/:/;/" $(TARGET_DIR)/Core/Inc/target_constants.h >> $(OUTPUT_DIR)/$(TARGET).hex


#######################################
# build the application
#######################################
# list of objects

OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(CPP_SOURCES:.cpp=.o)))
vpath %.cpp $(sort $(dir $(CPP_SOURCES)))

# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))


$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.cpp Makefile | $(BUILD_DIR) 
	$(CXX) -c $(CXXFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cpp=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(OUTPUT_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(OUTPUT_DIR)
	$(HEX) $< $@
	
$(OUTPUT_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(OUTPUT_DIR)
	$(BIN) $< $@	
	
$(BUILD_DIR):
	-mkdir $@	
	
$(OUTPUT_DIR): $(BUILD_DIR)
	-mkdir $@

#######################################
# clean up
#######################################
clean:
	-rm -fR $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).bin
	STM32_Programmer_CLI -c port=SWD freq=3900 ap=0 -w $(BUILD_DIR)/$(TARGET).elf -rst

upload: $(BUILD_DIR)/$(TARGET).bin
	openocd -f board/stm32f4discovery.cfg -c "reset_config trst_only combined" -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***