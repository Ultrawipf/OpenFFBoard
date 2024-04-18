/*
 * EncoderSources.cpp
 *
 *  Created on: Apr 18, 2024
 *      Author: Yannick
 */

#include "constants.h"
#include "EncoderLocal.h"
#include "MtEncoderSPI.h"
#include "EncoderBissC.h"
#include "EncoderSSI.h"
// 0-63 valid ids
#ifndef ENCODERSOURCES_DEFAULT_OVERRIDE
std::vector<class_entry<Encoder>> const Encoder::all_encoders =
	{
		add_class<Encoder, Encoder>(0),

#ifdef LOCALENCODER
		add_class<EncoderLocal, Encoder>(2),
#endif

#ifdef MTENCODERSPI
		add_class<MtEncoderSPI, Encoder>(4),
#endif
#ifdef BISSENCODER
		add_class<EncoderBissC, Encoder>(5),
#endif
#ifdef SSIENCODER
		add_class<EncoderSSI, Encoder>(6),
#endif
};
#endif
