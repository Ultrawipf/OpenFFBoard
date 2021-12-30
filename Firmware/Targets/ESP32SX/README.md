## OpenFFBoard for ESP32S2/ESP32S3

ESP32-S serial chips allows all of us to DIY at a lower cost. You can get the same fantastic experience as STM32 in some functions. See this [PR](https://github.com/Ultrawipf/OpenFFBoard/pull/46) for supported features and progress.

| Supported Targets | Status              |
| ----------------- | ------------------- |
| ESP32-S2          | Basically OK        |
| ESP32-S3          | Testing is required |

### Hardware

Any ESP board that supports USB is OK, but you need enough experience to handle the connection with peripherals.

[openffboard-esp32](https://github.com/TDA-2030/OpenFFBoard/tree/openffb/esp32s2%2Bvesc/openffboard-esp32s2) is a simple board for ESP32S2 and ESp32S3. It is designed with [KiCad](https://www.kicad.org/) and realizes most of the functions of OpenFFBoard. You may start your journey from this board.

#### Pin Assignment

| OpenFFBoard signals | ESP32S2 PINs | ESP32S3 PINs |
| ------------------- | ------------ | ------------ |
| Switch              | IO0          | IO0          |
| AIN0                | IO1          | IO1          |
| AIN1                | IO2          | IO2          |
| AIN2                | IO3          | IO3          |
| ENCODER_A           | IO4          | IO4          |
| PWM1                | IO5          | IO5          |
| ENCODER_B           | IO6          | IO6          |
| PWM2                | IO7          | IO7          |
| DIN3                | IO8          | IO8          |
| DIN2                | IO9          | IO9          |
| DIN1                | IO10         | IO10         |
| DIN0                | IO11         | IO11         |
| GPIO_DRV            | IO12         | IO12         |
| DRV_ENABLE          | IO13         | IO13         |
| DRV_FLAG            | IO14         | IO14         |
| ENCODER_Z           | IO15         | IO15         |
| PWM3                | IO16         | IO16         |
| PWM4                | IO17         | IO17         |
| USB_D-              | IO19         | IO19         |
| USB_D+              | IO20         | IO20         |
| BRAKE_CTRL          | IO21         | IO21         |
| SPI1_CS2            | IO33         | IO47         |
| SPI1_CS3            | IO34         | IO48         |
| CAN_RX              | IO35         | IO35         |
| CAN_TX              | IO36         | IO36         |
| CAN_S               | IO37         | IO37         |
| SPI1_MISO           | IO38         | IO38         |
| SPI1_MOSI           | IO39         | IO39         |
| LED_ERR             | IO40         | IO40         |
| LED_CLIP            | IO41         | IO41         |
| LED_SYS             | IO42         | IO42         |
| SPI1_CS1            | IO45         | IO45         |
| SPI1_SCK            | IO46         | IO46         |



### Quick Download Firmware

`Firmware/Targets/ESP32SX/firmware` contains a set of compiled firmware. You can write it from 0x00 address to esp32s2/esp32s3.

1. First connect IO0 to GND, and then connect USB to the computer

2. Download [flash_download_tool](https://www.espressif.com/en/support/download/other-tools) from Espressif

3. Run the `flash_download_tool`

    ![download_1](https://s1.328888.xyz/2022/04/29/AHV7v.png)

    ![download_2](https://s1.328888.xyz/2022/04/29/AHF2i.png)
    
    Complete the configuration as shown in the figure, and finally click the `START` button. Please select the correct COM port before downloading.

### How to Build

1. Checkout esp-idf to commit: `c29343eb94d`

``` bash
cd esp-idf
git fetch
git checkout c29343eb94d
git submodule update --init --recursive
```

2. Build the project and flash it to the board, then run monitor tool to view serial output:

``` bash
cd <your dir>/OpenFFBoard/Firmware/Targets/ESP32SX
idf.py set-target esp32s2
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.