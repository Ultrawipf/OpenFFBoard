/*
 * host_usb.cpp
 *
 * Recording fakes of the TinyUSB device API used by the firmware.
 * See support/HostUsb.h for the test control interface.
 */
#include "HostUsb.h"
#include "tusb.h"

#include <algorithm>

namespace {
HostUsb::State usbState;
}

namespace HostUsb {
State &state() { return usbState; }
void reset() { usbState = State(); }
} // namespace HostUsb

extern "C" {

bool tud_mounted(void) { return usbState.mounted; }
bool tud_suspended(void) { return false; }
bool tud_connected(void) { return usbState.mounted; }

// ---------------- CDC ----------------
bool tud_cdc_n_connected(uint8_t) { return usbState.cdcConnected; }
uint32_t tud_cdc_n_available(uint8_t) { return 0; }
uint32_t tud_cdc_n_read(uint8_t, void *, uint32_t) { return 0; }
uint32_t tud_cdc_n_write_available(uint8_t) { return usbState.cdcWriteAvailable; }

uint32_t tud_cdc_n_write(uint8_t, void const *buffer, uint32_t bufsize) {
	uint32_t len = std::min(bufsize, usbState.cdcWriteAvailable);
	usbState.cdcTx.append(static_cast<const char *>(buffer), len);
	usbState.cdcWrites++;
	return len;
}

uint32_t tud_cdc_n_write_flush(uint8_t) {
	usbState.cdcFlushes++;
	return 0;
}

// ---------------- HID ----------------
bool tud_hid_n_ready(uint8_t) { return usbState.hidReady; }

bool tud_hid_n_report(uint8_t instance, uint8_t report_id, void const *report, uint16_t len) {
	if (!usbState.hidReportResult) {
		return false;
	}
	const uint8_t *data = static_cast<const uint8_t *>(report);
	usbState.hidReports.push_back({instance, report_id, std::vector<uint8_t>(data, data + len)});
	return true;
}

} // extern "C"
