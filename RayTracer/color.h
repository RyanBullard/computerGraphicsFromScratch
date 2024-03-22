#pragma once

#include "standardHeader.h"

typedef struct rgb { // Stores color information in a more readable way compared to a uint32_t.
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb;

uint32_t getColor(rgb);
rgb colorMul(rgb, double);
rgb colorAdd(rgb, rgb);