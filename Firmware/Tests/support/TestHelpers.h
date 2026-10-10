/*
 * TestHelpers.h
 *
 * Small helpers shared by all test cases.
 */
#ifndef HOSTTEST_TESTHELPERS_H_
#define HOSTTEST_TESTHELPERS_H_

#include "doctest.h"

#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <memory>
#include <thread>

/**
 * Marks a test case that describes the intended behaviour but currently fails
 * because of a suspected defect in the firmware.
 *
 *   TEST_CASE("parser rejects text values" * KNOWN_ISSUE("hangs in CmdParser::parse")) { ... }
 *
 * The test suite stays green while the issue exists. As soon as the firmware is fixed
 * the test case is reported as failed ("should have failed but didn't") and the marker
 * has to be removed. List all of them with: grep -rn KNOWN_ISSUE cases/
 */
#define KNOWN_ISSUE(text) doctest::description("KNOWN ISSUE: " text) * doctest::should_fail()

/**
 * Checks that two numbers differ by not more than an absolute tolerance.
 * Used for forces which pass float calculations and integer truncation.
 */
#define CHECK_NEAR(actual, expected, tolerance) 	do { 		const double actual_ = static_cast<double>(actual); 		const double expected_ = static_cast<double>(expected); 		INFO(#actual " = " << actual_ << ", expected " << expected_ << " +- " << (tolerance)); 		CHECK(std::fabs(actual_ - expected_) <= static_cast<double>(tolerance)); 	} while (0)

/**
 * Like KNOWN_ISSUE for defects causing undefined behaviour (e.g. out of bounds access inside an object).
 * These tests are skipped in sanitizer builds because the sanitizer aborts the whole test run.
 */
#ifdef HOSTTEST_SANITIZERS
#define KNOWN_ISSUE_UB(text) doctest::description("KNOWN ISSUE (undefined behaviour): " text) * doctest::skip()
#else
#define KNOWN_ISSUE_UB(text) doctest::description("KNOWN ISSUE (undefined behaviour): " text) * doctest::should_fail()
#endif

/**
 * Runs a function in a separate thread and waits for it to finish.
 * Use for code that is suspected to never return. The state used by the function
 * must be owned by the function object (capture shared_ptr by value) because the
 * thread keeps running if the timeout expires.
 * @return true if the function returned in time
 */
inline bool finishesWithin(std::chrono::milliseconds timeout, std::function<void()> fn) {
	auto done = std::make_shared<std::promise<void>>();
	std::future<void> future = done->get_future();
	std::thread([fn = std::move(fn), done]() mutable {
		fn();
		fn = nullptr; // Release captured state before the waiting thread continues
		done->set_value();
	}).detach();
	return future.wait_for(timeout) == std::future_status::ready;
}

#endif
