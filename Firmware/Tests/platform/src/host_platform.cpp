/*
 * host_platform.cpp
 *
 * Fake hardware layer for the host unit tests: HAL, time base, flash storage and LEDs.
 * Replaces the target specific code (Targets/<target>/Core) and the hardware bound
 * helpers (flash_helpers.cpp, ledEffects.cpp, parts of cppmain.cpp).
 * See support/HostPlatform.h for the test control interface.
 */
#include "HostPlatform.h"

#include "main.h"
#include "cmsis_compiler.h"
#include "cppmain.h"
#include "flash_helpers.h"
#include "ledEffects.h"
#include "UART.h"

uint32_t hosttest_ipsr = 0;

UART_HandleTypeDef huart1 = {};
CAN_HandleTypeDef hcan1 = {};
UARTPort external_uart{UART_PORT_EXT}; // Normally defined in cpp_target_config.cpp

namespace {
uint64_t timeUs = 0;
HostPlatform::LedState ledState;
HostPlatform::UartState uartState;
uint8_t *uartRxBuffer = nullptr; // Buffer armed by HAL_UART_Receive_IT
UART_HandleTypeDef *uartRxHandle = nullptr;
} // namespace

namespace HostPlatform {

void reset() {
	timeUs = 0;
	hosttest_ipsr = 0;
	flash().clear();
	ledState = LedState();
	uartState = UartState();
	uartRxBuffer = nullptr;
	uartRxHandle = nullptr;
	huart1 = {};
	// Release the port in case a test left a transfer unfinished
	external_uart.giveSemaphore(true);
	external_uart.giveSemaphore(false);
}

void uartCompleteTx() {
	IsrContext isr;
	for (UartHandler *handler : UartHandler::getUARTHandlers()) {
		handler->uartTxComplete(&huart1);
	}
}

void uartReceive(const std::string &data) {
	for (char c : data) {
		if (uartRxBuffer == nullptr) {
			uartState.rxLost++;
			continue;
		}
		*uartRxBuffer = (uint8_t)c;
		UART_HandleTypeDef *huart = uartRxHandle;
		uartRxBuffer = nullptr; // Single shot. Must be rearmed by the firmware
		IsrContext isr;
		for (UartHandler *handler : UartHandler::getUARTHandlers()) {
			handler->uartRxComplete(huart);
		}
	}
}

uint64_t nowUs() { return timeUs; }
void setTimeUs(uint64_t us) { timeUs = us; }
void advanceUs(uint64_t us) { timeUs += us; }
void setInIsr(bool isr) { hosttest_ipsr = isr ? 1 : 0; }

std::map<uint16_t, uint16_t> &flash() {
	static std::map<uint16_t, uint16_t> storage;
	return storage;
}

LedState &leds() { return ledState; }
UartState &uart() { return uartState; }

} // namespace HostPlatform

// ---------------- Time base ----------------
uint32_t micros() { return (uint32_t)timeUs; }
void refreshWatchdog() {}
unsigned long getRunTimeCounterValue(void) { return (unsigned long)timeUs; }

extern "C" {

uint32_t HAL_GetTick(void) { return (uint32_t)(timeUs / 1000); }
void HAL_Delay(uint32_t ms) { timeUs += (uint64_t)ms * 1000; }
void HAL_NVIC_SystemReset(void) {}
void Error_Handler(void) {}

// ---------------- GPIO ----------------
void HAL_GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *) {}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state) {
	if (port) {
		port->ODR = state == GPIO_PIN_SET ? (port->ODR | pin) : (port->ODR & ~pin);
	}
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin) {
	return (port && (port->IDR & pin)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin) {
	if (port) {
		port->ODR ^= pin;
	}
}

// ---------------- UART ----------------
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *) {
	uartState.initCount++;
	return HAL_OK;
}
static HAL_StatusTypeDef recordUartTx(const uint8_t *data, uint16_t size) {
	uartState.tx.append((const char *)data, size);
	uartState.txTransfers++;
	return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *, const uint8_t *data, uint16_t size, uint32_t) { return recordUartTx(data, size); }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *, const uint8_t *data, uint16_t size) { return recordUartTx(data, size); }
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *, const uint8_t *data, uint16_t size) { return recordUartTx(data, size); }
HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *, uint8_t *, uint16_t, uint32_t) { return HAL_TIMEOUT; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t) {
	uartState.rxArmedCount++;
	uartRxBuffer = data;
	uartRxHandle = huart;
	return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_DMA(UART_HandleTypeDef *, uint8_t *, uint16_t) { return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *) { return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *) { return HAL_OK; }
uint32_t HAL_UART_GetError(UART_HandleTypeDef *huart) { return huart->ErrorCode; }

} // extern "C"

// ---------------- LEDs (ledEffects.h) ----------------
void blinkLed(Ledstruct_t *, uint16_t, uint16_t) {}
void pulseSysLed() { ledState.sysPulses++; }
void pulseErrLed() { ledState.errPulses++; }
void pulseClipLed() { ledState.clipPulses++; }
void blinkSysLed(uint16_t, uint16_t) {}
void blinkErrLed(uint16_t, uint16_t) {}
void blinkClipLed(uint16_t, uint16_t) {}
void updateLed(Ledstruct_t *) {}
void updateLeds() {}
void setLed(Ledstruct_t *, uint8_t) {}
void setClipLed(uint8_t on) { ledState.clipLed = on; }
void setErrLed(uint8_t on) { ledState.errLed = on; }
void setSysLed(uint8_t on) { ledState.sysLed = on; }

// ---------------- Persistent storage (flash_helpers.h) ----------------
bool Flash_Init() { return true; }

bool Flash_Write(uint16_t adr, uint16_t dat) {
	HostPlatform::flash()[adr] = dat;
	return true;
}

bool Flash_Read(uint16_t adr, uint16_t *buf, bool) {
	auto it = HostPlatform::flash().find(adr);
	if (it == HostPlatform::flash().end()) {
		return false;
	}
	*buf = it->second;
	return true;
}

bool Flash_ReadWriteDefault(uint16_t adr, uint16_t *buf, uint16_t def) {
	if (!Flash_Read(adr, buf)) {
		Flash_Write(adr, def);
		*buf = def;
		return false;
	}
	return true;
}

void Flash_Dump(std::vector<std::tuple<uint16_t, uint16_t>> *result, bool) {
	for (const auto &entry : HostPlatform::flash()) {
		result->emplace_back(entry.first, entry.second);
	}
}

bool Flash_Format() {
	HostPlatform::flash().clear();
	return true;
}

void Flash_Write_Defaults() {}
bool OTP_Write(uint16_t, uint64_t) { return false; }
bool OTP_Read(uint16_t, uint64_t *) { return false; }
