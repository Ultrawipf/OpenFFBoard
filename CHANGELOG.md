### Changes this version from 1.16 to 1.15:
- Added MyActuator RMD CAN support class.
  - Temporary implementation until CAN protocol changes. Usable but might be improved in the future
- Fixed issues in CAN analog class for packet 2. Allow shorter frames
- F407: ADC now triggered by timer to reduce interrupt frequency
- Using analog VREF for voltage sensing (better accuracy with unstable 3.3V)
- Added chip temperature readout

Internal changes:
- CAN port interfaces rewritten
- HAL updated
- Migrated to CPP20
- Upgraded to tinyusb 0.17
- Improved microsecond counter implementation