#include <stdint.h>
#include "color.h"

/*
 * getColor - Converts the color struct to a 32-bit int containing 8 bits of filler, the 8 red bits, the 8 green bits, then the 8
 * blue bits, in that order. Used to set the appropriate pixel in the bitmap to a color.
 */
uint32_t getColor(rgb c) {
    uint32_t compColor = 0;
    compColor += c.red;
    compColor <<= 8;
    compColor += c.green;
    compColor <<= 8;
    compColor += c.blue;
    return compColor;
}

/*
 * colorMul - Multiplies a color by a constant. This function clamps the color down to 255.
 */
rgb colorMul(rgb color, double mul) {
    double red = color.red * mul;
    double green = color.green * mul;
    double blue = color.blue * mul;

    uint8_t redComp = 0;
    if (red < 255) {
        redComp = (uint8_t)red;
    } else {
        redComp = 255;
    }
    uint8_t greenComp = 0;
    if (green < 255) {
        greenComp = (uint8_t)green;
    } else {
        greenComp = 255;
    }
    uint8_t blueComp = 0;
    if (blue < 255) {
        blueComp = (uint8_t)blue;
    } else {
        blueComp = 255;
    }
    return (rgb) { 
        .red = redComp,
        .blue = blueComp,
        .green = greenComp
    };
}

/*
 * colorAdd - Adds two colors together, rounds to 255 if it is higher.
 */
rgb colorAdd(rgb color, rgb color2) {
    uint32_t red = color.red + color2.red;
    uint32_t green = color.green + color2.green;
    uint32_t blue = color.blue + color2.blue;

    uint8_t redComp = 0;
    if (red < 255) {
        redComp = (uint8_t)red;
    } else {
        redComp = 255;
    }
    uint8_t greenComp = 0;
    if (green < 255) {
        greenComp = (uint8_t)green;
    } else {
        greenComp = 255;
    }
    uint8_t blueComp = 0;
    if (blue < 255) {
        blueComp = (uint8_t)blue;
    } else {
        blueComp = 255;
    }
    return (rgb) { .red = redComp, .blue = blueComp, .green = greenComp };
}