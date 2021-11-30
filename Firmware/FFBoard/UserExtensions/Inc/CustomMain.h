/*
 * exampleMain.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef CUSTOMMAIN_H_
#define CUSTOMMAIN_H_

#include <FFBoardMain.h>

class CustomMain: public FFBoardMain {
	enum class CustomMain_commands : uint32_t{
		command=0
	};
public:


	CustomMain();
	virtual ~CustomMain();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void registerCommands();
	std::string getHelpstring();

private:
	int32_t examplevar = 0;
};

#endif /* CUSTOMMAIN_H_ */
