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


//template<size_t AxisAmount,bool AllowFilters, bool AllowAutoscale>
//class AnalogAxisProcessing_tpl
//struct Empty{};
//{
//
//public:
//
//	AnalogAxisProcessing_tpl(CommandHandler* cmdHandler = nullptr)
//	{
//
//	};
//
//protected:
//
//	const uint32_t axisAmount = AxisAmount;
////	std::conditional<AllowFilters,std::array<Biquad,AxisAmount>,Empty> filters; // Optional filters
////	std::vector<MinMaxPair> minMaxVals;
////	std::vector<std::pair<uint16_t,uint16_t>> minMaxValAddr = ManualMinMaxAddresses;
////
////	const float filterF = 30.0/1000.0 , filterQ = 0.5;
////
////	AnalogProcessingConfig conf;
//	uint32_t filterSamples = 0;
//	const uint32_t waitFilterSamples = 500;
//
//	float autorangeScale = 1.05; // Multiplies autorange scale to add some margin
//	};

//template<class T>
//class AnalogAxisProcessingAddresses{
//public:
//	template<size_t minmaxlen>
//	AnalogAxisProcessingAddresses(uint16_t settingsAddr,std::array<std::pair<uint16_t,uint16_t>,minmaxlen>) : data(data[length]),length(length),settingsAddr(settingsAddr)
//	{
//
//	};
//
//private:
//	uint32_t length;
//	T data[];
//	uint16_t settingsAddr;
//};
struct AnalogProcessingConfig{
	bool autorange = false;
	bool filtersEnabled = false;
	bool raw = false;
};
struct MinMaxPair{
	int32_t min = INT_MAX;
	int32_t max = INT_MIN;
};
class AnalogAxisProcessing {

	enum class AnalogAxisProcessing_commands {
		filter=0xA1,autoscale=0xA2,min=0xA3,max=0xA4
	};

public:
	//AnalogAxisProcessing(const uint32_t axisAmount,CommandHandler* cmdHandler = nullptr,bool allowFilters = true,bool allowAutoscale = true,const std::optional<const std::vector<std::pair<uint16_t,uint16_t>>*> manualMinMaxAddresses=std::nullopt);
	AnalogAxisProcessing(const uint32_t axisAmount,CommandHandler* cmdHandler = nullptr,bool allowFilters = true,bool allowAutoscale = true, bool allowManualScale = false);
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
	std::vector<Biquad> filters; // Optional filters
	std::vector<MinMaxPair> minMaxVals; // TODO maybe use 2 minMax vectors to separate manual and autoscale later?
	//const std::vector<std::pair<uint16_t,uint16_t>> *minMaxValAddr = nullptr;

	const float filterF = 30.0/1000.0 , filterQ = 0.5;
	uint32_t filterSamples = 0;
	const uint32_t waitFilterSamples = 500;

	float autorangeScale = 1.05; // Multiplies autorange scale to add some margin

private:
	const struct AnalogProcessingMode{
		bool allowFilters : 1;
		bool allowAutoscale : 1;
		bool allowManualScale : 1;
	} modes;
};


//class AnalogAxisProcessing {
//
//	struct AnalogProcessingConfig{
//		bool autorange = false;
//		bool filtersEnabled = false;
//	};
//	struct MinMaxPair{
//		int32_t min = INT_MAX;
//		int32_t max = INT_MIN;
//	};
//	enum class AnalogAxisProcessing_commands {
//		filter=0xA1,autoscale=0xA2,min=0xA3,max=0xA4
//	};
//
//public:
//	// Public contructor w. template for static_assert
//	template<size_t AddressSize>
//    AnalogAxisProcessing(const uint32_t axisAmount,CommandHandler* cmdHandler,bool allowFilters,bool allowAutoscale, const std::array<std::pair<uint16_t, uint16_t>,AddressSize> &addr)
//	: AnalogAxisProcessing(axisAmount,cmdHandler, allowFilters,allowAutoscale, std::optional(addr))
//    {
//        //static_assert(addr.size() >= axisAmount, "Not enough addresses supplied");
//    };
//	template<size_t axisAmount>
//	AnalogAxisProcessing(CommandHandler* cmdHandler = nullptr,bool allowFilters = true,bool allowAutoscale = true)
//	: AnalogAxisProcessing(axisAmount,cmdHandler, allowFilters,allowAutoscale, std::nullopt){
//
//	};
//
//
//	virtual ~AnalogAxisProcessing();
//	CommandStatus command(const ParsedCommand& cmd,std::vector<CommandReply>& replies);
//	void setupFilters();
//	void processAxes(std::vector<int32_t>& buf);
//protected:
//
//	const uint32_t axisAmount;
//	std::vector<Biquad> filters; // Optional filters
//	std::vector<MinMaxPair> minMaxVals;
//	const std::pair<uint16_t,uint16_t> *minMaxValAddr = nullptr;
//
//	const float filterF = 30.0/1000.0 , filterQ = 0.5;
//
//	AnalogProcessingConfig conf;
//	uint32_t filterSamples = 0;
//	const uint32_t waitFilterSamples = 500;
//
//	float autorangeScale = 1.05; // Multiplies autorange scale to add some margin
//
//private:
//	AnalogAxisProcessing(const uint32_t axisAmount,CommandHandler* cmdHandler = nullptr,bool allowFilters = true,bool allowAutoscale = true,const std::optional<const std::pair<uint16_t,uint16_t>*> manualMinMaxAddresses = std::nullopt);
//};



#endif /* SRC_ANALOGAXISPROCESSING_H_ */
