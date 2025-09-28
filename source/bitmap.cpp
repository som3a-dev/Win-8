#include "bitmap.hpp"

#include <assert.h>
#include <stdint.h>


void Bitmap::DrawRect(int x, int y, int rect_w, int rect_h, uint8_t r, uint8_t g, uint8_t b)
{
    assert(bpp == 4);

    int right = x + rect_w;
    int bottom = y + rect_h;

    if (x > w) return;
    if (y > h) return;
    if (right < 0) return;
    if (bottom < 0) return;

    for (int y2 = y; (y2 < bottom) && (y2 < h); y2++)
    {
        if (y2 < 0) continue;
        for (int x2 = x; (x2 < right) && (x2 < w); x2++)
        {
            if (x2 < 0) continue;

            int offset = (y2 * w) + x2;
            uint32_t* pixel = ((uint32_t*)data) + offset;
            
            //R and B are flipped due to endianness
            *pixel = (static_cast<uint32_t>(r) << 16) | 
                     (static_cast<uint32_t>(g) << 8)  | 
                     static_cast<uint32_t>(b);
        }
    }
}

void Bitmap::Clear(uint8_t r, uint8_t g, uint8_t b)
{
    assert(bpp == 4);

    for (int y2 = 0; y2 < h; y2++)
    {
        if (y2 < 0) continue;
        for (int x2 = 0; x2 < w; x2++)
        {
            if (x2 < 0) continue;

            int offset = (y2 * w) + x2;
            uint32_t* pixel = ((uint32_t*)data) + offset;

            *pixel = (static_cast<uint32_t>(r) << 16) | 
                     (static_cast<uint32_t>(g) << 8)  | 
                     static_cast<uint32_t>(b);
        }
    }
}
