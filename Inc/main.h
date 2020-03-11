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
#define Bset(data,val) data|=(val)
#define Bclr(data,val) data&=~(val)
#define Btest(data,val) ((data&(val))==(val))
#define Bchg(data,val) if (Btest(data,val)) Bclr(data,val); else Bset(data,val)
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void RebootDFU();
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DIN2_Pin GPIO_PIN_13
#define DIN2_GPIO_Port GPIOC
#define DIN1_Pin GPIO_PIN_14
#define DIN1_GPIO_Port GPIOC
#define DIN0_Pin GPIO_PIN_15
#define DIN0_GPIO_Port GPIOC
#define AIN5_Pin GPIO_PIN_0
#define AIN5_GPIO_Port GPIOC
#define AIN4_Pin GPIO_PIN_1
#define AIN4_GPIO_Port GPIOC
#define AIN3_Pin GPIO_PIN_2
#define AIN3_GPIO_Port GPIOC
#define AIN2_Pin GPIO_PIN_3
#define AIN2_GPIO_Port GPIOC
#define AIN1_Pin GPIO_PIN_0
#define AIN1_GPIO_Port GPIOA
#define AIN0_Pin GPIO_PIN_1
#define AIN0_GPIO_Port GPIOA
#define ADC_VEXT_Pin GPIO_PIN_2
#define ADC_VEXT_GPIO_Port GPIOA
#define ADC_VINT_Pin GPIO_PIN_3
#define ADC_VINT_GPIO_Port GPIOA
#define SPI1_SS1_Pin GPIO_PIN_4
#define SPI1_SS1_GPIO_Port GPIOA
#define DRV_ENABLE_Pin GPIO_PIN_4
#define DRV_ENABLE_GPIO_Port GPIOC
#define FLAG_Pin GPIO_PIN_5
#define FLAG_GPIO_Port GPIOC
#define FLAG_EXTI_IRQn EXTI9_5_IRQn
#define DRV_BRAKE_Pin GPIO_PIN_0
#define DRV_BRAKE_GPIO_Port GPIOB
#define SPI1_SS2_Pin GPIO_PIN_1
#define SPI1_SS2_GPIO_Port GPIOB
#define BUTTON_A_Pin GPIO_PIN_2
#define BUTTON_A_GPIO_Port GPIOB
#define BUTTON_A_EXTI_IRQn EXTI2_IRQn
#define SPI1_SS3_Pin GPIO_PIN_10
#define SPI1_SS3_GPIO_Port GPIOB
#define SPI2_NSS_Pin GPIO_PIN_12
#define SPI2_NSS_GPIO_Port GPIOB
#define ENCODER_A_Pin GPIO_PIN_6
#define ENCODER_A_GPIO_Port GPIOC
#define ENCODER_B_Pin GPIO_PIN_7
#define ENCODER_B_GPIO_Port GPIOC
#define ENCODER_Z_Pin GPIO_PIN_8
#define ENCODER_Z_GPIO_Port GPIOC
#define ENCODER_Z_EXTI_IRQn EXTI9_5_IRQn
#define OTG_FS_DM_Pin GPIO_PIN_11
#define OTG_FS_DM_GPIO_Port GPIOA
#define OTG_FS_DP_Pin GPIO_PIN_12
#define OTG_FS_DP_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define GP3_Pin GPIO_PIN_15
#define GP3_GPIO_Port GPIOA
#define GP2_Pin GPIO_PIN_10
#define GP2_GPIO_Port GPIOC
#define GP1_Pin GPIO_PIN_11
#define GP1_GPIO_Port GPIOC
#define LED_CLIP_Pin GPIO_PIN_12
#define LED_CLIP_GPIO_Port GPIOC
#define LED_ERR_Pin GPIO_PIN_2
#define LED_ERR_GPIO_Port GPIOD
#define GP4_Pin GPIO_PIN_3
#define GP4_GPIO_Port GPIOB
#define DIN7_Pin GPIO_PIN_4
#define DIN7_GPIO_Port GPIOB
#define LED_SYS_Pin GPIO_PIN_5
#define LED_SYS_GPIO_Port GPIOB
#define DIN6_Pin GPIO_PIN_6
#define DIN6_GPIO_Port GPIOB
#define DIN5_Pin GPIO_PIN_7
#define DIN5_GPIO_Port GPIOB
#define DIN4_Pin GPIO_PIN_8
#define DIN4_GPIO_Port GPIOB
#define DIN3_Pin GPIO_PIN_9
#define DIN3_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
