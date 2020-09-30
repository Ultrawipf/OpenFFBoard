/*
 * SpiHandler.cpp
 *
 *  Created on: Sep 29, 2020
 *      Author: Yannick
 */

#include "SpiHandler.h"

SpiHandler::SpiHandler() {
	extern std::vector<SpiHandler*> spiHandlers;
	addCallbackHandler(&spiHandlers, this);

}

SpiHandler::~SpiHandler() {
	extern std::vector<SpiHandler*> spiHandlers;
	removeCallbackHandler(&spiHandlers, this);
}



void SpiTxCplt(SPI_HandleTypeDef *hspi){

}

void SpiRxCplt(SPI_HandleTypeDef *hspi){

}

void SpiTxRxCplt(SPI_HandleTypeDef *hspi){

}

void SpiRxHalfCplt(SPI_HandleTypeDef *hspi){

}

void SpiTxHalfCplt(SPI_HandleTypeDef *hspi){

}

void SpiTxRxHalfCplt(SPI_HandleTypeDef *hspi){

}

void SpiError(SPI_HandleTypeDef *hspi){

}



