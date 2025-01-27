#include <algorithm>

#include "SPI.h"
#include "semaphore.hpp"
#include "cppmain.h"
#include "math.h"

static bool operator==(const SPI_InitTypeDef& lhs, const SPI_InitTypeDef& rhs) {
	return memcmp(&lhs,&rhs,sizeof(SPI_InitTypeDef)) == 0;
}



SPIPort::SPIPort(SPI_HandleTypeDef &hspi,const std::vector<OutputPin>& csPins,uint32_t baseclk,bool allowReconfigure)
: hspi{hspi}{
	this->current_device = nullptr;
	this->allowReconfigure = allowReconfigure;
	this->csPins = csPins;
	this->freePins = csPins;
	this->baseclk = baseclk;
}

// ----------------------------------
//CS pins
bool SPIPort::reserveCsPin(OutputPin pin){
	auto it = std::find(freePins.begin(), freePins.end(), pin);
	if(it != freePins.end()){
		freePins.erase(it);
		return true;
	}
	// Pin not found or not free
	return false;
}



bool SPIPort::freeCsPin(OutputPin pin){
	// is in pins
	if(std::find(csPins.begin(),csPins.end(), pin) != csPins.end()){
		// is not in free pins
		if(std::find(freePins.begin(),freePins.end(), pin) == freePins.end()){
			freePins.push_back(pin);
			return true;
		}
	}

	// Pin not found
	return false;
}

bool SPIPort::isPinFree(OutputPin pin){
	return(std::find(freePins.begin(),freePins.end(), pin) != freePins.end());
}

std::vector<OutputPin>& SPIPort::getFreeCsPins(){

	return freePins;
}


std::vector<OutputPin>& SPIPort::getCsPins(){
	return csPins;
}

OutputPin* SPIPort::getCsPin(uint16_t idx){
	if(idx < csPins.size()){
		return &csPins[idx];
	}
	return nullptr;
}

void SPIPort::configurePort(SPI_InitTypeDef* config){
	if(config == nullptr || hspi.Init == *config){
		return; // No need to reconfigure
	}
	hspi.Init = *config;
	HAL_SPI_Init(&hspi);
}

SPI_HandleTypeDef* SPIPort::getPortHandle(){
	return &hspi;
}

// ----------------------------------
/**
 * Transmits using DMA
 * Warning: DMA depends on interrupts not being missed
 */
void SPIPort::transmit_DMA(const uint8_t* buf,uint16_t size,SPIDevice* device){
	device->beginSpiTransfer(this);
	current_device = device; // Will call back this device
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	HAL_SPI_Transmit_DMA(&this->hspi,const_cast<uint8_t*>(buf),size);
	// Request completes in tx complete callback
}

/**
 * Transmit and receive using DMA
 * Warning: DMA depends on interrupts not being missed
 */
void SPIPort::transmitReceive_DMA(const uint8_t* txbuf,uint8_t* rxbuf,uint16_t size,SPIDevice* device){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	current_device = device; // Will call back this device
	HAL_SPI_TransmitReceive_DMA(&this->hspi,const_cast<uint8_t*>(txbuf),rxbuf,size);
	// Request completes in rxtx complete callback
}

/**
 * Receives using DMA. Not recommended
 * Warning: DMA depends on interrupts not being missed
 */
void SPIPort::receive_DMA(uint8_t* buf,uint16_t size,SPIDevice* device){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	current_device = device;
	HAL_SPI_Receive_DMA(&this->hspi,buf,size);
	// Request completes in rx complete callback
}


void SPIPort::transmit_IT(const uint8_t* buf,uint16_t size,SPIDevice* device){
	device->beginSpiTransfer(this);
	current_device = device; // Will call back this device
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	HAL_SPI_Transmit_IT(&this->hspi,const_cast<uint8_t*>(buf),size);
	// Request completes in tx complete callback
}

void SPIPort::transmitReceive_IT(const uint8_t* txbuf,uint8_t* rxbuf,uint16_t size,SPIDevice* device){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	current_device = device; // Will call back this device
	HAL_SPI_TransmitReceive_IT(&this->hspi,const_cast<uint8_t*>(txbuf),rxbuf,size);
	// Request completes in rxtx complete callback
}

void SPIPort::receive_IT(uint8_t* buf,uint16_t size,SPIDevice* device){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	current_device = device;
	HAL_SPI_Receive_IT(&this->hspi,buf,size);
	// Request completes in rx complete callback
}

void SPIPort::transmit(const uint8_t* buf,uint16_t size,SPIDevice* device,uint16_t timeout){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	HAL_SPI_Transmit(&this->hspi,const_cast<uint8_t*>(buf),size,timeout);
	device->endSpiTransfer(this);
}

void SPIPort::receive(uint8_t* buf,uint16_t size,SPIDevice* device,int16_t timeout){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	HAL_SPI_Receive(&this->hspi,buf,size,timeout);
	device->endSpiTransfer(this);
}

void SPIPort::transmitReceive(const uint8_t* txbuf,uint8_t* rxbuf,uint16_t size,SPIDevice* device,uint16_t timeout){
	device->beginSpiTransfer(this);
	if(!takenExclusive && this->allowReconfigure){
		this->configurePort(&device->getSpiConfig()->peripheral);
	}
	HAL_SPI_TransmitReceive(&this->hspi,const_cast<uint8_t*>(txbuf),rxbuf,size,timeout);
	device->endSpiTransfer(this);
}
// --------------------------------

void SPIPort::takeSemaphore(){
	bool isIsr = inIsr();

	if(isIsr){
		BaseType_t taskWoken = 0;
		this->semaphore.TakeFromISR(&taskWoken);
		portYIELD_FROM_ISR(taskWoken);
	}else{
		this->semaphore.Take();
	}
	isTakenFlag = true;
}

void SPIPort::giveSemaphore(){
	bool isIsr = inIsr();
	isTakenFlag = false;
	if(isIsr){
		BaseType_t taskWoken = 0;
		this->semaphore.GiveFromISR(&taskWoken);
		portYIELD_FROM_ISR(taskWoken);
	}else{
		this->semaphore.Give();
	}

}

bool SPIPort::isTaken(){
	return isTakenFlag;
}

void SPIPort::takeExclusive(bool exclusive){
	takenExclusive = exclusive;
}

bool SPIPort::hasFreePins(){
	return !takenExclusive && this->getCsPins().size() > 0;
}

/**
 * Returns the peripheral base clock frequency
 */
uint32_t SPIPort::getBaseClk(){
	return this->baseclk;
}

// interrupt callbacks
void SPIPort::SpiTxCplt(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}
	current_device->endSpiTransfer(this);
	current_device->spiTxCompleted(this);

	current_device = nullptr;

}
void SPIPort::SpiRxCplt(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}
	current_device->endSpiTransfer(this);
	current_device->spiRxCompleted(this);

	current_device = nullptr;
}
void SPIPort::SpiTxRxCplt(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}
	current_device->endSpiTransfer(this);
	current_device->spiTxRxCompleted(this);

	current_device = nullptr;
}

void SPIPort::SpiError(SPI_HandleTypeDef *hspi) {
	if (current_device == nullptr) {
		return;
	}

	if (hspi->Instance != this->hspi.Instance) {
		return;
	}
	current_device->endSpiTransfer(this);
	current_device->spiRequestError(this);

	current_device = nullptr;
}

/**
 * Calculates the closest possible clock achievable with the current base clock and prescalers
 * Returns a pair of {prescaler,actual_clock}
 */
std::pair<uint32_t,float> SPIPort::getClosestPrescaler(float clock){
	std::vector<std::pair<uint32_t,float>> distances;
#if defined(SPI_BAUDRATEPRESCALER_2)
	distances.push_back({SPI_BAUDRATEPRESCALER_2,(baseclk/2.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_4)
	distances.push_back({SPI_BAUDRATEPRESCALER_4,(baseclk/4.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_8)
	distances.push_back({SPI_BAUDRATEPRESCALER_8,(baseclk/8.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_16)
	distances.push_back({SPI_BAUDRATEPRESCALER_16,(baseclk/16.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_32)
	distances.push_back({SPI_BAUDRATEPRESCALER_32,(baseclk/32.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_64)
	distances.push_back({SPI_BAUDRATEPRESCALER_64,(baseclk/64.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_128)
	distances.push_back({SPI_BAUDRATEPRESCALER_128,(baseclk/128.0)});
#endif
#if defined(SPI_BAUDRATEPRESCALER_256)
	distances.push_back({SPI_BAUDRATEPRESCALER_256,(baseclk/256.0)});
#endif
	// Calc distances
	std::pair<uint32_t,float> bestVal = distances[0];
	float bestDist = INFINITY;
	for(auto& val : distances){
		if(std::abs(clock-val.second) < bestDist){
			bestDist = abs(clock-val.second);
			bestVal = val;
		}
	}
	return bestVal;
}

SPIDevice::SPIDevice(SPIPort& port,SPIConfig& spiConfig) : spiPort{port},spiConfig{spiConfig}{
	spiPort.reserveCsPin(spiConfig.cs);
}
SPIDevice::SPIDevice(SPIPort& port,OutputPin csPin) : spiPort{port},spiConfig{csPin}{
	this->spiConfig.cs = csPin;
	spiPort.reserveCsPin(spiConfig.cs);
}
SPIDevice::~SPIDevice() {
	spiPort.freeCsPin(spiConfig.cs);
}

bool SPIDevice::updateCSPin(OutputPin& csPin){
	if(csPin == spiConfig.cs){
		return true;
	}
	if(!spiPort.isPinFree(csPin)){
		return false;
	}

	if(!spiPort.freeCsPin(spiConfig.cs)){
		return false;
	}
	spiConfig.cs = csPin;
	return spiPort.reserveCsPin(csPin);
}

/*
 * Called before the spi transfer starts.
 * Can be used to take the semaphore and set CS pins by default
 */
void SPIDevice::beginSpiTransfer(SPIPort* port){
	port->takeSemaphore();
	assertChipSelect();
}

/*
 * Called after a transfer is finished
 * Gives a semaphore back and resets the CS pin
 */
void SPIDevice::endSpiTransfer(SPIPort* port){
	clearChipSelect();
	port->giveSemaphore();
}

void SPIDevice::assertChipSelect() {
	spiConfig.cs.write(!spiConfig.cspol);
}

void SPIDevice::clearChipSelect() {
	spiConfig.cs.write(spiConfig.cspol);
}
