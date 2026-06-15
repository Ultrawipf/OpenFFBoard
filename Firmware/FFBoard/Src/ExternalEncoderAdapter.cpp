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
        float theta = encoder->getPos_f();
        float omega = encoder->getSpeed();
        float dt_spi = 0.000050f; // 50 us latency compensation
        float theta_extrapolated = theta + omega * dt_spi;
        motor->setExternalPhiE(theta_extrapolated);
    }
}
