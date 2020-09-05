![Logo](doc/ffboard_logo.svg)
# Open FFBoard
The Open FFBoard is an open source force feedback interface with the goal of creating a platform for highly compatible simulation devices.

This firmware is optimized for the Open FFBoard.
At the moment the software is far from finished. Features may not work completely or contain errors.

More documentation about this project is on the [hackaday.io page](https://hackaday.io/project/163904-open-ffboard).

The hardware designs are found under [OpenFFBoard-hardware](https://github.com/Ultrawipf/OpenFFBoard-hardware).

The GUI for configuration is found at [OpenFFBoard-configurator](https://github.com/Ultrawipf/OpenFFBoard-configurator).

These git submodules can be pulled with `git submodule init` and `git submodule update`

Updates often require matching firmware and GUI versions!

## Documentation
Documentation will be added in the [GitHub Wiki](https://github.com/Ultrawipf/OpenFFBoard/wiki)

For discussion and progress updates we have a [Discord server](https://discord.com/invite/gHtnEcP).

### Extensions
The modular structure means you are free to implement your own main classes.
Take a look into the FFBoardMain and ExampleMain class files in the UserExtensions folder.
Helper functions for parsing CDC commands and accessing the flash are included.

The firmware is class based in a way that for example the whole main class can be changed at runtime and with it for example even the usb device and complete behavior of the firmware.

For FFB the motor drivers, button sources or encoders also have their own interfaces.

A simplified command parser is available and recommended for setting parameters at runtime. (see `CmdParser.h` and `CommandHandler.h` and the example main)

Callbacks like command parsers and timers or external interrupts are also based on virtual classes that can be implemented to add this functionality to any other module. Take a look at `global_callbacks.cpp` for some of them.



### Copyright notice:
Some parts of this software may contain source code by ST.
The license applying to these files is found in the header of the file.
For all other parts the LICENSE file applies.
