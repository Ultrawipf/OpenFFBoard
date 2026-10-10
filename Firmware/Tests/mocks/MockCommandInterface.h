/*
 * MockCommandInterface.h
 *
 * A command interface without any transport and parser.
 * Tests inject already parsed commands and inspect the results that the command
 * thread passes back. Also used as a second interface to observe broadcasts.
 */
#ifndef HOSTTEST_MOCKCOMMANDINTERFACE_H_
#define HOSTTEST_MOCKCOMMANDINTERFACE_H_

#include "CommandInterface.h"
#include "FFBoardMainCommandThread.h"
#include <vector>

class MockCommandInterface : public CommandInterface {
public:
	struct Reply {
		std::vector<CommandResult> results;
		CommandInterface *originalInterface = nullptr;
	};

	/** Queues a parsed command and wakes up the command thread like a real interface */
	void queueCommand(const ParsedCommand &cmd) {
		pending.push_back(cmd);
		parserReady = true;
		FFBoardMainCommandThread::wakeUp();
	}

	/** Builds and queues a command for a handler */
	void queueCommand(CommandHandler &target, uint32_t cmdId, CMDtype type, int64_t val = 0, int64_t adr = 0) {
		ParsedCommand cmd;
		cmd.target = &target;
		cmd.cmdId = cmdId;
		cmd.type = type;
		cmd.val = val;
		cmd.adr = adr;
		cmd.instance = target.getCommandHandlerInfo()->instance;
		queueCommand(cmd);
	}

	bool getNewCommands(std::vector<ParsedCommand> &commands) override {
		commands = std::move(pending);
		pending.clear();
		parserReady = false;
		return !commands.empty();
	}

	void sendReplies(const std::vector<CommandResult> &results, CommandInterface *originalInterface) override {
		replies.push_back({results, originalInterface});
	}

	bool readyToSend() override {
		readyToSendCalls++;
		return ready;
	}

	void batchDone() override { batches++; }

	/** All results of all sendReplies calls in order */
	std::vector<CommandResult> allResults() const {
		std::vector<CommandResult> all;
		for (const Reply &r : replies) {
			all.insert(all.end(), r.results.begin(), r.results.end());
		}
		return all;
	}

	std::vector<ParsedCommand> pending;
	std::vector<Reply> replies;
	bool ready = true;
	uint32_t batches = 0;
	uint32_t readyToSendCalls = 0;
};

#endif
