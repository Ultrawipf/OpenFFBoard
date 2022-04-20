/*
 * I2CHandler.cpp
 *
 *  Created on: Mar 10, 2022
 *      Author: Yannick
 */

#include "I2CHandler.h"

I2CHandler::I2CHandler() {
	addCallbackHandler(getI2CHandlers(), this);

}

I2CHandler::~I2CHandler() {
	addCallbackHandler(getI2CHandlers(), this);
}

void I2CHandler::I2cTxCplt(I2C_HandleTypeDef *hi2c){

}
void I2CHandler::I2cRxCplt(I2C_HandleTypeDef *hi2c){

}
void I2CHandler::I2cError(I2C_HandleTypeDef *hi2c){

}
