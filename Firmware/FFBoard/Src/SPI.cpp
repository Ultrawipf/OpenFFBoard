#include <algorithm>

#include "SPI.h"

static bool operator==(const SPI_InitTypeDef& lhs, const SPI_InitTypeDef& rhs) {
	return lhs.BaudRatePrescaler == rhs.BaudRatePrescaler
		&& lhs.CLKPhase == rhs.CLKPhase
		&& lhs.CLKPolarity == rhs.CLKPolarity
		&& lhs.CRCCalculation == rhs.CRCCalculation
		&& lhs.CRCPolynomial == rhs.CRCPolynomial
		&& lhs.DataSize == rhs.DataSize
		&& lhs.Direction == rhs.Direction
		&& lhs.FirstBit == rhs.FirstBit
		&& lhs.Mode == rhs.Mode
		&& lhs.NSS == rhs.NSS
		&& lhs.TIMode == rhs.TIMode;
}

static void assertChipSelect(const SPIConfig& config) {
	config.cs.write(!config.cspol);
}

static void clearChipSelect(const SPIConfig& config) {
	config.cs.write(config.cspol);
}

void SPIPort::process() {
	if (port_busy.test_and_set(std::memory_order_acquire)) {
		return;
	}

	for (std::size_t i=0; i < devices.size(); ++i) {
		next_device_index %= devices.size();
		auto &device = *devices[next_device_index];
		++next_device_index;

		if (device.requestPending()) {
			current_device = &device;
			auto &config{device.getConfig()};

			if (need_to_reconfigure_peripheral) {
				hspi.Init = config.peripheral;
				HAL_SPI_Init(&hspi);
			}

			assertChipSelect(config);
			device.beginRequest(pipe);
			return;
		}
	}

	// No requests found, port is free.
	port_busy.clear(std::memory_order_release);
}

void SPIPort::addDevice(SPIDevice& device) {
	if (std::find(devices.begin(), devices.end(), &device) == devices.end()) {
		devices.push_back(&device);

		if (!need_to_reconfigure_peripheral) {
			if (devices.size() == 1) {
				// First device, configure port for this device.
				auto &config{device.getConfig()};
				hspi.Init = config.peripheral;
				HAL_SPI_Init(&hspi);
			} else {
				// Not the first device, see if peripheral config matches.
				// If it does, we can avoid reconfiguring the peripheral.
				need_to_reconfigure_peripheral = devices.front()->getConfig().peripheral == device.getConfig().peripheral;
			}
		}
	}

	clearChipSelect(device.getConfig());
}

void SPIPort::removeDevice(SPIDevice& device) {
	devices.erase(std::remove(devices.begin(), devices.end(), &device), devices.end());
}

void SPIPort::SpiTxCplt(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}

	current_device->txCompleted(pipe);
}
void SPIPort::SpiRxCplt(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}

	current_device->rxCompleted(pipe);
}
void SPIPort::SpiTxRxCplt(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}

	current_device->txRxCompleted(pipe);
}

void SPIPort::SpiError(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}

	current_device->requestError(pipe);
}

void SPIPort::Pipe::beginTx(uint8_t *data, uint16_t size) {
	HAL_SPI_Transmit_DMA(&port.hspi, data, size);
}

void SPIPort::Pipe::beginRx(uint8_t *data, uint16_t size) {
	HAL_SPI_Receive_DMA(&port.hspi, data, size);
}

void SPIPort::Pipe::beginTxRx(uint8_t *txData, uint8_t *rxData, uint16_t size) {
	HAL_SPI_TransmitReceive_DMA(&port.hspi, txData, rxData, size);
}

void SPIPort::Pipe::close() {
	clearChipSelect(port.current_device->getConfig());

	port.current_device = nullptr;
	port.port_busy.clear(std::memory_order_release);
	port.process();
}

void SPIDevice::attachToPort(SPIPort &port) {
	port.addDevice(*this);
	this->port = &port;
}

SPIDevice::~SPIDevice() {
	if (port != nullptr) {
		port->removeDevice(*this);
	}
}
