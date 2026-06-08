#ifndef ENCODERMANAGER_H_
#define ENCODERMANAGER_H_

#include "TimerHandler.h"
#include "Encoder.h"

class EncoderManager : public TimerHandler {
public:
    static EncoderManager& getInstance();

    EncoderManager(const EncoderManager&) = delete;
    EncoderManager& operator=(const EncoderManager&) = delete;

    void init();
    void registerEncoder(Encoder* encoder);
    void deregisterEncoder(Encoder* encoder);

    void timerElapsed(TIM_HandleTypeDef* htim) override;

private:
    EncoderManager();
    ~EncoderManager() override = default;

    static constexpr uint8_t MAX_ENCODERS = 8;
    Encoder* active_encoders[MAX_ENCODERS] = {nullptr};
    uint32_t tick_count = 0;
    bool initialized = false;
};

#endif // ENCODERMANAGER_H_
