# ESP32 IOT Module

## Features
* Ethernet support
* Web UI for configuration based on React 16 with Bootstrap
* TCP to RTU transparent mode

## Hardware
This firmware can be run on any ESP32 board with LAN8720 Ethernet PHY.
Tested on my [EtherESP board](http://www.maxx.net.ua/?p=484). This board fully compatible with [wESP32 board](https://wesp32.com/)

## Status
These functions tested:
* Modbus gateway - read input register, read holding registers

# Build

1. Prepare environment - https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html
2. Build
```
idf.py build
idf.py -p COMX flash
```

# Debug usage
## Set state
TOPIC: /HOST/gpio/command
{
  "pin": 23,
  "state": 5
}

## Upload file via CURL
curl -k -vvv -F upload=@/mnt/d/Projects/wesp32/esp32-modbus-tcp2rtu/front/dist/main.test.gz https://192.168.0.42/ota/upload/file

## Links
[Setup webapp](https://www.valentinog.com/blog/babel/)
[Bootstrap](https://www.turtle-techies.com/post/react-navbar-with-bootstrap-4/)
[IEEE754 converter](https://www.h-schmidt.net/FloatConverter/IEEE754.html)
[OTA](https://github.com/versamodule/ESP32-OTA-Webserver/blob/master/OTAServer.c)
[ESP32 pins reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)

Copyright (C) 2019 Maksym Krasovskyi