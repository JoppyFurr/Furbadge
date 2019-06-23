/*
 * led_show based on github.com/adafruit/Adafruit_NeoPixel (LGPLv3)
 * Adafruit_NeoPixel.cpp
 *
 * Hard-coding for Port B, pin 2.
 */

#include <stdint.h>
#include <avr/io.h>

void led_show(uint8_t *data)
{
    volatile uint16_t num_bytes = 6;
    volatile uint8_t *ptr = data;
    volatile uint8_t b = *ptr++; /* Current byte value */
    volatile uint8_t hi; /* PORTB with output bit set high */
    volatile uint8_t lo; /* PORTB with output bit set low */
    volatile uint8_t n1 = 0; /* First bit out */
    volatile uint8_t n2 = 0; /* Next bit out */

    hi = PORTB |  (1 << PB2);
    lo = PORTB & ~(1 << PB2);

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
