#include "main.h"
#include "cppmain.h"
#include "mainclass_chooser.h"
#include "flash_helpers.h"
#include "global_callbacks.h"
#include "cpp_target_config.h"
#include "cmsis_os.h"
#include "tusb.h"

uint32_t clkmhz = HAL_RCC_GetHCLKFreq() / 100000;

#ifdef HAL_IWDG_MODULE_ENABLED
extern IWDG_HandleTypeDef hiwdg; // Watchdog
#endif

bool running = true;
bool mainclassChosen = false;

uint16_t main_id = 0;

FFBoardMain* mainclass __attribute__((section (CCRAM_SEC)));
ClassChooser<FFBoardMain> mainchooser(class_registry);


#define USBD_STACK_SIZE     (3*configMINIMAL_STACK_SIZE/2)
StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;


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
	if(!Flash_Init()){
		Error_Handler();
	}

//	// Check if flash is initialized
//	uint16_t lastVersion = 0;
//	if(!Flash_Read(ADR_SW_VERSION, &lastVersion)){ // Version never written
//		Flash_Write(ADR_SW_VERSION, (SW_VERSION_INT[0]<<8) | SW_VERSION_INT[1]);
//	}
//	Flash_Read(ADR_SW_VERSION,&lastVersion);
//	if((lastVersion & 0xff00) != (SW_VERSION_INT[0]<<8)){
//		//EE_Format(); // Major version changed or could not write initial value. force a format
//		Flash_Write(ADR_SW_VERSION, (SW_VERSION_INT[0]<<8) | SW_VERSION_INT[1]);
//	}
	// Check if flash is initialized
	uint16_t lastFlashVersion = 0;
	if(!Flash_Read(ADR_FLASH_VERSION, &lastFlashVersion)){ // Version never written
		Flash_Write(ADR_FLASH_VERSION, FLASH_VERSION);
	}
	Flash_Read(ADR_FLASH_VERSION,&lastFlashVersion);
	if(lastFlashVersion != FLASH_VERSION){
		Flash_Format(); // Major version changed or could not write initial value. force a format
		Flash_Write(ADR_FLASH_VERSION, FLASH_VERSION);
	}

	// ------------------------

	startADC(); // enable ADC DMA

	// If switch pressed at boot select failsafe implementation
#ifdef BTNFAILSAFE
	if(HAL_GPIO_ReadPin(BUTTON_A_GPIO_Port, BUTTON_A_Pin) == 1){
		main_id = 0;
	}else
#endif
	if(!Flash_ReadWriteDefault(ADR_CURRENT_CONFIG, &main_id,DEFAULTMAIN)){
		Error_Handler();
	}

	PersistentStorage::restoreFlashStartupCb(); // Flash is initialized. allow restoring now

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


/**
 * TIM_MICROS_HALTICK MUST be reset by the HAL tick OR be the same tick timer to count microseconds since last tick update.
 * By default ST HAL initializes the tick timer with 1MHz and 1kHz overrun interrupts so TIM_MICROS_HALTICK can be defined as that timer.
 * Alternatively an actual freerunning 32b can be defined as TIM_MICROS to use its count directly.
 * Otherwise the cyclecounter is used.
 */
uint32_t micros(){
#ifdef TIM_MICROS_HALTICK
	extern TIM_HandleTypeDef TIM_MICROS_HALTICK;
	return (HAL_GetTick() * 1000) + TIM_MICROS_HALTICK.Instance->CNT;
#elif defined(TIM_MICROS)
	extern TIM_HandleTypeDef TIM_MICROS;
	return TIM_MICROS.Instance->CNT;
#else
	return DWT->CYCCNT / clkmhz;
#endif

}


void* malloc(size_t size)
{
    return pvPortMalloc(size);
}

void free(void *p)
{
    vPortFree(p);
}

/**
 * Helper function for RTOS run time measurements
 * Should return a reasonably accurate and large counter value
 */
unsigned long getRunTimeCounterValue(void){
	return micros();
}

