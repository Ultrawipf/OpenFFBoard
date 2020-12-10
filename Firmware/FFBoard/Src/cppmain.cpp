#include "main.h"
#include "cppmain.h"
#include "mainclass_chooser.h"
#include "flash_helpers.h"
#include "global_callbacks.h"

uint32_t clkmhz = HAL_RCC_GetHCLKFreq() / 100000;
extern TIM_HandleTypeDef TIM_MICROS;

#ifdef HAL_IWDG_MODULE_ENABLED
extern IWDG_HandleTypeDef hiwdg; // Watchdog
#endif

bool running = true;

uint16_t main_id = 0;

FFBoardMain* mainclass __attribute__((section (".ccmram")));
ClassChooser<FFBoardMain> mainchooser(class_registry);

USBD_HandleTypeDef hUsbDeviceFS __attribute__((section (".ccmram")));

void cppmain() {
	// Flash init
	HAL_FLASH_Unlock();
	if( EE_Init() != EE_OK){
		Error_Handler();
	}
	HAL_FLASH_Lock();

	TIM_MICROS.Instance->CR1 = 1; // Enable microsecond clock

	startADC(); // enable ADC DMA

	// If switch pressed at boot select failsafe implementation
	if(HAL_GPIO_ReadPin(BUTTON_A_GPIO_Port, BUTTON_A_Pin) == 1){
		main_id = 0;
	}else{
		if(!Flash_ReadWriteDefault(ADR_CURRENT_CONFIG, &main_id,0)){
			Error_Handler();
		}
	}
	// Enable uart interrupt
	extern volatile char uart_buf[UART_BUF_SIZE];
	HAL_UART_Receive_IT(&UART_PORT,(uint8_t*)uart_buf,1);


	mainclass = mainchooser.Create(main_id);
	usb_init(&hUsbDeviceFS); // Init usb

	while(running){
		// TODO dynamically add functions to loop
		mainclass->update();
		mainclass->updateSys();
		updateLeds();

		refreshWatchdog();
	}

}

void refreshWatchdog(){
#ifdef HAL_IWDG_MODULE_ENABLED
	HAL_IWDG_Refresh(&hiwdg); // Refresh watchdog
#endif
}

void usb_init(USBD_HandleTypeDef* hUsbDeviceFS){
	mainclass->usbInit(hUsbDeviceFS); // Let mainclass initialize usb
}


uint32_t micros(){
	//return DWT->CYCCNT / clkmhz;
	return TIM_MICROS.Instance->CNT;
}

