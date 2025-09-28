#pragma once

#include <stdint.h>

struct Bitmap
{
    char* data = NULL;
    int w = 0;
    int h = 0;
    int bpp = 4;


    void DrawRect(int x, int y, int rect_w, int rect_h, uint8_t r, uint8_t g, uint8_t b);
    void Clear(uint8_t r, uint8_t g, uint8_t b);
};