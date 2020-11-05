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



