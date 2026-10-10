/*
 * PidHost.h
 *
 * Simulates the host side of the USB HID PID (Physical Interface Device) protocol,
 * i.e. what the operating systems force feedback driver sends to the device.
 *
 * Reports are passed to the registered UsbHidHandler exactly like the usb callbacks
 * in global_callbacks.cpp do (tud_hid_set_report_cb / tud_hid_get_report_cb):
 *  - Output reports: the report id is the first byte of the buffer
 *  - Feature set reports: the buffer does not contain the report id
 *  - Feature get reports: the reply buffer does not contain the report id
 *
 * Reference: Device Class Definition for PID 1.01 (https://usb.org/sites/default/files/documents/pid1_01.pdf)
 */
#ifndef HOSTTEST_PIDHOST_H_
#define HOSTTEST_PIDHOST_H_

#include "UsbHidHandler.h"
#include "ffb_defs.h"
#include "hid.h"
#include <cstring>
#include <vector>

class PidHost {
public:
	// Effect Operation values (PID 1.01: Effect Operation Report)
	static constexpr uint8_t OP_START = 1;
	static constexpr uint8_t OP_START_SOLO = 2;
	static constexpr uint8_t OP_STOP = 3;

	// PID Device Control bits of the report descriptor (PID 1.01: PID Device Control Report)
	static constexpr uint8_t DC_ENABLE_ACTUATORS = 0x01;
	static constexpr uint8_t DC_DISABLE_ACTUATORS = 0x02;
	static constexpr uint8_t DC_STOP_ALL_EFFECTS = 0x04;
	static constexpr uint8_t DC_DEVICE_RESET = 0x08;
	static constexpr uint8_t DC_DEVICE_PAUSE = 0x10;
	static constexpr uint8_t DC_DEVICE_CONTINUE = 0x20;

	// Block Load Status (PID 1.01: PID Block Load Report)
	static constexpr uint8_t LOAD_SUCCESS = 1;
	static constexpr uint8_t LOAD_FULL = 2;
	static constexpr uint8_t LOAD_ERROR = 3;

	// Axes Enable / Direction Enable bits of the 2 axis descriptor
	static constexpr uint8_t AXIS_X = 0x01;
	static constexpr uint8_t AXIS_Y = 0x02;
	static constexpr uint8_t DIRECTION_ENABLE_2AXIS = 0x04;
	static constexpr uint8_t DIRECTION_ENABLE_1AXIS = 0x02;

	struct BlockLoad {
		uint8_t effectBlockIndex = 0;
		uint8_t loadStatus = 0;
		uint16_t ramPoolAvailable = 0;
	};

	struct Pool {
		uint16_t ramPoolSize = 0;
		uint8_t maxSimultaneousEffects = 0;
		uint8_t memoryManagement = 0;
	};

	/** Optional parameters of the Set Effect report */
	struct EffectParams {
		uint16_t duration = 0xFFFF; //!< ms. 0xFFFF infinite
		uint8_t gain = 255;
		uint8_t enableAxis = AXIS_X | DIRECTION_ENABLE_2AXIS;
		uint16_t directionX = 9000; //!< 0.01 degrees. 9000 = east = full force on X in polar coordinates
		uint16_t directionY = 0;
		uint16_t startDelay = 0;
		uint16_t samplePeriod = 0;
		uint16_t triggerRepeatInterval = 0;
	};

	// ------------------------------------------------------------------
	// Raw transfers
	// ------------------------------------------------------------------

	/** Sends an output report. The first byte of the data must be the report id */
	void sendOutput(const uint8_t *data, uint16_t len) {
		handler()->hidOut(data[0], HID_REPORT_TYPE_OUTPUT, data, len);
	}

	template <class T>
	void sendOutput(const T &report) {
		uint8_t buffer[sizeof(T)];
		std::memcpy(buffer, &report, sizeof(T));
		sendOutput(buffer, sizeof(T));
	}

	/** Sends a feature report. The data does not contain the report id */
	void setFeature(uint8_t reportId, const uint8_t *data, uint16_t len) {
		handler()->hidOut(reportId, HID_REPORT_TYPE_FEATURE, data, len);
	}

	/** Requests a feature report. Returns the reply without report id */
	std::vector<uint8_t> getFeature(uint8_t reportId, uint16_t maxLen = 64) {
		std::vector<uint8_t> buffer(maxLen, 0);
		uint16_t len = handler()->hidGet(reportId, HID_REPORT_TYPE_FEATURE, buffer.data(), maxLen);
		buffer.resize(len);
		return buffer;
	}

	// ------------------------------------------------------------------
	// Effect management
	// ------------------------------------------------------------------

	/** Create New Effect feature report */
	void createNewEffect(uint8_t effectType, uint16_t byteCount = 0) {
		FFB_CreateNewEffect_Feature_Data_t report;
		report.effectType = effectType;
		report.byteCount = byteCount;
		uint8_t buffer[sizeof(report)];
		std::memcpy(buffer, &report, sizeof(report));
		setFeature(HID_ID_NEWEFREP, buffer, sizeof(report));
	}

	/** PID Block Load feature report */
	BlockLoad blockLoad() {
		std::vector<uint8_t> data = getFeature(HID_ID_BLKLDREP);
		BlockLoad result;
		if (data.size() >= sizeof(FFB_BlockLoad_Feature_Data_t)) {
			FFB_BlockLoad_Feature_Data_t report;
			std::memcpy(&report, data.data(), sizeof(report));
			result = {report.effectBlockIndex, report.loadStatus, report.ramPoolAvailable};
		}
		return result;
	}

	/** PID Pool feature report */
	Pool pool() {
		std::vector<uint8_t> data = getFeature(HID_ID_POOLREP);
		Pool result;
		if (data.size() >= sizeof(FFB_PIDPool_Feature_Data_t)) {
			FFB_PIDPool_Feature_Data_t report;
			std::memcpy(&report, data.data(), sizeof(report));
			result = {report.ramPoolSize, report.maxSimultaneousEffects, report.memoryManagement};
		}
		return result;
	}

	/**
	 * Allocates an effect like the host driver: Create New Effect followed by a Block Load request.
	 * @return the effect block index (1 based) or 0 if the device could not allocate the effect
	 */
	uint8_t allocateEffect(uint8_t effectType) {
		createNewEffect(effectType);
		BlockLoad load = blockLoad();
		return load.loadStatus == LOAD_SUCCESS ? load.effectBlockIndex : 0;
	}

	/** PID Block Free report */
	void blockFree(uint8_t effectBlockIndex) {
		uint8_t report[2] = {HID_ID_BLKFRREP, effectBlockIndex};
		sendOutput(report, sizeof(report));
	}

	// ------------------------------------------------------------------
	// Effect parameters
	// ------------------------------------------------------------------

	/** Set Effect report */
	void setEffect(uint8_t effectBlockIndex, uint8_t effectType, const EffectParams &params) {
		FFB_SetEffect_t report;
		report.reportId = HID_ID_EFFREP;
		report.effectBlockIndex = effectBlockIndex;
		report.effectType = effectType;
		report.duration = params.duration;
		report.triggerRepeatInterval = params.triggerRepeatInterval;
		report.samplePeriod = params.samplePeriod;
		report.startDelay = params.startDelay;
		report.gain = params.gain;
		report.triggerButton = 0;
		report.enableAxis = params.enableAxis;
		report.directionX = params.directionX;
		report.directionY = params.directionY;
		sendOutput(report);
	}

	void setEffect(uint8_t effectBlockIndex, uint8_t effectType) { setEffect(effectBlockIndex, effectType, EffectParams()); }

	/** Set Constant Force report */
	void setConstantForce(uint8_t effectBlockIndex, int16_t magnitude) {
		FFB_SetConstantForce_Data_t report;
		report.reportId = HID_ID_CONSTREP;
		report.effectBlockIndex = effectBlockIndex;
		report.magnitude = magnitude;
		sendOutput(report);
	}

	/** Set Periodic report. Phase in 0.01 degrees, period in ms */
	void setPeriodic(uint8_t effectBlockIndex, uint16_t magnitude, int16_t offset, uint16_t phase, uint32_t period) {
		FFB_SetPeriodic_Data_t report;
		report.reportId = HID_ID_PRIDREP;
		report.effectBlockIndex = effectBlockIndex;
		report.magnitude = magnitude;
		report.offset = offset;
		report.phase = phase;
		report.period = period;
		sendOutput(report);
	}

	/** Set Condition report. parameterBlockOffset selects the axis the condition block applies to */
	void setCondition(uint8_t effectBlockIndex, uint8_t parameterBlockOffset, int16_t cpOffset, int16_t positiveCoefficient, int16_t negativeCoefficient,
					  uint16_t positiveSaturation, uint16_t negativeSaturation, uint16_t deadBand) {
		FFB_SetCondition_Data_t report;
		report.reportId = HID_ID_CONDREP;
		report.effectBlockIndex = effectBlockIndex;
		report.parameterBlockOffset = parameterBlockOffset;
		report.cpOffset = cpOffset;
		report.positiveCoefficient = positiveCoefficient;
		report.negativeCoefficient = negativeCoefficient;
		report.positiveSaturation = positiveSaturation;
		report.negativeSaturation = negativeSaturation;
		report.deadBand = deadBand;
		sendOutput(report);
	}

	/** Set Envelope report. Times in ms */
	void setEnvelope(uint8_t effectBlockIndex, uint16_t attackLevel, uint16_t fadeLevel, uint32_t attackTime, uint32_t fadeTime) {
		FFB_SetEnvelope_Data_t report;
		report.reportId = HID_ID_ENVREP;
		report.effectBlockIndex = effectBlockIndex;
		report.attackLevel = attackLevel;
		report.fadeLevel = fadeLevel;
		report.attackTime = attackTime;
		report.fadeTime = fadeTime;
		sendOutput(report);
	}

	/** Set Ramp Force report */
	void setRamp(uint8_t effectBlockIndex, int16_t startLevel, int16_t endLevel) {
		FFB_SetRamp_Data_t report;
		report.reportId = HID_ID_RAMPREP;
		report.effectBlockIndex = effectBlockIndex;
		report.startLevel = (uint16_t)startLevel;
		report.endLevel = (uint16_t)endLevel;
		sendOutput(report);
	}

	// ------------------------------------------------------------------
	// Control
	// ------------------------------------------------------------------

	/** Effect Operation report */
	void effectOperation(uint8_t effectBlockIndex, uint8_t operation, uint8_t loopCount = 1) {
		FFB_EffOp_Data_t report;
		report.reportId = HID_ID_EFOPREP;
		report.effectBlockIndex = effectBlockIndex;
		report.state = operation;
		report.loopCount = loopCount;
		sendOutput(report);
	}

	void startEffect(uint8_t effectBlockIndex) { effectOperation(effectBlockIndex, OP_START); }
	void stopEffect(uint8_t effectBlockIndex) { effectOperation(effectBlockIndex, OP_STOP); }

	/** PID Device Control report */
	void deviceControl(uint8_t control) {
		uint8_t report[2] = {HID_ID_CTRLREP, control};
		sendOutput(report, sizeof(report));
	}

	/** Device Gain report. 0-255 */
	void deviceGain(uint8_t gain) {
		uint8_t report[2] = {HID_ID_GAINREP, gain};
		sendOutput(report, sizeof(report));
	}

private:
	static UsbHidHandler *handler() { return UsbHidHandler::globalHidHandler; }
};

#endif
