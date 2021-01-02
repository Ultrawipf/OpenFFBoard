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

// Enabled features

// Main classes
#define FFBWHEEL
#define MIDI
#define TMCDEBUG


// Extra features
#define LOCALBUTTONS
#define SPIBUTTONS
//#define SPIBUTTONS2 // Additional spi source
#define SHIFTERBUTTONS
#define ANALOGAXES
#define TMC4671DRIVER
#define LOCALENCODER
//#define PWMDRIVER // Not supported! No free timer
//----------------------


#define TIM_ENC htim3
// Timer 3 is used by the encoder.
#define TIM_PWM htim2

#define TIM_MICROS htim10
extern UART_HandleTypeDef huart1;
#define UART_PORT huart1 // main uart port
#define UART_BUF_SIZE 1 // How many bytes to expect via DMA

extern ADC_HandleTypeDef hadc1;
// ADC Channels
#define ADC1_CHANNELS 9 	// how many analog input values to be read by dma

#define VSENSE_HADC hadc1
#define ADC_CHAN_VINT 7	// adc buffer index of internal voltage sense
#define ADC_CHAN_VEXT 6 // adc buffer index of supply voltage sense
extern volatile uint32_t ADC1_BUF[ADC1_CHANNELS]; // Buffer
#define VSENSE_ADC_BUF ADC1_BUF

#define AIN_HADC hadc1	// main adc for analog pins
#define ADC_PINS 6	// Amount of analog channel pins
#define ADC_CHAN_FPIN 0 // First analog channel pin
#define VOLTAGE_MULT_DEFAULT 24.6 // Voltage in mV = adc*VOLTAGE_MULT (24.6 for 976k/33k divider)

#define BUTTON_PINS 8



#define HSPIDRV hspi1
extern SPI_HandleTypeDef HSPIDRV;
#define HSPI2 hspi2
extern SPI_HandleTypeDef HSPI2;

// Flash. 2 pages used
#define EEPROM_START_ADDRESS  ((uint32_t)0x08004000) /* EEPROM emulation start address: from sector1*/
#define PAGE_SIZE             (uint32_t)0x4000  /* Page size = 16KByte */

#endif /* INC_TARGET_CONSTANTS_H_ */
