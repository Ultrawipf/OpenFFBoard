/*
 * VescCAN.h
 *
 *  Created on: Aug 08, 2021
 *      Author: Vincent Manoukian (manoukianvg@gmail.com)
 */

#ifndef USEREXTENSIONS_SRC_VESCCAN_H_
#define USEREXTENSIONS_SRC_VESCCAN_H_
#include "MotorDriver.h"
#include "cpp_target_config.h"
#include "CAN.h"
#include "Encoder.h"
#include "thread.hpp"
#include "CanHandler.h"
#include "CommandHandler.h"
#include "PersistentStorage.h"
#include <math.h>

#ifdef VESC
#define VESC_THREAD_MEM 512
#define VESC_THREAD_PRIO 25 // Must be higher than main thread
#define BUFFER_RX_SIZE	32

#define FW_MIN_RELEASE ((5 << 16) | (3 << 8) | 51)

enum class VescState : uint32_t {
	VESC_STATE_UNKNOWN = 0,
	VESC_STATE_INCOMPATIBLE = 1,
	VESC_STATE_PONG = 2,
	VESC_STATE_COMPATIBLE = 3,
	VESC_STATE_READY = 4,
	VESC_STATE_ERROR = 5

};

enum class VescCANMsg : uint8_t {
	CAN_PACKET_FILL_RX_BUFFER = 5,
	CAN_PACKET_FILL_RX_BUFFER_LONG = 6,
	CAN_PACKET_PROCESS_RX_BUFFER = 7,
	CAN_PACKET_PROCESS_SHORT_BUFFER = 8,
	CAN_PACKET_SET_CURRENT_REL = 10,
	CAN_PACKET_PING = 17,
	CAN_PACKET_PONG = 18,
	CAN_PACKET_POLL_ROTOR_POS = 56
};

enum class VescCmd : uint8_t {
	COMM_FW_VERSION = 0,
	COMM_ROTOR_POSITION = 22,
	COMM_GET_VALUES_SELECTIVE = 50
};
enum class VescEncoderMode : uint8_t {
	ENCODER_OFF = 0, ENCODER_ON = 3
};

enum class VescCAN_commands : uint32_t {
	offbcanid,vesccanid,canspd,errorflags,vescstate,voltage,encrate,pos,torque,forcePosRead,useEncoder,offset
};

struct VescFlashAddrs{
	uint16_t canId = 	ADR_VESC1_CANID;
	uint16_t data = 	ADR_VESC1_DATA;
	uint16_t offset = 	ADR_VESC1_OFFSET;
};

class VescCAN: public MotorDriver,
		public PersistentStorage,
		public Encoder,
		public CanHandler,
		public CommandHandler,
		cpp_freertos::Thread {
public:
	VescCAN(uint8_t address);
	virtual ~VescCAN();
	void setAddress(uint8_t address);

	// MotorDriver impl
	void turn(int16_t power) override;
	void stopMotor() override;
	void startMotor() override;
	Encoder* getEncoder() override;
	bool hasIntegratedEncoder() override;
	bool motorReady() override;

	// PersistentStorage impl
	void saveFlash() override;
	void restoreFlash() override;

	// Encoder impl
	EncoderType getType() { return EncoderType::absolute;}
	float getPos_f() override;
	uint32_t getCpr() override;
	int32_t getPos() override;
	void setPos(int32_t pos) override;

	// CanHandler impl
	void canRxPendCallback(CAN_HandleTypeDef *hcan, uint8_t *rxBuf,
			CAN_RxHeaderTypeDef *rxHeader, uint32_t fifo) override;

	// CommandHandler impl
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands(); // Function reserved to register commands in the command list.
	virtual std::string getHelpstring() {
		return "VESC CAN interface";
	};

	// Thread impl
	void Run();

private:

	// Vesc interface and motor state
	VescFlashAddrs flashAddrs;
	volatile VescState state = VescState::VESC_STATE_UNKNOWN;
	volatile uint8_t vescErrorFlag;
	bool activeMotor = false;
	float lastTorque;
	float voltage;

	// Encoder section

	bool useEncoder = true;
	float lastPos = 0;				// last motor position known in "turn unit"
	float mtPos = 0;				// number of turn known
	float prevPos360 = 0;			// previous position in [0-360] position
	float posOffset = 0;			// used to center the wheel
	volatile uint32_t encCount = 0;			// count incoming message
	volatile uint32_t encStartPeriod = 0;	// start time of period to compute encoder Rate
	volatile float encRate = 0;				// encoder rate to test if can speed setting is OK
	volatile uint32_t lastVescResponse = 0;	// record last time when vesc respond

	void saveFlashOffset();

	// CAN section

	CANPort *port = &canport;
	int32_t filterId = 0;
	//uint8_t baudrate = CANSPEEDPRESET_500; 	// 250000, 500000, 1M
	uint8_t OFFB_can_Id = 0x40; 			// Default OpenFFBoard CAN ID
	uint8_t VESC_can_Id = 0xFF;				// Default VESC CAN id
	uint8_t buffer_rx[BUFFER_RX_SIZE];		// Used to store multi-frame can message

	//void setCanRate(uint8_t canRate);
	void getFirmwareInfo();
	bool isFirmwareCompatible();
	void sendPing();
	void setTorque(float torque);
	void decodeEncoderPosition(float newPos);
	void askGetValue();
	void askPositionEncoder();
	void sendMsg(uint8_t cmd, uint8_t *buffer, uint8_t len);
	void decode_buffer(uint8_t *buffer, uint8_t len);

	void buffer_append_float32(uint8_t *buffer, float number, float scale, int32_t *index);
	void buffer_append_int32(uint8_t *buffer, int32_t number, int32_t *index);
	void buffer_append_uint32(uint8_t* buffer, uint32_t number, int32_t *index);
	uint32_t buffer_get_uint32(const uint8_t *buffer, int32_t *index);
	int32_t buffer_get_int32(const uint8_t *buffer, int32_t *index);
	int16_t buffer_get_int16(const uint8_t *buffer, int32_t *index);
	float buffer_get_float16(const uint8_t *buffer, float scale, int32_t *index);
	float buffer_get_float32(const uint8_t *buffer, float scale, int32_t *index);
	unsigned short crc16(unsigned char *buf, unsigned int len);

	const unsigned short crc16_tab[256] = { 0x0000, 0x1021, 0x2042, 0x3063, 0x4084,
			0x50a5, 0x60c6, 0x70e7, 0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad,
			0xe1ce, 0xf1ef, 0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7,
			0x62d6, 0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
			0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485, 0xa56a,
			0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d, 0x3653, 0x2672,
			0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719,
			0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5, 0x6886, 0x78a7,
			0x0840, 0x1861, 0x2802, 0x3823, 0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948,
			0x9969, 0xa90a, 0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50,
			0x3a33, 0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b,
			0xab1a, 0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
			0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97,
			0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70, 0xff9f, 0xefbe,
			0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca,
			0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0x00a1, 0x30c2, 0x20e3,
			0x5004, 0x4025, 0x7046, 0x6067, 0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d,
			0xd31c, 0xe37f, 0xf35e, 0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214,
			0x6277, 0x7256, 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c,
			0xc50d, 0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
			0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c, 0x26d3,
			0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634, 0xd94c, 0xc96d,
			0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806,
			0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3, 0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e,
			0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1,
			0x1ad0, 0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b,
			0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0,
			0x0cc1, 0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
			0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0 };

};

// *** Creation of the first concrete class (VESC_1) used by OpenFFBoard of VescCan interface ***
class VESC_1 : public VescCAN {
public:
	VESC_1() : VescCAN{0} { inUse = true; }
	~VESC_1() { inUse = false; }

	static ClassIdentifier info;
	static bool inUse;

	static bool isCreatable();
	const ClassIdentifier getInfo();
};

// *** Creation of the second concrete class (VESC_2) used by OpenFFBoard of VescCan interface ***
/*class VESC_2 : public VescCAN {
public:
	VESC_2() : VescCAN{1} { inUse = true; }
	~VESC_2() { inUse = false; }

	static ClassIdentifier info;
	static bool inUse;

	static bool isCreatable();
	const ClassIdentifier getInfo();
};
*/
#endif /*             VESC               */
#endif /*  USEREXTENSIONS_SRC_VESCCAN_H_ */
