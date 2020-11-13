/*
 * voltagesense.h
 *
 *  Created on: Feb 1, 2020
 *      Author: Yannick
 */

#ifndef VOLTAGESENSE_H_
#define VOLTAGESENSE_H_
#include "target_constants.h"

uint16_t getIntV();
uint16_t getExtV();
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
void setupBrakePin(uint32_t vdiffAct,uint32_t vdiffDeact,uint32_t vMax);

#endif /* VOLTAGESENSE_H_ */
