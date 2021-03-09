/*
 * HidCommandHandler.h
 *
 *  Created on: 8 Mar 2021
 *      Author: jonli
 */

#ifndef INC_HIDCOMMANDHANDLER_H_
#define INC_HIDCOMMANDHANDLER_H_

#include "hid_cmd_defs.h"

class HidCommandHandler {
public:
	HidCommandHandler() {};
	~HidCommandHandler() {};
	virtual bool processHidCommand(HID_Custom_Data_t *data) {return false;};
};

#endif /* INC_HIDCOMMANDHANDLER_H_ */
