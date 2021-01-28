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
#include "PersistentStorage.h"
#include "ExtiHandler.h"
#include "UartHandler.h"
#include "AdcHandler.h"
#include "TimerHandler.h"
#include "CommandHandler.h"
#include "SpiHandler.h"
#ifdef CANBUS
#include "CanHandler.h"
#endif


extern FFBoardMain* mainclass;

#ifdef ADC1_CHANNELS
volatile uint32_t ADC1_BUF[ADC1_CHANNELS] = {0};
extern ADC_HandleTypeDef hadc1;
#endif
#ifdef ADC2_CHANNELS
volatile uint32_t ADC2_BUF[ADC2_CHANNELS] = {0};
extern ADC_HandleTypeDef hadc2;
#endif
#ifdef ADC3_CHANNELS
volatile uint32_t ADC3_BUF[ADC3_CHANNELS] = {0};
extern ADC_HandleTypeDef hadc3;
#endif

volatile char uart_buf[UART_BUF_SIZE] = {0};


// Externally stored so it can be used before the main class is initialized
std::vector<CommandHandler*> cmdHandlers;
std::vector<PersistentStorage*> flashHandlers;


std::vector<AdcHandler*> adcHandlers;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
	//Pulse braking mosfet if internal voltage is higher than supply.
	if(hadc == &VSENSE_HADC)
		brakeCheck();

	uint8_t chans = 0;
	volatile uint32_t* buf = getAnalogBuffer(hadc,&chans);
	if(buf == NULL)
		return;

	for(AdcHandler* c : adcHandlers){
		c->adcUpd(buf,chans,hadc);
	}
}


std::vector<TimerHandler*> timerHandlers;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
	for(TimerHandler* c : timerHandlers){
		c->timerElapsed(htim);
	}
}


std::vector<ExtiHandler*> extiHandlers;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	for(ExtiHandler* c : extiHandlers){
		c->exti(GPIO_Pin);
	}
}

std::vector<UartHandler*> uartHandlers;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if(huart == &UART_PORT){
		  // Received uart data
		if(HAL_UART_Receive_IT(&UART_PORT,(uint8_t*)uart_buf,UART_BUF_SIZE) != HAL_OK){
			pulseErrLed(); // Should never happen
			return;
		}

		for(UartHandler* c : uartHandlers){
			c->uartRcv((char*)uart_buf);
		}
	}
}

#ifdef CANBUS
// CAN
std::vector<CanHandler*> canHandlers;
uint8_t canRxBuf0[8];
CAN_RxHeaderTypeDef canRxHeader0; // Receive header 0
uint8_t canRxBuf1[8];
CAN_RxHeaderTypeDef canRxHeader1; // Receive header 1
// RX
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &canRxHeader0, canRxBuf0) == HAL_OK){
		for(CanHandler* c : canHandlers){
			c->canRxPendCallback(hcan,canRxBuf0,&canRxHeader0,CAN_RX_FIFO0);
		}
	}
}
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan){
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &canRxHeader1, canRxBuf1) == HAL_OK){
		for(CanHandler* c : canHandlers){
			c->canRxPendCallback(hcan,canRxBuf1,&canRxHeader1,CAN_RX_FIFO1);
		}
	}
}
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canRxFullCallback(hcan,CAN_RX_FIFO0);
	}
}
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canRxFullCallback(hcan,CAN_RX_FIFO1);
	}
}
// TX
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canTxCpltCallback(hcan,CAN_TX_MAILBOX0);
	}
}
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canTxCpltCallback(hcan,CAN_TX_MAILBOX1);
	}
}
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canTxCpltCallback(hcan,CAN_TX_MAILBOX2);
	}
}
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canTxAbortCallback(hcan,CAN_TX_MAILBOX0);
	}
}
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canTxAbortCallback(hcan,CAN_TX_MAILBOX1);
	}
}
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canTxAbortCallback(hcan,CAN_TX_MAILBOX2);
	}
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : canHandlers){
		c->canErrorCallback(hcan);
	}
}
#endif

// SPI

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiTxCplt(hspi);
	}
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiRxCplt(hspi);
	}
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiTxRxCplt(hspi);
	}
}

void HAL_SPI_TxHalfCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiTxHalfCplt(hspi);
	}
}

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiRxHalfCplt(hspi);
	}
}

void HAL_SPI_TxRxHalfCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiTxRxHalfCplt(hspi);
	}
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : SpiHandler::getSPIHandlers()){
		c->SpiError(hspi);
	}
}



// USB Specific callbacks
void CDC_Callback(uint8_t* Buf, uint32_t *Len){
	pulseSysLed();
	if(mainclass!=nullptr)
		mainclass->cdcRcv((char*)Buf,Len);
}
void CDC_Finished(){
	if(mainclass!=nullptr)
		mainclass->cdcFinished();
}

/* USB Out Endpoint callback
 * HID Out and Set Feature
 */
UsbHidHandler* globalHidHandler = nullptr;
std::vector<UsbHidHandler*> hidCmdHandlers; // called only for custom cmd report ids
void USBD_OutEvent_HID(uint8_t* report){
	if(globalHidHandler!=nullptr)
		globalHidHandler->hidOut(report);

	if(report[0] == HID_ID_CUSTOMCMD){ // called only for the vendor defined report
		for(UsbHidHandler* c : hidCmdHandlers){
			c->hidOutCmd((HID_Custom_Data_t*)(report));
		}
	}
}
/*
 * HID Get Feature
 */
void USBD_GetEvent_HID(uint8_t id,uint16_t len,uint8_t** return_buf){
	if(globalHidHandler!=nullptr)
		globalHidHandler->hidGet(id, len, return_buf);

}

void USB_SOF(){
	if(mainclass!=nullptr)
		mainclass->SOF();
}


/*
 * Called on usb disconnect and suspend
 */
void USBD_Suspend(){
	mainclass->usbSuspend();
}

/*
 * Called on usb resume. Not called on connect... use SOF instead to detect first activity
 */
void USBD_Resume(){
	mainclass->usbResume();
}


volatile uint32_t* getAnalogBuffer(ADC_HandleTypeDef* hadc,uint8_t* chans){
	#ifdef ADC1_CHANNELS
	if(hadc == &hadc1){
		*chans = ADC1_CHANNELS;
		return ADC1_BUF;
	}
	#endif

	#ifdef ADC2_CHANNELS
	if(hadc == &hadc2){
		*chans = ADC2_CHANNELS;
		return ADC2_BUF;
	}
	#endif

	#ifdef ADC3_CHANNELS
	if(hadc == &hadc3){
		*chans = ADC3_CHANNELS;
		return ADC3_BUF;
	}
	#endif
	return NULL;
}

void startADC(){
	#ifdef ADC1_CHANNELS
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)ADC1_BUF, ADC1_CHANNELS);
	#endif
	#ifdef ADC2_CHANNELS
	HAL_ADC_Start_DMA(&hadc2, (uint32_t*)ADC2_BUF, ADC2_CHANNELS);
	#endif
	#ifdef ADC3_CHANNELS
	HAL_ADC_Start_DMA(&hadc3, (uint32_t*)ADC3_BUF, ADC3_CHANNELS);
	#endif
}


