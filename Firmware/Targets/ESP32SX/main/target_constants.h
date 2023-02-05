/*
 * target_constants.h
 *
 *  Created on: 01.05.2022
 *      Author: Zhouli
 */

#ifndef INC_TARGET_CONSTANTS_H_
#define INC_TARGET_CONSTANTS_H_

#include "sdkconfig.h"
/*
 * Add settings and peripheral maps for this specific target here
 */

// Hardware name string
#define HW_TYPE "ESP32SX"
#define HW_TYPE_INT 2
#define FW_DEVID 0x413 // F407

#if !(defined(CONFIG_IDF_TARGET_ESP32S2)) && !(defined(CONFIG_IDF_TARGET_ESP32S3))
#error "The target chip is not supported, you should select a proper target by 'idf.py set-target esp32s2/3'"
#endif

#define HW_ESP32SX // Defining this macro will implement the STM32 HAL library using some of the functions of ESP-IDF

#ifdef HW_ESP32SX
#if (!defined OPENFFBOARD_ESP32S2_V1_0) && (!defined OPENFFBOARD_ESP32S2_V1_1)
#define OPENFFBOARD_ESP32S2_V1_1 // Default use a V1.1 version of the board
#endif

#include "glue.h"

#endif

// Enabled features

// Main classes
#define FFBWHEEL
// #define FFBJOYSTICK
// #define MIDI
// #define TMCDEBUG
// #define CANBRIDGE

/*
 * FFBWheel uses 2 FFB axis descriptor instead of 1 axis.
 * Might improve compatibility with direct input but will report a 2 axis ffb compatible device
 */
//#define FFBWHEEL_USE_1AXIS_DESC

// Extra features
#define LOCALBUTTONS
// #define SPIBUTTONS
// #define SHIFTERBUTTONS
// #define PCF8574BUTTONS // Requires I2C
#define ANALOGAXES
#define TMC4671DRIVER
#define PWMDRIVER
#define LOCALENCODER
#define CANBUS
#define ODRIVE
#define VESC
// #define MTENCODERSPI // requires SPI3, not support on ESP32SX now
// #define CANBUTTONS // Requires CAN, not support on ESP32SX now, besause the can filter has not been implemented in glue.c
// #define CANANALOG // Requires CAN, not support on ESP32SX now, besause the can filter has not been implemented in glue.c
// #define ADS111XANALOG // Requires I2C, not support on ESP32SX now
// #define UARTCOMMANDS // Not support on ESP32SX now, besause the uart has not been implemented in glue.c

//#define TMCTEMP // Enable tmc temperature shutdown. replaced by hardware selection
//----------------------


#define TIM_PWM htim1

#define UART_PORT_EXT huart1 // main uart port

#define UART_PORT_MOTOR huart3 // motor uart port

#define UART_BUF_SIZE 1 // How many bytes to expect via DMA

// #define I2C_PORT hi2c1 // open it will change two GPIOs: DIN2->I2C_SDA, DIN3->I2C_SCL

// ADC Channels
#define ADC1_CHANNELS 3 	// how many analog input values to be read by dma
// #define ADC2_CHANNELS 2		// VSENSE

// extern ADC_HandleTypeDef hadc2;
// #define VSENSE_HADC hadc2
// #define ADC_CHAN_VINT 1	// adc buffer index of internal voltage sense
// #define ADC_CHAN_VEXT 0 // adc buffer index of supply voltage sense
// extern volatile uint32_t ADC1_BUF[ADC1_CHANNELS]; // Buffer
// #define VSENSE_ADC_BUF ADC2_BUF
#define VOLTAGE_MULT_DEFAULT 24.6 // Voltage in mV = adc*VOLTAGE_MULT (24.6 for 976k/33k divider)

extern ADC_HandleTypeDef hadc1;
#define AIN_HADC hadc1	// main adc for analog pins
#define ADC_PINS 3	// Amount of analog channel pins
#define ADC_CHAN_FPIN 0 // First analog channel pin. last channel = fpin+ADC_PINS-1

#ifdef OPENFFBOARD_ESP32S2_V1_1
#define BUTTON_PINS 4 // The ESP32SX board has only 4 button input 
#else
#define BUTTON_PINS 3
#endif

extern SPI_HandleTypeDef hspi1;
#define HSPIDRV hspi1

// CAN
#ifdef CANBUS
extern CAN_HandleTypeDef hcan1;
#define CANPORT hcan1

#define CANSPEEDPRESET_50 0
#define CANSPEEDPRESET_100 1
#define CANSPEEDPRESET_125 2
#define CANSPEEDPRESET_250 3
#define CANSPEEDPRESET_500 4
#define CANSPEEDPRESET_1000 5

#endif

// System


#endif /* INC_TARGET_CONSTANTS_H_ */
