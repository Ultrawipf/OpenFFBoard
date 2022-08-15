/*
 * AnalogAxisProcessing.h
 *
 *  Created on: Jun 29, 2022
 *      Author: Yannick
 */

#ifndef SRC_ANALOGAXISPROCESSING_H_
#define SRC_ANALOGAXISPROCESSING_H_
#include "constants.h"
#include <optional>
#include <vector>
#include <array>
#include "CommandHandler.h"
#include "Filters.h"
#include "limits.h"
#include "AnalogSource.h"


struct AnalogProcessingConfig{
	bool autorange = false;
	bool filtersEnabled = false;
	bool raw = false;
};
struct MinMaxPair{
	int32_t min = 0x7fff;
	int32_t max = -0x7fff;
};
class AnalogAxisProcessing {

	enum class AnalogAxisProcessing_commands {
		values=0xAA0,filter=0xAA2,autoscale=0xAA3,rawvalues=0XAA1,min=0xAA4,max=0xAA5
	};

public:
	//AnalogAxisProcessing(const uint32_t axisAmount,CommandHandler* cmdHandler = nullptr,bool allowFilters = true,bool allowAutoscale = true,const std::optional<const std::vector<std::pair<uint16_t,uint16_t>>*> manualMinMaxAddresses=std::nullopt);
	AnalogAxisProcessing(const uint32_t axisAmount,AnalogSource* analogSource, CommandHandler* cmdHandler = nullptr,bool allowFilters = true,bool allowAutoscale = true, bool allowRawValues=false, bool allowManualScale = false);
	virtual ~AnalogAxisProcessing();
	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
	void setupFilters();
	void processAxes(std::vector<int32_t>& buf);
	AnalogProcessingConfig& getAnalogProcessingConfig();
	void setAnalogProcessingConfig(AnalogProcessingConfig conf);
//	void saveFlash();
//	void restoreSettings(uint16_t val);

	static AnalogProcessingConfig decodeAnalogProcessingConfFromInt(uint16_t val);
	static uint16_t encodeAnalogProcessingConfToInt(AnalogProcessingConfig& conf);
	//void setRawValues(std::vector<int32_t>* rawValues);

	/**
	 * Loads min and max values
	 * @param[in] minMaxAddresses addresses to store manual min/max values in (pair of min,max addresses).
	 */
	template<size_t size>
	void restoreMinMaxValues(const std::array<std::pair<uint16_t,uint16_t>,size>& minMaxAddresses){
		if(!modes.allowManualScale){
			return; // Do not save if autoscale is on
		}
		for(uint8_t i = 0; i < std::min<uint32_t>(size, this->axisAmount); i++){
			uint16_t bufMin,bufMax;
			if(Flash_Read(minMaxAddresses[i].first, &bufMin) && Flash_Read(minMaxAddresses[i].second, &bufMax)){
				this->minMaxVals[i].min = (int16_t)bufMin;
				this->minMaxVals[i].max = (int16_t)bufMax;
			}
		}
	}
	/**
	 * Saves min and max values
	 * @param[in] minMaxAddresses addresses to store manual min/max values in (pair of min,max addresses).
	 */
	template<size_t size>
	void saveMinMaxValues(const std::array<std::pair<uint16_t,uint16_t>,size>& minMaxAddresses){
		if(conf.autorange || !modes.allowManualScale){
			return; // Do not save if autoscale is on
		}
		for(uint8_t i = 0; i < std::min<uint32_t>(size, this->axisAmount); i++){
			Flash_Write(minMaxAddresses[i].first, (uint16_t)this->minMaxVals[i].min);
			Flash_Write(minMaxAddresses[i].second, (uint16_t)this->minMaxVals[i].max);
		}
	}

protected:
	AnalogProcessingConfig conf;
	const uint32_t axisAmount;
	AnalogSource* analogSource;

	std::vector<Biquad> filters; // Optional filters
	std::vector<MinMaxPair> minMaxVals; // TODO maybe use 2 minMax vectors to separate manual and autoscale later?
	//const std::vector<std::pair<uint16_t,uint16_t>> *minMaxValAddr = nullptr;

	const float filterF = 30.0/1000.0 , filterQ = 0.5;
	uint32_t filterSamples = 0;
	const uint32_t waitFilterSamples = 500;

	float autorangeScale = 1.05; // Multiplies autorange scale to add some margin

	std::vector<int32_t> rawValues; // Pointer to unscaled raw values

private:
	const struct AnalogProcessingMode{
		bool allowFilters : 1;
		bool allowAutoscale : 1;
		bool allowRawvalues : 1;
		bool allowManualScale : 1;
	} modes;
};



#endif /* SRC_ANALOGAXISPROCESSING_H_ */
