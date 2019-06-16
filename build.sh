#!/bin/sh

# Furry Badge

# Compile
avr-gcc -g -Os -Wall -mcall-prologues -mmcu=attiny85 *.c -o badge.obj || exit

# Generate .hex
avr-objcopy -R .eeprom -O ihex badge.obj badge.hex || exit

# Set fuses
# avrdude -p t85 -c avr910 -P /dev/ttyUSB0 -U lfuse:w:0xe2:m

# Program
avrdude -p t85 -c avr910 -P /dev/ttyUSB0 -U flash:w:badge.hex

