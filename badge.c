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

            hsv2rgb_rainbow (192, 0xff, value + 1, &rgb);
            memcpy (&pixels [LEFT],  &rgb, 3);
            memcpy (&pixels [RIGHT], &rgb, 3);
            break;

        case 1: /* Mode 1: Rainbow */

            hsv2rgb_rainbow (frame, 0xff, 0x10, &rgb);
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

    led_show (pixels);
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
