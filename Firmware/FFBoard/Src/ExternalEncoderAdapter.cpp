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
        float dt_spi = 0.000050f; // 50 us latency compensation
        float theta_extrapolated = theta + omega * dt_spi;
        motor->setExternalPhiE(theta_extrapolated);
    }
}
