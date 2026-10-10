/*
 * HostPlatform.h
 *
 * Test control interface of the fake hardware layer (platform/src/host_platform.cpp):
 * simulated time, interrupt context, flash storage, status LEDs and the external UART.
 */
#ifndef HOSTTEST_HOSTPLATFORM_H_
#define HOSTTEST_HOSTPLATFORM_H_

#include <cstdint>
#include <map>
#include <string>

namespace HostPlatform {

/** Resets time to 0, leaves interrupt context, clears the flash, LED counters and UART buffers */
void reset();

// --- Simulated time. HAL_GetTick(), micros() and the RTOS tick are derived from it ---
uint64_t nowUs();
void setTimeUs(uint64_t us);
void advanceUs(uint64_t us);
inline void setTimeMs(uint64_t ms) { setTimeUs(ms * 1000); }
inline void advanceMs(uint64_t ms) { advanceUs(ms * 1000); }

/** Simulates interrupt context. inIsr() returns true while set */
void setInIsr(bool isr);

/** RAII helper to execute a block in simulated interrupt context */
struct IsrContext {
	IsrContext() { setInIsr(true); }
	~IsrContext() { setInIsr(false); }
};

/** Emulated EEPROM content used by Flash_Read/Flash_Write. address -> value */
std::map<uint16_t, uint16_t> &flash();

struct LedState {
	uint32_t sysPulses = 0;
	uint32_t errPulses = 0;
	uint32_t clipPulses = 0;
	bool clipLed = false;
	bool errLed = false;
	bool sysLed = false;
};
LedState &leds();

struct UartState {
	std::string tx;			   //!< All bytes transmitted via the HAL uart functions
	uint32_t txTransfers = 0;  //!< Amount of transmit calls
	uint32_t rxArmedCount = 0; //!< Amount of HAL_UART_Receive_IT calls
	uint32_t initCount = 0;	   //!< Amount of HAL_UART_Init calls
	uint32_t rxLost = 0;	   //!< Bytes received while no receive transfer was armed
};
UartState &uart();

/**
 * Simulates bytes received on the external uart.
 * Every byte is stored in the buffer armed by HAL_UART_Receive_IT and the receive
 * complete callback of all UartHandlers is called in interrupt context like on the target.
 * Bytes are lost if the firmware did not rearm the receive interrupt.
 */
void uartReceive(const std::string &data);

/** Simulates the transmit complete interrupt of the external uart. Releases the port for the next transfer */
void uartCompleteTx();

} // namespace HostPlatform

#endif
