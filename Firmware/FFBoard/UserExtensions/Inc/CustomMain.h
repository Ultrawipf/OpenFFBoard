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
public:


	CustomMain();
	virtual ~CustomMain();

	static ClassIdentifier info;
	const ClassIdentifier getInfo();
	ParseStatus command(ParsedCommand* cmd,std::string* reply);


private:
	int32_t examplevar = 0;
};

#endif /* CUSTOMMAIN_H_ */
