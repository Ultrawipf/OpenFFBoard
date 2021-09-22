/*
 * FastAvg.h
 *
 *  Created on: Sept 28, 2021
 *      Author: Vincent Manoukian
 */

#ifndef FAST_AVERAGE_H_
#define FAST_AVERAGE_H_

#include <FFBoardMain.h>
#define NB_SPEED_SAMPLE 36

class FastAvg {

public:
	FastAvg();
	~FastAvg();
	void addValue(float value);
	float getAverage();

private:
	float samples[NB_SPEED_SAMPLE+1];
	int currentIndex = 0;
	float sumOfSamples = 0;
};


#endif
