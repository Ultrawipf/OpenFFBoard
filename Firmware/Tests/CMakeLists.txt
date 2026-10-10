# OpenFFBoard firmware host unit tests
#
# Builds the hardware independent firmware sources with the native compiler of the
# development machine and links them against a fake platform layer (HAL, FreeRTOS, TinyUSB).
#
#   cmake -S Firmware/Tests -B Firmware/Tests/build
#   cmake --build Firmware/Tests/build
#   ctest --test-dir Firmware/Tests/build --output-on-failure
#
# See README.md in this directory.
cmake_minimum_required(VERSION 3.16)
project(OpenFFBoardHostTests LANGUAGES C CXX)

# The hardware targets are built with USE_DSP_FUNCTIONS. Default: ON if the CMSIS-DSP submodule is checked out
set(FFB_TESTS_USE_DSP "AUTO" CACHE STRING "Build the firmware with USE_DSP_FUNCTIONS and CMSIS-DSP like the hardware targets (ON, OFF, AUTO)")
set_property(CACHE FFB_TESTS_USE_DSP PROPERTY STRINGS AUTO ON OFF)
option(FFB_TESTS_SANITIZE "Build with address and undefined behaviour sanitizers (gcc/clang on linux/macOS)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # gnu++20 like the firmware build
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Debug)
endif()

set(FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(FFBOARD_DIR ${FW_DIR}/FFBoard)

# ---------------------------------------------------------------------------
# Firmware sources under test. These are the unmodified firmware files.
# Add a file here to make it available to the tests. If it needs something from
# the hardware extend the fake platform in platform/ instead of changing the source.
# ---------------------------------------------------------------------------
set(FIRMWARE_SOURCES
	# Command system
	${FFBOARD_DIR}/Src/CmdParser.cpp
	${FFBOARD_DIR}/Src/CommandHandler.cpp
	${FFBOARD_DIR}/Src/CommandInterface.cpp
	${FFBOARD_DIR}/Src/HidCommandInterface.cpp
	${FFBOARD_DIR}/Src/CanCommandInterface.cpp
	${FFBOARD_DIR}/Src/FFBoardMainCommandThread.cpp
	${FFBOARD_DIR}/Src/CDCcomm.cpp
	${FFBOARD_DIR}/Src/UART.cpp
	${FFBOARD_DIR}/Src/UartHandler.cpp
	${FFBOARD_DIR}/Src/CAN.cpp
	${FFBOARD_DIR}/Src/CanHandler.cpp
	# FFB
	${FFBOARD_DIR}/Src/EffectsCalculator.cpp
	${FFBOARD_DIR}/Src/HidFFB.cpp
	${FFBOARD_DIR}/Src/UsbHidHandler.cpp
	${FFBOARD_DIR}/Src/Filters.cpp
	# Device class interfaces
	${FFBOARD_DIR}/Src/Encoder.cpp
	${FFBOARD_DIR}/Src/MotorDriver.cpp
	# Infrastructure
	${FFBOARD_DIR}/Src/ErrorHandler.cpp
	${FFBOARD_DIR}/Src/PersistentStorage.cpp
	${FFBOARD_DIR}/Src/ringbuffer.c
	${FFBOARD_DIR}/Src/ringbufferwrapper.cpp
	${FFBOARD_DIR}/Src/cthread.cpp
	${FFBOARD_DIR}/Src/csemaphore.cpp
	${FFBOARD_DIR}/Src/cmutex.cpp
)

set(PLATFORM_SOURCES
	platform/src/host_freertos.cpp
	platform/src/host_platform.cpp
	platform/src/host_usb.cpp
	platform/src/host_stubs.cpp
)

# Every test_*.cpp in cases/ is picked up automatically
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cases/test_*.cpp)

add_executable(ffb_unit_tests
	${FIRMWARE_SOURCES}
	${PLATFORM_SOURCES}
	${TEST_SOURCES}
	support/test_main.cpp
)

# Order matters: test doubles and the fake platform shadow firmware and target headers.
target_include_directories(ffb_unit_tests PRIVATE
	fakes
	platform/include
	support
	mocks
	third_party
	${FFBOARD_DIR}/Inc
	${FFBOARD_DIR}/UserExtensions/Inc
	${FFBOARD_DIR}/USB
	${FFBOARD_DIR}/USB/common
	${FFBOARD_DIR}/USB/device
	${FFBOARD_DIR}/USB/class/hid
	${FFBOARD_DIR}/USB/class/cdc
	${FFBOARD_DIR}/USB/class/midi
)

target_compile_definitions(ffb_unit_tests PRIVATE HOSTTEST)

set(DSP_DIR ${FW_DIR}/Libraries/CMSIS-DSP)
set(USE_DSP ${FFB_TESTS_USE_DSP})
if(FFB_TESTS_USE_DSP STREQUAL "AUTO")
	if(EXISTS ${DSP_DIR}/Include/arm_math_types.h)
		set(USE_DSP ON)
	else()
		set(USE_DSP OFF)
	endif()
endif()
message(STATUS "OpenFFBoard tests: USE_DSP_FUNCTIONS=${USE_DSP}")

if(USE_DSP)
	# CMSIS-DSP has a portable C implementation which is used on the host.
	# Only the functions used by the tested firmware sources are compiled.
	if(NOT EXISTS ${DSP_DIR}/Include/arm_math_types.h)
		message(FATAL_ERROR "CMSIS-DSP not found. Run: git submodule update --init Firmware/Libraries/CMSIS-DSP")
	endif()
	target_sources(ffb_unit_tests PRIVATE
		${DSP_DIR}/Source/CommonTables/arm_common_tables.c
		${DSP_DIR}/Source/FastMathFunctions/arm_sin_f32.c
		${DSP_DIR}/Source/FastMathFunctions/arm_cos_f32.c
		${DSP_DIR}/Source/FilteringFunctions/arm_biquad_cascade_df1_f32.c
		${DSP_DIR}/Source/FilteringFunctions/arm_biquad_cascade_df1_init_f32.c
		${DSP_DIR}/Source/InterpolationFunctions/arm_spline_interp_f32.c
		${DSP_DIR}/Source/InterpolationFunctions/arm_spline_interp_init_f32.c
	)
	target_include_directories(ffb_unit_tests PRIVATE ${DSP_DIR}/Include ${DSP_DIR}/PrivateInclude)
	# __GNUC_PYTHON__ is the CMSIS-DSP switch for host builds without CMSIS core headers
	target_compile_definitions(ffb_unit_tests PRIVATE USE_DSP_FUNCTIONS ARM_MATH_LOOPUNROLL __GNUC_PYTHON__)
endif()

find_package(Threads REQUIRED) # Only used by tests which guard against endless loops
target_link_libraries(ffb_unit_tests PRIVATE Threads::Threads)

# Firmware sources are built with the warning settings of the firmware makefile.
target_compile_options(ffb_unit_tests PRIVATE -Wall $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>)
if(MINGW)
	# Packed HID report structs must use the gcc layout like on the target
	target_compile_options(ffb_unit_tests PRIVATE -mno-ms-bitfields)
	target_link_options(ffb_unit_tests PRIVATE -static)
endif()

if(FFB_TESTS_SANITIZE)
	target_compile_definitions(ffb_unit_tests PRIVATE HOSTTEST_SANITIZERS)
	target_compile_options(ffb_unit_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
	target_link_options(ffb_unit_tests PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME ffb_unit_tests COMMAND ffb_unit_tests)
