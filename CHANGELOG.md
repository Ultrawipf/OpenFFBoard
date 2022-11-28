### Changes this version:
- Changed default power from 2000 to 5000 as 2000 is not enough to calibrate many motors
- Internal change moving effects into effectscalculator to simplify managing effects from different sources
- Effect intensity tuning value now only affects game effects. Fixes the effect intensity incorrectly affecting the endstop.

### Changes since 1.9.x:
- Added local encoder index option to reload a previously stored offset
- Fixed an issue with 2 axis FFB effects on second axis
- Added TMC4671 biquad filter option
  - Lowpass, notch and peak modes (fixed Q factor, saved frequency)
- Improved BISS performance when used with TMC
- Fixed an issue with live effects statistics jumping to 0 using double buffers
- Added missing command flags and help messages