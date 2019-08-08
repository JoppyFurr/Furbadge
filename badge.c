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
#include <stdlib.h>
#include <string.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
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

#define MODE_COUNT 8

/*
 * State
 */
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

/*
 * Set the piezo output.
 */
void sound_set (uint16_t freq, Volume_t volume)
{

    OCR1C = 31250 / freq - 1; /* Count to */
    OCR1B = OCR1C / 2; /* 50% duty cycle */

    switch (volume)
    {
        case VOLUME_LOUD:
            /* Dual-pin PWM */
            GTCCR = (GTCCR & ~((1 << COM1B1) | (1 << COM1B0))) | (1 << COM1B0);
            break;

        case VOLUME_SOFT:
            /* Single-pin PWM */
            GTCCR = (GTCCR & ~((1 << COM1B1) | (1 << COM1B0))) | (1 << COM1B1);
            break;

        case VOLUME_OFF:
        default:
            /* Disable PWM */
            GTCCR &= ~((1 << COM1B1) | (1 << COM1B0));
            break;
    }
}

typedef struct Sound_s
{
    uint16_t freq;
    uint16_t duration_ms;
    uint8_t volume;
    int8_t freq_shift;
} Sound_t;

const Sound_t sound_mode_change [] PROGMEM = {
    { 440, 100, VOLUME_SOFT, 0 },
    {   0, 100, VOLUME_OFF,  0 },
    { 880, 100, VOLUME_SOFT, 0 },
    { }
};

const Sound_t sound_boop_siren [] PROGMEM = {
    { 1500, 800, VOLUME_LOUD, -25 },
    {  500, 800, VOLUME_LOUD,  25 },
    { 1500, 800, VOLUME_LOUD, -25 },
    {  500, 800, VOLUME_LOUD,  25 },
    { 1500, 800, VOLUME_LOUD, -25 },
    { }
};

#define BOOP_COUNT 3

const Sound_t sound_boop_0 [] PROGMEM = {
    { 440, 300,  VOLUME_LOUD,  100 },
    { 1940, 100, VOLUME_LOUD, -100 },
    { }
};

const Sound_t sound_boop_1 [] PROGMEM = {
    { 440, 200, VOLUME_LOUD, 100 },
    {   0,  20, VOLUME_OFF,  0 },
    { 440, 200, VOLUME_LOUD, 100 },
    { }
};

const Sound_t sound_boop_2 [] PROGMEM = {
    { 440,  60, VOLUME_LOUD, 0 },
    {   0,  40, VOLUME_OFF,  0 },
    { 440,  60, VOLUME_LOUD, 0 },
    {   0,  40, VOLUME_OFF,  0 },
    { 440, 200, VOLUME_LOUD, 100 },
    { }
};

const Sound_t *get_next_boop_sound (void)
{
    static uint8_t sound_index = 0;
    const Sound_t *sound = NULL;

    switch (sound_index)
    {
        case 0:
            sound = sound_boop_0;
            break;
        case 1:
            sound = sound_boop_1;
            break;
        case 2:
        default:
            sound = sound_boop_2;
            break;
    }

    sound_index = (sound_index + 1) % BOOP_COUNT;
    return sound;
}

const Sound_t *play_sound = NULL;

/*
 * Update the piezo for the current state.
 */
void tick_sound (void)
{
    static uint8_t update_delay = 0;

    static uint8_t volume = 0;
    static uint16_t frequency = 0;
    static int8_t freq_shift = 0;

    if (play_sound == NULL)
    {
        return;
    }

    if (update_delay == 0)
    {
        frequency = pgm_read_word (&(play_sound->freq));
        volume = pgm_read_byte (&(play_sound->volume));
        freq_shift = pgm_read_byte (&(play_sound->freq_shift));

        sound_set (frequency, volume);

        if (pgm_read_byte (&(play_sound->duration_ms)))
        {
            update_delay = pgm_read_word (&(play_sound->duration_ms)) / 20;
            play_sound++;
        }
        else
        {
            play_sound = NULL;
            return;
        }
    }

    if (update_delay)
    {
        if (freq_shift)
        {
            frequency += freq_shift;
            sound_set (frequency, volume);
        }
        update_delay--;
    }
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

uint8_t triangle (uint8_t range, uint8_t period, uint8_t frame)
{
    uint16_t half_period = period >> 1;

    return abs ((frame % period) - half_period) * range / half_period;
}

void tick_leds (void)
{
    memset (pixels, 0, 6);

    switch (mode)
    {
        case 0:
            /* Mode: Purple & Pink
             * Boop: Strobe */

            if (boop)
            {
                BOOP_32_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set ((frame & 0x04) ? HUE_VIOLET : HUE_PINK, 0xff, (frame & 0x02) ? 0x18 : 0, EYE_BOTH);
            }
            else
            {
                CYCLE_64_FRAMES;

                eye_hsv_set (HUE_VIOLET + triangle (32, 64, frame), 0xff, 0x10, EYE_LEFT);
                eye_hsv_set (HUE_VIOLET + triangle (32, 64, frame + 32), 0xff, 0x10, EYE_RIGHT);
            }
            break;

        case 1:
            /* Mode: Orange
             * Boop: Strobe */

            if (boop)
            {
                BOOP_32_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set (HUE_ORANGE, 0xff, (frame & 0x04) ? 0x00 : 0x18, EYE_LEFT);
                eye_hsv_set (HUE_ORANGE, 0xff, (frame & 0x04) ? 0x18 : 0x00, EYE_RIGHT);
            }
            else
            {
                CYCLE_64_FRAMES;

                eye_hsv_set (HUE_ORANGE - 0x10 + triangle (32, 64, frame), 0xff, 0x10, EYE_LEFT);
                eye_hsv_set (HUE_ORANGE - 0x10 + triangle (32, 64, frame + 32), 0xff, 0x10, EYE_RIGHT);
            }
            break;

        case 2:
            /* Mode: Red
             * Boop: Strobe */

            if (boop)
            {
                BOOP_32_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set (HUE_RED, 0xff, (frame & 0x04) ? 0x00 : 0x18, EYE_LEFT);
                eye_hsv_set (HUE_RED, 0xff, (frame & 0x04) ? 0x18 : 0x00, EYE_RIGHT);
            }
            else
            {
                CYCLE_64_FRAMES;

                eye_hsv_set (HUE_RED - 0x10 + triangle (32, 64, frame), 0xff, 0x10, EYE_LEFT);
                eye_hsv_set (HUE_RED - 0x10 + triangle (32, 64, frame + 32), 0xff, 0x10, EYE_RIGHT);
            }
            break;

        case 3:
            /* Mode: Green
             * Boop: Strobe */

            if (boop)
            {
                BOOP_32_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set (HUE_GREEN, 0xff, (frame & 0x04) ? 0x00 : 0x18, EYE_LEFT);
                eye_hsv_set (HUE_GREEN, 0xff, (frame & 0x04) ? 0x18 : 0x00, EYE_RIGHT);
            }
            else
            {
                CYCLE_64_FRAMES;

                eye_hsv_set (HUE_GREEN - 0x10 + triangle (32, 64, frame), 0xff, 0x10, EYE_LEFT);
                eye_hsv_set (HUE_GREEN - 0x10 + triangle (32, 64, frame + 32), 0xff, 0x10, EYE_RIGHT);
            }
            break;

        case 4:
            /* Mode: Blue
             * Boop: Strobe */

            if (boop)
            {
                BOOP_32_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set (HUE_AQUA + 0x10, 0xff, (frame & 0x04) ? 0x00 : 0x18, EYE_LEFT);
                eye_hsv_set (HUE_AQUA + 0x10, 0xff, (frame & 0x04) ? 0x18 : 0x00, EYE_RIGHT);
            }
            else
            {
                CYCLE_64_FRAMES;

                eye_hsv_set (HUE_AQUA - 0x10 + triangle (32, 64, frame), 0xff, 0x10, EYE_LEFT);
                eye_hsv_set (HUE_AQUA - 0x10 + triangle (32, 64, frame + 32), 0xff, 0x10, EYE_RIGHT);
            }
            break;

        case 5:
            /* Mode: Rainbow
             * Boop: Bright, fast, and crazy */
            if (boop)
            {
                BOOP_128_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set ((frame << 2) +  64, 0xff, 0x18, EYE_LEFT);
                eye_hsv_set ((frame << 2) + 192, 0xff, 0x18, EYE_RIGHT);
            }
            else
            {
                CYCLE_256_FRAMES;
                eye_hsv_set ((frame << 1), 0xff, 0x10, EYE_BOTH);
            }
            break;

        case 6:
            /* Mode: Rainbow-crossed
             * Boop: Bright, fast, and crazy */
            if (boop)
            {
                BOOP_128_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = get_next_boop_sound ();
                }

                eye_hsv_set ((frame << 2) +  64, 0xff, 0x18, EYE_LEFT);
                eye_hsv_set ((frame << 2) + 192, 0xff, 0x18, EYE_RIGHT);
            }
            else
            {
                CYCLE_256_FRAMES;
                eye_hsv_set ((frame << 1),       0xff, 0x10, EYE_LEFT);
                eye_hsv_set ((frame << 1) + 128, 0xff, 0x10, EYE_RIGHT);
            }
            break;

        case 7:
            /* Mode: Pirihimana
             * Boop: Pirihi-strobe */
        default:
            if (boop)
            {
                BOOP_256_FRAMES;

                if (boop && frame == 0)
                {
                    play_sound = sound_boop_siren;
                }

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

            /* Beep */
            play_sound = sound_mode_change;
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
ISR (TIMER0_COMPA_vect)
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

    /* Output: Piezo */
    DDRB |= (1 << DDB3) | (1 << DDB4);

    /* Output: LED Data */
    DDRB |= (1 << DDB2);

    boop_calibrate ();

    /* Use Timer0 for the 50 Hz tick interrupt */
    TCCR0A = 0x02; /* CTC Mode */
    TCCR0B = 0x05; /* Prescale clock down to 7.8125 kHz */
    OCR0A  = 0x9b; /* Count 0->155 giving 50.08 Hz */
    TIMSK |= 0x10; /* Timer0 Output Compare A interrupt enable */

    /* Use Timer1 in PWM to time the piezo */
    TCCR1  = 0x09; /* Count at 31.25 kHz */
    GTCCR  = 0x40; /* Enable PWM B, outputs off */
    OCR1C  = 0x46; /* Default 440 Hz */
    OCR1B  = 0x23; /* Default 50% duty cycle */

    /* Enable interrupts */
    sei ();

    while (true)
    {
        _delay_ms (10);
    }
}
