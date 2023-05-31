### Changes this version:
- Added uid command (`sys.uid?` returns first 64 bits as val and second 32 as adr)
 
### Changes in 1.13.x
- Added PWM direction toggle
- Added basic iterative TMC PI autotuning
- Fixed issues with CAN transmission with multiple axes
- Added SSI encoder support (AMT232B)
- Fixed SPI buttons not working (SPI2 DMA on F407)
- Dynamic TMC encoder alignment current based on current limit
- Added effect monitoring per axis
 