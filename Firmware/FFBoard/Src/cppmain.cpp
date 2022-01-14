#include "main.h"
#include "cppmain.h"
#include "mainclass_chooser.h"
#include "flash_helpers.h"
#include "global_callbacks.h"
#include "cpp_target_config.h"
#include "cmsis_os.h"
#include "stm32f4xx_hal_flash.h"
#include "RessourceManager.h"

#include "tusb.h"

uint32_t clkmhz = HAL_RCC_GetHCLKFreq() / 100000;
extern TIM_HandleTypeDef TIM_MICROS;

#ifdef HAL_IWDG_MODULE_ENABLED
extern IWDG_HandleTypeDef hiwdg; // Watchdog
#endif

bool running = true;
bool mainclassChosen = false;

uint16_t main_id = 0;

FFBoardMain* mainclass __attribute__((section (".ccmram")));
ClassChooser<FFBoardMain> mainchooser(class_registry);


#define USBD_STACK_SIZE     (3*configMINIMAL_STACK_SIZE/2)
StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

RessourceManager ressourceManager = RessourceManager();

void cppmain() {
#ifdef FW_DEVID
	if(HAL_GetDEVID() != FW_DEVID){
		/**
		 * Firmware is not intended for this chip!
		 * This can be caused by accidentially flashing an incorrect firmware file and likely screws up clock and pin configs
		 * Do not proceed.
		 */
		while(true){ // Block forever to prevent an incorrect firmware from damaging hardware
			Error_Handler();
		}
	}
#endif

	// Flash init
	// TODO verify why or if flash does not erase or initialize correctly on some new chips
	HAL_FLASH_Unlock();
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

	if( EE_Init() != EE_OK){
		Error_Handler();
	}
	// Check if flash is initialized
	uint16_t lastVersion = 0;
	if(!Flash_Read(ADR_SW_VERSION, &lastVersion)){ // Version never written
		Flash_Write(ADR_SW_VERSION, (SW_VERSION_INT[0]<<8) | SW_VERSION_INT[1]);
	}
	Flash_Read(ADR_SW_VERSION,&lastVersion);
	if((lastVersion & 0xff00) != (SW_VERSION_INT[0]<<8)){
		EE_Format(); // Major version changed or could not write initial value. force a format
		Flash_Write(ADR_SW_VERSION, (SW_VERSION_INT[0]<<8) | SW_VERSION_INT[1]);
	}
	HAL_FLASH_Lock();
	// ------------------------

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

	mainclass = mainchooser.Create(main_id);
	if(mainclass == nullptr){ // invalid id
		mainclass = mainchooser.Create(0); // Baseclass
	}
	mainclassChosen = true;

	mainclass->usbInit(); // Let mainclass initialize usb

	while(running){
		mainclass->update();
		updateLeds();
		//external_spi.process();
		refreshWatchdog();
		taskYIELD(); // Change task if higher priority task wants to run
	}
}

void refreshWatchdog(){
#ifdef HAL_IWDG_MODULE_ENABLED
	HAL_IWDG_Refresh(&hiwdg); // Refresh watchdog
#endif
}



uint32_t micros(){
	//return DWT->CYCCNT / clkmhz;
	return TIM_MICROS.Instance->CNT;
}


void* malloc(size_t size)
{
    return pvPortMalloc(size);
}

void free(void *p)
{
    vPortFree(p);
}

unsigned long getRunTimeCounterValue(void){
	return micros();
}

