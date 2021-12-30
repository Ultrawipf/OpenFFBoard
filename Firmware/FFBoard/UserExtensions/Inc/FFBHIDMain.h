/*
 * FFBWheel.h
 *
 *  Created on: 31.01.2020
 *      Author: Yannick
 */

#ifndef SRC_FFBWHEEL_H_
#define SRC_FFBWHEEL_H_
//#include <CmdParser.h>
#include <CanAnalog.h>
#include <FFBoardMain.h>
#include <MotorPWM.h>

#include "TMC4671.h"
#include "flash_helpers.h"
#include "ButtonSource.h"
#include "EncoderLocal.h"

#include "Axis.h"
#include "cppmain.h"
#include "HidFFB.h"
#include "TimerHandler.h"
#include "ClassChooser.h"
#include "ExtiHandler.h"
#include "AxesManager.h"
#include "EffectsCalculator.h"
#include "ffb_defs.h"
#include "UsbHidHandler.h"
#include "ErrorHandler.h"
#include "memory"
#include "HidCommandInterface.h"

#include "thread.hpp"

#ifdef HW_ESP32SX
#define FFBHIDMAIN_THREAD_MEM 4096
#define FFBHIDMAIN_THREAD_PRIO (30*25/56)
#else
#define FFBHIDMAIN_THREAD_MEM 256
#define FFBHIDMAIN_THREAD_PRIO 30
#endif

class FFBHIDMain: public FFBoardMain, public cpp_freertos::Thread, PersistentStorage,ExtiHandler,public UsbHidHandler, ErrorHandler{
	enum class FFBWheel_commands : uint32_t{
		ffbactive,axes,btntypes,lsbtn,addbtn,aintypes,lsain,addain,hidrate,hidsendspd,estop,cfrate
	};

public:
	FFBHIDMain(uint8_t axisCount);
	virtual ~FFBHIDMain();
	void setFFBEffectsCalc(std::shared_ptr<EffectsControlItf> ffb,std::shared_ptr<EffectsCalculator> effects_calc);

	//static ClassIdentifier info;
	const ClassIdentifier getInfo() = 0;
	static bool isCreatable() {return true;};

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	virtual std::string getHelpstring(){
		return "Force feedback HID game controller";
	}
	void setBtnTypes(uint16_t btntypes);
	void addBtnType(uint16_t id);
	void clearBtnTypes();

	void setAinTypes(uint16_t aintypes);
	void addAinType(uint16_t id);
	void clearAinTypes();


	virtual void usbInit() = 0; // initialize a composite usb device
	void usbSuspend(); // Called on usb disconnect and suspend
	void usbResume(); // Called on usb resume

	void saveFlash();
	void restoreFlash();

	void Run();
	void updateControl();

	void emergencyStop(bool reset);
	uint32_t getRate();
	bool getFfbActive();

	//void timerElapsed(TIM_HandleTypeDef* htim);
	void exti(uint16_t GPIO_Pin);

	void errorCallback(const Error &error, bool cleared);

	void systick();

protected:
	std::shared_ptr<EffectsControlItf> ffb;
	std::shared_ptr<EffectsCalculator> effects_calc;
	uint8_t axisCount=0;

private:
	volatile Control_t control;
	void send_report();
	const bool allowEstopReset = true; // Resets the Estop when the pin is released
	//bool lastEstopState = false;
	const Error estopError = Error(ErrorCode::emergencyStop, ErrorType::critical, "Emergency stop button triggered");

	/* USB Report rate
	 * Warning: Report rate initialized by bInterval is overridden by saved speed preset at startup!
	 */
	void setReportRate(uint8_t rateidx);
	uint8_t usb_report_rate = HID_BINTERVAL; //1 = 1000hz, 2 = 500hz, 3 = 333hz 4 = 250hz, 5 = 200hz 6 = 166hz, 8 = 125hz etc...
	uint8_t usb_report_rate_idx = 0;
	const uint8_t usb_report_rates[4] = {1,2,4,8}; // Maps stored hid speed to report rates

	std::string usb_report_rates_names();

	uint8_t report_rate_cnt = 0;

	std::unique_ptr<AxesManager> axes_manager;
	//TIM_HandleTypeDef* timer_update;

	std::vector<std::unique_ptr<ButtonSource>> btns;
	std::vector<std::unique_ptr<AnalogSource>> analog_inputs;

	reportHID_t reportHID;
	reportHID_t lastReportHID;
	uint8_t reportSendCounter = 0;

	const uint8_t analogAxisCount = 8;
	uint16_t btnsources = 1; // Default ID0 = local buttons
	uint16_t ainsources = 1;


	ClassChooser<ButtonSource> btn_chooser;
	ClassChooser<AnalogSource> analog_chooser;

	//HID_CommandInterface hidCommands; // Enables full HID control
	std::unique_ptr<HID_CommandInterface> hidCommands = std::make_unique<HID_CommandInterface>();

	uint32_t lastUsbReportTick = 0;
	cpp_freertos::BinarySemaphore sourcesSem = cpp_freertos::BinarySemaphore(true);

	volatile uint32_t lastEstop = 0;
};

#endif /* SRC_FFBWHEEL_H_ */
