/*
 * host_stubs.cpp
 *
 * Minimal definitions for firmware classes which are referenced by the tested
 * sources but are too hardware bound to be compiled for the host.
 *
 * Keep this file as small as possible. Anything defined here is NOT tested.
 * If a class from here becomes relevant for tests compile the real source instead.
 */
#include "SystemCommands.h"

// SystemCommands.cpp (reboot, dfu, chip ids, heap stats...) is not compiled.
// Only the global flags evaluated by CommandHandler and ErrorHandler are provided.
bool SystemCommands::debugMode = false;
bool SystemCommands::errorPrintingEnabled = true;
SystemCommands *SystemCommands::systemCommandsInstance = nullptr;
void SystemCommands::replyErrors(std::vector<CommandReply> &) {}
