/*
 * FfbFixture.h
 *
 * Fixture for force feedback tests: an effects calculator, the HID PID handler,
 * a simulated PID host and two axis test doubles.
 */
#ifndef HOSTTEST_FFBFIXTURE_H_
#define HOSTTEST_FFBFIXTURE_H_

#include "FirmwareFixture.h"
#include "PidHost.h"

#include "Axis.h" // Test double from fakes/
#include "EffectsCalculator.h"
#include "HidFFB.h"

#include <memory>
#include <vector>

struct FfbFixture : FirmwareFixture {
	explicit FfbFixture(uint8_t axisCount = 2) : ffb(ec, axisCount) {
		for (uint8_t i = 0; i < axisCount; i++) {
			axes.push_back(std::make_unique<Axis>());
		}
	}

	~FfbFixture() override {
		UsbHidHandler::globalHidHandler = nullptr; // Not done by the firmware
	}

	/** Effect storage of an effect block index (1 based like in the HID reports) */
	FFB_Effect &effect(uint8_t effectBlockIndex) { return ec->effects.at(effectBlockIndex - 1); }

	/** Runs one cycle of the effect calculation like the FFB update loop does */
	void calculate() { ec->calculateEffects(axes); }

	/** Advances time by 1ms steps and calculates the effects every step like the 1kHz update loop */
	void run(uint32_t ms) {
		for (uint32_t i = 0; i < ms; i++) {
			HostPlatform::advanceMs(1);
			calculate();
		}
	}

	/** Calculates the effects and returns the torque passed to an axis */
	int32_t torque(uint8_t axis = 0) {
		calculate();
		return axes.at(axis)->ffbEffectTorque;
	}

	metric_t &metrics(uint8_t axis = 0) { return axes.at(axis)->metrics; }

	/**
	 * Creates, configures and starts an effect like a game does.
	 * @return effect block index
	 */
	uint8_t startEffect(uint8_t type, const PidHost::EffectParams &params = PidHost::EffectParams()) {
		uint8_t index = host.allocateEffect(type);
		host.setEffect(index, type, params);
		host.startEffect(index);
		return index;
	}

	/** Sends a command to the effects calculator command handler */
	CommandStatus fxCommand(EffectsCalculator_commands cmd, CMDtype type, int64_t val = 0, std::vector<CommandReply> *replies = nullptr, int64_t adr = 0) {
		std::vector<CommandReply> localReplies;
		ParsedCommand parsed;
		parsed.cmdId = static_cast<uint32_t>(cmd);
		parsed.type = type;
		parsed.val = val;
		parsed.adr = adr;
		parsed.target = ec.get();
		return ec->command(parsed, replies ? *replies : localReplies);
	}

	std::shared_ptr<EffectsCalculator> ec = std::make_shared<EffectsCalculator>();
	HidFFB ffb;
	PidHost host;
	std::vector<std::unique_ptr<Axis>> axes;
};

struct FfbFixture1Axis : FfbFixture {
	FfbFixture1Axis() : FfbFixture(1) {}
};

#endif
