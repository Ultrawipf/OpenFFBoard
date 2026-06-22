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

#ifndef LOCAL_ENCODER_SPEED_MODE
#define LOCAL_ENCODER_SPEED_MODE 0
#endif

#ifdef LOCALENCODER
extern TIM_HandleTypeDef TIM_ENC_ABN_HW;



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

	void triggerRead() override;

	int32_t getTimerCount();

	uint32_t getCpr() override;
	void setCpr(uint32_t cpr);	// Encoder counts per rotation
	uint32_t getScaler() override;

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

#if LOCAL_ENCODER_SPEED_MODE
	uint32_t last_timestamp = 0;
	int32_t last_speed_pos = 0;
	int32_t last_pos_1ms = 0;
#endif
};
#endif
#endif /* ENCODERLOCAL_H_ */
