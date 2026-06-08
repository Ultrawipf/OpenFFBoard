#ifndef EXTERNALENCODERADAPTER_H_
#define EXTERNALENCODERADAPTER_H_

#include "TimerHandler.h"
#include "MotorDriver.h"
#include "Encoder.h"
#include "thread.hpp"

class ExternalEncoderAdapter : public TimerHandler, public cpp_freertos::Thread {
public:
    ExternalEncoderAdapter(MotorDriver* motor, Encoder* encoder);
    ~ExternalEncoderAdapter() override = default;

    void timerElapsed(TIM_HandleTypeDef* htim) override;
    void Run() override;

private:
    MotorDriver* motor;
    Encoder* encoder;
};

#endif // EXTERNALENCODERADAPTER_H_
