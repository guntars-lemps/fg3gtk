FG3GTK is Linux GUI application for controling frequency generation devices which are compatible with FG3 protocol. This is an upgraded version which has unified UART protocol, now it automatically checks for device capabilities, adjusts time units and ranges.

<img src="img/1.png" alt="FG3" width="800"> 

Currently compatible devices are 
1. Generator based on avr mcu (Atmega 328) https://github.com/guntars-lemps/fg3avr
2. Generator based on Raspberry Pi Pico 2 (RP2350) https://github.com/guntars-lemps/fg3pico

Pico2 based generator has the highest capabilites, max frequency it can generate is 33Mhz with time resolution of 5ns. 

For building it requires libgtk dev package, on Ubuntu or Mint Linux it can be installed by the command
```
$ sudo apt install libgtk-3-dev
```

How to build and install
```
$ make
$ sudo make install
```
