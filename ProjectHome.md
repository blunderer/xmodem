# XMODEM #

**xmodem** is a file sender compatible with the hyperterminal (windows) implementation.

## History ##
xmodem is a simple protocol used for transfering files using a serial line created in 1977 by Ward Christensen.

Since then, several little evolutions occured and made this one protocol become several.
Packet size, checksum len, checksum type, header byte and some other are not always what we expect them to be.

## How to use it ##
Download binary from download section and rename it to xmodem or build it from sources.
```
usage: ./xmodem [-m <mode> -s <speed>] -p <serial port> -i <input file>
options are:
	-s <baud>: 1200 1800 2400 4800 9600 19200 38400 57600 230400 115200. default is 115200
	-m <mode>: [5-8] [N|E|O] [1-2] ex: 7E2. default is 8N1 
	-p <serial port>
	-i <file>
```
## Why this project ##
This project is here because some people had trouble sending files thru serial line using xmodem from minicom or other unix tools. This feature is often requested in embedded software development to write a bootloader or kernel into the flash memory of some device.

## Autor / Contact ##
tristan.lelong@blunderer.org