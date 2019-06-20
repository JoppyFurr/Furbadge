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

#include "rgb.h"
#include "hsv2rgb.h"
#include "led_bitbang.h"

#define LEFT 0
#define LEFT_G 0
#define LEFT_R 1
#define LEFT_B 2

#define RIGHT 3
#define RIGHT_G 3
#define RIGHT_R 4
#define RIGHT_B 5

uint8_t pixels[6] = {0, 0, 0, 0, 0, 0};

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
static uint8_t mode  = 0;
static uint8_t frame = 0;
static uint8_t boop  = 0;

/*
 * Update the piezo for the current state.
 */
void tick_sound (void)
{
    /* TODO */
}

/*
 * Update the LED pattern for the current state.
 */
#define CYCLE_32_FRAMES     frame = (frame & 0x1f)
#define CYCLE_64_FRAMES     frame = (frame & 0x3f)
#define CYCLE_192_FRAMES    frame = (frame % 0xc0)
#define CYCLE_256_FRAMES
#define BOOP_32_FRAMES      if (frame == 0x1f) { boop = false; frame = 0; }
#define BOOP_64_FRAMES      if (frame == 0x3f) { boop = false; frame = 0; }
#define BOOP_128_FRAMES     if (frame == 0x7f) { boop = false; frame = 0; }

void tick_leds (void)
{
    uint8_t value = 0;
    RGB_t rgb;

    memset (pixels, 0, 6);

    switch (mode)
    {
        case 0:
            /* Mode: Purple Fade
             * Boop: Strobe */
            if (boop)
            {
                BOOP_32_FRAMES;
                hsv2rgb_rainbow (192, 0xff, (frame & 0x02) ? 0x18 : 0, &rgb);
            }
            else
            {
                CYCLE_64_FRAMES;

                if (frame < 0x20)
                {
                    /* Ramp up */
                    value = frame >> 1;
                }
                else
                {
                    /* Ramp down */
                    value = (0x20 - (frame - 0x20)) >> 1;
                }

                hsv2rgb_rainbow (192, 0xff, value + 1, &rgb);
            }

            memcpy (&pixels [LEFT],  &rgb, 3);
            memcpy (&pixels [RIGHT], &rgb, 3);

            break;

        case 1:
            /* Mode: Rainbow
             * Boop: Bright, fast, and crazy */
            if (boop)
            {
                BOOP_64_FRAMES;
                hsv2rgb_rainbow ((frame << 2) + 64, 0xff, 0x18, &rgb);
                memcpy (&pixels [LEFT],  &rgb, 3);
                hsv2rgb_rainbow ((frame << 2) + 192, 0xff, 0x18, &rgb);
                memcpy (&pixels [RIGHT], &rgb, 3);
            }
            else
            {
                CYCLE_256_FRAMES;
                hsv2rgb_rainbow (frame, 0xff, 0x10, &rgb);
                memcpy (&pixels [LEFT],  &rgb, 3);
                memcpy (&pixels [RIGHT], &rgb, 3);
            }
            break;

        case 2:
            /* Mode: Pirihimana
             * Boop: Pirihi-strobe */
        default:
            if (boop)
            {
                BOOP_128_FRAMES;

                if ((frame / 12) & 1)
                {
                    /* Red */
                    pixels [LEFT_R] = pixels [RIGHT_R] = (frame & 0x02) ? 0x10 : 0;
                }
                else
                {
                    /* Blue */
                    pixels [LEFT_B] = pixels [RIGHT_B] = (frame & 0x02) ? 0x10 : 0;
                }
            }
            else
            {
                CYCLE_32_FRAMES;

                if (frame & 0x08)
                {
                    pixels [LEFT_R] = pixels [RIGHT_B] = 0x08;
                }
                else
                {
                    pixels [LEFT_B] = pixels [RIGHT_R] = 0x08;
                }
            }
            break;
    }

    frame++;

    led_show (pixels);
}

/*
 * Read the boop sensor and update state as needed.
 */
void tick_state (void)
{
    static uint8_t boop_length = 0;
    static uint8_t boop_holdoff = 0;

    /* Debounce the booper */
    if (boop_holdoff)
    {
        boop_holdoff--;
        return;
    }

    /* Check for a boop */
    if (boop_sense () > 0x02)
    {
        boop_length++;

        /* To keep the short-boop responsive, trigger it right away */
        if (boop_length == 1)
        {
            boop = true;
            frame = 0;
        }

        /* A long-boop should cancel the short-boop and instead cycle through modes */
        if ((boop_length & 0x3f) == 0x20)
        {
            mode = (mode + 1) % MODE_COUNT;
            boop = false;

            /* Don't overflow, even for very long boops */
            if (boop_length > 0x20)
            {
                boop_length = 0x20;
            }
        }
    }

    /* The current boop has completed, start the hold-off timer */
    else if (boop_length)
    {
        boop_holdoff = 16;
        boop_length = 0;
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
    led_show (pixels);

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
