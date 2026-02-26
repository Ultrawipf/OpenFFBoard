### Changes this version:
- Added SPI speed selector to MagnTek encoders
- Added "reg" and "save" commands to MagnTek encoder. Allows programming MT6835 encoders (debug=1 mode required!)
- Set ABN encoder filter to 5 for F407 and F407_DISCO. Should improve encoder stability in noisy environments.

### Changes in 1.16:

Internal changes:
- CAN port interfaces rewritten
- HAL updated
- Migrated to CPP20
- Upgraded to tinyusb 0.17
- Improved microsecond counter implementation
- Added MyActuator RMD CAN support class.
  - Temporary implementation until CAN protocol changes. Usable but might be improved in the future
- Fixed issues in CAN analog class for packet 2. Allow shorter frames
- F407: ADC now triggered by timer to reduce interrupt frequency
- Using analog VREF for voltage sensing (better accuracy with unstable 3.3V)
- Added chip temperature readout
- Added remote CAN button/analog source mainclass
- Added exponential torque postprocessing for game effects
- Reformatted USB serial string as hex and added command to request UID as hex string
- Added device name to USB Product name
- Added support for F407 OTP section
- Added support for MagnTek MT6835 via SPI (SPI3 port, MagnTek encoder class)