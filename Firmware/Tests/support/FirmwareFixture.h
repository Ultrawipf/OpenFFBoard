/*
 * FirmwareFixture.h
 *
 * Common test fixtures. Use with TEST_CASE_FIXTURE(...).
 *
 * FirmwareFixture       Resets the fake platform and all global firmware state.
 * CommandSystemFixture  Additionally runs the main command thread so commands
 *                       received by any command interface are executed.
 */
#ifndef HOSTTEST_FIRMWAREFIXTURE_H_
#define HOSTTEST_FIRMWAREFIXTURE_H_

#include "HostPlatform.h"
#include "HostRtos.h"
#include "HostUsb.h"

#include "CDCcomm.h"
#include "CommandHandler.h"
#include "CommandInterface.h"
#include "ErrorHandler.h"
#include "FFBoardMainCommandThread.h"
#include "SystemCommands.h"

#include <string>
#include <vector>

struct FirmwareFixture {
	FirmwareFixture() { resetGlobals(); }
	virtual ~FirmwareFixture() { resetGlobals(); }

	static void resetGlobals() {
		HostPlatform::reset();
		HostUsb::reset();
		HostRtos::reset();
		ErrorHandler::clearAll();
		SystemCommands::debugMode = false;
		CommandHandler::setLogsEnabled(true);
		// Release the cdc port in case a test left a transfer unfinished
		CDCcomm::clearRemainingBuffer(0);
		CDCcomm::cdcFinished(0);
	}

	/** Returns the info strings of all currently active errors with a specific code */
	static std::vector<std::string> errorsWithCode(ErrorCode code) {
		std::vector<std::string> found;
		for (const Error &e : ErrorHandler::getErrors()) {
			if (e.isError() && e.code == code) {
				found.push_back(e.info);
			}
		}
		return found;
	}
};

struct CommandSystemFixture : FirmwareFixture {
	FFBoardMainCommandThread commandThread{nullptr};

	/**
	 * Lets the firmware threads run until all work is done.
	 * Completes pending CDC transfers like the USB stack does (tud_cdc_tx_complete_cb).
	 */
	void process() {
		for (int i = 0; i < 1000; i++) {
			uint32_t activations = HostRtos::runUntilIdle();

			// Transfers started by the threads or by the retry in the previous completion callback
			bool cdcSent = HostUsb::state().cdcWrites != cdcWritesSeen;
			cdcWritesSeen = HostUsb::state().cdcWrites;
			bool uartSent = HostPlatform::uart().txTransfers != uartTransfersSeen;
			uartTransfersSeen = HostPlatform::uart().txTransfers;

			if (cdcSent) {
				CDCcomm::cdcFinished(0);
			}
			if (uartSent) {
				HostPlatform::uartCompleteTx();
			}
			if (activations == 0 && !cdcSent && !uartSent) {
				break;
			}
		}
	}

private:
	uint32_t cdcWritesSeen = 0;
	uint32_t uartTransfersSeen = 0;
};

/** Counts how many handlers are registered in the global command handler list */
inline size_t registeredCommandHandlers() { return CommandHandler::getCommandHandlers().size(); }

#endif
