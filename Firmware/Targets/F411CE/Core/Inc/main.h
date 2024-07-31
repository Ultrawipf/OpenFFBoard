/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void RebootDFU();
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_SYS_Pin GPIO_PIN_13
#define LED_SYS_GPIO_Port GPIOC
#define ENCODER_Z_Pin GPIO_PIN_14
#define ENCODER_Z_GPIO_Port GPIOC
#define ENCODER_Z_EXTI_IRQn EXTI15_10_IRQn
#define E_STOP_Pin GPIO_PIN_15
#define E_STOP_GPIO_Port GPIOC
#define E_STOP_EXTI_IRQn EXTI15_10_IRQn
#define AIN0_Pin GPIO_PIN_0
#define AIN0_GPIO_Port GPIOA
#define AIN1_Pin GPIO_PIN_1
#define AIN1_GPIO_Port GPIOA
#define AIN2_Pin GPIO_PIN_2
#define AIN2_GPIO_Port GPIOA
#define AIN3_Pin GPIO_PIN_3
#define AIN3_GPIO_Port GPIOA
#define AIN4_Pin GPIO_PIN_4
#define AIN4_GPIO_Port GPIOA
#define AIN5_Pin GPIO_PIN_5
#define AIN5_GPIO_Port GPIOA
#define ADC_VEXT_Pin GPIO_PIN_6
#define ADC_VEXT_GPIO_Port GPIOA
#define ADC_VINT_Pin GPIO_PIN_7
#define ADC_VINT_GPIO_Port GPIOA
#define PWM3_Pin GPIO_PIN_0
#define PWM3_GPIO_Port GPIOB
#define PWM4_Pin GPIO_PIN_1
#define PWM4_GPIO_Port GPIOB
#define DRV_BRAKE_Pin GPIO_PIN_2
#define DRV_BRAKE_GPIO_Port GPIOB
#define DIN5_Pin GPIO_PIN_10
#define DIN5_GPIO_Port GPIOB
#define DIN0_Pin GPIO_PIN_12
#define DIN0_GPIO_Port GPIOB
#define DIN1_Pin GPIO_PIN_13
#define DIN1_GPIO_Port GPIOB
#define DIN2_Pin GPIO_PIN_14
#define DIN2_GPIO_Port GPIOB
#define DIN3_Pin GPIO_PIN_15
#define DIN3_GPIO_Port GPIOB
#define ENCODER_A_Pin GPIO_PIN_8
#define ENCODER_A_GPIO_Port GPIOA
#define ENCODER_B_Pin GPIO_PIN_9
#define ENCODER_B_GPIO_Port GPIOA
#define DIN4_Pin GPIO_PIN_10
#define DIN4_GPIO_Port GPIOA
#define SPI1_SS1_Pin GPIO_PIN_15
#define SPI1_SS1_GPIO_Port GPIOA
void   MX_USB_OTG_FS_PCD_Init(void);
/* USER CODE BEGIN Private defines */
#define GPIO_UNEXPOSED_PORT                   GPIOE
#define GPIO_UNEXPOSED_OUTPUT_Pin             GPIO_PIN_0
#define GPIO_UNEXPOSED_OUTPUT_GPIO_PORT       GPIO_UNEXPOSED_PORT
#define GPIO_UNEXPOSED_INPUT_LOW_Pin          GPIO_PIN_1
#define GPIO_UNEXPOSED_INPUT_LOW_GPIO_PORT    GPIO_UNEXPOSED_PORT
#define GPIO_UNEXPOSED_INPUT_HIGH_Pin         GPIO_PIN_2
#define GPIO_UNEXPOSED_INPUT_HIGH_GPIO_PORT   GPIO_UNEXPOSED_PORT
#define LED_ERR_Pin                           GPIO_UNEXPOSED_OUTPUT_Pin
#define LED_ERR_GPIO_Port                     GPIO_UNEXPOSED_OUTPUT_GPIO_PORT
//#define LED_SYS_Pin                           GPIO_UNEXPOSED_OUTPUT_Pin
//#define LED_SYS_GPIO_Port                     GPIO_UNEXPOSED_OUTPUT_GPIO_PORT
#define LED_CLIP_Pin                          GPIO_UNEXPOSED_OUTPUT_Pin
#define LED_CLIP_GPIO_Port                    GPIO_UNEXPOSED_OUTPUT_GPIO_PORT
#define BUTTON_A_Pin                          GPIO_UNEXPOSED_INPUT_LOW_Pin
#define BUTTON_A_GPIO_Port                    GPIO_UNEXPOSED_INPUT_LOW_GPIO_PORT

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
