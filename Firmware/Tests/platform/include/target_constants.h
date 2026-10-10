/*
 * target_constants.h for the host unit test "target"
 *
 * Equivalent to Targets/<target>/Core/Inc/target_constants.h
 * Selects the firmware features compiled into the host test build.
 */
#ifndef HOSTTEST_TARGET_CONSTANTS_H_
#define HOSTTEST_TARGET_CONSTANTS_H_

#define HW_TYPE "HOSTTEST"
#define HW_TYPE_INT 0xFF
#define FW_DEVID 0

#include "main.h"

#define DEFAULTMAIN 0

// Features under test
#define CANBUS
#define CANCOMMANDS
#define UARTCOMMANDS

#define UART_BUF_SIZE 1
extern UART_HandleTypeDef huart1;
#define UART_PORT_EXT huart1

extern CAN_HandleTypeDef hcan1;
#define CANPORT hcan1

#endif
