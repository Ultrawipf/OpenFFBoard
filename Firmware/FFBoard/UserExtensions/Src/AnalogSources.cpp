/*
 * AnalogSources.cpp
 *
 *  Created on: Apr 18, 2024
 *      Author: Yannick
 */

#include "constants.h"
#include "LocalAnalog.h"
#include "CanAnalog.h"
#include "ADS111X.h"

// Register possible analog sources (id 0-15)
#ifndef ANALOGSOURCES_DEFAULT_OVERRIDE
const std::vector<class_entry<AnalogSource>> AnalogSource::all_analogsources =
{
#ifdef ANALOGAXES
		add_class<LocalAnalog,AnalogSource>(0),
#endif
#ifdef CANANALOG
		add_class<CanAnalog<8>,AnalogSource>(1),
#endif
#ifdef ADS111XANALOG
		add_class<ADS111X_AnalogSource,AnalogSource>(2),
#endif
};
#endif
