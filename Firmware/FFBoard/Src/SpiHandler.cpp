/*
 * SpiHandler.cpp
 *
 *  Created on: Sep 29, 2020
 *      Author: Yannick
 */

#include "SpiHandler.h"

SpiHandler::SpiHandler() {
	addCallbackHandler(&getSPIHandlers(), this);

}

SpiHandler::~SpiHandler() {
	removeCallbackHandler(&getSPIHandlers(), this);
}

void SpiHandler::SpiTxCplt(SPI_HandleTypeDef *hspi){

}

void SpiHandler::SpiRxCplt(SPI_HandleTypeDef *hspi){

}

void SpiHandler::SpiTxRxCplt(SPI_HandleTypeDef *hspi){

}

void SpiHandler::SpiRxHalfCplt(SPI_HandleTypeDef *hspi){

}

void SpiHandler::SpiTxHalfCplt(SPI_HandleTypeDef *hspi){

}

void SpiHandler::SpiTxRxHalfCplt(SPI_HandleTypeDef *hspi){

}

void SpiHandler::SpiError(SPI_HandleTypeDef *hspi){

}



