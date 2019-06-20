/*
 * HSV to RGB implementation based on https://github.com/FastLED/FastLED (MIT)
 */

#define HUE_RED  0x00
#define HUE_BLUE 0xa0
#define HUE_VIOLET 0xc0

void hsv2rgb_rainbow (uint8_t hue, uint8_t sat, uint8_t val, RGB_t *rgb);
