# OpenFFBoard firmware unit tests

Functional unit tests for the hardware independent parts of the firmware.
The tests run on the development PC and in GitHub Actions. No board, debugger or ARM toolchain is required.

## Concept

The **unmodified firmware sources** (`FFBoard/Src/*.cpp`) are compiled with the native compiler of the PC
and linked against a **fake platform** instead of the STM32 HAL, FreeRTOS and TinyUSB:

```
 test cases (cases/)         what should happen
     |
 mocks (mocks/)              MockCommandHandler, MockMotorDriver, MockEncoder, MockCANPort ...
 support (support/)          fixtures, simulated PID host, test control of the fake platform
     |
 firmware sources            CmdParser, CommandInterface, HidFFB, EffectsCalculator ... (real code)
     |
 fake platform (platform/)   HAL, FreeRTOS, TinyUSB, flash, clock (recording fakes)
```

Properties of the fake platform:

* **Time is simulated.** `HAL_GetTick()` and `micros()` only change when a test advances the clock
  (`HostPlatform::advanceMs`). Tests are deterministic and do not wait.
* **No scheduler.** Threads are registered but only run when a test executes them until they block
  (`HostRtos::runUntilBlocked`, `HostRtos::runUntilIdle` or `process()` of the fixtures).
* **Everything sent to the outside is recorded**: CDC data and HID reports (`HostUsb`), uart bytes,
  flash content and LEDs (`HostPlatform`), CAN frames (`MockCANPort`), motor torque (`MockMotorDriver`).

Framework: [doctest](https://github.com/doctest/doctest) 2.4.11 (single header, MIT license, `third_party/doctest.h`).

## Running the tests

Requirements: CMake >= 3.16 and gcc/g++ with C++20 support (MinGW-w64 on Windows).

```sh
cd Firmware/Tests
cmake -S . -B build                # Windows with MinGW: add  -G "MinGW Makefiles"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

or with presets (CMake >= 3.21): `cmake --preset host && cmake --build --preset host && ctest --preset host`
(`host-mingw` on Windows).

The test executable can be started directly for more control:

```sh
build/ffb_unit_tests                        # all tests, prints details of known issues
build/ffb_unit_tests -ts=CmdParser          # one test suite
build/ffb_unit_tests -tc="*sine*"           # test cases by name
build/ffb_unit_tests --list-test-cases
build/ffb_unit_tests -s                     # also print successful assertions
```

### Build options

| Option | Default | Description |
|---|---|---|
| `FFB_TESTS_USE_DSP` | `AUTO` | Build with `USE_DSP_FUNCTIONS` and CMSIS-DSP like the hardware targets. `AUTO` enables it if the `Firmware/Libraries/CMSIS-DSP` submodule is checked out. With `OFF` the fallback math of the firmware is tested. Both variants must pass. |
| `FFB_TESTS_SANITIZE` | `OFF` | Address and undefined behaviour sanitizers (Linux/macOS). |

## Directory layout

| Path | Content |
|---|---|
| `cases/test_*.cpp` | Test cases. Every file is picked up automatically. |
| `mocks/` | Implementations of firmware class interfaces for tests. |
| `fakes/` | Headers replacing firmware headers (currently only `Axis.h`). |
| `support/` | Fixtures and helpers: `FirmwareFixture.h`, `FfbFixture.h`, `PidHost.h`, `TestHelpers.h`, `Host*.h`. |
| `platform/include` | Fake `main.h` (HAL), FreeRTOS, CMSIS headers and the `target_constants.h` of the test "target". |
| `platform/src` | Implementation of the fake platform. |
| `CMakeLists.txt` | Build. Contains the list of firmware sources under test. |

## Test suites

| Suite | File | Tested firmware |
|---|---|---|
| `CmdParser` | `test_cmdparser.cpp` | String command syntax, addressing, buffering, malformed input |
| `CommandExecution`, `CommandHandler` | `test_command_execution.cpp` | Command thread, access flags, internal commands, reply distribution, handler registry |
| `StringCommandInterface`, `CDC_CommandInterface`, `UART_CommandInterface` | `test_string_command_interfaces.cpp` | Reply formatting, end to end tests text in -> text out |
| `HID_CommandInterface` | `test_hid_command_interface.cpp` | Vendor HID command reports |
| `CAN_CommandInterface` | `test_can_command_interface.cpp` | CAN command frames |
| `HidFFB` | `test_hid_ffb.cpp` | HID PID report handling, effect allocation, axis magnitudes from directions |
| `EffectsCalculator` | `test_effects_calculator.cpp` | Force calculation of all effect types, gains, envelope, filters, settings |
| `Encoder`, `MotorDriver` | `test_device_mocks.cpp` | Base class interfaces and the mocks |
| `platform` | `test_platform.cpp` | Self test of the fake platform |

## Mocks and helpers

| Class | Use |
|---|---|
| `MockCommandHandler` | Command handler with one command per access type and flag. Target for all command interface tests. |
| `MockCommandInterface` | Command interface without transport. Injects parsed commands, records results and broadcasts. |
| `MockMotorDriver` | `MotorDriver` recording torque, start/stop, emergency stop. Readiness and slew rate are controlled by the test. |
| `MockEncoder` | `Encoder` with a position set by the test in counts, rotations or degrees. |
| `MockCANPort` | `CANPort` recording sent frames. Received frames are injected in interrupt context. |
| `PidHost` | Simulates the PID driver of the host: create effect, set effect/condition/periodic..., effect operation, device control. |
| `Axis` (fake) | Replaces the real axis for the effect calculation: metrics are set by the test, the effect torque is recorded. |
| `CommandSystemFixture` | Resets everything, owns the command thread. `process()` runs all threads until idle. |
| `FfbFixture` | Effects calculator + `HidFFB` + `PidHost` + two fake axes. |

`MockMotorDriver` and `MockEncoder` provide `classEntry()` to make them selectable in a `ClassChooser`.
`Axis::driverChooser` and `Axis::encoderChooser` are public, so a test can select the mocks in a real axis
without changing the firmware (see next steps).

## Writing tests

```cpp
#include "doctest.h"
#include "FfbFixture.h"

TEST_SUITE("EffectsCalculator") {

TEST_CASE_FIXTURE(FfbFixture, "constant force is passed to the axis") {
	uint8_t cf = startEffect(FFB_EFFECT_CONSTANT);  // create, set effect and start like a game
	host.setConstantForce(cf, 10000);               // PID report
	CHECK(torque(0) == -10000);                     // torque passed to the X axis
}

}
```

Rules:

* Use a fixture. It resets the global state of the firmware so tests do not depend on their order.
* Test through the public interfaces (reports, commands, class interfaces), not private members.
* One behaviour per test case. The name is a statement that is true if the test passes.
* Never sleep. Advance the simulated time.

### Naming conventions

* **`characterization: ...`** documents what the firmware does today where the correct behaviour still has to be
  verified (for example the axis magnitudes against the PID specification). These tests protect against
  unintended changes during refactoring but do not claim the behaviour is correct.
* **`KNOWN_ISSUE("...")`** marks a test that describes the intended behaviour but fails because of a suspected
  defect in the firmware. The suite stays green while the defect exists. When the firmware is fixed the test
  is reported as *"Should have failed but didn't"* and the marker must be removed.
  `KNOWN_ISSUE_UB` is the same for undefined behaviour and is skipped in sanitizer builds.

List all known issues:

```sh
grep -rn "KNOWN_ISSUE\|KNOWN ISSUE" cases/
```

### Adding firmware sources to the tests

1. Add the file to `FIRMWARE_SOURCES` in `CMakeLists.txt`.
2. If it does not compile or link because something of the hardware is missing, extend the fake platform
   (`platform/include/main.h`, `platform/src/host_platform.cpp`). Keep the fakes minimal.
3. Do not add `#ifdef` for tests to firmware sources. If a class can not be used on the host because of its
   dependencies, give it a test double in `fakes/` or a stub in `platform/src/host_stubs.cpp`.

## Limitations

* The host is a 64 bit little endian machine with a different compiler backend. Stack usage, timing, alignment
  faults and float to integer conversions of out of range values are not covered.
* No preemption. Race conditions between threads and interrupts can not be found.
* `Axis`, the main classes, the real motor drivers and encoders and `SystemCommands` are not compiled yet.
* The USB descriptors are not parsed. The report layouts are compared with constants in the tests.

## Next steps

* Verification of the `characterization` tests of `HidFFB` against PID 1.01 (direction handling,
  condition blocks, device control, block load status).
* Integration test with the real `Axis.cpp`: PID report -> effect -> axis -> `MockMotorDriver` torque with the
  position from `MockEncoder`. Requires a few more HAL declarations for the headers included by `Axis.h`
  and `#include <cmath>` in `Axis.cpp`.
* Tests for `Axis` (scaling, endstop, speed and slew rate limiter), `Filters` and `ErrorHandler`.
