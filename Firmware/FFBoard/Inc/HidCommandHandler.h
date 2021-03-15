/*
 * HidCommandHandler.h
 *
 *  Created on: 8 Mar 2021
 *      Author: jonli
 */

#ifndef INC_HIDCOMMANDHANDLER_H_
#define INC_HIDCOMMANDHANDLER_H_

#include "hid_cmd_defs.h"
#include <vector>

class HidCommandHandler {
public:
	HidCommandHandler();
	virtual ~HidCommandHandler();

	static std::vector<HidCommandHandler*> hidCmdHandlers; // called only for custom cmd report ids

	virtual void processHidCommand(HID_Custom_Data_t *data);
	virtual bool sendHidCmd(HID_Custom_Data_t* data);
};

#endif /* INC_HIDCOMMANDHANDLER_H_ */


