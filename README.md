<div align="center">
    <a href="https://github.com/Ultrawipf/OpenFFBoard">
        <img width="200" height="200" src="doc/img/ffboard_logo.svg">
    </a>
	<br>
	<br>
	<div style="display: flex;">
		<a href="https://discord.gg/gHtnEcP">
            <img src="https://img.shields.io/discord/704355326291607614">
		</a>
		<a href="https://github.com/Ultrawipf/OpenFFBoard/stargazers">
            <img src="https://img.shields.io/github/stars/Ultrawipf/OpenFFBoard">
		</a>
		<a href="https://github.com/Ultrawipf/OpenFFBoard/actions/workflows/build-firmware.yml">
            <img src="https://github.com/Ultrawipf/OpenFFBoard/actions/workflows/build-firmware.yml/badge.svg?branch=master">
		</a>
	</div>
</div>



# Open FFBoard
The Open FFBoard is an open source force feedback interface with the goal of creating a platform for highly compatible FFB simulation devices like steering wheels and joysticks.

This firmware is optimized for the Open FFBoard and mainly designed for use with DD steering wheels.
Remember this software is experimental and is intended for advanced users. Features may contain errors and can change at any time.

More documentation about this project is on the [hackaday.io page](https://hackaday.io/project/163904-open-ffboard).

The hardware designs are found under [OpenFFBoard-hardware](https://github.com/Ultrawipf/OpenFFBoard-hardware).

The GUI for configuration is found at [OpenFFBoard-configurator](https://github.com/Ultrawipf/OpenFFBoard-configurator).

These git submodules can be pulled with `git submodule init` and `git submodule update`

Updates often require matching firmware and GUI versions!

## Documentation
Documentation will be updated in the [GitHub Wiki](https://github.com/Ultrawipf/OpenFFBoard/wiki).

Available commands are listed on the [Commands wiki page](https://github.com/Ultrawipf/OpenFFBoard/wiki/Commands)

Code summary and documentation of the latest stable version is available as a [Doxygen site](https://ultrawipf.github.io/OpenFFBoard/doxygen/).

For discussion and progress updates we have a [Discord server](https://discord.com/invite/gHtnEcP).

### Extensions
The modular structure means you are free to implement your own main classes.
Take a look into the FFBoardMain and ExampleMain class files in the UserExtensions folder.
Helper functions for parsing CDC commands and accessing the flash are included.

The firmware is class based in a way that for example the whole main class can be changed at runtime and with it for example even the usb device and complete behavior of the firmware.

For FFB the motor drivers, button sources or encoders also have their own interfaces.

A unified command system supporting different interfaces is available and recommended for setting parameters at runtime. (see `CommandHandler.h` and the example mainclass)


### Copyright notice:
Some parts of this software may contain third party libraries and source code licenced under different terms.
The license applying to these files is found in the header of the file.
For all other parts in the `Firmware/FFBoard` folder the LICENSE file applies.
