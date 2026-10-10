/*
 * Self tests of the fake platform layer.
 * If these fail the results of all other tests are meaningless.
 */
#include "doctest.h"

#include "HostPlatform.h"
#include "HostRtos.h"
#include "cppmain.h"
#include "thread.hpp"
#include "flash_helpers.h"

namespace {

class CountingThread : public cpp_freertos::Thread {
public:
	CountingThread() : Thread("TEST", 64, 1) { Start(); }
	void Run() override {
		while (true) {
			WaitForNotification();
			activations++;
		}
	}
	uint32_t activations = 0;
};

class PollingThread : public cpp_freertos::Thread {
public:
	PollingThread() : Thread("POLL", 64, 1) { Start(); }
	void Run() override {
		while (true) {
			Delay(1); // Never waits for a notification
			loops++;
		}
	}
	uint32_t loops = 0;
};

} // namespace

TEST_SUITE("platform") {

TEST_CASE("simulated time drives HAL_GetTick and micros") {
	HostPlatform::reset();
	CHECK(HAL_GetTick() == 0);
	CHECK(micros() == 0);

	HostPlatform::advanceUs(1500);
	CHECK(HAL_GetTick() == 1);
	CHECK(micros() == 1500);

	HostPlatform::setTimeMs(1000);
	CHECK(HAL_GetTick() == 1000);
	CHECK(micros() == 1000000);
}

TEST_CASE("interrupt context can be simulated") {
	HostPlatform::reset();
	CHECK_FALSE(inIsr());
	{
		HostPlatform::IsrContext isr;
		CHECK(inIsr());
	}
	CHECK_FALSE(inIsr());
}

TEST_CASE("flash emulation stores values and reports missing addresses") {
	HostPlatform::reset();
	uint16_t value = 0;
	CHECK_FALSE(Flash_Read(0x123, &value));
	CHECK(Flash_Write(0x123, 0xBEEF));
	CHECK(Flash_Read(0x123, &value));
	CHECK(value == 0xBEEF);
	CHECK(HostPlatform::flash().at(0x123) == 0xBEEF);
}

TEST_CASE("threads only run when explicitly executed and stop when they would block") {
	HostPlatform::reset();
	HostRtos::reset();
	CountingThread thread;

	HostRtos::runUntilBlocked(thread);
	CHECK(thread.activations == 0); // Nothing pending

	thread.Notify();
	CHECK(thread.activations == 0); // Not executed implicitly
	CHECK(HostRtos::pendingNotifications(thread) == 1);

	HostRtos::runUntilBlocked(thread);
	CHECK(thread.activations == 1);
	CHECK(HostRtos::pendingNotifications(thread) == 0);

	thread.Notify();
	CHECK(HostRtos::runUntilIdle() == 1);
	CHECK(thread.activations == 2);
}

TEST_CASE("threads are unregistered when destroyed") {
	size_t before = HostRtos::taskCount();
	{
		CountingThread thread;
		CHECK(HostRtos::taskCount() == before + 1);
	}
	CHECK(HostRtos::taskCount() == before);
}

TEST_CASE("polling threads are stopped by the delay budget and advance time") {
	HostPlatform::reset();
	HostRtos::reset();
	HostRtos::setDelayBudgetMs(50);
	PollingThread thread;

	HostRtos::runUntilBlocked(thread);
	CHECK(thread.loops == 49);
	CHECK(HAL_GetTick() == 50);
	HostRtos::reset();
}

} // TEST_SUITE
