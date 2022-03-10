/*
 * I2CHandler.h
 *
 *  Created on: Mar 10, 2022
 *      Author: Yannick
 */

#ifndef SRC_I2CHANDLER_H_
#define SRC_I2CHANDLER_H_
#include "global_callbacks.h"

class I2CHandler {
public:
	I2CHandler();
	virtual ~I2CHandler();

	virtual void I2cTxCplt(I2C_HandleTypeDef *hi2c);
	virtual void I2cRxCplt(I2C_HandleTypeDef *hi2c);
	virtual void I2cError(I2C_HandleTypeDef *hi2c);

	static std::vector<I2CHandler*>& getI2CHandlers() {
		static std::vector<I2CHandler*> i2cHandlers{};
		return i2cHandlers;
	}
};

#endif /* SRC_I2CHANDLER_H_ */
