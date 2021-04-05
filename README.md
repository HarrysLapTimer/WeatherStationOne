# WeatherStationOne

This is part of the Weather Station One project, a complete and modular weather station. The design of the system has been driven by a number of requirements:

- collect all common weather channels including temperature, humidity, barometric pressure, rain, wind speed and direction, illumination / sun hours, ground humidity
- modular design allowing one to implement part of it, or extend the range of sensors
- high depth of production; DIY as much as possible
- long range transmission support - with the point of measurement far from home
- integration into a home automation system
- functional, easy to print, and beautiful 3D design

This repository includes everything you need to setup the electronic and software part of Weather Station One.

## Other Sources

Besides this repository, the 3D printable parts are provided on [Harry's Prusa Prints](https://www.prusaprinters.org/social/92858-harry/prints).

[Building instructions](http://www.met.fu-berlin.de/%7Estefan/huette.html) for the weather hut / Stevenson Screen created as part of the project.

## System Environment

WeatherStationOne is built using two ESP32 based units. This README describes how to implement the electronics and software part using the Arduino IDE.

## Electronics

The plan for the **remote node (weatherstation)** is provided in `plan.svg`. It is quite complex and combines the ESP32 with a solar powered charger, a HC-12 for long range communication, and a green LED to signal messages sent. Furthermore, it connects with all the sensors supported currently. Opposed to this, the **base node (weatherbase)** is simple. It is made up from an ESP32 plus a power supply, a red and a green LEDs, and a second HC-12. There is no dedicated plan for this. The HC-12 is connected precisely like the weatherstation side's HC-12. Same for the green LED. The red LED is connected to +3.3V using a 220Ohm resistor to signal power supply. That's it.

## Software Installation

Start with setting up your Arduino environment. When using the electronic parts I have used, here are the basic steps to follow:

- install support for your ESP32 developer board
- install ESP32 Sketch Data Upload, a tutorial is available [here](https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/)
- install RemoteDebug library required for our own libraries

`weatherbase` will run a web server to allow the user to monitor current weather data. The content (icons and html) is provided on SPIFF stored files. We will use ESP32 Sketch Data Upload to upload this files from the sketch folder to `weatherbase`.

WeatherStationOne is based on the **Bolbro library** my projects are based on. It comes with a number of convenience functions including WiFi, preference, time, web server, and log handling. As the implementation consists of two Arduino sketches using common code and structures, a second - project specific - **Weather library** is used.

-  copy library/Bolbro and library/Weather to your Arduino library directory; on macOS, this is ~/Documents/Arduino/libraries
-  restart Arduino IDE afterwards

Now, you are ready to compile and flash the two sketches. `weatherbase` requires a customization to work:

- rename `weatherbase.ino.customize` to `weatherbase.ino`
- edit the call to `Bolbro.addWiFi()`

By adding a WiFi made up from an SSID and a password, it will be considered as a connection point of your local network. In case you have more than one, call `addWiFi()` multiple times. The best one will be choosen.

Finally, the sketches:

- compile and flash `weatherbase`
- make sure Serial Monitor is closed and hit Tools/ESP32 Sketch Data Upload; this will upload the web content
- compile and flash `weatherstation`

Once `weatherbase` is powered, connect your browser to `http://weatherbasedebug.local`

## TODO

- add support for illumination and ground humidity
- add help on all the pitfalls one may run into creating this project
- enrich `weatherbase`-side data
- add support for `weatherstation` calibration settings using the `weatherbase` web interface


