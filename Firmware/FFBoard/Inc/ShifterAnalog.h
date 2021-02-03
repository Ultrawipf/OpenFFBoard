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

/*
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

public:
	static constexpr int number_of_modes{4};
	const std::array<std::string, number_of_modes> mode_names {"G29-H","G29 Sequential","G27-H","G27 Sequential"};
	static constexpr std::array<bool, number_of_modes> mode_uses_spi {false, false, true, true};
	static constexpr std::array<bool, number_of_modes> mode_uses_local_reverse {true, true, false, false};

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
	virtual std::string getHelpstring(){return "Shifter analog: shifter_mode,shifter_x_12,shifter_x_56,shifter_y_135,shifter_y_246,shifter_rev_btn,shifter_cs_pin,shifter_x_chan,shifter_y_chan,shifter_vals,shifter_gear\n";}

private:
	class G27ShifterButtonClient : SPIDevice {
	public:
		G27ShifterButtonClient(const OutputPin& csPin);

		static constexpr int numUserButtons{12};

		void requestUpdate() {
			requestPort();
		}

		void updateCSPin(const OutputPin& csPin);

		uint16_t getUserButtons();
		bool getReverseButton();
	private:
		SPIConfig config;
		uint16_t buttonStates{0};

		const SPIConfig& getConfig() const override;
		void beginRequest(SPIPort::Pipe& pipe) override;
	};

	ShifterMode mode;

	uint8_t x_chan{6};
	uint8_t y_chan{5};
	uint8_t reverseButtonNum{1};
	uint8_t cs_pin_num{1};

	// H-Shifter axis values (Measured for G29)
	uint16_t X_12{1600};
	uint16_t X_56{2500};
	uint16_t Y_135{3200};
	uint16_t Y_246{850};

	uint16_t x_val{0};
	uint16_t y_val{0};
	bool reverseButtonState{false};
	uint8_t gear{0};

	G27ShifterButtonClient *g27ShifterButtonClient{nullptr};

	static bool isG27Mode(ShifterMode m);

	void updateAdc();
	void setMode(ShifterMode newMode);
	void setCSPin(uint8_t new_cs_pin_num);
	void calculateGear();
	void updateReverseState();
	int getUserButtons(uint32_t* buf);
};

#endif /* SHIFTERANALOG_H_ */
