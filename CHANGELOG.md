### Changes this version:
- Added exponential torque postprocessing for game effects

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