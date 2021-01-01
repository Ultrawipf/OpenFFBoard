/*
 * SpiHandler.h
 *
 *  Created on: Sep 29, 2020
 *      Author: Yannick
 */

#ifndef SPIHANDLER_H_
#define SPIHANDLER_H_
#include "cppmain.h"
#include "global_callbacks.h"

class SpiHandler {
public:
	SpiHandler();
	virtual ~SpiHandler();

	virtual void SpiTxCplt(SPI_HandleTypeDef *hspi);
	virtual void SpiRxCplt(SPI_HandleTypeDef *hspi);
	virtual void SpiTxRxCplt(SPI_HandleTypeDef *hspi);
	virtual void SpiTxHalfCplt(SPI_HandleTypeDef *hspi);
	virtual void SpiRxHalfCplt(SPI_HandleTypeDef *hspi);
	virtual void SpiTxRxHalfCplt(SPI_HandleTypeDef *hspi);
	virtual void SpiError(SPI_HandleTypeDef *hspi);

	static std::vector<SpiHandler*>& getSPIHandlers() {
		static std::vector<SpiHandler*> spiHandlers{};
		return spiHandlers;
	}
};

#endif /* SPIHANDLER_H_ */
