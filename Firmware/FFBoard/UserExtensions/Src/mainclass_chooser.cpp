/*
 * mainclass_chooser.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */
#include <CustomMain.h>
#include "cppmain.h"
#include "mainclass_chooser.h"
#include "target_constants.h"

#ifdef FFBWHEEL
#include "FFBWheel.h"
#endif
#include "FFBoardMain.h"
#ifdef TMCDEBUG
#include "TMCDebugBridge.h"
#endif
#ifdef MIDI
#include "MidiMain.h"
#endif
#ifdef CANBRIDGE
#include "CanBridge.h"
#endif

// Add all classes here
const std::vector<class_entry<FFBoardMain>> class_registry =
{
		add_class<FFBoardMain,FFBoardMain>(),
#ifdef FFBWHEEL
		add_class<FFBWheel,FFBoardMain>(),
#endif
#ifdef TMCDEBUG
		add_class<TMCDebugBridge,FFBoardMain>(),
#endif
#ifdef MIDI
		add_class<MidiMain,FFBoardMain>(),
#endif
#ifdef CANBRIDGE
		add_class<CanBridge,FFBoardMain>(),
#endif
		add_class<CustomMain,FFBoardMain>()
};


