/*
 * CAN.h
 *
 *  Created on: 21.06.2021
 *      Author: Yannick
 */

#ifndef SRC_CAN_H_
#define SRC_CAN_H_
#include "target_constants.h"
#ifdef CANBUS
//#include "CanHandler.h"
#include "main.h"
#include <vector>
#include "semaphore.hpp"
#include <GPIOPin.h>
#include "PersistentStorage.h"
#include "CommandHandler.h"
#include <span>


class CANPort;

#if defined(CAN1)
#define CANTYPE_2B
#define CAN_INITTYPE uint32_t
#define CAN_MSGBUFSIZE 8
#elif defined(FDCAN1)
#define CANTYPE_FDCAN
#define CAN_INITTYPE FDCAN_InitTypeDef&
#define CAN_MSGBUFSIZE 64
const uint8_t DLCtoBytes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64};
#endif

//class CANPort;


class CANPortHardwareConfig{
public:
	struct PresetEntry{// Helper for preset entries
		const CAN_INITTYPE init;
		const uint32_t speed;
		const char* name;
		constexpr PresetEntry(const CAN_INITTYPE init,const uint32_t speed,const char* name):init(init),speed(speed),name(name){}
	};
	constexpr CANPortHardwareConfig(const bool canChangeSpeed,std::span<const PresetEntry> presets_list)
	: canChangeSpeed(canChangeSpeed),presets(presets_list){}

	// Values
	const bool canChangeSpeed;
	constexpr PresetEntry getPreset(uint8_t idx) const {return PresetEntry(presets[std::min<uint8_t>(idx,presets.size())]);}
	constexpr uint32_t speedToPreset(uint32_t speed) const {
		auto it = std::find_if( presets.begin(), presets.end(), [&speed](const PresetEntry &e){return e.speed == speed;});
		return it == presets.end() ? 255 : std::distance(presets.begin(),it);
	}
	constexpr uint32_t presetToSpeed(uint8_t preset) const {return presets[preset].speed;}
	const std::span<const PresetEntry> presets; // Name for listing and init types for setup
};

typedef struct {
	uint32_t id = 0;
	uint32_t length = 0;
	bool extId = false;
	bool rtr = false;
	bool fdcan = false; // Placeholder
//	bool fdcan_rateswitch = false;
} CAN_msg_header_tx;

class CAN_msg_header_rx{
public:
	uint32_t id = 0;
	uint32_t length = 0;
	bool extId = false;
	bool rtr = false;
	uint32_t filter = 0;
	uint32_t timestamp = 0;
	bool fdcan = false;
//	bool fdcan_rateswitch = false;
	CAN_msg_header_rx(){};
#if defined(CANTYPE_2B)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	CAN_msg_header_rx(CAN_RxHeaderTypeDef* hdr) : id(hdr->IDE==CAN_ID_STD ? hdr->StdId : hdr->ExtId),length(hdr->DLC),extId(hdr->IDE==CAN_ID_EXT),rtr(hdr->RTR == CAN_RTR_REMOTE),filter(hdr->FilterMatchIndex),timestamp(hdr->Timestamp),fdcan(false) {};
#elif defined(CANTYPE_FDCAN)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	CAN_msg_header_rx(FDCAN_RxHeaderTypeDef* hdr) : id(hdr->Identifier),length(DLCtoBytes[hdr->DataLength >> 16]),extId(hdr->FDFormat),rtr(hdr->RxFrameType == FDCAN_REMOTE_FRAME),filter(hdr->FilterIndex),timestamp(hdr->RxTimestamp),fdcan(hdr->FDFormat == FDCAN_FD_CAN) {};
#endif
	virtual ~CAN_msg_header_rx(){};
};

typedef struct{
	uint8_t data[CAN_MSGBUFSIZE] = {0};
	//CAN_TxHeaderTypeDef header = {0,0,0,CAN_RTR_DATA,8,(FunctionalState)0};
	CAN_msg_header_tx header;
} CAN_tx_msg;

// Unused?
typedef struct{
	uint8_t data[CAN_MSGBUFSIZE] = {0};
	//CAN_RxHeaderTypeDef header = {0,0,0,0,0,0};
	CAN_msg_header_rx header;
	uint8_t fifo = 0;
	uint32_t size(){return header.length;}
} CAN_rx_msg;

/**
 * New CAN Filter helper common for 2B and FDCAN
 */
typedef struct {
	uint32_t filter_id = 0;
	uint32_t filter_mask = 0;
	uint32_t buffer = 0; // FIFO index or fdcan buffer mode
	bool active = true;
	bool extid = false;
//	bool dedicated_buffer = false; // Not yet supported
} CAN_filter;



//template<class INIT>
class CANPort : public PersistentStorage { //  : public CanHandler if interrupt callbacks needed
	static std::vector<CANPort*> canPorts;
public:

	CANPort(const CANPortHardwareConfig& presets,uint8_t instance = 0);
	virtual ~CANPort();

	// ---- Implement these in the CAN port ---
	/**
	 * Sets up and starts the CAN port
	 */
	virtual bool start() = 0;

	/**
	 * Stops the can port and releases the bus
	 */
	virtual bool stop() = 0;

	virtual bool sendMessage(CAN_tx_msg& msg) = 0;
	virtual bool sendMessage(CAN_msg_header_tx *pHeader, uint8_t aData[],uint32_t *pTxMailbox = nullptr) = 0;
	/**
	 * Stops and clears pending requests if port is stuck
	 */
	virtual void abortTxRequests() = 0;

	virtual int32_t addCanFilter(CAN_filter filter) = 0;
	virtual void removeCanFilter(uint8_t filterId) = 0;

	virtual void setSpeed(uint32_t speed) = 0;
	virtual void setSpeedPreset(uint8_t preset) = 0;
	virtual uint32_t getSpeed() = 0;
	virtual uint8_t getSpeedPreset() = 0;

	virtual void setSilentMode(bool silent) = 0;


	// ---------------------------------------------

	// Common implementations
	virtual int32_t getPortUsers(){return portUsers;}
	virtual void takePort(){
		if(portUsers++ == 0){
			start();
		}
	}
	virtual void freePort(){
		if(portUsers>0){
			portUsers--;
		}

		if(portUsers == 0){
			stop();
		}
	}
	virtual void giveSemaphore(){
		bool isIsr = inIsr();
		BaseType_t taskWoken = 0;
		if(isIsr)
			this->semaphore.GiveFromISR(&taskWoken);
		else
			this->semaphore.Give();
		isWaitingFlag = false;
		portYIELD_FROM_ISR(taskWoken);
	}
	virtual bool takeSemaphore(uint32_t delay = portMAX_DELAY){
		bool isIsr = inIsr();
		BaseType_t taskWoken = 0;
		bool success;
		if(isIsr)
			success = this->semaphore.TakeFromISR(&taskWoken);
		else{
			success = this->semaphore.Take(delay);
		}
		//isTakenFlag = true;
		portYIELD_FROM_ISR(taskWoken);
		return success;
	}
	static CANPort* handleToPort(void* handle);
	virtual uint8_t speedToPreset(uint32_t speed){return presets.speedToPreset(speed);} //!< Gets preset index for a speed
	virtual uint32_t presetToSpeed(uint8_t preset){return presets.presetToSpeed(preset);} //!< Gets speed for a preset index

protected:
	cpp_freertos::BinarySemaphore semaphore = cpp_freertos::BinarySemaphore(true); // Semaphore will block
	cpp_freertos::BinarySemaphore configSem = cpp_freertos::BinarySemaphore(true); // Semaphore will block
	const CANPortHardwareConfig& presets; //!< CAN port presets for different speeds. Hardware dependent
	bool isWaitingFlag = false;
	int32_t portUsers = 0;
	virtual void* getHandle() = 0;
};




#endif /* SRC_CAN_H_ */
#endif
