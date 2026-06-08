#include "ExternalEncoderAdapter.h"
#include "cppmain.h"

#ifdef TIM_TMC
extern TIM_HandleTypeDef TIM_TMC;
#endif

ExternalEncoderAdapter::ExternalEncoderAdapter(MotorDriver* motor, Encoder* encoder)
    : TimerHandler(), cpp_freertos::Thread("ENC_ADAPT", 128, 33), motor(motor), encoder(encoder) {
    this->Start();
}

void ExternalEncoderAdapter::timerElapsed(TIM_HandleTypeDef* htim) {
#ifdef TIM_TMC
    if (htim == &TIM_TMC && motor != nullptr && encoder != nullptr) {
        static uint32_t ticks = 0;
        ticks++;
        if (ticks >= 2) { // 5kHz is the sweet spot between SPI/CPU context switch overhead and TMC position latency
            ticks = 0;
            this->NotifyFromISR();
        }
    }
#endif
}

void ExternalEncoderAdapter::Run() {
    while (true) {
        this->WaitForNotification();
        if (motor != nullptr && encoder != nullptr) {
            float theta = encoder->getPos_f();
            float omega = encoder->getSpeed();
            float dt_spi = 0.000050f; // 50 us latency compensation
            float theta_extrapolated = theta + omega * dt_spi;
            motor->setExternalPhiE(theta_extrapolated);
        }
    }
}
