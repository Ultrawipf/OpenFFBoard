### v1.17.1
- Fixed CAN filters for extended IDs
- VESC uses CAN filters
- Reduce PendSV interrupts by delaying the idle loop by default

### v1.17.0
- Added SPI speed selector to MagnTek encoders
- Added "reg" and "save" commands to MagnTek encoder. Allows programming MT6835 encoders (debug=1 mode required!)
- Set ABN encoder filter to 5 for F407 and F407_DISCO. Should improve encoder stability in noisy environments.
- Fixed Magntek encoder forwarding causing choppy FFB
- Fixed TMC4671 ext encoder having inverted forces if previously inverted by dual enc mode

### v1.16.6
- Added SPI speed selector to MagnTek encoders
- Added "reg" and "save" commands to MagnTek encoder. Allows programming MT6835 encoders (debug=1 mode required!)
- Set ABN encoder filter to 5 for F407 and F407_DISCO. Should improve encoder stability in noisy environments.

### v1.16.5
- Reformatted USB serial string as hex and added command to request UID as hex string
- Added device name to USB Product name
- Added support for F407 OTP section
- Added support for MagnTek MT6835 via SPI (SPI3 port, MagnTek encoder class)

### v1.16.4
- Added exponential torque postprocessing for game effects

### v1.16.3
- Added remote CAN button/analog source mainclass

### v1.16.2
- Inverted Y axis direction vector magnitude
  - Fixes 2 axis setups in XPforce and DCS and other flight sims
- Changed 2 axis conditional effects to ignore direction vectors (Fixes DCS)
- Modified HID 2 axis descriptor, added back second direction for compliance
- Fixed chip temp sometimes glitching
- TMC debug mode: Changed openloopspeed command to use torque mode instead of raw PWM. Added new openloopspeedpwm to control raw PWM.
- CAN bus corrected packet length when packet is sent as command
- Corrected CAN speed preset in can bridge GVRET mode (savvycan works again)
- Using interrupt transfer for TMC4671 encoder forwarding (Fixed again)

### v1.16.0
- Added MyActuator RMD CAN support class.
  - Temporary implementation until CAN protocol changes. Usable but might be improved in the future
- Fixed issues in CAN analog class for packet 2. Allow shorter frames
- F407: ADC now triggered by timer to reduce interrupt frequency
- Using analog VREF for voltage sensing (better accuracy with unstable 3.3V)
- Added chip temperature readout
- CAN port interfaces rewritten
- HAL updated
- Migrated to CPP20
- Upgraded to tinyusb 0.17
- Improved microsecond counter implementation

### v1.15.1
- Fixed BISS-C encoder sometimes overflowing one rotation at startup
- Added BISS-C direction inversion function (Default true). Most BISS-C encoders count CW while most others and TMC count CCW.
- Standardized encoder counting direction counting up in CCW direction as a more common industrial standard
- Fixed idle spring effect not working before first save
- Retuned speed limiter function. Removed averaging. Should be more stable for high resolution encoders if high bandwidth speed filter preset is selected
- Force ramps up slowly on potential sharp position changes such as recentering
- FFB led now shows FFB state. On when FFB actuators enabled. Still blinks on clipping

### v1.15.0
- Added independend friction and inertia effects to axis
- ODrive class can save encoder position offset
- Reverted the forza fix for 2 axis setups. 
  - TODO: test and report if behaviour works for all games with the angle always being used for 1 axis modes (Games must send 90° on X axis effects instead of 0°).

### v1.14.4
- TMC E-Stop handled even during calibration by pausing and disabling driver
- E-Stop checked correctly after startup. You can now disable force and delay startup by setting E-Stop during startup.
- Digital and Analog sources are disabled by default
- Biss-C 1 rotation offset glitch at first packet fixed
- Reverted CAN retransmission to enabled temporarily. Fixes 2 axis ODrive issues.

### v1.14.3
- Only activate brake resistor if vint and vext are >6.5V. Prevents board from activating resistor if only usb powered and a fault reset loop
- Changed behaviour of direction enable and axis enable bits in set_effect report to always apply direction vector
    - Fix for Forza Motorsport

### v1.14.2
- Added local button pulse mode

### v1.14.0
- Save TMC space vector PWM mode in flash. Should be usually on for BLDC motors if the star point is isolated.
- Allow using the motors flux component to dissipate energy with the TMC4671 instead of the brake resistor. May cause noticable braking in the motor but takes stress off the resistor.
- Axis speed limiter usable and saved in flash.
- Removed unused hall direction flash setting.

### v1.13.3
- Added uid command (`sys.uid?` returns first 64 bits as val and second 32 as adr)

### v1.13.2
- Added effect monitoring per axis

### v1.13.1
- Added PWM direction toggle

### v1.13.0
- Added basic iterative TMC PI autotuning
- Fixed issues with CAN transmission with multiple axes
- Added SSI encoder support (AMT232B)
- Fixed SPI buttons not working (SPI2 DMA on F407)
- Dynamic TMC encoder alignment current based on current limit

### v1.12.1
- Added part of unique serial number to usb interface names for easier identification of multiple devices
- Enabled Simplemotion for F407_DISCO build

### v1.12.0
- Added support for Simplemotion V2 (Ioni/Argon motor drivers)

### v1.11.2
- Fixed a possible crash if unparsable/too large numbers are sent in a command
- Removed unused direction field from descriptor in 2 axis desc
- Workarounds for 2 axis conditional effects giving condition blocks priority over direction angles

### v1.11.1
- Added new subproject for third party devkits (F407DISCO target)
- Added serialfx effect manager for a command based FFB mainclass (Instead of PID FFB)
- Added Serial FFB mainclass mode
- Added mosfet BBM time to TMC hardware selection
- TMC enable pin is set when TMC debug mode starts
- Added option to change SPI speed for buttons. Can be helpful if the connection is unreliable
- Added back second VESC instance for 2 axis vesc setups
- Separate motor driver selection lists per axis (No double odrive/vesc/tmc instance options in motor driver lists)

### v1.10.1
- Changed default power from 2000 to 5000 as 2000 is not enough to calibrate many motors
- Internal change moving effects into effectscalculator to simplify managing effects from different sources
- Effect intensity tuning value now only affects game effects. Fixes the effect intensity incorrectly affecting the endstop.

### v1.10.0
- Added local encoder index option to reload a previously stored offset
- Fixed an issue with 2 axis FFB effects on second axis
- Added TMC4671 biquad filter option
  - Lowpass, notch and peak modes (fixed Q factor, saved frequency)
- Improved BISS performance when used with TMC
- Fixed an issue with live effects statistics jumping to 0 using double buffers
- Added missing command flags and help messages

### v1.9.7
- Fixed CDC serial port sometimes losing replies on with some USB ports

### v1.9.6: 
- Added analog filter option
- Main effect loop runs in higher priority thread than idle
- Added ADS111X analog source
- Added user configurable axis encoder ratios for setups with reductions
- Added effect filter option (Speed/accel filter presets for different encoders)
- Added effects monitoring
- Added some analog autorange margin
- Added min/max commands to analog processing for manual ranges
- Added analog processing functions to ADS111X
- Selecting a "none" encoder will remove the axis value. Allows analog inputs to be used as the primary axis.
- Added constant force rate command
- Highly improved uart command stability (default baud rate 115200)
- Added command to check command flags (cls.cmdinfo?cmdid)
- Added advanced filter mode to switch between custom and default conditional effect output filters ("fx.filterProfile_id")
- Automatic flash erase condition changed from major version change to separate flash version counter

### v1.8.8
- Rescaled endstop to encoder angle (makes strength feel the same at every range)
- Changed SPI button saved count from 0-63 to 1-64 (will invalidate your setting)
- Added CAN next frame length command to send frames with different headers
- Emergency stop can be reset and only disables torque
- Added estop command
- Optimized string based command interfaces
- Effects honor the gain setting (Makes Forza Horizon work)

### v1.8.7
- FFBWheel and FFBJoystick classes split for 1 and 2 axis FFB (Allows to use different HID descriptors)
- Added a single axis HID descriptor (currently not used for compatibility reasons. enable by defining FFBWHEEL_USE_1AXIS_DESC)
- Default SPI button speed increased to 1.3MHz
- Added CAN button source
- Added CAN analog source
- Moved CAN and i2c speed settings to port class
- Fixed CAN bridge RTR frames
- HID interface sends ACKs
- Improved help command formatting and added flags
- Added I2C fast mode 400kHz
- Added BISS-C encoder
- Fixed MT Encoder
- Reworked TMC external encoder system
- Digital and analog source readout command


# Persistent changelog
Append changes at the top. 
The first section is used in release comments