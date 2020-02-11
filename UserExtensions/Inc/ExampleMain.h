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



private:

};

#endif /* EXAMPLEMAIN_H_ */
