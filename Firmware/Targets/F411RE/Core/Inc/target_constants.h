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

#define HW_TYPE "F411RE"

#define TIM_ENC htim3
// Timer 3 is used by the encoder.

#define TIM_MICROS htim10
extern UART_HandleTypeDef huart1;
#define UART &huart1 // main uart port
#define UART_BUF_SIZE 1 // How many bytes to expect via DMA

extern ADC_HandleTypeDef hadc1;
#define AIN_HADC hadc1	// main adc for analog pins

#define VSENSE_HADC hadc1
#define ADC_CHAN_VINT 7	// adc buffer index of internal voltage sense
#define ADC_CHAN_VEXT 6 // adc buffer index of supply voltage sense

#define ADC_PINS 6	// Amount of analog channel pins
#define ADC_CHAN_FPIN 0 // First analog channel pin
#define VOLTAGE_MULT_DEFAULT 24.6 // Voltage in mV = adc*VOLTAGE_MULT (24.6 for 976k/33k divider)

// ADC Channels
#define ADC1_CHANNELS 9 	// how many analog input values to be read by dma

#define HSPIDRV hspi1
extern SPI_HandleTypeDef HSPIDRV;
#define HSPI2 hspi2
extern SPI_HandleTypeDef HSPI2;




#endif /* INC_TARGET_CONSTANTS_H_ */
