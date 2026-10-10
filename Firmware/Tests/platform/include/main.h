/*
 * Host test replacement for the CubeMX generated main.h and the STM32 HAL headers.
 *
 * Only declares the minimal set of HAL types, constants and functions that are
 * referenced by the firmware sources which are compiled for the host tests.
 * Functions are implemented in platform/src/host_platform.cpp
 *
 * If a newly tested source file needs more of the HAL add it here. Keep it minimal.
 */
#ifndef HOSTTEST_MAIN_H_
#define HOSTTEST_MAIN_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { HAL_OK = 0x00U, HAL_ERROR = 0x01U, HAL_BUSY = 0x02U, HAL_TIMEOUT = 0x03U } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;
typedef enum { RESET = 0U, SET = !RESET } FlagStatus, ITStatus;
typedef enum { DISABLE = 0U, ENABLE = !DISABLE } FunctionalState;

// --- GPIO ---
typedef struct { uint32_t ODR; uint32_t IDR; } GPIO_TypeDef;
typedef struct { uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed; uint32_t Alternate; } GPIO_InitTypeDef;
#define GPIO_NOPULL 0x00000000U
#define GPIO_PULLUP 0x00000001U
#define GPIO_PULLDOWN 0x00000002U
#define GPIO_SPEED_FREQ_LOW 0x00000000U
#define GPIO_MODE_INPUT 0x00000000U
#define GPIO_MODE_OUTPUT_PP 0x00000001U
#define GPIO_MODE_OUTPUT_OD 0x00000011U
void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin);

// --- Peripheral handles. Opaque for the tested code ---
typedef struct { uint32_t CNT; uint32_t ARR; uint32_t PSC; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef struct { void *Instance; } ADC_HandleTypeDef;

typedef struct { uint32_t Mode; uint32_t Direction; uint32_t DataSize; uint32_t CLKPolarity; uint32_t CLKPhase; uint32_t NSS; uint32_t BaudRatePrescaler; uint32_t FirstBit; uint32_t TIMode; uint32_t CRCCalculation; uint32_t CRCPolynomial; } SPI_InitTypeDef;
typedef struct { void *Instance; SPI_InitTypeDef Init; } SPI_HandleTypeDef;

typedef struct { uint32_t ClockSpeed; uint32_t DutyCycle; uint32_t OwnAddress1; uint32_t AddressingMode; uint32_t DualAddressMode; uint32_t OwnAddress2; uint32_t GeneralCallMode; uint32_t NoStretchMode; } I2C_InitTypeDef;
typedef struct { void *Instance; I2C_InitTypeDef Init; } I2C_HandleTypeDef;

typedef struct { uint32_t SR; uint32_t DR; } USART_TypeDef;
typedef struct { uint32_t BaudRate; uint32_t WordLength; uint32_t StopBits; uint32_t Parity; uint32_t Mode; uint32_t HwFlowCtl; uint32_t OverSampling; } UART_InitTypeDef;
typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; uint32_t ErrorCode; } UART_HandleTypeDef;

#define TIM_CHANNEL_1 0x00000000U
#define TIM_CHANNEL_2 0x00000004U
#define TIM_CHANNEL_3 0x00000008U
#define TIM_CHANNEL_4 0x0000000CU

#define SPI_MODE_MASTER 0x00000104U
#define SPI_DIRECTION_2LINES 0x00000000U
#define SPI_DATASIZE_8BIT 0x00000000U
#define SPI_POLARITY_LOW 0x00000000U
#define SPI_PHASE_1EDGE 0x00000000U
#define SPI_NSS_SOFT 0x00000200U
#define SPI_BAUDRATEPRESCALER_4 0x00000008U
#define SPI_FIRSTBIT_MSB 0x00000000U
#define SPI_TIMODE_DISABLE 0x00000000U
#define SPI_CRCCALCULATION_DISABLE 0x00000000U

// --- UART. Transmitted data is recorded, see support/HostPlatform.h ---
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *huart);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, const uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *huart);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *huart);
uint32_t HAL_UART_GetError(UART_HandleTypeDef *huart);

// --- bxCAN. Selects the classic CAN 2.0B variant of CAN.h ---
#define CAN1 ((void *)0x40006400)
#define CAN_ID_STD (0x00000000U)
#define CAN_ID_EXT (0x00000004U)
#define CAN_RTR_DATA (0x00000000U)
#define CAN_RTR_REMOTE (0x00000002U)
typedef struct { uint32_t StdId; uint32_t ExtId; uint32_t IDE; uint32_t RTR; uint32_t DLC; uint32_t Timestamp; uint32_t FilterMatchIndex; } CAN_RxHeaderTypeDef;
typedef struct { void *Instance; } CAN_HandleTypeDef;

// --- System ---
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
void HAL_NVIC_SystemReset(void);
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif
