/*
 * exampleMain.h
 *
 *  Created on: 23.01.2020
 *      Author: Yannick
 */

#ifndef EXAMPLEMAIN_H_
#define EXAMPLEMAIN_H_

#include <FFBoardMain.h>

class ExampleMain: public FFBoardMain {
public:


	ExampleMain();
	virtual ~ExampleMain();

	static FFBoardMainIdentifier info;
	const FFBoardMainIdentifier getInfo();
	bool executeUserCommand(ParsedCommand* cmd,std::string* reply);


private:
	int32_t examplevar = 0;
};

#endif /* EXAMPLEMAIN_H_ */
