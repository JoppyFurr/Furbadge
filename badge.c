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

#include <avr/interrupt.h>
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

#define BIT_0 0x01
#define BIT_1 0x02
#define BIT_2 0x04
#define BIT_3 0x08
#define BIT_4 0x10
#define BIT_5 0x20
#define BIT_6 0x40
#define BIT_7 0x80

#define MODE_COUNT 3;

/*
 * State
 */
static uint8_t tick = 1;
static uint8_t mode  = 0;
static uint8_t frame = 0;
static uint8_t boop  = 0;
static uint8_t pixels[6] = {0, 0, 0, 0, 0, 0};
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

typedef enum Volume_e {
    VOLUME_OFF,
    VOLUME_SOFT,
    VOLUME_LOUD
} Volume_t;

uint8_t sound_toggle = 0x00;

/*
 * Called at twice the current sound frequency.
 * Toggles the appropriate GPIO pins.
 *
 * TODO: To get decent sound, something needs to change.
 *       -> Is it possible to do this in hardware with Timer1 with OC1B and ~OC1B? By swapping pins 7 and 3 in the schematic?
 *       -> Can the led bit-bang be adjusted to leave the rest of PORTB alone?
 *       -> Failing the above, choose short bursts of sound where issues won't be noticed.
 */
ISR (TIMER0_COMPA_vect)
{
    PORTB ^= sound_toggle;
}

/*
 * Set the piezo output.
 */
void sound_set (uint16_t freq, Volume_t volume)
{
    if (volume == VOLUME_OFF)
    {
        TIMSK &= ~0x10; /* Timer0 Output Compare A interrupt disable */
        PORTB &= ~(BIT_2 | BIT_3);
        sound_toggle = 0;
        return;
    }

    /* Count to */
    OCR0A = 31250 / freq - 1;

    switch (volume)
    {
        case VOLUME_LOUD:
            /* Start the pins at opposite values */
            PORTB &= ~BIT_2;
            PORTB |=  BIT_3;
            sound_toggle = BIT_2 | BIT_3;
            break;

        case VOLUME_SOFT:
        default:
            /* Start the pins at the same value */
            PORTB &= ~(BIT_2 | BIT_3);
            sound_toggle = BIT_2;
            break;
    }

    TIMSK |= 0x10; /* Timer0 Output Compare A interrupt enable */
}

/*
 * Update the piezo for the current state.
 */
void tick_sound (void)
{
    static uint8_t beep_counter = 0;

    /* Three second loop */
    beep_counter = (beep_counter + 1) % 150;

    /* 1.00 s: 200 ms soft beep */
         if (beep_counter == 50) { sound_set (440, VOLUME_SOFT); }
    else if (beep_counter == 60) { sound_set (440, VOLUME_OFF); }

    /* 1.50 s: 200 ms soft beep */
    else if (beep_counter == 75) { sound_set (880, VOLUME_SOFT); }
    else if (beep_counter == 85) { sound_set (880, VOLUME_OFF); }

    /* 2.00 s: 200 ms loud beep */
    else if (beep_counter == 100) { sound_set (440, VOLUME_LOUD); }
    else if (beep_counter == 110) { sound_set (440, VOLUME_OFF); }

    /* 2.50 s: 200 ms loud beep */
    else if (beep_counter == 125) { sound_set (880, VOLUME_LOUD); }
    else if (beep_counter == 135) { sound_set (880, VOLUME_OFF); }
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
#define BOOP_256_FRAMES     if (frame == 0xff) { boop = false; frame = 0; }

typedef enum Eye_e {
    EYE_LEFT,
    EYE_RIGHT,
    EYE_BOTH
} Eye_t;

void eye_hsv_set (uint8_t hue, uint8_t sat, uint8_t val, Eye_t eye)
{
    RGB_t rgb;

    hsv2rgb_rainbow (hue, sat, val, &rgb);

    switch (eye)
    {
        case EYE_LEFT:
            memcpy (&pixels [LEFT],  &rgb, 3);
            break;
        case EYE_RIGHT:
            memcpy (&pixels [RIGHT], &rgb, 3);
            break;
        case EYE_BOTH:
            memcpy (&pixels [LEFT],  &rgb, 3);
            memcpy (&pixels [RIGHT], &rgb, 3);
        default:
            break;
    }
}

void tick_leds (void)
{
    uint8_t value = 0;

    memset (pixels, 0, 6);

    switch (mode)
    {
        case 0:
            /* Mode: Purple Fade
             * Boop: Strobe */
            if (boop)
            {
                BOOP_64_FRAMES;
                eye_hsv_set (HUE_VIOLET, 0xff, (frame & 0x02) ? 0x18 : 0, EYE_BOTH);
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

                eye_hsv_set (HUE_VIOLET, 0xff, value + 1, EYE_BOTH);
            }
            break;

        case 1:
            /* Mode: Rainbow
             * Boop: Bright, fast, and crazy */
            if (boop)
            {
                BOOP_128_FRAMES;
                eye_hsv_set ((frame << 2) +  64, 0xff, 0x18, EYE_LEFT);
                eye_hsv_set ((frame << 2) + 192, 0xff, 0x18, EYE_RIGHT);
            }
            else
            {
                CYCLE_256_FRAMES;
                eye_hsv_set (frame, 0xff, 0x10, EYE_BOTH);
            }
            break;

        case 2:
            /* Mode: Pirihimana
             * Boop: Pirihi-strobe */
        default:
            if (boop)
            {
                BOOP_256_FRAMES;

                if ((frame / 12) & 1)
                {
                    /* Red */
                    eye_hsv_set (HUE_RED,  0xff, (frame & 0x02) ? 0x10 : 0x00, EYE_BOTH);
                }
                else
                {
                    /* Blue */
                    eye_hsv_set (HUE_BLUE, 0xff, (frame & 0x02) ? 0x10 : 0x00, EYE_BOTH);
                }
            }
            else
            {
                CYCLE_32_FRAMES;

                if (frame & 0x08)
                {
                    eye_hsv_set (HUE_RED,  0xff, 0x08, EYE_LEFT);
                    eye_hsv_set (HUE_BLUE, 0xff, 0x08, EYE_RIGHT);
                }
                else
                {
                    eye_hsv_set (HUE_BLUE, 0xff, 0x08, EYE_LEFT);
                    eye_hsv_set (HUE_RED,  0xff, 0x08, EYE_RIGHT);
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
 * Sets the tick flag at 50 Hz.
 */
ISR (TIMER1_COMPA_vect)
{
    tick++;
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

    /* Output: Piezo */
    DDRB |= (1 << DDB2) | (1 << DDB3);

    /* Output: LED Data */
    DDRB |= (1 << DDB4);

    boop_calibrate ();

    /* Use Timer0 in CTC to time the piezo */
    TCCR0A = 0x02; /* CTC mode */
    TCCR0B = 0x04; /* Count at 31.25 kHz */

    /* Use Timer1 for the 50 Hz tick interrupt */
    TCCR1  = 0x8b; /* Reset at OCR1A, count at 7.8125 kHz */
    OCR1A  = 0x9b; /* Count 0->194 for 50.08 Hz */
    TIMSK |= 0x40; /* Timer1 Output Compare A interrupt enable */

    /* Enable interrupts */
    sei ();

    while (true)
    {
        /* Tick using a variable to avoid spending too much time in interrupt context */
        /* This is done to try minimize sound glitches, but seems to result in LED glitches */
        if (tick)
        {
            tick_sound ();
            tick_leds ();
            tick_state ();
            tick--;
        }
        else
        {
            _delay_us (100);
        }
    }
}
