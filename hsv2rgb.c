/*
 * HSV to RGB implementation based on https://github.com/FastLED/FastLED (MIT)
 */

#include <stdint.h>

#include "rgb.h"

static uint8_t scale8 (uint16_t i, uint16_t scale)
{
    return (uint16_t)((i * (1 + scale)) >> 8);
}

void hsv2rgb_rainbow (uint8_t hue, uint8_t sat, uint8_t val, RGB_t *rgb)
{
    uint8_t offset = hue & 0x1F; // 0..31
    uint16_t third = (((uint16_t)offset) * 86) / 32;
    uint16_t twothirds = (((uint16_t)offset) * 171) / 32;
    uint8_t r, g, b;

    switch (hue & 0xe0)
    {
        case 0x00: /* Red -> Orange */
            r = 255 - third;
            g = third;
            b = 0;
            break;
        case 0x20: /* Orange -> Yellow */
            r = 171;
            g = 85 + third ;
            b = 0;
            break;
        case 0x40: /* Yellow -> Green */
            r = 171 - twothirds;
            g = 170 + third;
            b = 0;
            break;
        case 0x60: /* Green -> Aqua */
            r = 0;
            g = 255 - third;
            b = third;
            break;
        case 0x80: /* Aqua -> Blue */
            r = 0;
            g = 171 - twothirds;
            b = 85  + twothirds;
            break;
        case 0xa0: /* Blue -> Purple */
            r = third;
            g = 0;
            b = 255 - third;
            break;
        case 0xc0: /* Purple -> Pink */
            r = 85 + third;
            g = 0;
            b = 171 - third;
            break;
        case 0xe0: /* Pink -> Red */
        default:
            r = 170 + third;
            g = 0;
            b = 85 - third;
            break;
    }

    /* Scale down colours if we're desaturated at all
     * and add the brightness_floor to r, g, and b. */
    if (sat != 255) {
        if (sat == 0) {
            r = 255; b = 255; g = 255;
        }
        else
        {
            if (r) r = scale8 (r, sat);
            if (g) g = scale8 (g, sat);
            if (b) b = scale8 (b, sat);

            uint8_t desat = 255 - sat;
            desat = scale8 (desat, desat);

            uint8_t brightness_floor = desat;
            r += brightness_floor;
            g += brightness_floor;
            b += brightness_floor;
        }
    }

    /* Now scale everything down if we're at value < 255. */
    if (val != 255) {
        if (val == 0) {
            r = 0; g = 0; b = 0;
        } else {
            if (r) r = scale8 (r, val);
            if (g) g = scale8 (g, val);
            if (b) b = scale8 (b, val);
        }
    }

    rgb->r = r;
    rgb->g = g;
    rgb->b = b;
}
