/*
 * ButtonSourceSPI.h
 *
 *  Created on: 11.02.2020
 *      Author: Yannick
 */

#ifndef SPIBUTTONS_H_
#define SPIBUTTONS_H_

#include "CommandHandler.h"
#include <ButtonSource.h>
#include "cppmain.h"
#include "ChoosableClass.h"

#include "SPI.h"
#include "cpp_target_config.h"

// Set this to 64, 128 or 256 to slow down SPI if unstable.
#define SPIBUTTONS_SPEED SPI_BAUDRATEPRESCALER_64

enum class SPI_BtnMode : uint8_t {TM=0,PISOSR=1};


struct ButtonSourceConfig{
	uint8_t numButtons = 8;
	bool cutRight = false; // if num buttons to read are not byte aligned specify where to shift
	bool invert = false;
	SPI_BtnMode mode=SPI_BtnMode::TM; // Mode preset
	uint8_t cs_num = 0;
};


class SPI_Buttons: public ButtonSource,public CommandHandler,public SPIDevice {

	enum class SPIButtons_commands : uint32_t {
		mode,btncut,btnpol,btnnum,cs
	};

public:
	const std::vector<std::string> mode_names = {"Thrustmaster/HEF4021BT","Shift register (74HC165)"};

	virtual ~SPI_Buttons();

	uint8_t readButtons(uint64_t* buf);

	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies) override;
	void registerCommands();
	virtual std::string getHelpstring(){return "SPI 2 Button";}

	void saveFlash();
	void restoreFlash();

	const uint8_t maxButtons = 64;
	std::string printModes();

	void setMode(SPI_BtnMode mode);
	void initSPI();
	const ClassType getClassType() override {return ClassType::Buttonsource;};

protected:
	SPI_Buttons(uint16_t configuration_address, uint16_t configuration_address_2);

private:
	uint16_t configuration_address;
	uint16_t configuration_address_2;
	bool ready = false;
	void setConfig(ButtonSourceConfig config);
	virtual ButtonSourceConfig* getConfig();
	void process(uint64_t* buf);
	uint8_t bytes = 4;
	uint64_t mask = 0xff;
	uint8_t offset = 0;

	ButtonSourceConfig conf;

	uint8_t spi_buf[4] = {0};
};

class SPI_Buttons_1 : public SPI_Buttons {
public:
	SPI_Buttons_1()
		: SPI_Buttons{ADR_SPI_BTN_1_CONF, ADR_SPI_BTN_1_CONF_2} {
			setInstance(0);
		}

	const ClassIdentifier getInfo() override;
	static ClassIdentifier info;
	static bool isCreatable();
	static bool exists;
};

class SPI_Buttons_2 : public SPI_Buttons {
public:
	SPI_Buttons_2()
		: SPI_Buttons{ADR_SPI_BTN_2_CONF, ADR_SPI_BTN_2_CONF_2} {
			setInstance(1);
		}

	const ClassIdentifier getInfo() override;
	static ClassIdentifier info;
	static bool isCreatable();
};

#endif /* SPIBUTTONS_H_ */
