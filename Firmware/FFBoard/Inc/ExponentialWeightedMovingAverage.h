#ifndef EXPONENTIALWEIGHTEDMOVINGAVERAGE_H_
#define EXPONENTIALWEIGHTEDMOVINGAVERAGE_H_

class ExponentialWeightedMovingAverage {
public:
	ExponentialWeightedMovingAverage(float factor);

	void clear();

	float process(float input);

private:
	float _factor = 0;
	float _result = 0;
};

#endif /* EXPONENTIALWEIGHTEDMOVINGAVERAGE_H_ */
