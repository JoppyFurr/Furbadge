#!/bin/sh

# Furry Badge build script.

# Configuration
TTY="/dev/ttyUSB0"

# Auto-detect chip by providing the wrong part number and checking the error
LINE="$(avrdude -c avr910 -P ${TTY} -p ATtiny2313 -q 2>&1 | grep 'Device signature')"
if $(echo "${LINE}" | grep -q "probably t85")
then
    echo "Detected an ATtiny85"
    CHIP="t85"
    GCC_CHIP="attiny85"
elif $(echo "${LINE}" | grep -q "probably t45")
then
    echo "Detected an ATtiny45"
    CHIP="t45"
    GCC_CHIP="attiny45"
else
    echo "Did not detect either a Tiny85 or Tiny45. Exiting."
    exit 1
fi

# Compile
avr-gcc -g -Os -Wall -mcall-prologues -mmcu=${GCC_CHIP} *.c -o badge.obj || exit

# Generate .hex
avr-objcopy -R .eeprom -O ihex badge.obj badge.hex || exit

# Set fuses if needed
LFUSE="$(avrdude -p ${CHIP} -c avr910 -P ${TTY} -U lfuse:r:-:h 2>&1 | grep '^0x')"
if [ "${LFUSE}" = "0xe2" ]
then
    echo "L-fuse already set to 0xe2"
else
    echo "Writing L-fuse to 0xe2"
    avrdude -p ${CHIP} -c avr910 -P ${TTY} -U lfuse:w:0xe2:m
fi

# Program
echo "Writing badge.hex to chip..."
avrdude -p ${CHIP} -c avr910 -P ${TTY} -U flash:w:badge.hex

