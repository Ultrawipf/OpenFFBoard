/*
 * FastAvg.h
 *
 *  Created on: Sept 28, 2021
 *      Author: Vincent Manoukian + Yannick Richter
 */

#ifndef FAST_AVERAGE_H_
#define FAST_AVERAGE_H_

#include <FFBoardMain.h>

template <std::size_t LEN>
class FastAvg {

public:
	FastAvg(){};
	~FastAvg(){};

	__attribute__((optimize("-Ofast")))
	void addValue(float value) {

		// Add the new value
		samples[currentIndex] = value;
		sumOfSamples += value;

		// remove the previous value
		int indexRemove = (currentIndex + 1) % (LEN+1);
		sumOfSamples -= samples[indexRemove];

		// prepare the next add
		currentIndex = indexRemove;
	}

	float getAverage() {
		return sumOfSamples / LEN;
	}

	float process(float value) {
		addValue(value);
		return sumOfSamples / LEN;
	}

private:
	float samples[LEN+1];
	int currentIndex = 0;
	float sumOfSamples = 0;

};


#endif
