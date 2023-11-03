/*
 * voltagesense.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#ifndef VOLTAGESENSE_H_
#define VOLTAGESENSE_H_
#include "target_constants.h"

int32_t getIntV();
int32_t getExtV();
void brakeCheck();

/*
 * Multiplier to convert ADC counts to a sensed voltage on the vInt and vExt pins.
 */
void setVSenseMult(float vSenseMultiplier);

/*
 * vMax: maximum voltage where the brake pin goes always high
 * vdiffAct: difference between vInt and vExt to activate brake pin
 * vdiffDeact: difference when to deactivate brake pin again (must be lower than vdiffAct)
 * Set vMax = 0 to completely deactivate the brake resistor function. DANGEROUS
 *
 */
void setupBrakePin(uint32_t vdiffAct,int32_t vdiffDeact,int32_t vMax);

/**
 * Converts an adc reading to millivolts using the internal calibration and reference voltage
 * Requires ADC_INTREF_VOL and VSENSE_ADC_RES
 */
float adcValToVoltage(uint32_t adcval);

/**
 * Helper function implementation for getting the chip temperature if read via internal ADC channel
 * __weak defined and can be overridden in chip dependent code
 */
int32_t getChipTemp();

#endif /* VOLTAGESENSE_H_ */
