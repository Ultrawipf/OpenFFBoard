#include "ExponentialWeightedMovingAverage.h"

ExponentialWeightedMovingAverage::ExponentialWeightedMovingAverage(float factor) {
	this->_factor = factor;
}

void ExponentialWeightedMovingAverage::clear() {
	this->_result = 0;
}

float ExponentialWeightedMovingAverage::process(float input) {
		_result = (_factor * input) - (_factor * _result) + _result;
		return _result;
}
