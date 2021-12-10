/*
 * hid_cmd_defs.h
 *
 *  Created on: 28.01.2021
 *      Author: Yannick
 */

#ifndef INC_HID_CMD_DEFS_H_
#define INC_HID_CMD_DEFS_H_

#include "cppmain.h"
#include "ffb_defs.h"
/*
 * Define global HID command ids here to keep track
 */
#define HID_CMD_STATUS 0x01	// for checking if the device runs
#define HID_CMD_MAINCLASS 0x10

#define HID_CMD_FFB_STRENGTH 0x20
#define HID_CMD_FFB_DEGREES 0x21
#define HID_CMD_FFB_ZERO 0x22
#define HID_CMD_FFB_IDLESPRING 0x23
#define HID_CMD_FFB_ESGAIN 0x24
#define HID_CMD_FFB_FRICTION 0x25
#define HID_CMD_FFB_FXRATIO 0x26



enum class HidCmdType : uint8_t {write = 0, request = 1, info = 2, writeAddr = 3, requestAddr = 4,ACK = 10, notFound = 13, notification = 14, err = 15};

typedef struct
{
	uint8_t		reportId = HID_ID_CUSTOMCMD; //HID_ID_CUSTOMCMD_FEATURE
	HidCmdType	type = HidCmdType::err;	// 0x01. Type of report. 0 = write, 1 = request, 2 = error
	uint32_t	cmd = 0;				// 0x02 Use this as an identifier for the command
	uint32_t	addr = 0;				// 0x03 Use this to transfer an optional address (CAN for example)
	uint64_t	data = 0;				// 0x04 Use this to transfer data

} __attribute__((packed)) HID_Custom_Data_t;


// TODO: can only be 25B long. tinyusb will hang otherwise!
typedef struct
{
	uint8_t		reportId = HID_ID_HIDCMD;
	HidCmdType	type = HidCmdType::err;	// 0x01. Type of report. 0 = write, 1 = request, 2 = error
	uint16_t	clsid = 0;
	uint8_t 	instance = 0;		// Class ID. 16 bits class id, 8 bits used for unique instance number. 255 for broadcast
	uint32_t	cmd = 0;				// 0x02 Use this as an identifier for the command. upper 16 bits for class type
	uint64_t	data = 0;				// 0x03 Use this to transfer data or the primary value
	uint64_t	addr = 0;				// 0x04 Use this to transfer an optional address or second value (CAN for example)

} __attribute__((packed)) HID_CMD_Data_t;



#endif /* INC_HID_CMD_DEFS_H_ */
