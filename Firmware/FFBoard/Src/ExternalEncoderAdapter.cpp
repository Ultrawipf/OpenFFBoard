#include "ExternalEncoderAdapter.h"
#include "cppmain.h"

#include "EncoderManager.h"

ExternalEncoderAdapter::ExternalEncoderAdapter(MotorDriver* motor, Encoder* encoder)
    : motor(motor), encoder(encoder) {
    EncoderManager::getInstance().registerAdapter(this);
}

ExternalEncoderAdapter::~ExternalEncoderAdapter() {
    EncoderManager::getInstance().deregisterAdapter(this);
}

void ExternalEncoderAdapter::update() {
    if (motor != nullptr && encoder != nullptr) {
        // Use absolute mechanical position (without user centering offset) so setPos(0) never shifts FOC phiE
        float theta = encoder->getPosAbs_f();
        float omega = encoder->getRawSpeed();
        // Extrapolate using real sample age plus fixed motor SPI write latency (~15 us)
        uint32_t age_us = micros() - encoder->getLastUpdateTime();
        if (age_us > 1000) {
            age_us = 1000; // Bound extrapolation to 1 ms max on stale/first sample
        }
        float dt_extrap = (float)(age_us + 15) * 1e-6f;
        float theta_extrapolated = theta + omega * dt_extrap;
        motor->setExternalPhiE(theta_extrapolated);
    }
}
