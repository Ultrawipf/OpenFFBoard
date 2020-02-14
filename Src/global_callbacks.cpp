/*
 * global_callbacks.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "vector"
#include <global_callbacks.h>
#include "main.h"
#include "usbd_cdc_if.h"
#include "cppmain.h"
#include "FFBoardMain.h"
#include "ledEffects.h"
#include "voltagesense.h"
#include "constants.h"

#include "UsbHidHandler.h"
#include "ExtiHandler.h"
#include "UartHandler.h"
#include "AdcHandler.h"
#include "TimerHandler.h"

extern FFBoardMain* mainclass;
volatile uint32_t ADC_BUF[ADC_CHANNELS] = {0};
volatile char uart_buf[UART_BUF_SIZE] = {0}; //
bool braking_flag = false;


uint32_t maxVoltage = 75000; // Force braking
uint32_t voltageDiffActivate = 8000;
uint32_t voltageDiffDeactivate = 3000;
std::vector<AdcHandler*> adcHandlers;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
	//Pulse braking mosfet if internal voltage is higher than supply. Conversion: V = 1/36
	uint16_t vint = getIntV();
	uint16_t vext = getExtV();
	braking_flag = (vint > vext + voltageDiffActivate) || vint > maxVoltage || (braking_flag && (vint > vext + voltageDiffDeactivate));
			//(ADC_BUF[ADC_CHAN_VINT] > ADC_BUF[ADC_CHAN_VEXT]+400 || (ADC_BUF[ADC_CHAN_VINT] > 3000));
	HAL_GPIO_WritePin(DRV_BRAKE_GPIO_Port,DRV_BRAKE_Pin, braking_flag ? GPIO_PIN_SET:GPIO_PIN_RESET);

	for(AdcHandler* c : adcHandlers){
		c->adcUpd(ADC_BUF);
	}
//	if(mainclass!=nullptr)
//		mainclass->adcUpd(ADC_BUF);
}

std::vector<TimerHandler*> timerHandlers;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
	for(TimerHandler* c : timerHandlers){
		c->timerElapsed(htim);
	}
//	if(mainclass!=nullptr)
//		mainclass->timerElapsed(htim);
}


std::vector<ExtiHandler*> extiHandlers;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	for(ExtiHandler* c : extiHandlers){
		c->exti(GPIO_Pin);
	}
	//if(mainclass!=nullptr)
		//mainclass->exti(GPIO_Pin);
}

std::vector<UartHandler*> uartHandlers;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == UART){
		  // Received uart data
		if(HAL_UART_Receive_IT(UART,(uint8_t*)uart_buf,UART_BUF_SIZE) != HAL_OK){
			pulseErrLed(); // Should never happen
			return;
		}

		for(UartHandler* c : uartHandlers){
			c->uartRcv((char*)uart_buf);
		}
	  }

//	if(mainclass!=nullptr)
//		mainclass->uartRcv(huart);
}


// USB Specific callbacks
void CDC_Callback(uint8_t* Buf, uint32_t *Len){
	pulseSysLed();
	if(mainclass!=nullptr)
		mainclass->cdcRcv((char*)Buf,Len);
}

// USB Out Endpoint callback
UsbHidHandler* globalHidHandler = nullptr;
void USBD_OutEvent_HID(uint8_t* report){
	if(globalHidHandler!=nullptr)
		globalHidHandler->hidOut(report);
}
void USBD_GetEvent_HID(uint8_t id,uint16_t len,uint8_t** return_buf){
	if(globalHidHandler!=nullptr)
		globalHidHandler->hidGet(id, len, return_buf);
}

void USB_SOF(){
	if(mainclass!=nullptr)
		mainclass->SOF();
}
