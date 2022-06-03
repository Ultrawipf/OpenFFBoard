/*
 * FastAvg.h
 *
 *  Created on: Sept 28, 2021
 *      Author: Vincent Manoukian + Yannick Richter
 */

#ifndef FAST_AVERAGE_H_
#define FAST_AVERAGE_H_

#include <FFBoardMain.h>

template <class T,std::size_t LEN>
class FastAvg {

public:
	FastAvg(){};
	~FastAvg(){};

	__attribute__((optimize("-Ofast")))
	void addValue(T value) {

		// Add the new value
		samples[currentIndex] = value;
		sumOfSamples += value;

		// remove the previous value
		int indexRemove = (currentIndex + 1) % (LEN+1);
		sumOfSamples -= samples[indexRemove];

		// prepare the next add
		currentIndex = indexRemove;
	}

	T getAverage() {
		return sumOfSamples / LEN;
	}

	T process(float value) {
		addValue(value);
		return sumOfSamples / LEN;
	}

	void clear() {
		memset(samples,0,LEN*sizeof(T));
		sumOfSamples = 0;
	}

private:
	T samples[LEN+1] = {static_cast<T>(0)};
	int currentIndex = 0;
	T sumOfSamples = 0;

};


#endif
