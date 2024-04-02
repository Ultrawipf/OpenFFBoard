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
#include "SelectableInputs.h"
#include "thread.hpp"

class FFBHIDMain: public FFBoardMain, public cpp_freertos::Thread, PersistentStorage,ExtiHandler,public UsbHidHandler, ErrorHandler, SelectableInputs
#ifdef TIM_FFB
, TimerHandler // Adds timer handler
#endif
{
	enum class FFBWheel_commands : uint32_t{
		ffbactive,axes,btntypes,lsbtn,addbtn,aintypes,lsain,addain,hidrate,hidsendspd,estop,cfrate
	};

public:
	FFBHIDMain(uint8_t axisCount,bool hidAxis32b = false);
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
#ifdef TIM_FFB
	void timerElapsed(TIM_HandleTypeDef* htim);
#endif

	float getCurFFBFreq();

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
	uint8_t usb_report_rate = HID_BINTERVAL; //for FS USB 1 = 1000hz, 2 = 500hz, 3 = 333hz 4 = 250hz, 5 = 200hz 6 = 166hz, 8 = 125hz etc...
	uint8_t usb_report_rate_idx = ffbrates.defaultmode;
#ifndef TIM_FFB
	uint8_t ffb_rate_divider = 0; // TODO support ffb without timers again
#endif


	struct FFB_update_rates{
		struct FFB_update_rate_divider{
			uint8_t basediv;
			uint8_t hiddiv;
		};

#if TUD_OPT_HIGH_SPEED // divider pair <FFB div, USB div from base>
		const uint8_t defaultmode = 3;
		uint32_t basefreq = 8000;
		std::array<FFB_update_rate_divider,7> dividers = {{{1,1},{2,1},{4,1},{8,1},{16,1},{32,1},{64,1}}}; // 8khz to 125hz
#else
		const uint8_t defaultmode = 0;
		uint32_t basefreq = 1000;
		std::array<FFB_update_rate_divider,4> dividers = {{{1,1},{2,1},{4,1},{8,1}}}; // 8 entries max. 1khz to 125hz
#endif
	};
	const static FFB_update_rates ffbrates;

	const bool hidAxis32b;

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
	//HID_CommandInterface hidCommands; // Enables full HID control
	std::unique_ptr<HID_CommandInterface> hidCommands = std::make_unique<HID_CommandInterface>();

	uint32_t lastUsbReportTick = 0;

	volatile uint32_t lastEstop = 0;
};

#endif /* SRC_FFBWHEEL_H_ */
