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

