### Changes this version:
- Added SPI speed selector to MagnTek encoders
- Added "reg" and "save" commands to MagnTek encoder. Allows programming MT6835 encoders (debug=1 mode required!)
- Set ABN encoder filter to 5 for F407 and F407_DISCO. Should improve encoder stability in noisy environments.
- Fixed Magntek encoder forwarding causing choppy FFB
- Fixed TMC4671 ext encoder having inverted forces if previously inverted by dual enc mode