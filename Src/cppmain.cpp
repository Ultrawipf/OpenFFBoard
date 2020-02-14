#include "main.h"
#include "cppmain.h"
#include "mainclass_chooser.h"
#include "flash_helpers.h"
#include <memory>
#include "global_callbacks.h"

bool running = true;

uint16_t main_id = 0;
FFBoardMain* mainclass;

extern uint32_t ADC_BUF[ADC_CHANNELS];

void cppmain() {
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

	mainclass = (SelectMain(main_id));
	mainclass->usbInit();

	while(running){

		mainclass->update();
		updateLeds();
	}

}





