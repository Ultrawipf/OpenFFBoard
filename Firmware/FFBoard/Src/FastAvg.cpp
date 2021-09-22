/*
 * FastAvg.h
 *
 *  Created on: Sept 28, 2021
 *      Author: Vincent Manoukian
 */

#include "FastAvg.h"

FastAvg::FastAvg() {
}

FastAvg::~FastAvg() {
}

void FastAvg::addValue(float value) {

	// Add the new value
	samples[currentIndex] = value;
	sumOfSamples += value;

	// remove the NB_SPEED_Sample previous value
	int indexRemove = (currentIndex + 1) % (NB_SPEED_SAMPLE+1);
	sumOfSamples -= samples[indexRemove];

	// prepare the next add
	currentIndex = indexRemove;
}

float FastAvg::getAverage() {
	return sumOfSamples / NB_SPEED_SAMPLE;
}
