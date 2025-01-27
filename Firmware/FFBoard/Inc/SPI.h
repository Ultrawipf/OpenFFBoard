/*
 * SPI.h
 *
 *  Created on: 27.12.2020
 *      Author: willson556
 */

#ifndef SPI_H_
#define SPI_H_

#include <GPIOPin.h>
#include <vector>
#include "cppmain.h"

#include "SpiHandler.h"
#include "semaphore.hpp"

struct SPIConfig {
	SPIConfig(OutputPin cs,bool cspol = true)
		:cs{cs}, cspol{cspol}{
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
	SPIPort(SPI_HandleTypeDef &hspi,const std::vector<OutputPin>& csPins,uint32_t baseclk,bool allowReconfigure = true);

	void takeSemaphore(); // Call before accessing this port
	void giveSemaphore(); // Call when finished using this port

	void configurePort(SPI_InitTypeDef* config); // Reconfigures the spi port

	std::vector<OutputPin>& getCsPins();
	OutputPin* getCsPin(uint16_t idx);
	std::vector<OutputPin>& getFreeCsPins(); // Returns a vector of cs pins assigned to this port that are not reserved
	bool reserveCsPin(OutputPin pin); // Signals that this pin is now in use
	bool freeCsPin(OutputPin pin); // Signals that this cs pin is not used anymore. Call this in the destructor
	bool isPinFree(OutputPin pin); // Returns if a pin is assigned to this port and not in use

	void transmit_DMA(const uint8_t* buf,uint16_t size,SPIDevice* device);
	void transmitReceive_DMA(const uint8_t* txbuf,uint8_t* rxbuf,uint16_t size,SPIDevice* device);
	void receive_DMA(uint8_t* buf,uint16_t size,SPIDevice* device);
	void transmit_IT(const uint8_t* buf,uint16_t size,SPIDevice* device);
	void transmitReceive_IT(const uint8_t* txbuf,uint8_t* rxbuf,uint16_t size,SPIDevice* device);
	void receive_IT(uint8_t* buf,uint16_t size,SPIDevice* device);
	void transmit(const uint8_t* buf,uint16_t size,SPIDevice* device,uint16_t timeout);
	void receive(uint8_t* buf,uint16_t size,SPIDevice* device,int16_t timeout);
	void transmitReceive(const uint8_t* txbuf,uint8_t* rxbuf,uint16_t size,SPIDevice* device,uint16_t timeout);

	void SpiTxCplt(SPI_HandleTypeDef *hspi) override;
	void SpiRxCplt(SPI_HandleTypeDef *hspi) override;
	void SpiTxRxCplt(SPI_HandleTypeDef *hspi) override;
	void SpiError(SPI_HandleTypeDef *hspi) override;

	bool isTaken(); // Returns true if semaphore was taken by another task

	void takeExclusive(bool exclusive);
	bool hasFreePins();

	uint32_t getBaseClk();
	std::pair<uint32_t,float> getClosestPrescaler(float clock);

	SPI_HandleTypeDef* getPortHandle();

private:
	void beginTransfer(SPIConfig* config);
	void endTransfer(SPIConfig* config);

	SPI_HandleTypeDef &hspi;
	SPIDevice* current_device = nullptr;
	std::vector<OutputPin> csPins; // cs pins and bool true if pin is reserved
	std::vector<OutputPin> freePins;

	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true);
	bool allowReconfigure = false; // Allow reconfiguration at runtime. Can reduce performance a lot
	volatile bool isTakenFlag = false;
	bool takenExclusive = false;
	uint32_t baseclk;
};

class SPIDevice {
public:
	SPIDevice(SPIPort& port,OutputPin csPin);
	SPIDevice(SPIPort& port,SPIConfig& spiConfig);
	virtual ~SPIDevice();

//	virtual const SPIConfig& getConfig(SPIPort& port);
	void assertChipSelect();
	void clearChipSelect();

	virtual bool updateCSPin(OutputPin& csPin);

	virtual void spiTxCompleted(SPIPort* port) { }
	virtual void spiRxCompleted(SPIPort* port) { }
	virtual void spiTxRxCompleted(SPIPort* port) { }
	virtual void spiRequestError(SPIPort* port) { }

	virtual void beginSpiTransfer(SPIPort* port);
	virtual void endSpiTransfer(SPIPort* port);

	virtual SPIConfig* getSpiConfig(){return &this->spiConfig;}

protected:
	virtual void setSpiConfig(SPIConfig config){spiConfig = config;}
	SPIPort& spiPort;
	SPIConfig spiConfig;
};

#endif
