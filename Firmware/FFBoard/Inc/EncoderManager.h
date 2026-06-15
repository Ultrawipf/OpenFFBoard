#ifndef ENCODERMANAGER_H_
#define ENCODERMANAGER_H_

#include "TimerHandler.h"
#include "Encoder.h"

class ExternalEncoderAdapter;

#include "thread.hpp"
#include <atomic>

class EncoderManager : public TimerHandler, public cpp_freertos::Thread {
public:
    static EncoderManager& getInstance();

    EncoderManager(const EncoderManager&) = delete;
    EncoderManager& operator=(const EncoderManager&) = delete;

    void init();
    void registerEncoder(Encoder* encoder);
    void deregisterEncoder(Encoder* encoder);

    void registerAdapter(ExternalEncoderAdapter* adapter);
    void deregisterAdapter(ExternalEncoderAdapter* adapter);
    void updateAdaptersForEncoder(Encoder* enc);

    void timerElapsed(TIM_HandleTypeDef* htim) override;
    void Run() override;

private:
    EncoderManager();
    ~EncoderManager() override = default;

    static constexpr uint8_t MAX_ENCODERS = 8;
    std::atomic<Encoder*> active_encoders[MAX_ENCODERS] = {};
    
    static constexpr uint8_t MAX_ADAPTERS = 3; // Max 3 axes per board, each tied to a single motor driver
    std::atomic<ExternalEncoderAdapter*> active_adapters[MAX_ADAPTERS] = {};
    uint32_t tick_count = 0;
    bool initialized = false;
};

#endif // ENCODERMANAGER_H_
