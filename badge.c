/*
 * Port B, pin 4
 *
 * Timing:
 *  0: 0.4 µs high, 0.85 µs low
 *  1: 0.8 µs high, 0.45 µs high
 *  50us between updates.
 *
 *  Implementation: Borrow the hand-tuned assembler from Adafruit_NeoPixel.cpp
 */
#define F_CPU 8000000UL

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>

#define LEFT_G 0
#define LEFT_R 1
#define LEFT_B 2

#define RIGHT_G 3
#define RIGHT_R 4
#define RIGHT_B 5

uint8_t pixels[6] = {0, 0, 0, 0, 0, 0};

/* NeoPixel 8 MHz implementation based on github.com/adafruit/Adafruit_NeoPixel (LGPLv3) */
void show(void)
{
    volatile uint16_t num_bytes = 6;
    volatile uint8_t *ptr = pixels;
    volatile uint8_t b = *ptr++; /* Current byte value */
    volatile uint8_t hi; /* PORTB with output bit set high */
    volatile uint8_t lo; /* PORTB with output bit set low */
    volatile uint8_t n1 = 0; /* First bit out */
    volatile uint8_t n2 = 0; /* Next bit out */

    hi = PORTB |  0x10;
    lo = PORTB & ~0x10;

    /* Set up the first bit out */
    n1 = (b & 0x80) ? hi : lo;

    /* Ten instruction clocks per bit: HH_____LLL.
     * Out instructions: T=0, T=2, T=7 ^ ^    ^
     */
    asm volatile (
        "headB:                         \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n2],      %[lo]       \n\t"
        "out    %[port],    %[n1]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    6           \n\t"
        "mov    %[n2],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "rjmp   .+0                     \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n1],      %[lo]       \n\t"
        "out    %[port],    %[n2]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    5           \n\t"
        "mov    %[n1],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "rjmp   .+0                     \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n2],      %[lo]       \n\t"
        "out    %[port],    %[n1]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    4           \n\t"
        "mov    %[n2],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "rjmp   .+0                     \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n1],      %[lo]       \n\t"
        "out    %[port],    %[n2]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    3           \n\t"
        "mov    %[n1],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "rjmp   .+0                     \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n2],      %[lo]       \n\t"
        "out    %[port],    %[n1]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    2           \n\t"
        "mov    %[n2],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "rjmp   .+0                     \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n1],      %[lo]       \n\t"
        "out    %[port],    %[n2]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    1           \n\t"
        "mov    %[n1],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "rjmp   .+0                     \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n2],      %[lo]       \n\t"
        "out    %[port],    %[n1]       \n\t"
        "rjmp   .+0                     \n\t"
        "sbrc   %[byte],    0           \n\t"
        "mov    %[n2],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "sbiw   %[count],   1           \n\t"
        "out    %[port],    %[hi]       \n\t"
        "mov    %[n1],      %[lo]       \n\t"
        "out    %[port],    %[n2]       \n\t"
        "ld     %[byte],    %a[ptr]+    \n\t"
        "sbrc   %[byte],    7           \n\t"
        "mov    %[n1],      %[hi]       \n\t"
        "out    %[port],    %[lo]       \n\t"
        "brne   headB                     \n"
        /* Read-write variables */
        : [byte]  "+r" (b),
          [n1]    "+r" (n1),
          [n2]    "+r" (n2),
          [count] "+w" (num_bytes)
        /* Read-only variables */
        : [port]  "I"  (_SFR_IO_ADDR(PORTB)),
          [ptr]   "e"  (ptr),
          [hi]    "r"  (hi),
          [lo]    "r"  (lo));
}

int main (void)
{
    int cycle = 0;
    _delay_ms (100);
    show ();

    /* LED data */
    DDRB |= (1 << DDB4);

    while (true)
    {
        memset (pixels, 0, 6);

        switch (cycle)
        {
            case 0: /* Red */
                pixels[LEFT_R] = pixels[RIGHT_R] = 0x10;
                break;
            case 1: /* Green */
                pixels[LEFT_G] = pixels[RIGHT_G] = 0x10;
                break;
            case 2: /* Blue */
                pixels[LEFT_B] = pixels[RIGHT_B] = 0x10;
                break;
            case 3: /* Cyan */
                pixels[LEFT_G] = pixels[RIGHT_G] = 0x10;
                pixels[LEFT_B] = pixels[RIGHT_B] = 0x10;
                break;
            case 4: /* Magenta */
                pixels[LEFT_R] = pixels[RIGHT_R] = 0x10;
                pixels[LEFT_B] = pixels[RIGHT_B] = 0x10;
                break;
            case 5: /* Yellow */
                pixels[LEFT_R] = pixels[RIGHT_R] = 0x10;
                pixels[LEFT_G] = pixels[RIGHT_G] = 0x10;
                break;
            case 6: /* White */
                pixels[LEFT_R] = pixels[RIGHT_R] = 0x10;
                pixels[LEFT_G] = pixels[RIGHT_G] = 0x10;
                pixels[LEFT_B] = pixels[RIGHT_B] = 0x10;
                break;
            case 7: /* Off */
            case 15:
                break;
            case 8: /* Red-Blue */
            case 10:
            case 12:
            case 14:
                pixels[LEFT_R] = pixels[RIGHT_B] = 0x10;
                break;
            case 9: /* Blue-Red */
            case 11:
            case 13:
                pixels[LEFT_B] = pixels[RIGHT_R] = 0x10;
                break;
        }

        show ();

        cycle = (cycle + 1) % 16;
        _delay_ms (500);
    }
}
