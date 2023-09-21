/*
 * CAN.cpp
 *
 *  Created on: 21.06.2021
 *      Author: Yannick
 */

#include "target_constants.h"
#ifdef CANBUS
#include "CAN.h"


// Common can port

std::vector<CANPort*> CANPort::canPorts;
CANPort::CANPort(const CANPortHardwareConfig& presets,uint8_t instance) : presets(presets) {
	canPorts.push_back(this);
}
CANPort::~CANPort() {
	std::erase(canPorts, this);
}

CANPort* CANPort::handleToPort(void* handle){
	CANPort* portInst = nullptr;
	for(CANPort* port : CANPort::canPorts){ // Determine port instance
		if(port->getHandle() == handle){
			portInst = port;
		}
	}
	return portInst;
}


// ------------------------


#endif
