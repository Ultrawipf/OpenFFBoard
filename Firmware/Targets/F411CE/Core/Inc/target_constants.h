/*
 * target_constants.h
 *
 *  Created on: 06.11.2020
 *      Author: Yannick
 */

#ifndef INC_TARGET_CONSTANTS_H_
#define INC_TARGET_CONSTANTS_H_
#include "main.h"
/*
 * Add settings and peripheral maps for this specific target here
 */

// Hardware name string
#define HW_TYPE "F411RE"
#define HW_TYPE_INT 1
#define FW_DEVID 0x431 // Firmware should run on this chip devid


// Enabled features

// Main classes
#define FFBWHEEL
#define FFBJOYSTICK
//#define MIDI
//#define TMCDEBUG
//#define CANBRIDGE
#define FFBHIDEXT

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
#define PWMDRIVER
#define LOCALENCODER
#define ADS111XANALOG // Requires I2C
#define UARTCOMMANDS

// Disabled features, can be easily reenabled through CUBEMX and
// target_constants.h and cpp_target_config.cpp
//#define TMC4671DRIVER
//#define MTENCODERSPI // requires SPI3
//#define BISSENCODER // Requires SPI3
//#define SSIENCODER // Requires SPI3
//#define SIMPLEMOTION
//#define BTNFAILSAFE

//Unsupported features - CAN, maybe with Serial to CAN converter
//#define CANBUS
//#define ODRIVE
//#define VESC
//#define CANBUTTONS
//#define CANANALOG

// Timer 1 for Local ABN Encoder
#define TIM_ENC htim1
// For PWM, DIR, RC
#define TIM_PWM htim3
#define TIM_USER htim9
#define TIM_MICROS htim10

extern I2C_HandleTypeDef hi2c1;
#define I2C_PORT hi2c1

extern UART_HandleTypeDef huart1;
#define UART_PORT_EXT huart1 // main uart port

#define UART_BUF_SIZE 1 // How many bytes to expect via DMA

extern ADC_HandleTypeDef hadc1;
// ADC Channels
#define ADC1_CHANNELS 8 	// how many analog input values to be read by dma

#define VSENSE_HADC hadc1
#define ADC_CHAN_VINT 7	// adc buffer index of internal voltage sense
#define ADC_CHAN_VEXT 6 // adc buffer index of supply voltage sense
extern volatile uint32_t ADC1_BUF[ADC1_CHANNELS]; // Buffer
#define VSENSE_ADC_BUF ADC1_BUF

#define AIN_HADC hadc1	// main adc for analog pins
#define ADC_PINS 6	// Amount of analog channel pins
#define ADC_CHAN_FPIN 0 // First analog channel pin
#define VOLTAGE_MULT_DEFAULT 24.6 // Voltage in mV = adc*VOLTAGE_MULT (24.6 for 976k/33k divider)

#define BUTTON_PINS 6



// Flash. 2 pages used
#define PAGE0_ID               FLASH_SECTOR_1
#define PAGE1_ID               FLASH_SECTOR_2
#define EEPROM_START_ADDRESS  ((uint32_t)0x08004000) /* EEPROM emulation start address: from sector1*/
#define PAGE_SIZE             (uint32_t)0x4000  /* Page size = 16KByte */

#endif /* INC_TARGET_CONSTANTS_H_ */
