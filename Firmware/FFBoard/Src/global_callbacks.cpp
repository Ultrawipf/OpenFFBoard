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
#include "CommandHandler.h"
#include "SpiHandler.h"

extern FFBoardMain* mainclass;

#ifdef ADC1_CHANNELS
volatile uint32_t ADC1_BUF[ADC1_CHANNELS] = {0};
#endif
#ifdef ADC2_CHANNELS
volatile uint32_t ADC2_BUF[ADC2_CHANNELS] = {0};
#endif
#ifdef ADC3_CHANNELS
volatile uint32_t ADC3_BUF[ADC3_CHANNELS] = {0};
#endif

volatile char uart_buf[UART_BUF_SIZE] = {0}; //


// Externally stored so it can be used before the main class is initialized
std::vector<CommandHandler*> cmdHandlers;


int _write(int file, char *ptr, int len)
{
  CDC_Transmit_FS(ptr, len);
  return len;
}

std::vector<AdcHandler*> adcHandlers;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
	//Pulse braking mosfet if internal voltage is higher than supply.
	brakeCheck();

	for(AdcHandler* c : adcHandlers){
		c->adcUpd(ADC1_BUF);
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
}


// SPI
std::vector<SpiHandler*> spiHandlers;
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
		c->SpiTxCplt(hspi);
	}
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
		c->SpiRxCplt(hspi);
	}
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
		c->SpiTxRxCplt(hspi);
	}
}

void HAL_SPI_TxHalfCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
		c->SpiTxHalfCplt(hspi);
	}
}

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
		c->SpiRxHalfCplt(hspi);
	}
}

void HAL_SPI_TxRxHalfCpltCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
		c->SpiTxRxHalfCplt(hspi);
	}
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi){
	for(SpiHandler* c : spiHandlers){
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



