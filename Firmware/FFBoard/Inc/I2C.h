/*
 * I2C.h
 *
 *  Created on: 10.03.2022
 *      Author: Yannick
 */

#ifndef SRC_I2C_H_
#define SRC_I2C_H_

#include "constants.h"
#ifdef I2CBUS
#include "semaphore.hpp"
#include "cppmain.h"
#include "I2CHandler.h"
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include <array>
//#define I2C_COMMANDS_DISABLED_IF_NOT_USED
#include <span>
/**
 * Helper class for hardware dependent config presets
 */
class I2CPort;
class I2CPortHardwareConfig{
	friend I2CPort;
public:
	struct I2CPortHardwareConfig_preset{// Helper for preset entries
		I2C_InitTypeDef init;
		const char* name;
	};
	constexpr I2CPortHardwareConfig(const bool canChangeSpeed,std::span<const I2CPortHardwareConfig_preset> presets_list)
	: canChangeSpeed(canChangeSpeed),presets(presets_list){}

	// Values
	const bool canChangeSpeed;
	I2CPortHardwareConfig_preset getPreset(uint8_t idx) const {return I2CPortHardwareConfig_preset(presets[std::min<uint8_t>(idx,presets.size())]);}
protected:
	const std::span<const I2CPortHardwareConfig_preset> presets; // Name for listing and init types for setup
};

class I2CDevice;
class I2CPort : public I2CHandler, public CommandHandler, public PersistentStorage{
	enum class I2CPort_commands : uint32_t {speed};
public:
	I2CPort(I2C_HandleTypeDef &hi2c,const I2CPortHardwareConfig& presets,uint8_t instance = 0);
	virtual ~I2CPort();


	void configurePort(I2C_InitTypeDef* config); // Reconfigures the i2c port
	void resetPort();

	void takePort(I2CDevice* device);
	void freePort(I2CDevice* device);
	int32_t getPortUsers(){return portUsers;}

	void setSpeedPreset(uint8_t preset);
	uint8_t getSpeedPreset();

	bool transmitMaster(I2CDevice* device, const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr = true);
	bool transmitMasterDMA(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr = true);
	bool transmitMasterIT(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr = true);

	bool receiveMaster(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr = true);
	bool receiveMasterDMA(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr = true);
	bool receiveMasterIT(I2CDevice* device,const uint16_t addr,uint8_t* pData,const uint16_t size,bool shiftAddr = true);

	bool writeMem(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr = true);
	bool readMem(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,const uint32_t timeout,bool shiftAddr = true);
	bool readMemIT(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,bool shiftAddr = true);
	bool writeMemIT(I2CDevice* device,const uint16_t devAddr,const uint16_t memAddr,const uint16_t memAddSize,uint8_t* pData,const uint16_t size,bool shiftAddr = true);

	bool isDeviceReady(I2CDevice* device,const uint16_t devAddr,const uint32_t trials,uint32_t timeout,bool shiftAddr = true);

	void takeSemaphore();
	void giveSemaphore();
	bool isTaken();

	void I2cTxCplt(I2C_HandleTypeDef *hi2c);
	void I2cRxCplt(I2C_HandleTypeDef *hi2c);
	void I2cError(I2C_HandleTypeDef *hi2c);

	// Config
	enum class CanPort_commands : uint32_t {speed};
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	void saveFlash();
	void restoreFlash();
	static ClassIdentifier info;
	const ClassIdentifier getInfo(){return this->info;}
	const ClassType getClassType() override {return ClassType::Port;};


private:
	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true);
	//volatile bool isTakenFlag = false;
	I2C_HandleTypeDef& hi2c;
	I2CDevice* currentDevice = nullptr;
	int32_t portUsers = 0;
	uint8_t speedPreset = 0;
	I2C_InitTypeDef config;
	const I2CPortHardwareConfig& presets;
};


class I2CDevice{
public:
	I2CDevice();
	//I2CDevice(I2CPort& port);
	virtual ~I2CDevice();

	virtual void startI2CTransfer(I2CPort* port);
	virtual void endI2CTransfer(I2CPort* port);
	virtual void i2cTxCompleted(I2CPort* port);
	virtual void i2cRxCompleted(I2CPort* port);
	virtual void i2cError(I2CPort* port);

//protected:
//	I2CDevice* port = nullptr;
};
#endif

#endif /* SRC_I2C_H_ */
