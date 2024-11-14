### Changes this version:
- Fixed BISS-C encoder sometimes overflowing one rotation at startup
- Added BISS-C direction inversion function (Default true). Most BISS-C encoders count CW while most others and TMC count CCW.
- Standardized encoder counting direction counting up in CCW direction as a more common industrial standard
- Fixed idle spring effect not working before first save
- Retuned speed limiter function. Removed averaging. Should be more stable for high resolution encoders if high bandwidth speed filter preset is selected
- Force ramps up slowly on potential sharp position changes such as recentering
- FFB led now shows FFB state. On when FFB actuators enabled. Still blinks on clipping