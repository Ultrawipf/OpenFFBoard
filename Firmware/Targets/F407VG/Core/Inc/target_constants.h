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
#define HW_TYPE "F407VG"
#include "main.h"

// Enabled features

// Main classes
#define FFBWHEEL
#define MIDI
#define TMCDEBUG
#define CANBRIDGE


// Extra features
#define LOCALBUTTONS
#define SPIBUTTONS
#define SHIFTERBUTTONS
#define ANALOGAXES
#define TMC4671DRIVER
#define PWMDRIVER
#define LOCALENCODER
#define CANBUS

#define TMCTEMP // Enable tmc temperature shutdown
//----------------------


#define TIM_ENC htim3
// Timer 3 is used by the encoder.
#define TIM_PWM htim1
#define TIM_PWM_FREQ 168000000

#define TIM_MICROS htim10
extern UART_HandleTypeDef huart1;
#define UART_PORT huart1 // main uart port
#define UART_BUF_SIZE 1 // How many bytes to expect via DMA


// ADC Channels
#define ADC1_CHANNELS 6 	// how many analog input values to be read by dma
#define ADC2_CHANNELS 2		// VSENSE

extern ADC_HandleTypeDef hadc2;
#define VSENSE_HADC hadc2
#define ADC_CHAN_VINT 1	// adc buffer index of internal voltage sense
#define ADC_CHAN_VEXT 0 // adc buffer index of supply voltage sense
extern volatile uint32_t ADC2_BUF[ADC2_CHANNELS]; // Buffer
#define VSENSE_ADC_BUF ADC2_BUF

extern ADC_HandleTypeDef hadc1;
#define AIN_HADC hadc1	// main adc for analog pins
#define ADC_PINS 6	// Amount of analog channel pins
#define ADC_CHAN_FPIN 0 // First analog channel pin. last channel = fpin+ADC_PINS-1
#define VOLTAGE_MULT_DEFAULT 24.6 // Voltage in mV = adc*VOLTAGE_MULT (24.6 for 976k/33k divider)

#define BUTTON_PINS 8

extern SPI_HandleTypeDef hspi1;
#define HSPIDRV hspi1
extern SPI_HandleTypeDef hspi2;
#define HSPI2 hspi2

// CAN
extern CAN_HandleTypeDef hcan1;
#define CANPORT hcan1



//Flash. 2 pages used
/* EEPROM start address in Flash*/
#define EEPROM_START_ADDRESS  ((uint32_t)0x08004000) /* EEPROM emulation start address: from sector1*/
#define PAGE_SIZE             (uint32_t)0x4000  /* Page size = 16KByte */


// System


#endif /* INC_TARGET_CONSTANTS_H_ */
