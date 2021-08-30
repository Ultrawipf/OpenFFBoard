/*
 * VescCAN.cpp
 *
 *  Created on: Aug 08, 2021
 *      Author: Vincent Manoukian (manoukianv@gmail.com)
 */
#include "target_constants.h"
#ifdef VESC
#include <VescCAN.h>


VescCAN::VescCAN() :
		Thread("VESC", VESC_THREAD_MEM, VESC_THREAD_PRIO) {

	VescCAN::vescInUse = true;
	restoreFlash();

	// Set up a filter to receive vesc commands
	CAN_FilterTypeDef sFilterConfig;
	sFilterConfig.FilterBank = 0;
	sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
	sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
	sFilterConfig.FilterIdHigh = 0x0000;
	sFilterConfig.FilterIdLow = 0x0000;
	sFilterConfig.FilterMaskIdHigh = 0x0000;
	sFilterConfig.FilterMaskIdLow = 0x0000;
	sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
	sFilterConfig.FilterActivation = ENABLE;
	sFilterConfig.SlaveStartFilterBank = 14;
	this->filterId = this->port->addCanFilter(sFilterConfig);

	this->port->setSpeedPreset(baudrate);

	this->Start();
}

VescCAN::~VescCAN() {
	VescCAN::vescInUse = false;
	this->stopMotor();
	//this->setEncoderMode(VescEncoderMode::ENCODER_OFF);
	this->state = VescState::VESC_STATE_UNKNOWN;
}

bool VescCAN::vescInUse = false;
ClassIdentifier VescCAN::info = { .name = "VESC", .id = 7, .unique = '1' };

const ClassIdentifier VescCAN::getInfo() {
	return info;
}

bool VescCAN::isCreatable() {
	return !VescCAN::vescInUse; // Creatable if not already in use for example by another axis
}

void VescCAN::turn(int16_t power) {
	float torque = ((float) power / (float) 0x7fff);
	lastTorque = torque;
	this->setTorque(torque);
}

void VescCAN::stopMotor() {
	activeMotor = false;
	this->setTorque(0.0);
}

void VescCAN::startMotor() {
	activeMotor = true;
}

Encoder* VescCAN::getEncoder() {
	return static_cast<Encoder*>(this);
}

bool VescCAN::motorReady() {
	return state == VescState::VESC_STATE_READY;
}

void VescCAN::restoreFlash() {
	uint16_t canIds = 0x0340;
	if (Flash_Read(ADR_VESC_CANDATA, &canIds)) {
		nodeId = canIds & 0xFF;

		uint8_t canspd = (canIds >> 8) & 0x5;
		this->setCanRate(canspd);
	}
}

void VescCAN::saveFlash() {
	uint16_t canIds = 0x0340;
	Flash_Read(ADR_VESC_CANDATA, &canIds); // Read again
	canIds &= ~0xFF; // reset bits
	canIds |= nodeId & 0xFF;

	canIds &= ~0x700;
	canIds |= (this->baudrate & 0x7) << 8;
	Flash_Write(ADR_VESC_CANDATA, canIds);
}


/**
 * Must be in encoder cpr if not just used to zero the axis
 */
void VescCAN::setPos(int32_t pos) {
	// Only change encoder count internally as offset
	posOffset = lastPos - ((float) pos / (float) getCpr());
}

float VescCAN::getPos_f() {
	if (state == VescState::VESC_STATE_READY) {
		this->askPositionEncoder();
	}
	return lastPos - posOffset;
}

int32_t VescCAN::getPos() {
	return getCpr() * getPos_f();
}

uint32_t VescCAN::getCpr() {
	return 0xFFFF;
}

ParseStatus VescCAN::command(ParsedCommand *cmd, std::string *reply) {
	// Prefix set but not our prefix
	if (cmd->prefix != this->getInfo().unique && cmd->prefix != '\0') {
		return ParseStatus::NOT_FOUND;
	}
	ParseStatus status = ParseStatus::OK;
	if (cmd->cmd == "vescCanId") {
		handleGetSet(cmd, reply, this->nodeId);
	} else if (cmd->cmd == "vescErrorFlag") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string(vescErrorFlag);
		}
	} else if (cmd->cmd == "vescState") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string((uint32_t) state);
		}
	} else if (cmd->cmd == "vescCanSpd") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string(baudrate);
		} else if (cmd->type == CMDtype::set) {
			this->setCanRate(cmd->val);
		}
	} else if (cmd->cmd == "vescEncRate") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string((uint32_t) encRate);
		}
	} else if (cmd->cmd == "vescPos") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string((int32_t) ((lastPos - posOffset) * 1000000000));
		}
	} else if (cmd->cmd == "vescTorque") {
		if (cmd->type == CMDtype::get) {
			*reply += std::to_string((int32_t) (lastTorque * 10000));
		}
	} else if (cmd->cmd == "vescPosReadForce") {
		if(cmd->type == CMDtype::set){
			this->askPositionEncoder();
		}
	} else {
		status = ParseStatus::NOT_FOUND;
	}
	return status;
}

/*
 * state 	= VESC_UNKOWN 	=> ping until the state go to PONG
 * 			= VESC_PONG		=> call a status message
 * 			= VESC_ERROR	=> call a status message
 *
 * 	if error => state = VESC_ERROR
 * 	   else=> state = VESC_RESPOND
 *
 * 	if RespondVescTime > 100 => VESC_NOT_RESPOND
 *
 */
void VescCAN::Run() {

	while (true) {
		Delay(500);

		// check the vesc status each 500ms
		this->askGetValue();

		// if no vesc response since 1000ms, there is a problem
		uint32_t period = HAL_GetTick() - lastVescResponse;
		if ( state != VescState::VESC_STATE_UNKNOWN && period > 1000) {
			state = VescState::VESC_STATE_UNKNOWN;
		}

		// compute encoderRate each second
		period = HAL_GetTick() - encStartPeriod;
		if (period > 1000) {
			encRate = encCount / (period / 1000.0);
			encCount = 0;
			encStartPeriod = HAL_GetTick();
		}

		// when motor is active we call vesc each 500ms, to disable the vesc watchdog
		// (if vesc not received message in 1sec, it cut off motor)
		if (lastTorque != 0.0 && activeMotor && state == VescState::VESC_STATE_READY) {
			this->setTorque(lastTorque);
		}


	}
}

/************************************************************************************************
 *
 * 										CAN SECTION
 */

void VescCAN::setCanRate(uint8_t canRate) {
	baudrate = clip<uint8_t, uint8_t>(canRate, 3, 5);
	port->setSpeedPreset(baudrate);
}

/**
 * Send a ping command to the vesc to check if it's here.
 */
void VescCAN::sendPing() {
	uint8_t buffer[1];
	buffer[0] = this->nodeId;

	this->sendMsg((uint8_t) VescCANMsg::CAN_PACKET_PING, buffer, sizeof(buffer));
}

/**
 * Send a torque command to the vesc : value is [-1,1] range relative to max torque settings
 * in the vesc fw.
 */
void VescCAN::setTorque(float torque) {

	// Check if vesc CAN is ok, if it is, send a message
	if (!this->motorReady()) return;

	uint8_t buffer[4];
	int32_t send_index = 0;
	this->buffer_append_float32(buffer, torque, 1e5, &send_index);

	this->sendMsg((uint8_t) VescCANMsg::CAN_PACKET_SET_CURRENT_REL, buffer,
			sizeof(buffer));

}

void VescCAN::askGetValue() {
    uint8_t msg[8];
	msg[0] =  nodeId;
	msg[1] =  0x00;					// ask vesc to process the msg
	msg[2] =  50;					// sub action is COMM_GET_VALUES_SELECTIVE : ask wanted data
	uint32_t request = ((uint32_t)1 << 15); 	// bit 15 = ask vesc status (1byte)
	//mask |= ((uint32_t)1 << 16);	// bit 16 = ask position	(4byte)

	int32_t index = 3;
	this->buffer_append_uint32(msg, request, &index);

	this->sendMsg((uint8_t) VescCANMsg::CAN_PACKET_PROCESS_SHORT_BUFFER, msg,
			sizeof(msg));


}

/**
 * Ask a position with the new system
 * Call the new short response command with a 2 messages communications
 */
void VescCAN::askPositionEncoder() {
	this->sendMsg((uint8_t) VescCANMsg::CAN_PACKET_POLL_ROTOR_POS, nullptr, 0);
}


/*
 * Compute the new position (laspos) value from data received by vesc:
 * 		newPos is a 0-360° angle position
 * 		if delta with previous and new position is > 180° then we count 1 turn
 * 		at 1kz, more than 180° mean 30000rpm, not admissible in simracing
 */
void VescCAN::decodeEncoderPosition(const float newPos) {
	float delta = newPos - prevPos360; // Compute the delta position

	bool moreThanHalfTurn = fabs(delta) > 180.0 ? 1 : 0;
	if (moreThanHalfTurn) {
		// if delta is neg, turn is CCW, decrement multi turn pos... else increment it
		mtPos += (delta > 0) ? -1.0 : 1.0;
	}

	lastPos = (newPos / 360.0) + mtPos; // normalize the position
	prevPos360 = newPos;

	encCount++;
}

/**
 * send the message to the CAN with an Extended Can message
 */
void VescCAN::sendMsg(uint8_t cmd, uint8_t *buffer, uint8_t len) {
	CAN_tx_msg msg;
	memcpy(&msg.data, buffer, len);
	msg.header.RTR = CAN_RTR_DATA;
	msg.header.DLC = len;
	msg.header.IDE = CAN_ID_EXT;
	msg.header.ExtId = VESC_CAN_ID | (cmd << 8);
	port->sendMessage(msg);
}

void VescCAN::decode_buffer(uint8_t *buffer, uint8_t len) {

	if (!len) {
		return;
	}

	VescCmd command = (VescCmd)(buffer[0] & 0xFF);
	buffer++;
	len--;

	switch (command) {

		case VescCmd::COMM_GET_VALUES_SELECTIVE : {
			int32_t ind = 0;
			uint32_t mask = this->buffer_get_uint32(buffer, &ind);

			if (mask & ((uint32_t)1 << 15)) {
				vescErrorFlag = buffer[ind++];

				// if the vesc respond but with an error, we put the vesc in error flag
				if (vescErrorFlag) {
					state = VescState::VESC_STATE_ERROR;
				} else {
					state = VescState::VESC_STATE_READY;
				}

			}
			if (mask & ((uint32_t)1 << 16)) {
				float pos = this->buffer_get_float32(buffer, 1e6, &ind);
				this->decodeEncoderPosition(pos);
			}
			break;

		}

		default:
			break;

	}

}

/*
 *	Process the received message on the CAN bus.
 *	Message have several struct, depends on message type, check the comm_can.c and
 *	command.c in the vesc source project to identify the struct format.
 *
 *	Quick tips :
 *	Msg struct for CAN_PACKET_PROCESS_RX_BUFFER and CAN_PACKET_PROCESS_SHORT_BUFFER :
 *	uint8_t[0] => can_emitter_id (vesc id)
 *	uint8_t[1] => send bool
 *	uint8_t[2] => COMM_PACKET_ID command
 *	uint8_t[3] -> uint8_t[7] => DATA (5 bytes)
 *
 *	Msg struct for CAN_PACKET_POLL_ROTOR_POS
 *	uint8_t[0]..uint8_t[3] : uint32 f_pos / 10000
 *
 */
void VescCAN::canRxPendCallback(CAN_HandleTypeDef *hcan, uint8_t *rxBuf,
		CAN_RxHeaderTypeDef *rxHeader, uint32_t fifo) {

	// we record the last time respond to check if vesc is OK
	lastVescResponse =  HAL_GetTick();

	// Check if message is for the openFFBoard
	uint16_t node = rxHeader->ExtId & 0xFF;

	// Extract the command encoded in the ExtId
	VescCANMsg cmd = (VescCANMsg) (rxHeader->ExtId >> 8);

	// if msg is not for this node && mesg not a poll result, bypass
	if ((node != this->nodeId) && (cmd != VescCANMsg::CAN_PACKET_POLL_ROTOR_POS )) {
		return;
	}

	// Process the CAN message received
	switch (cmd) {

		case VescCANMsg::CAN_PACKET_FILL_RX_BUFFER: {
			memcpy(buffer_rx + rxBuf[0], rxBuf + 1, rxHeader->DLC - 1);
			break;
		}

		case VescCANMsg::CAN_PACKET_FILL_RX_BUFFER_LONG: {
			uint32_t rxbuf_ind = (unsigned int)rxBuf[0] << 8;
			rxbuf_ind |= rxBuf[1];
			if (rxbuf_ind < BUFFER_RX_SIZE) {
				memcpy(buffer_rx + rxbuf_ind, rxBuf + 2, rxHeader->DLC - 2);
			}
			break;
		}

		case VescCANMsg::CAN_PACKET_PROCESS_RX_BUFFER: {

			// remove 2 first byte to exclude "can emitter id" and the "send bool" flag from buffer
			int32_t index = 2;

			uint32_t rxbuf_length = (uint32_t)rxBuf[index++] << 8;
			rxbuf_length |= (uint32_t)rxBuf[index++];

			if (rxbuf_length > BUFFER_RX_SIZE) {
				break;
			}

			uint8_t crc_high = rxBuf[index++];
			uint8_t crc_low = rxBuf[index++];

			if (crc16(buffer_rx, rxbuf_length) == ((uint8_t) crc_high << 8 | (uint8_t) crc_low)) {

				this->decode_buffer(buffer_rx, rxbuf_length);

			}

			break;
		}
		case VescCANMsg::CAN_PACKET_PROCESS_SHORT_BUFFER: {

			// remove 2 first byte to exclude "can emitter id" and the "send bool" flag from buffer
			this->decode_buffer(rxBuf + 2, rxHeader->DLC - 2);

			break;
		}

		case VescCANMsg::CAN_PACKET_PONG:
			state = VescState::VESC_STATE_PONG;
			break;

		case VescCANMsg::CAN_PACKET_POLL_ROTOR_POS : { // decode encoder data
			int32_t index = 0;
			float pos = this->buffer_get_int32(rxBuf, &index) / 100000.0; // extract the 0-360 float position
			this->decodeEncoderPosition(pos);
			break;
		}

		default:
			break;
		}

}

void VescCAN::buffer_append_float32(uint8_t *buffer, float number, float scale,
		int32_t *index) {
	this->buffer_append_int32(buffer, (int32_t) (number * scale), index);
}

void VescCAN::buffer_append_int32(uint8_t *buffer, int32_t number,
		int32_t *index) {
	buffer[(*index)++] = number >> 24;
	buffer[(*index)++] = number >> 16;
	buffer[(*index)++] = number >> 8;
	buffer[(*index)++] = number;
}

void VescCAN::buffer_append_uint32(uint8_t* buffer, uint32_t number, int32_t *index) {
	buffer[(*index)++] = number >> 24;
	buffer[(*index)++] = number >> 16;
	buffer[(*index)++] = number >> 8;
	buffer[(*index)++] = number;
}

uint32_t VescCAN::buffer_get_uint32(const uint8_t *buffer, int32_t *index) {
	uint32_t res =	((uint32_t) buffer[*index]) << 24 |
					((uint32_t) buffer[*index + 1]) << 16 |
					((uint32_t) buffer[*index + 2]) << 8 |
					((uint32_t) buffer[*index + 3]);
	*index += 4;
	return res;
}


int32_t VescCAN::buffer_get_int32(const uint8_t *buffer, int32_t *index) {
	int32_t res = ((uint32_t) buffer[*index]) << 24
			| ((uint32_t) buffer[*index + 1]) << 16
			| ((uint32_t) buffer[*index + 2]) << 8
			| ((uint32_t) buffer[*index + 3]);
	*index += 4;
	return res;
}


float VescCAN::buffer_get_float32(const uint8_t *buffer, float scale, int32_t *index) {
    return (float)buffer_get_int32(buffer, index) / scale;
}

unsigned short VescCAN::crc16(unsigned char *buf, unsigned int len) {
	unsigned int i;
	unsigned short cksum = 0;
	for (i = 0; i < len; i++) {
		cksum = crc16_tab[(((cksum >> 8) ^ *buf++) & 0xFF)] ^ (cksum << 8);
	}
	return cksum;
}

#endif
