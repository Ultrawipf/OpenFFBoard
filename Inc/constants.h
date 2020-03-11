#ifndef __CONSTANTS_H
#define __CONSTANTS_H

#include "main.h"

#define TIM_ENC htim3;
// Timer 3 is used by the encoder. All other timers are free to use


#define SW_VERSION 1

#define ADC_CHANNELS 9 	// how many analog input values to be read by dma

extern ADC_HandleTypeDef hadc1;
#define HADC hadc1	// main adc for analog pins

extern UART_HandleTypeDef huart1;
#define UART &huart1 // main uart port
#define UART_BUF_SIZE 1 // How many bytes to expect via DMA

#define ADC_CHAN_VINT 7
#define ADC_CHAN_VEXT 6
#define ADC_PINS 6
#define ADC_CHAN_FPIN 0
#define VOLTAGE_MULT 24.6 // Voltage in mV = adc*VOLTAGE_MULT (24.6 for 976k/33k divider)

#define HSPIDRV hspi1
extern SPI_HandleTypeDef HSPIDRV;
#define HSPI2 hspi2
extern SPI_HandleTypeDef HSPI2;


#define CDC_INTERFACE 0x00
#define CDC_INTERFACE_DATA 0x01

#define CDC_IDX 0
#define HID_IDX 1

#endif
