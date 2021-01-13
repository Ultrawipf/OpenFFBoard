/*
 * SPI.h
 *
 *  Created on: 27.12.2020
 *      Author: willson556
 */

#ifndef SPI_H_
#define SPI_H_

#include <vector>
#include <atomic>

#include "stm32f4xx_hal.h"

#include "SpiHandler.h"
#include "OutputPin.h"

struct SPIConfig {
	SPIConfig(OutputPin cs)
		:cs{cs}, cspol{false} {
		peripheral.Mode = SPI_MODE_MASTER;
		peripheral.Direction = SPI_DIRECTION_2LINES;
		peripheral.DataSize = SPI_DATASIZE_8BIT;
		peripheral.CLKPolarity = SPI_POLARITY_LOW;
		peripheral.CLKPhase = SPI_PHASE_1EDGE;
		peripheral.NSS = SPI_NSS_SOFT;
		peripheral.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
		peripheral.FirstBit = SPI_FIRSTBIT_MSB;
		peripheral.TIMode = SPI_TIMODE_DISABLE;
		peripheral.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
		peripheral.CRCPolynomial = 10;
	}

	OutputPin cs;

	/// CSPOL=true === active low
	bool cspol;
	SPI_InitTypeDef peripheral;
};

class SPIDevice;

class SPIPort: public SpiHandler {
public:
	SPIPort(SPI_HandleTypeDef &hspi)
		: hspi{hspi} {}

	class Pipe {
	public:
		friend SPIPort; // Only allow the port to create a pipe.

		 /// Begin a transmission.
		void beginTx(uint8_t *data, uint16_t size);

		/// Begin a receive.
		void beginRx(uint8_t *data, uint16_t size);

		/// Begin a transmit and receive.
		void beginTxRx(uint8_t *txData, uint8_t *rxData, uint16_t size);

		/// Close the pipe. This allows other devices to use the port.
		void close();
	private:
		Pipe(SPIPort& port)
			: port{port} {}

		SPIPort& port;
	};

	/// Devices should be added during system initialization.
	/// Thread-safety is not guaranteed if the SPI port is in use
	/// when a device is added.
	/// Also, it's recommended that SPIDevice::attachToPort() be used rather
	/// than calling this directly so that the SPIDevice is automatically
	/// removed if deleted.
	void addDevice(SPIDevice& device);
	void removeDevice(SPIDevice& device);

	/// Call from the main loop to check for pending requests.
	/// Will also be called when a request completes.
	void process();

	void SpiTxCplt(SPI_HandleTypeDef *hspi) override;
	void SpiRxCplt(SPI_HandleTypeDef *hspi) override;
	void SpiTxRxCplt(SPI_HandleTypeDef *hspi) override;
	void SpiError(SPI_HandleTypeDef *hspi) override;

private:
	SPI_HandleTypeDef &hspi;
	std::vector<SPIDevice*> devices;
	std::size_t next_device_index{0};
	std::atomic_flag port_busy{false};
	Pipe pipe{*this};
	SPIDevice* current_device{nullptr};
	bool need_to_reconfigure_peripheral{false};
};

class SPIDevice {
public:
	virtual ~SPIDevice();

	virtual const SPIConfig& getConfig() const = 0;
	virtual void beginRequest(SPIPort::Pipe& pipe) = 0;

	virtual void txCompleted(SPIPort::Pipe& pipe) { completeRequest(pipe); }
	virtual void rxCompleted(SPIPort::Pipe& pipe) { completeRequest(pipe); }
	virtual void txRxCompleted(SPIPort::Pipe& pipe) { completeRequest(pipe); }
	virtual void requestError(SPIPort::Pipe& pipe) { completeRequest(pipe); }

	bool requestPending() const { return request_port; }
protected:
	void requestPort() { request_port = true; }
	void completeRequest(SPIPort::Pipe& pipe) {
		request_port = false;
		pipe.close();
	}

	void attachToPort(SPIPort &port);
private:
	volatile bool request_port{false};
	SPIPort* port{nullptr};
};

#endif
