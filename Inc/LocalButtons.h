/*
 * LocalButtons.h
 *
 *  Created on: 09.02.2020
 *      Author: Yannick
 */

#ifndef LOCALBUTTONS_H_
#define LOCALBUTTONS_H_

#include <ButtonSource.h>

class LocalButtons: public ButtonSource {
public:
	LocalButtons();
	virtual ~LocalButtons();

	void readButtons(uint8_t* buf ,uint16_t len);
	uint16_t getBtnNum();

private:
	const uint16_t button_pins[8] = {DIN0_Pin,DIN1_Pin,DIN2_Pin,DIN3_Pin,DIN4_Pin,DIN5_Pin,DIN6_Pin,DIN7_Pin};
	GPIO_TypeDef* button_ports[8] = {DIN0_GPIO_Port,DIN1_GPIO_Port,DIN2_GPIO_Port,DIN3_GPIO_Port,DIN4_GPIO_Port,DIN5_GPIO_Port,DIN6_GPIO_Port,DIN7_GPIO_Port};

};

#endif /* LOCALBUTTONS_H_ */
