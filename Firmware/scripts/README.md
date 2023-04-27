## Helper scripts

### generate_memory.py
Generates the `eeprom_addresses.c` and `eeprom_addresses.h` files based on the `memory_map.csv` file and does a sanity check.
Always use this script and update the csv file instead of manually altering the source code for these files to keep track of addresses.