/*
 * TMC4671_configs.cpp
 *
 *  Created on: Feb 20, 2024
 *      Author: Yannick
 */

#include "TMC4671.h"
#include "constants.h"
#include "span"

#if !defined(TMC4671_OVERRIDE_HWCONFS) && defined(TMC4671DRIVER)
// Default configs for officially supported hardware


const TMC4671HardwareTypeConf conf1_0 = {
		.name = "v1.0 AD8417 (1.5mOhm)",
		.hwVersion = (uint8_t)TMC_HW_Ver::v1_0,
		.adcOffset = 1000,
		.thermistorSettings = {
			.thermistor_R2 = 0,
			.thermistor_R = 0,
			.thermistor_Beta = 0,
			.temp_limit = 90,
			.temperatureEnabled = false,
		},
		.currentScaler = 2.5 / (0x7fff * 60.0 * 0.0015), // w. 60x 1.5mOhm sensor
		.brakeLimLow = 52400,
		.brakeLimHigh = 52800,
		.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
		.vSenseMult = VOLTAGE_MULT_DEFAULT,
		.clockfreq = 25e6,
		.bbm = 20,
};
const TMC4671HardwareTypeConf conf1_2 = {
		.name = "v1.2 AD8417 (1.5mOhm)",
		.hwVersion = (uint8_t)TMC_HW_Ver::v1_2,
		.adcOffset = 1000,
		.thermistorSettings = {
			.thermistor_R2 = 1500,
			.thermistor_R = 22000,
			.thermistor_Beta = 4300,
			.temp_limit = 90,
			.temperatureEnabled = true,
		},
		.currentScaler = 2.5 / (0x7fff * 60.0 * 0.0015), // w. 60x 1.5mOhm sensor
		.brakeLimLow = 50700,
		.brakeLimHigh = 50900,
		.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
		.vSenseMult = VOLTAGE_MULT_DEFAULT,
		.clockfreq = 25e6,
		.bbm = 20,
};
const TMC4671HardwareTypeConf conf1_2_2 = {
		.name = "v1.2.2 LEM 10 (80mV/A)",
		.hwVersion = (uint8_t)TMC_HW_Ver::v1_2_2,
		.adcOffset = 0,
		.thermistorSettings = {
			.thermistor_R2 = 1500,
			.thermistor_R = 10000,
			.thermistor_Beta = 4300,
			.temp_limit = 90,
			.temperatureEnabled = true,
		},
		.currentScaler = 2.5 / (0x7fff * 0.08), // w. LEM 10 sensor 80mV/A
		.brakeLimLow = 50700,
		.brakeLimHigh = 50900,
		.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
		.vSenseMult = VOLTAGE_MULT_DEFAULT,
		.clockfreq = 25e6,
		.bbm = 20,
};
const TMC4671HardwareTypeConf conf1_3 = {
		.name = "v1.3 ACS724 (66mV/A)",
		.hwVersion = (uint8_t)TMC_HW_Ver::v1_3_66mv,
		.adcOffset = 0,
		.thermistorSettings = {
			.thermistor_R2 = 1500,
			.thermistor_R = 10000,
			.thermistor_Beta = 4300,
			.temp_limit = 90,
			.temperatureEnabled = true,
		},
		.currentScaler = 2.5 / (0x7fff * 0.066), // sensor 66mV/A
		.brakeLimLow = 50700,
		.brakeLimHigh = 50900,
		.vmScaler = (2.5 / 0x7fff) * ((1.5+71.5)/1.5),
		.vSenseMult = VOLTAGE_MULT_DEFAULT,
		.clockfreq = 25e6,
		.bbm = 40, // May need longer deadtime
		.flags{
			.mot_none = 1,
			.mot_dc = 1,
			.mot_bldc = 1,
			.mot_stepper = 1,

			.enc_none = 1,
			.enc_abn = 1,
			.enc_sincos = 1,
			.enc_uvw = 1,
			.enc_hall = 1,
			.enc_ext = 1,

			.allowFluxDissipationDeactivation = 1
		}
};
const auto tmc4671_hw_configs_array = std::to_array<const TMC4671HardwareTypeConf>({conf1_3,conf1_2_2,conf1_2,conf1_0});
std::span<const TMC4671HardwareTypeConf> TMC4671::tmc4671_hw_configs = tmc4671_hw_configs_array;
#endif

// Only a single config with default settings. Some defaults can be overridden by defines
#ifdef TMC4671_CUSTOM_DEFAULT_HWCONF
const TMC4671HardwareTypeConf defaultconf;
const auto tmc4671_hw_configs_array = std::to_array<const TMC4671HardwareTypeConf>({defaultconf});
std::span<const TMC4671HardwareTypeConf> TMC4671::tmc4671_hw_configs = tmc4671_hw_configs_array;

#endif
