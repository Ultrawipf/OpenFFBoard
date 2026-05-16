/*
 * ButtonSources.cpp
 *
 *  Created on: Apr 18, 2024
 *      Author: Yannick
 */
#include "constants.h"
#include "SPIButtons.h"
#include "CanButtons.h"
#include "LocalButtons.h"
#include <ShifterAnalog.h>
#include "PCF8574.h"

#ifndef BUTTONSOURCES_DEFAULT_OVERRIDE
// Register possible button sources (id 0-15)
const std::vector<class_entry<ButtonSource>> ButtonSource::all_buttonsources =
{
#ifdef LOCALBUTTONS
		add_class<LocalButtons,ButtonSource>(0),
#endif
#ifdef SPIBUTTONS
		add_class<SPI_Buttons_1,ButtonSource>(1),
#endif
#ifdef SPIBUTTONS2
		add_class<SPI_Buttons_2,ButtonSource>(2),
#endif
#ifdef SHIFTERBUTTONS
		add_class<ShifterAnalog,ButtonSource>(3),
#endif
#ifdef PCF8574BUTTONS
		add_class<PCF8574Buttons,ButtonSource>(4),
#endif
#ifdef CANBUTTONS
		add_class<CanButtons,ButtonSource>(5),
#endif
};
#endif
