# Arduino BIOS Programmer
Arduino as WINBOND W25Q*** SPI Flash (used for modern PC mainboard BIOS) reader/writer

It's slow (for 16 Mb flash @ 115200 baud it takes about a hour), but simple to build, need no special software and does the work.

To work with the programmer use any terminal emulator with X-Modem transfer support (e.g. hyperterminal.exe) at 115200

## Connections

WINBOND -- Arduino

1 -- 10

2 -- 12

3 -- 3.3V

4 -- GND

5 -- 11

6 -- 13

7 -- 3.3V

8 -- 3.3V


**Don't connect any pin of the chip to 5V instead of 3.3V, this will probably kill the chip.**
**Use a level shifter to drop arduino's logic 5V to the 3.3V on the WINBOND chip on pins 1,2,5,6.**


## Commands

0 - initialize

r - read content

e - erase all chip (must be done before write)

w - write content


## Python XMODEM BIOS dumper/writer

pip2.7 install -r requirements.txt
- Edit xmodembiosread.py and xmodembiosread.py according to your needs.

## TODO:
./xmodembios.py write|read file.rom [serial_port] [baud_rate] 
