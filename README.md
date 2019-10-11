# RTU to TCP modbus gateway


## Features
* Ethernet support
* TCP to RTU transparent mode

## Hardware
This firmware can be run on any ESP32 board with LAN8720 Ethernet PHY.
Tested on my [EtherESP board](http://www.maxx.net.ua/?p=484). This board fully compatible with [wESP32 board](https://wesp32.com/)

## Status
These functions tested:
* Read input register

# Build

1. Prepare environment - https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html
2. Build
```
idf.py build
idf.py -p COMX flash
```


# Documentation

http://www.simplymodbus.ca/TCP.htm
https://en.wikipedia.org/wiki/Modbus#Modbus_TCP_frame_format_(primarily_used_on_Ethernet_networks)

# Tools

[qmodmaster](https://sourceforge.net/projects/qmodmaster/)

[IEEE754 converter](https://www.h-schmidt.net/FloatConverter/IEEE754.html)

