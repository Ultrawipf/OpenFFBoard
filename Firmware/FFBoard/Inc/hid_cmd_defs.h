/*
 * hid_cmd_defs.h
 *
 *  Created on: 28.01.2021
 *      Author: Yannick
 */

#ifndef INC_HID_CMD_DEFS_H_
#define INC_HID_CMD_DEFS_H_


/*
 * Define global HID command ids here to keep track
 */
#define HID_CMD_STATUS 0x01	// for checking if the device runs
#define HID_CMD_MAINCLASS 0x10

#define HID_CMD_FFB_STRENGTH 0x20
#define HID_CMD_FFB_DEGREES 0x21
#define HID_CMD_FFB_ZERO 0x22

enum class HidCmdType : uint8_t {err = 0, write = 1, request = 2,stop_stream = 3, start_stream = 4};

typedef struct
{
	uint8_t		reportId = HID_ID_CUSTOMCMD; //HID_ID_CUSTOMCMD_FEATURE
	HidCmdType	type = HidCmdType::err;	// Type of report. 0 = error, 1 = write, 2 = request
	uint32_t	cmd = 0;		// Use this as an identifier
	uint64_t	data = 0;	// Use this to transfer data
} __attribute__((packed)) HID_Custom_Data_t;


#endif /* INC_HID_CMD_DEFS_H_ */
