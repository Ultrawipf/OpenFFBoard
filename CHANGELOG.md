### Changes this version:
- Fixed a possible crash if unparsable/too large numbers are sent in a command


### Changes in (v1.11.x):
- Added new subproject for third party devkits (F407DISCO target)
- Added serialfx effect manager for a command based FFB mainclass (Instead of PID FFB)
- Added Serial FFB mainclass mode
- Added mosfet BBM time to TMC hardware selection
- TMC enable pin is set when TMC debug mode starts
- Added option to change SPI speed for buttons. Can be helpful if the connection is unreliable
- Added back second VESC instance for 2 axis vesc setups
- Separate motor driver selection lists per axis (No double odrive/vesc/tmc instance options in motor driver lists)