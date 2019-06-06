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

static uint16_t boop_baseline = 0;

/*
 * Read the boop sensor, subtracting the baseline if one has been set.
 */
uint16_t boop_sense (void)
{
    uint16_t fall_time = 0;

    /* Set the driven pin high */
    PORTB |= (1 << 1);
    _delay_ms (10);

    /* Set the driven pin low */
    PORTB &= ~(1 << 1);

    /* Time the falling edge, in loops */
    while (PINB & 0x01)
    {
        fall_time++;
    }

    if (fall_time < boop_baseline)
    {
        fall_time = boop_baseline;
    }

    return fall_time - boop_baseline;
}

/*
 * Get a baseline non-boop reading to subtract from future readings.
 */
void boop_calibrate (void)
{
    uint16_t boop_sum = 0;
    for (int i = 0; i < 10; i++)
    {
        boop_sum += boop_sense ();
        _delay_ms (50);
    }

    boop_baseline = boop_sum / 10;
}

#define MODE_COUNT 3;

int main (void)
{
    uint16_t boop_holdoff = 0;
    uint8_t  mode = 0;

    _delay_ms (10);
    show ();

    /* Output: LED data */
    DDRB |= (1 << DDB1);
    DDRB |= (1 << DDB4);

    boop_calibrate ();

    while (true)
    {
        memset (pixels, 0, 6);

        if (boop_holdoff)
        {
            boop_holdoff--;
        }
        else
        {
            if (boop_sense () > 0x02)
            {
                mode = (mode + 1) % MODE_COUNT;
                boop_holdoff = 8;
            }
        }

        switch (mode)
        {
            case 0: /* Mode 0: Red */
                pixels [LEFT_R] = pixels [RIGHT_R] = 0x08;
                break;

            case 1: /* Mode 1: Green */
                pixels [LEFT_G] = pixels [RIGHT_G] = 0x08;
                break;

            case 2: /* Mode 2: Blue */
            default:
                pixels [LEFT_B] = pixels [RIGHT_B] = 0x08;
                break;
        }

        show ();

        _delay_ms (50);
    }
}
