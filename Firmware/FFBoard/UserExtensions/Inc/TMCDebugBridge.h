/*
 * TMCDebugBridge.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef TMCDebugBridge_H_
#define TMCDebugBridge_H_

#include "constants.h"
#ifdef TMCDEBUG
#include <FFBoardMain.h>
#include <vector>
#include "TMC4671.h"

extern "C"{
#include "main.h"
}


extern SPI_HandleTypeDef HSPIDRV;

class TMCDebugBridge: public FFBoardMain {
	enum class TMCDebugBridge_commands : uint32_t{
		torque,pos,openloopspeed,velocity,mode,reg,openloopspeedpwm
	};
public:

	TMCDebugBridge();
	virtual ~TMCDebugBridge();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable() {return true;};

	void cdcRcv(char* Buf, uint32_t *Len);
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	std::string getHelpstring(){return "Compatible with TMCL-IDE. To use the FOC modes position, velocity and torque you first must manually select the encoder and wait for alignment.";}


private:
	const TMC4671PIDConf tmcpids = TMC4671PIDConf({
		 .fluxI = 400,
		 .fluxP = 400,
		 .torqueI = 400,
		 .torqueP = 300,
		 .velocityI = 10,
		 .velocityP = 500,
		 .positionI = 0,
		 .positionP = 40,
		 .sequentialPI = true});

	const TMC4671Limits tmclimits = TMC4671Limits({
		.pid_torque_flux_ddt	= 32767,
		.pid_uq_ud				= 30000,
		.pid_torque_flux		= 32767,
		.pid_acc_lim			= 50, // Safety
		.pid_vel_lim			= 2147483647,
		.pid_pos_low				= -2147483647,
		.pid_pos_high			= 2147483647
	});
	static void sendCdc(char* dat, uint32_t len);
	std::unique_ptr<TMC4671> drv;

	uint8_t checksum(std::vector<uint8_t> *buffer,uint8_t len);
	HAL_StatusTypeDef SPI_transmit_receive(uint8_t *tx_data,uint8_t *rx_data,uint16_t len,uint32_t timeout);
};

#endif /* TMCDebugBridge_H_ */
#endif
