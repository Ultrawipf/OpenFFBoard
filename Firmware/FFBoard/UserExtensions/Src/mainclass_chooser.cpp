/*
 * mainclass_chooser.cpp
 *
 *  Created on: 25.01.2020
 *      Author: Yannick
 */
#include "cppmain.h"
#include "mainclass_chooser.h"

#include "ExampleMain.h"
#include "FFBWheel.h"
#include "FFBoardMain.h"
#include "TMCDebugBridge.h"
#include "MidiMain.h"

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
		add_class<ExampleMain,FFBoardMain>()
};


