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

#pragma once

#include <math.h>
#include <stdint.h>
#include <cstddef>


struct Log4kTable {
    uint16_t data[4097];
    Log4kTable();
};

extern const Log4kTable log4k;

void extern build_table(uint16_t newchances[4096][2], uint32_t factor, unsigned int max_p);

class SimpleBitChanceTable
{
public:
    uint16_t newchance[4096][2]; // stored as 12-bit numbers
    uint32_t alpha;

    void init(int cut, int alpha_) {
        alpha = alpha_;
        build_table(newchance, alpha_, 4096-cut);
    }

    SimpleBitChanceTable(int cut = 2, int alpha = 0xFFFFFFFF / 19) {
        init(cut, alpha);
    }
};


class SimpleBitChance
{
protected:
    uint16_t chance; // stored as a 12-bit number

public:
    typedef SimpleBitChanceTable Table;

    SimpleBitChance() {
        chance = 0x800;
    }

    uint16_t inline get_12bit() const {
        return chance;
    }
    void set_12bit(uint16_t chance) {
        this->chance = chance;
    }
    void inline put(bool bit, const Table &table) {
        chance = table.newchance[chance][bit];
    }

    void estim(bool bit, uint64_t &total) const {
        total += log4k.data[bit ? chance : 4096-chance];
    }
};