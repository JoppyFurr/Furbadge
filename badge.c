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

#define LEFT 0
#define LEFT_G 0
#define LEFT_R 1
#define LEFT_B 2

#define RIGHT 3
#define RIGHT_G 3
#define RIGHT_R 4
#define RIGHT_B 5

uint8_t pixels[6] = {0, 0, 0, 0, 0, 0};

/* NeoPixel 8 MHz implementation based on github.com/adafruit/Adafruit_NeoPixel (LGPLv3) */
void led_show(void)
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
static uint8_t  mode = 0;
static uint8_t frame = 0;

/*
 * Update the piezo for the current state.
 */
void tick_sound (void)
{
    /* TODO */
}

/*
 * RGB struct stores colours in NeoPixel order.
 */
typedef struct RGB_s {
    union {
        uint8_t g;
        uint8_t green;
    };
    union {
        uint8_t r;
        uint8_t red;
    };
    union {
        uint8_t b;
        uint8_t blue;
    };
} RGB_t;

#define HSV_SECTION_6 (0x20)
#define HSV_SECTION_3 (0x40)

/*
 * HSV to RGB implementation based on https://github.com/FastLED/FastLED (MIT)
 *
 * Hue rages from 0-191
 *
 * TODO: Switch to the rainbow implementation.
 */
void hsv2rgb (uint8_t hue, uint8_t sat, uint8_t val, RGB_t *rgb)
{
    /* The brightness floor is minimum number that all of
     * R, G, and B will be set to. */
    uint8_t invsat = 255 - sat;
    uint8_t brightness_floor = (val * invsat) / 256;

    /* The colour amplitude is the maximum amount of R, G, and B
     * that will be added on top of the brightness_floor to
     * create the specific hue desired. */
    uint8_t color_amplitude = val - brightness_floor;

    /* Figure out which section of the hue wheel we're in,
     * and how far offset we are withing that section */
    uint8_t section = hue / HSV_SECTION_3; /* 0..2 */
    uint8_t offset = hue % HSV_SECTION_3;  /* 0..63 */

    uint8_t rampup = offset; /* 0..63 */
    uint8_t rampdown = (HSV_SECTION_3 - 1) - offset; /* 63..0 */

    /* compute color-amplitude-scaled-down versions of rampup and rampdown */
    uint8_t rampup_amp_adj   = (rampup   * color_amplitude) / (256 / 4);
    uint8_t rampdown_amp_adj = (rampdown * color_amplitude) / (256 / 4);

    /* add brightness_floor offset to everything */
    uint8_t rampup_adj_with_floor   = rampup_amp_adj   + brightness_floor;
    uint8_t rampdown_adj_with_floor = rampdown_amp_adj + brightness_floor;

    if( section ) {
        if( section == 1) {
            /* section 1: 0x40..0x7F */
            rgb->r = brightness_floor;
            rgb->g = rampdown_adj_with_floor;
            rgb->b = rampup_adj_with_floor;
        } else {
            /* section 2; 0x80..0xBF */
            rgb->r = rampup_adj_with_floor;
            rgb->g = brightness_floor;
            rgb->b = rampdown_adj_with_floor;
        }
    } else {
        /* section 0: 0x00..0x3F */
        rgb->r = rampdown_adj_with_floor;
        rgb->g = rampup_adj_with_floor;
        rgb->b = brightness_floor;
    }
}

/*
 * Update the LED pattern for the current state.
 */
#define FRAME_OF_32  (frame & 0x1f)
#define FRAME_OF_192 (frame % 0xc0)
void tick_leds (void)
{
    uint8_t value = 0;
    RGB_t rgb;

    memset (pixels, 0, 6);

    switch (mode)
    {
        case 0: /* Mode 0: Purple fading */
            frame = FRAME_OF_32;

            if (frame < 0x10)
            {
                /* Ramp up */
                value = frame;
            }
            else
            {
                /* Ramp down */
                value = 0x10 - (frame - 0x10);
            }

            hsv2rgb (144, 0xff, value + 1, &rgb);
            memcpy (&pixels [LEFT],  &rgb, 3);
            memcpy (&pixels [RIGHT], &rgb, 3);
            break;

        case 1: /* Mode 1: Rainbow */
            frame = FRAME_OF_192;

            hsv2rgb(frame, 0xff, 0x10, &rgb);
            memcpy (&pixels [LEFT],  &rgb, 3);
            memcpy (&pixels [RIGHT], &rgb, 3);
            break;

        case 2: /* Mode 2: Pirihimana */
        default:
            frame = FRAME_OF_32;
            if (frame & 0x08)
            {
                pixels [LEFT_R] = pixels [RIGHT_B] = 0x08;
            }
            else
            {
                pixels [LEFT_B] = pixels [RIGHT_R] = 0x08;
            }
            break;
    }

    frame++;

    led_show ();
}

/*
 * Read the boop sensor and update state as needed.
 */
void tick_state (void)
{
    static uint8_t boop_holdoff = 0;

    if (boop_holdoff)
    {
        boop_holdoff--;
    }
    else
    {
        if (boop_sense () > 0x02)
        {
            mode = (mode + 1) % MODE_COUNT;
            boop_holdoff = 16;
        }
    }
}

/*
 * Timebase for running the badge.
 * Called every 25 ms.
 * TODO: Switch to an interrupt.
 */
void tick ()
{
    tick_sound ();
    tick_leds ();
    tick_state ();
}

/*
 * Entry point.
 */
int main (void)
{
    _delay_ms (10);
    led_show ();

    /* Output: Boop driver */
    DDRB |= (1 << DDB1);

    /* Output: LED Data */
    DDRB |= (1 << DDB4);

    boop_calibrate ();

    while (true)
    {
        tick ();
        _delay_ms (25);
    }
}
