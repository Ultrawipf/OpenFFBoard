/*
 * EncoderLocal.h
 *
 *  Created on: 02.02.2020
 *      Author: Yannick
 */

#ifndef ENCODERLOCAL_H_
#define ENCODERLOCAL_H_

#include "cppmain.h"
#include <Encoder.h>
#include "constants.h"
#include "ExtiHandler.h"
#include "TimerHandler.h"
#include "CommandHandler.h"
#include "PersistentStorage.h"

#ifdef LOCALENCODER
extern TIM_HandleTypeDef TIM_ENC;



class EncoderLocal: public Encoder,public ExtiHandler,TimerHandler,public CommandHandler, public PersistentStorage{
	enum class EncoderLocal_commands : uint32_t{
		cpr,useindex
	};
public:
	static bool inUse;
	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	static bool isCreatable() {return !inUse;};

	EncoderLocal();
	virtual ~EncoderLocal();

	EncoderType getEncoderType();

	void registerCommands();

	int32_t getPos();
	void setPos(int32_t pos);
	//void setOffset(int32_t offset);
	void setPeriod(uint32_t period);
	void overflowCallback();
	void exti(uint16_t GPIO_Pin) override;
	void timerElapsed(TIM_HandleTypeDef* htim);

	int32_t getTimerCount();

	void setCpr(uint32_t cpr);	// Encoder counts per rotation

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	std::string getHelpstring(){return "Local ABN encoder\n";}
	void saveFlash(); 		// Write to flash here
	void restoreFlash();	// Load from flash

	//const ClassType getClassType() override {return ClassType::Encoder;};
private:
	TIM_HandleTypeDef* htim;
	int16_t offset = 0;
	int32_t pos = 0; // Extra position counter for overflows
	uint32_t indexpos = 0;

	bool indexHit = false;
	bool useIndex = false;
};
#endif
#endif /* ENCODERLOCAL_H_ */
