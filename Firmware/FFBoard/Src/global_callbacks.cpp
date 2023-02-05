/*
 * global_callbacks.cpp
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#include "vector"
#include <global_callbacks.h>
#include "main.h"
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
#include "EffectsCalculator.h"
#include "SpiHandler.h"
#include "HidCommandInterface.h"
#include "I2CHandler.h"

#ifdef CANBUS
#include "CanHandler.h"
#endif

#ifdef MIDI
#include "MidiHandler.h"
#include "midi_device.h"
#endif

#include "cdc_device.h"
#include "CDCcomm.h"


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

/**
 * Callback after an adc finished conversion
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc){
#ifndef HW_ESP32SX
	//Pulse braking mosfet if internal voltage is higher than supply.
	if(hadc == &VSENSE_HADC)
		brakeCheck();
#endif

	uint8_t chans = 0;
	volatile uint32_t* buf = getAnalogBuffer(hadc,&chans);
	if(buf == NULL)
		return;

	for(AdcHandler* c : AdcHandler::adcHandlers){
		c->adcUpd(buf,chans,hadc);
	}
}

/**
 * Note: this is normally generated in the main.c
 * A call to HAL_TIM_PeriodElapsedCallback_CPP must be added there instead!
 */
#ifndef HW_ESP32SX
__weak void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim){
	HAL_TIM_PeriodElapsedCallback_CPP(htim);
}
#endif
void HAL_TIM_PeriodElapsedCallback_CPP(TIM_HandleTypeDef* htim) {
	for(TimerHandler* c : TimerHandler::timerHandlers){
		c->timerElapsed(htim);
	}
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Callback for GPIO interrupts
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin){
	for(ExtiHandler* c : ExtiHandler::extiHandlers){
		c->exti(GPIO_Pin);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	for(UartHandler* c : UartHandler::getUARTHandlers()){
		c->uartRxComplete(huart);
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
	for(UartHandler* c : UartHandler::getUARTHandlers()){
		c->uartTxComplete(huart);
	}
}

#if defined(CANBUS) && !defined(HW_ESP32SX)
// CAN

uint8_t canRxBuf0[8];
CAN_RxHeaderTypeDef canRxHeader0; // Receive header 0
uint8_t canRxBuf1[8];
CAN_RxHeaderTypeDef canRxHeader1; // Receive header 1
// RX
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan){
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &canRxHeader0, canRxBuf0) == HAL_OK){
		for(CanHandler* c : CanHandler::canHandlers){
			c->canRxPendCallback(hcan,canRxBuf0,&canRxHeader0,CAN_RX_FIFO0);
		}
	}
}
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan){
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &canRxHeader1, canRxBuf1) == HAL_OK){
		for(CanHandler* c : CanHandler::canHandlers){
			c->canRxPendCallback(hcan,canRxBuf1,&canRxHeader1,CAN_RX_FIFO1);
		}
	}
}
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canRxFullCallback(hcan,CAN_RX_FIFO0);
	}
}
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canRxFullCallback(hcan,CAN_RX_FIFO1);
	}
}
// TX
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canTxCpltCallback(hcan,CAN_TX_MAILBOX0);
	}
}
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canTxCpltCallback(hcan,CAN_TX_MAILBOX1);
	}
}
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canTxCpltCallback(hcan,CAN_TX_MAILBOX2);
	}
}
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canTxAbortCallback(hcan,CAN_TX_MAILBOX0);
	}
}
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canTxAbortCallback(hcan,CAN_TX_MAILBOX1);
	}
}
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
		c->canTxAbortCallback(hcan,CAN_TX_MAILBOX2);
	}
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan){
	for(CanHandler* c : CanHandler::canHandlers){
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

// I2C

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef * hi2c){
	for(I2CHandler* c : I2CHandler::getI2CHandlers()){
		c->I2cTxCplt(hi2c);
	}
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef * hi2c){
	for(I2CHandler* c : I2CHandler::getI2CHandlers()){
		c->I2cRxCplt(hi2c);
	}
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef * hi2c){
	for(I2CHandler* c : I2CHandler::getI2CHandlers()){
		c->I2cTxCplt(hi2c);
	}
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef * hi2c){
	for(I2CHandler* c : I2CHandler::getI2CHandlers()){
		c->I2cRxCplt(hi2c);
	}
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c){
	for(I2CHandler* c : I2CHandler::getI2CHandlers()){
		c->I2cError(hi2c);
	}
}
#ifdef __cplusplus
}
#endif

//void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c);
//void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);

// USB Callbacks
USBdevice* usb_device;
uint8_t const * tud_descriptor_device_cb(void){
  return usb_device->getUsbDeviceDesc();
}

uint8_t const * tud_descriptor_configuration_cb(uint8_t index){
	return usb_device->getUsbConfigurationDesc(index);
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid){
	return usb_device->getUsbStringDesc(index, langid);
}

uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf){
	return UsbHidHandler::getHidDesc();
}

void tud_cdc_rx_cb(uint8_t itf){
	pulseSysLed();
	if(mainclass!=nullptr){
		mainclass->cdcRcvReady(itf);
	}
}

void tud_cdc_tx_complete_cb(uint8_t itf){

	CDCcomm::cdcFinished(itf);
}



/**
 * USB Out Endpoint callback
 * HID Out and Set Feature
 */
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize){
	if(report_type == HID_REPORT_TYPE_INVALID && report_id == 0){
		report_id = *buffer;
	}

	if(UsbHidHandler::globalHidHandler!=nullptr)
		UsbHidHandler::globalHidHandler->hidOut(report_id,report_type,buffer,bufsize);

	if(report_id == HID_ID_HIDCMD){
		if(HID_CommandInterface::globalInterface != nullptr)
			HID_CommandInterface::globalInterface->hidCmdCallback((HID_CMD_Data_t*)(buffer));
	}


}

/**
 * HID Get Feature
 */
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,uint8_t* buffer, uint16_t reqlen){
	if(UsbHidHandler::globalHidHandler != nullptr)
		return UsbHidHandler::globalHidHandler->hidGet(report_id, report_type, buffer,reqlen); // reply buffer should not contain report ID in first byte
	return 0;
}

/**
 * HID transfer complete
 */
void tud_hid_report_complete_cb(uint8_t itf, uint8_t const* report, uint8_t len){
	if(HID_CommandInterface::globalInterface != nullptr){
		HID_CommandInterface::globalInterface->transferComplete(itf, report, len);
	}
	if(UsbHidHandler::globalHidHandler != nullptr){
		UsbHidHandler::globalHidHandler->transferComplete(itf, report, len);
	}
}

#ifdef MIDI
MidiHandler* midihandler = nullptr;
/**
 * Midi receive callback
 */
void tud_midi_rx_cb(uint8_t itf){
	if(!midihandler) return;

	if(tud_midi_n_packet_read(itf,MidiHandler::buf)){
		midihandler->midiRx(itf, MidiHandler::buf);
	}
}
#endif

/**
 * Called on usb disconnect and suspend
 */
void tud_suspend_cb(){
	mainclass->usbSuspend();
}
void tud_umount_cb(){
	mainclass->usbSuspend();
}

/**
 * Called on usb mount
 */
void tud_mount_cb(){
	mainclass->usbResume();
}
void tud_resume_cb(){
	mainclass->usbResume();
}

#ifdef HW_ESP32SX
extern "C" volatile uint32_t* getAnalogBuffer(ADC_HandleTypeDef* hadc,uint8_t* chans)
{
	uint8_t result[12] = {0};
    uint32_t ret_num = 0;
	esp_err_t ret = adc_digi_read_bytes(result, 12, &ret_num, ADC_MAX_DELAY);
	if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) 
	{
		for (size_t i = 0; i < ret_num; i+=2)
		{
			adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
#ifdef CONFIG_IDF_TARGET_ESP32S3
        	ADC1_BUF[p->type2.channel] = p->type2.data;
#elif defined CONFIG_IDF_TARGET_ESP32S2
        	ADC1_BUF[p->type1.channel] = p->type1.data;
#endif
		}
		ESP_LOGV(__FUNCTION__, "[%d] Channel: 0|%04d, 1|%04d, 2|%04d", ret_num, 
				ADC1_BUF[0],
				ADC1_BUF[1],
				ADC1_BUF[2]);
	}
	*chans = ADC1_CHANNELS;
	return ADC1_BUF;
}
#else

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
#endif

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
