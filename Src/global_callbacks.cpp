/*
 * global_callbacks.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */


#include <global_callbacks.h>
#include "main.h"
#include "usbd_cdc_if.h"
#include "cppmain.h"
#include "FFBoardMain.h"
#include "ledEffects.h"
#include "voltagesense.h"
#include "UsbHidHandler.h"

extern FFBoardMain* mainclass;

extern ADC_HandleTypeDef HADC;
ADC_HandleTypeDef* adc = &HADC;
volatile uint32_t ADC_BUF[ADC_CHANNELS] = {0};

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
	//Pulse braking mosfet if internal voltage is higher than supply. Conversion: V = 1/36
	uint16_t vint = getIntV();
	uint16_t vext = getExtV();
	bool braking_flag = (vint > vext + 8000) || vint > 75000;
			//(ADC_BUF[ADC_CHAN_VINT] > ADC_BUF[ADC_CHAN_VEXT]+400 || (ADC_BUF[ADC_CHAN_VINT] > 3000));
	HAL_GPIO_WritePin(DRV_BRAKE_GPIO_Port,DRV_BRAKE_Pin, braking_flag ? GPIO_PIN_SET:GPIO_PIN_RESET);

	if(mainclass!=nullptr)
		mainclass->adcUpd(ADC_BUF);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
	if(mainclass!=nullptr)
		mainclass->timerElapsed(htim);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	if(mainclass!=nullptr)
		mainclass->exti(GPIO_Pin);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(mainclass!=nullptr)
		mainclass->uartRcv(huart);
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
