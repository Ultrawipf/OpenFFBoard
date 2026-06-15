#include "EncoderManager.h"
#include "ExternalEncoderAdapter.h"
#include "cppmain.h"

#ifdef TIM_ENC
extern TIM_HandleTypeDef TIM_ENC;
#endif

EncoderManager::EncoderManager() : TimerHandler(), cpp_freertos::Thread("ENCMGR", 128, 33) {
    this->Start();
}

EncoderManager& EncoderManager::getInstance() {
    static EncoderManager instance;
    return instance;
}

void EncoderManager::init() {
    if (initialized) return;

#ifdef TIM_ENC
    TIM_HandleTypeDef* timer = &TIM_ENC;
    
    // Stop first to prevent race conditions during reconfiguration
    HAL_TIM_Base_Stop_IT(timer);

    // PSC counts microseconds (1 MHz frequency)
#ifdef TIM_ENC_BCLK
    timer->Instance->PSC = ((TIM_ENC_BCLK) / 1000000) - 1;
#else
    timer->Instance->PSC = (SystemCoreClock / 2 / 1000000) - 1;
#endif

    timer->Instance->ARR = 100; // 100 us period = 10 kHz
    timer->Instance->CNT = 0;
    timer->Instance->CR1 = TIM_CR1_CEN; // Enable counter

    // Start timer interrupts
    HAL_TIM_Base_Start_IT(timer);
#endif

    initialized = true;
}

void EncoderManager::registerEncoder(Encoder* encoder) {
    if (encoder == nullptr) return;
    int empty_idx = -1;
    for (int i = 0; i < MAX_ENCODERS; ++i) {
        Encoder* curr = active_encoders[i].load(std::memory_order_acquire);
        if (curr == encoder) {
            return; // Already registered
        }
        if (curr == nullptr && empty_idx == -1) {
            empty_idx = i;
        }
    }
    if (empty_idx != -1) {
        active_encoders[empty_idx].store(encoder, std::memory_order_release);
    }
}

void EncoderManager::deregisterEncoder(Encoder* encoder) {
    if (encoder == nullptr) return;
    for (int i = 0; i < MAX_ENCODERS; ++i) {
        if (active_encoders[i].load(std::memory_order_acquire) == encoder) {
            active_encoders[i].store(nullptr, std::memory_order_release);
            return;
        }
    }
}

void EncoderManager::registerAdapter(ExternalEncoderAdapter* adapter) {
    if (adapter == nullptr) return;
    int empty_idx = -1;
    for (int i = 0; i < MAX_ADAPTERS; ++i) {
        ExternalEncoderAdapter* curr = active_adapters[i].load(std::memory_order_acquire);
        if (curr == adapter) {
            return; // Already registered
        }
        if (curr == nullptr && empty_idx == -1) {
            empty_idx = i;
        }
    }
    if (empty_idx != -1) {
        active_adapters[empty_idx].store(adapter, std::memory_order_release);
    }
}

void EncoderManager::deregisterAdapter(ExternalEncoderAdapter* adapter) {
    if (adapter == nullptr) return;
    for (int i = 0; i < MAX_ADAPTERS; ++i) {
        if (active_adapters[i].load(std::memory_order_acquire) == adapter) {
            active_adapters[i].store(nullptr, std::memory_order_release);
            return;
        }
    }
}

void EncoderManager::updateAdaptersForEncoder(Encoder* enc) {
    if (enc == nullptr) return;
    for (int i = 0; i < MAX_ADAPTERS; ++i) {
        ExternalEncoderAdapter* adapter = active_adapters[i].load(std::memory_order_acquire);
        if (adapter != nullptr && adapter->getEncoder() == enc) {
            adapter->update();
        }
    }
}

void EncoderManager::timerElapsed(TIM_HandleTypeDef* htim) {
#ifdef TIM_ENC
    if (htim == &TIM_ENC) {
        this->NotifyFromISR();
    }
#endif
}

void EncoderManager::Run() {
    while(true) {
        this->WaitForNotification();
        tick_count++;
        for (int i = 0; i < MAX_ENCODERS; ++i) {
            Encoder* enc = active_encoders[i].load(std::memory_order_acquire);
            if (enc == nullptr || enc->getEncoderType() == EncoderType::NONE) {
                continue;
            }
            uint32_t scaler = enc->getScaler();
            if (scaler == 0) scaler = 1;
            if ((tick_count % scaler) == 0) {
                enc->triggerRead();
            }
        }
    }
}
