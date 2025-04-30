/*
 * target_constants.h
 *
 *  Created on: 06.11.2020
 *      Author: Yannick
 */

#ifndef INC_TARGET_CONSTANTS_H_
#define INC_TARGET_CONSTANTS_H_

/*
 * Add settings and peripheral maps for this specific target here
 */

// Hardware name string
#define HW_TYPE "F407VG_DISCO"
#define HW_TYPE_INT 3 // Discovery board
#define FW_DEVID 0x413 // F407

#include "main.h"

// Enabled features
#define DEFAULTMAIN 1 // FFBWHeel
// Main classes
#define FFBWHEEL
#define FFBJOYSTICK
//#define MIDI
//#define TMCDEBUG
#define CANBRIDGE
#define FFBHIDEXT
#define CANINPUTMAIN

/*
 * FFBWheel uses 2 FFB axis descriptor instead of 1 axis.
 * Might improve compatibility with direct input but will report a 2 axis ffb compatible device
 */
//#define FFBWHEEL_USE_1AXIS_DESC

// Extra features
#define LOCALBUTTONS
#define SPIBUTTONS
#define SHIFTERBUTTONS
#define PCF8574BUTTONS // Requires I2C
#define ANALOGAXES
//#define TMC4671DRIVER
#define PWMDRIVER
#define LOCALENCODER
#define CANBUS
#define ODRIVE
#define VESC
#define MTENCODERSPI // requires SPI3
#define CANBUTTONS // Requires CAN
#define CANANALOG // Requires CAN
#define ADS111XANALOG // Requires I2C
#define SIMPLEMOTION

//#define UARTCOMMANDS

//----------------------


#define TIM_ENC htim2
// Timer 2 is used by the encoder.
#define TIM_PWM htim1

#define TIM_MICROS_HALTICK htim7 // Micros timer MUST be reset by hal tick timer or isr to count microseconds since last tick
#define TIM_USER htim9 // Timer with full core clock speed available for the mainclass
#define TIM_TMC htim6 // Timer running at half clock speed

extern UART_HandleTypeDef huart1;
#define UART_PORT_EXT huart1 // main uart port

extern UART_HandleTypeDef huart3;
#define UART_PORT_MOTOR huart3 // motor uart port

#define UART_BUF_SIZE 1 // How many bytes to expect via DMA

extern I2C_HandleTypeDef hi2c1;
#define I2C_PORT hi2c1



// ADC Channels
#define ADC1_CHANNELS 5 	// how many analog input values to be read by dma
#define ADC2_CHANNELS 2		// VSENSE

extern ADC_HandleTypeDef hadc2;
#define VSENSE_HADC hadc2
#define ADC_CHAN_VINT 1	// adc buffer index of internal voltage sense
#define ADC_CHAN_VEXT 0 // adc buffer index of supply voltage sense
extern volatile uint32_t ADC2_BUF[ADC2_CHANNELS]; // Buffer
#define VSENSE_ADC_BUF ADC2_BUF

extern ADC_HandleTypeDef hadc1;
#define AIN_HADC hadc1	// main adc for analog pins
#define ADC_PINS 3	// Amount of analog channel pins
#define ADC_CHAN_FPIN 0 // First analog channel pin. last channel = fpin+ADC_PINS-1
//#define VOLTAGE_MULT_DEFAULT 30.12 // mV adc * scaler = voltage //(30.12 for 976k/33k divider)

extern volatile uint32_t ADC1_BUF[ADC1_CHANNELS]; // Buffer
#define TEMPSENSOR_ADC_VAL ADC1_BUF[3] // ADC1 ch 4
#define ADC_INTREF_VAL ADC1_BUF[4] // ADC1 ch 5.

#define BUTTON_PINS 8

//extern SPI_HandleTypeDef hspi1;
//#define HSPIDRV hspi1
extern SPI_HandleTypeDef hspi2;
#define HSPI2 hspi2
extern SPI_HandleTypeDef hspi3;
#define EXT3_SPI_PORT hspi3

// CAN
#ifdef CANBUS
extern CAN_HandleTypeDef hcan1;
#define CANPORT hcan1
#endif


//Flash. 2 pages used
/* EEPROM start address in Flash
 * PAGE_ID sectors 1 and 2!
 * */
#define USE_EEPROM_EMULATION
#define PAGE0_ID               FLASH_SECTOR_1
#define PAGE1_ID               FLASH_SECTOR_2
#define EEPROM_START_ADDRESS  ((uint32_t)0x08004000) /* EEPROM emulation start address: from sector1*/
#define PAGE_SIZE             (uint32_t)0x4000  /* Page size = 16KByte */


// System
#define CCRAM_SEC ".ccmram"

#endif /* INC_TARGET_CONSTANTS_H_ */
