/*//////////////////////////////////////////////////////////////////////////////////////////////////////

Copyright 2010-2016, Jon Sneyers & Pieter Wuille
Copyright 2019, Jon Sneyers, Cloudinary (jon@cloudinary.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

//////////////////////////////////////////////////////////////////////////////////////////////////////*/

#include "chance.h"
#include "bit.h"
#include <string.h>


void build_table(uint16_t newchances[4096][2], uint32_t factor, unsigned int max_p) {
    const int64_t one = 1LL << 32;
    const int size = 4096;
    int64_t p;
    unsigned int last_p8, p8;
    unsigned int i;

    memset(newchances,0,sizeof(uint16_t) * size * 2);

    last_p8 = 0;
    p = one / 2;
    for (i = 0; i < size / 2; i++) {
        p8 = (size * p + one / 2) >> 32; //FIXME try without the one
        if (p8 <= last_p8) p8 = last_p8 + 1;
        if (last_p8 && last_p8 < size && p8 <= max_p) newchances[last_p8][1] = p8;

        p += ((one - p) * factor + one / 2) >> 32;
        last_p8 = p8;
    }

    for (i = size - max_p; i <= max_p; i++) {
        if (newchances[i][1]) continue;

        p = (i * one + size / 2) / size;
        p += ((one - p) * factor + one / 2) >> 32;
        p8 = (size * p + one / 2) >> 32; //FIXME try without the one
        if (p8 <= i) p8 = i + 1;
        if (p8 > max_p) p8 = max_p;
        newchances[i][1] = p8;
    }

    for (i = 1; i < size; i++)
        newchances[i][0] = size - newchances[size - i][1];

}

/** Computes an approximation of log(4096 / x) / log(2) * base */
static uint32_t log4kf(int x, uint32_t base) {
    int bits = 8 * sizeof(int) - __builtin_clz(x);
    uint64_t y = ((uint64_t)x) << (32 - bits);
    uint32_t res = base * (13 - bits);
    uint32_t add = base;
    while ((add > 1) && ((y & 0x7FFFFFFF) != 0)) {
        y = (((uint64_t)y) * y + 0x40000000) >> 31;
        add >>= 1;
        if ((y >> 32) != 0) {
            res -= add;
            y >>= 1;
        }
    }
    return res;
}

Log4kTable::Log4kTable() {
    data[0] = 0;
    for (int i = 1; i <= 4096; i++) {
        data[i] = (log4kf(i, (65535UL << 16) / 12) + (1 << 15)) >> 16;
    }
}

const Log4kTable log4k;
