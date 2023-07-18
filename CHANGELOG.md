### Changes this version:
- Save TMC space vector PWM mode in flash instead of unused hall direction. Should be usually on for BLDC motors if the star point is isolated.
- Prefer using the motors flux component to dissipate energy with the TMC4671 instead of the brake resistor.
 
### Changes in 1.13.x
- Added PWM direction toggle
- Added basic iterative TMC PI autotuning
- Fixed issues with CAN transmission with multiple axes
- Added SSI encoder support (AMT232B)
- Fixed SPI buttons not working (SPI2 DMA on F407)
- Dynamic TMC encoder alignment current based on current limit
- Added effect monitoring per axis
- Added uid command (`sys.uid?` returns first 64 bits as val and second 32 as adr)