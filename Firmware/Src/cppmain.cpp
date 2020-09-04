#include "main.h"
#include "cppmain.h"
#include "mainclass_chooser.h"
#include "flash_helpers.h"
#include <memory>
#include "global_callbacks.h"

uint32_t clkmhz = HAL_RCC_GetHCLKFreq() / 100000;
extern TIM_HandleTypeDef TIM_MICROS;

bool running = true;

uint16_t main_id = 0;
FFBoardMain* mainclass;
ClassChooser<FFBoardMain> mainchooser(class_registry);
extern uint32_t ADC_BUF[ADC_CHANNELS];

USBD_HandleTypeDef hUsbDeviceFS;

void cppmain() {
	TIM_MICROS.Instance->CR1 = 1; // Enable microsecond clock

	HAL_ADC_Start_DMA(&HADC, ADC_BUF, ADC_CHANNELS);

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
	HAL_UART_Receive_IT(UART,(uint8_t*)uart_buf,1);


	mainclass = mainchooser.Create(main_id);//(SelectMain(main_id));
	usb_init();

	while(running){

		mainclass->update();
		updateLeds();
	}

}

void usb_init(){
	mainclass->usbInit();
}


uint32_t micros(){
	//return DWT->CYCCNT / clkmhz;
	return TIM_MICROS.Instance->CNT;
}

