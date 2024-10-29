### Changes this version:
- Fixed BISS-C encoder sometimes overflowing one rotation at startup
- Added BISS-C direction inversion function (Default true). Most BISS-C encoders count CW while most others and TMC count CCW.
- Standardized encoder counting direction counting up in CCW direction as a more common industrial standard
- Fixed idle spring effect not working before first save