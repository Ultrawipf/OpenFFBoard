### Changes since v1.8: 

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