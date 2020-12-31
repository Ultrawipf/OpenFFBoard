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
#include "ButtonSink.h"
#include "CommandHandler.h"
#include "PersistentStorage.h"

class ShifterAnalog : public ButtonSource,CommandHandler,ButtonSink {

/*
 * Button mapper for analog (Logitech G29) shifters (6 gears + reverse)
 * Connection:
 * X-Axis: A5
 * Y-Axis: A6
 * Reverse Button: D1
 *
 * Invert flag switches between H-mode and sequential mode (90° rotated)
 */

enum class ShifterMode : uint8_t {G29_H=0,G29_seq=1};

public:
	const std::vector<std::string> mode_names = {"G2x-H","G2x Sequential"};

	ShifterAnalog();
	virtual ~ShifterAnalog();

	const ClassIdentifier getInfo();
	static ClassIdentifier info;

	void readButtons(uint32_t* buf);
	uint16_t getBtnNum(); // Amount of readable buttons

	void saveFlash() override;
	void restoreFlash() override;

	void printModes(std::string* reply);

	ParseStatus command(ParsedCommand* cmd,std::string* reply);

private:
	ShifterMode mode;

	static constexpr uint8_t x_chan{5};
	static constexpr uint8_t y_chan{4};

	uint8_t reverseButtonNum{1};
	bool reverseButtonState{false};

	// H-Shifter axis values (Measured for G26)
	uint16_t X_12{1600};
	uint16_t X_56{2500};
	uint16_t Y_135{3200};
	uint16_t Y_246{850};

	uint16_t x_val{0};
	uint16_t y_val{0};

	uint8_t gear{0};

	void updateAdc();
	void updateButtonState(std::uint32_t* buf, std::uint16_t numButtons) override;
};

#endif /* SHIFTERANALOG_H_ */
