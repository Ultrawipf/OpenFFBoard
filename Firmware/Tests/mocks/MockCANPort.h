/*
 * MockCANPort.h
 *
 * CAN port implementing the CANPort class interface without hardware.
 * Records transmitted frames and filters. Received frames are injected by the
 * test and dispatched to all registered CanHandlers like the CAN RX interrupt does.
 */
#ifndef HOSTTEST_MOCKCANPORT_H_
#define HOSTTEST_MOCKCANPORT_H_

#include "CAN.h"
#include "CanHandler.h"
#include "HostPlatform.h"
#include <array>
#include <cstring>
#include <vector>

class MockCANPort : public CANPort {
public:
	MockCANPort() : CANPort(config()) {}

	// --- CANPort interface ---
	bool start() override {
		started = true;
		startCalls++;
		return true;
	}
	bool stop() override {
		started = false;
		stopCalls++;
		return true;
	}
	bool sendMessage(CAN_tx_msg &msg) override {
		if (!sendResult) {
			return false;
		}
		sent.push_back(msg);
		return true;
	}
	bool sendMessage(CAN_msg_header_tx *pHeader, uint8_t aData[], uint32_t * = nullptr) override {
		CAN_tx_msg msg;
		msg.header = *pHeader;
		std::memcpy(msg.data, aData, std::min<uint32_t>(pHeader->length, CAN_MSGBUFSIZE));
		return sendMessage(msg);
	}
	void abortTxRequests() override {}
	int32_t addCanFilter(CAN_filter filter) override {
		filters.push_back(filter);
		return filters.size() - 1;
	}
	void removeCanFilter(uint8_t filterId) override {
		if (filterId < filters.size()) {
			filters[filterId].active = false;
		}
	}
	void setSpeed(uint32_t speed) override { this->speed = speed; }
	void setSpeedPreset(uint8_t preset) override { speed = presetToSpeed(preset); }
	uint32_t getSpeed() override { return speed; }
	uint8_t getSpeedPreset() override { return speedToPreset(speed); }
	void setSilentMode(bool silent) override { this->silent = silent; }

	// --- Test helpers ---
	/**
	 * Simulates a received frame. Calls canRxPendCallback of all CAN handlers in interrupt context.
	 * Frames not matching an active filter are dropped like the hardware would do.
	 * @return true if the frame passed a filter
	 */
	bool receive(uint32_t id, const uint8_t *data, uint32_t len, bool rtr = false, bool extId = false) {
		if (!passesFilter(id, extId)) {
			return false;
		}
		CAN_rx_msg msg;
		msg.header.id = id;
		msg.header.length = len;
		msg.header.rtr = rtr;
		msg.header.extId = extId;
		std::memcpy(msg.data, data, std::min<uint32_t>(len, CAN_MSGBUFSIZE));
		HostPlatform::IsrContext isr;
		for (CanHandler *handler : CanHandler::getCANHandlers()) {
			handler->canRxPendCallback(this, msg);
		}
		return true;
	}

	bool receive(uint32_t id, const std::array<uint8_t, 8> &data) { return receive(id, data.data(), 8); }

	bool passesFilter(uint32_t id, bool extId = false) const {
		for (const CAN_filter &f : filters) {
			if (f.active && f.extid == extId && (id & f.filter_mask) == (f.filter_id & f.filter_mask)) {
				return true;
			}
		}
		return false;
	}

	size_t activeFilters() const {
		size_t count = 0;
		for (const CAN_filter &f : filters) {
			count += f.active ? 1 : 0;
		}
		return count;
	}

	// --- State ---
	std::vector<CAN_tx_msg> sent;
	std::vector<CAN_filter> filters;
	bool started = false;
	bool silent = false;
	bool sendResult = true;
	uint32_t speed = 500000;
	uint32_t startCalls = 0;
	uint32_t stopCalls = 0;

protected:
	void *getHandle() override { return this; }

private:
	static const CANPortHardwareConfig &config() {
		static constexpr std::array<CANPortHardwareConfig::PresetEntry, 2> presets{{{0, 500000, "500k"}, {1, 1000000, "1000k"}}};
		static const CANPortHardwareConfig cfg(true, presets);
		return cfg;
	}
};

#endif
