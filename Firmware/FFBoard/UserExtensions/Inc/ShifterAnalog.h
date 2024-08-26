/*
 * ShifterG29.h
 *
 *  Created on: 27.02.2020
 *      Author: Yannick
 */

#ifndef SHIFTERANALOG_H_
#define SHIFTERANALOG_H_

#include <vector>

#include "ButtonSource.h"
#include "CommandHandler.h"
#include "PersistentStorage.h"
#include "SPI.h"

/**
 * Button mapper for analog (Logitech G2x) shifters (6 gears + reverse) optionally SPI buttons on G27.
 *
 * Connection:
 *  X-Axis: mappable analog
 *  Y-Axis: mappable analog
 *  Reverse Button: mappable digital for G29, SPI button for G27.
 *
 */
class ShifterAnalog : public ButtonSource,CommandHandler {

enum class ShifterMode : uint8_t {G29_H=0,G29_seq=1,G27_H=2,G27_seq=3};

enum class ShifterAnalog_commands : uint32_t{
	mode,x12,x56,y135,y246,revbtn,cspin,xchan,ychan,vals,gear
};

public:
	static constexpr int number_of_modes{4};
	const std::array<std::string, number_of_modes> mode_names {"G29-H","G29 Sequential","G27-H","G27 Sequential"};
	static constexpr std::array<bool, number_of_modes> mode_uses_spi {false, false, true, true};
	static constexpr std::array<bool, number_of_modes> mode_uses_local_reverse {true, true, false, false};

	ShifterAnalog();
	virtual ~ShifterAnalog();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;
	static bool isCreatable() {return true;};

	uint8_t readButtons(uint64_t* buf) override;
	uint16_t getBtnNum(); // Amount of readable buttons

	void saveFlash() override;
	void restoreFlash() override;

	std::string printModes();

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;
	void registerCommands();
	virtual std::string getHelpstring(){return "Analog 6+1 gear shifter button source";}
	const ClassType getClassType() override {return ClassType::Buttonsource;};
private:
	class G27ShifterButtonClient : public SPIDevice {
	public:
		G27ShifterButtonClient(OutputPin& csPin);

		static constexpr int numUserButtons{12};

		uint16_t getUserButtons();
		bool getReverseButton();
	private:
		uint16_t buttonStates{0};

	};

	ShifterMode mode;

	uint8_t x_chan{6};
	uint8_t y_chan{5};
	uint8_t reverseButtonNum{1};
	uint8_t cs_pin_num{1};

	// H-Shifter axis values (Measured for G29)
	uint16_t X_12{25600};
	uint16_t X_56{40000};
	uint16_t Y_135{51200};
	uint16_t Y_246{13600};

	uint16_t x_val{0};
	uint16_t y_val{0};
	bool reverseButtonState{false};
	uint8_t gear{0};

	uint8_t bitshift = 0;

	std::unique_ptr<G27ShifterButtonClient> g27ShifterButtonClient;

	static bool isG27Mode(ShifterMode m);

	void updateAdc();
	void setMode(ShifterMode newMode);
	void setCSPin(uint8_t new_cs_pin_num);
	void calculateGear();
	void updateReverseState();
	int getUserButtons(uint64_t* buf);
};

#endif /* SHIFTERANALOG_H_ */
