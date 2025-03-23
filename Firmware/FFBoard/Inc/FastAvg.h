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

/**
 * Calculates a moving average of variable length
 * Can periodically get and reset a value to average for an unknown amount of data points
 * If len != 0 calculates an exponential moving average with length len.
 * If len = 0 length equals the current amount of samples up to 0x7FFFFFFF.
 */
template <class T>
class FastMovingAverage{
public:
	FastMovingAverage(int32_t len = 0) : fixedLen(len > 0 ? len : INT32_MAX), count(0){};
	~FastMovingAverage(){};

	void clear(){
		curAvg = 0;
		count=0;
	}
	/**
	 * Gets current average and clears current average and counter
	 */
	T getAndReset(){
		T t = curAvg;
		clear();
		return t;
	}

	/**
	 * Gets current average without resetting
	 */
	T getAverage(){
		return curAvg;
	}
	/**
	 * Adds a value and returns current average
	 */
	T addValue(T v){
		if(count < fixedLen)
			count++;
		curAvg += (v - curAvg)/count;
		return curAvg;
	}
private:
	T curAvg = 0;
	const int32_t fixedLen;
	int32_t count = 0;
};

#endif
