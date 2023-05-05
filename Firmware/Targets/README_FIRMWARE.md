## Firmware versions

Use the correct firmware for your board.
Flashing an incorrect firmware can result in a non working device or even damage to the hardware.
If you accidentially flashed an incorrect firmware stay in DFU mode (or use ST-link) and do a clean full erase.

Use the .hex file for updates.

The following builds are available:

* F407VG: Official FFBoard 1.2+
    - Includes all features. Requires VBUS connection
* F407VG_DISCO: Third party development kits (ST F407 Discovery board, different pin mapping)
    - See https://github.com/Ultrawipf/OpenFFBoard/wiki/Pinouts-and-peripherals#f407-disco-pinout
    - No VBUS required
* F411RE: FFBoard 1.0 (Only supports TMC driver!)