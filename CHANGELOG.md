### Changes this version:
- Added independend friction and inertia effects to axis
- ODrive class can save encoder position offset
- Reverted the forza fix for 2 axis setups. 
  - TODO: test and report if behaviour works for all games with the angle always being used for 1 axis modes (Games must send 90° on X axis effects instead of 0°).