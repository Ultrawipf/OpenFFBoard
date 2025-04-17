### Changes this version:
- Inverted Y axis direction vector magnitude
  - Fixes 2 axis setups in XPforce and DCS and other flight sims
- Changed 2 axis conditional effects to ignore direction vectors (Fixes DCS)
- Modified HID 2 axis descriptor, added back second direction for compliance
- Fixed chip temp sometimes glitching
- TMC debug mode: Changed openloopspeed command to use torque mode instead of raw PWM. Added new openloopspeedpwm to control raw PWM.
- CAN bus corrected packet length when packet is sent as command
- Corrected CAN speed preset in can bridge GVRET mode (savvycan works again)
- Using interrupt transfer for TMC4671 encoder forwarding (Fixed again)


### Changes in 1.16:
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