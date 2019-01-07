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

#include <vector>
#include <assert.h>
#include "util.h"
#include "chance.h"

template <typename RAC> class UniformSymbolCoder {
private:
    RAC &rac;

public:
    explicit UniformSymbolCoder(RAC &racIn) : rac(racIn) { }

#ifdef HAS_ENCODER
    void write_int(int min, int max, int val);
    void write_int(int bits, int val) { write_int(0, (1<<bits) -1, val); };
#endif
    int read_int(int min, int len) {
        assert(len >= 0);
        if (len == 0) return min;

        // split in [0..med] [med+1..len]
        int med = len/2;
        bool bit = rac.read_bit();
        if (bit) {
            return read_int(min+med+1, len-(med+1));
        } else {
            return read_int(min, med);
        }
    }
    int read_int(int bits) { return read_int(0, (1<<bits)-1); }
};

typedef enum {
    BIT_ZERO,
    BIT_SIGN,
    BIT_EXP,
    BIT_MANT,
} SymbolChanceBitType;

static const uint16_t ZERO_CHANCE = 1024;
static const uint16_t SIGN_CHANCE = 2048;
static const uint16_t MANT_CHANCE = 1024;


template <typename BitChance, int bits> class SymbolChance {
public:
    BitChance bit_zero;
    BitChance bit_sign;
    BitChance bit_exp[bits-1];
    BitChance bit_mant[bits];

public:

    BitChance inline &bitZero()      {
        return bit_zero;
    }

    BitChance inline &bitSign()      {
        return bit_sign;
    }

    // all exp bits 0         -> int(log2(val)) == 0  [ val == 1 ]
    // exp bits up to i are 1 -> int(log2(val)) == i+1
    BitChance inline &bitExp(int i)  {
        assert(i >= 0 && i < (bits-1));
        return bit_exp[i];
    }
    BitChance inline &bitMant(int i) {
        assert(i >= 0 && i < bits);
        return bit_mant[i];
    }

    BitChance inline &bit(SymbolChanceBitType typ, int i = 0) {
        switch (typ) {
        default:
        case BIT_ZERO:
            return bitZero();
        case BIT_SIGN:
            return bitSign();
        case BIT_EXP:
            return bitExp(i);
        case BIT_MANT:
            return bitMant(i);
        }
    }
    SymbolChance() { } // don't init  (needed for fast copy constructor in CompoundSymbolChances?)

    SymbolChance(uint16_t zero_chance) {
        bitZero().set_12bit(zero_chance);
//        bitSign().set_12bit(0x800); // 50%, which is the default anyway
        uint64_t p = zero_chance;       // implicit denominator of 0x1000
        uint64_t rp = 0x1000 - zero_chance;  // 1-p

        // assume geometric distribution: Pr(X=k) = (1-p)^k p
        // (p = zero_chance)
        // cumulative distribution function: Pr(X < k) = 1-(1-p)^k = 1-rp^k
        //                                   Pr(X >= k) = (1-p)^k = rp^k
        // bitExp(i) == true   iff   X >= 2^i && X < 2^(i+1)
        // bitExp(i) is only used if the lower bound holds
        // Pr(X >= 2^i && X < 2^(i+1) | X >= 2^i ) = (rp^2^i - rp^2^(i+1)) / rp^2^i = 1 - rp^2^i

        for (int i=0; i<bits-1; i++) {
            if (rp < 0x100) rp = 0x100;
            if (rp > 0xf00) rp = 0xf00;
            bitExp(i).set_12bit(0x1000 - rp);
            rp = (rp*rp + 0x800) >> 12;     // square it
        }
        for (int i=0; i<bits; i++) {
            bitMant(i).set_12bit(MANT_CHANCE);
        }
    }
};

template <typename SymbolCoder> int reader(SymbolCoder& coder, int bits) {
  int pos=0;
  int value=0;
  int b=1;
  while (pos++ < bits) {
    if (coder.read(BIT_MANT, pos)) value += b;
    b *= 2;
  }
  return value;
}

template <int bits, typename SymbolCoder> int reader(SymbolCoder& coder, int min, int max) ATTRIBUTE_HOT;

template <int bits, typename SymbolCoder> int reader(SymbolCoder& coder, int min, int max) {
    assert(min<=max);
    if (min == max) return min;

    bool sign;
    assert(min <= 0 && max >= 0); // should always be the case, because guess should always be in valid range

      if (coder.read(BIT_ZERO)) return 0;
      if (min < 0) {
        if (max > 0) {
          sign = coder.read(BIT_SIGN);
        } else {sign = false; }
      } else {sign = true; }

    const int amax = (sign? max : -min);
    const int emax = maniac::util::ilog2(amax);

    int e = 0;
    for (; e < emax; e++) {
        if (coder.read(BIT_EXP,e)) break;
    }

    int have = (1 << e);

    for (int pos = e; pos>0;) {
        pos--;
        int minabs1 = have | (1<<pos);
        if (minabs1 > amax) continue; // 1-bit is impossible
        if (coder.read(BIT_MANT,pos)) have = minabs1;
    }
    return (sign ? have : -have);
}

template <typename BitChance, typename RAC, int bits> class SimpleSymbolBitCoder {
    typedef typename BitChance::Table Table;

private:
    const Table &table;
    SymbolChance<BitChance,bits> &ctx;
    RAC &rac;

public:
    SimpleSymbolBitCoder(const Table &tableIn, SymbolChance<BitChance,bits> &ctxIn, RAC &racIn) : table(tableIn), ctx(ctxIn), rac(racIn) {}

#ifdef HAS_ENCODER
    void write(bool bit, SymbolChanceBitType typ, int i = 0);
#endif

    bool read(SymbolChanceBitType typ, int i = 0) {
        BitChance& bch = ctx.bit(typ, i);
        bool bit = rac.read_12bit_chance(bch.get_12bit());
        bch.put(bit, table);
        return bit;
    }
};

template <typename BitChance, typename RAC, int bits> class SimpleSymbolCoder {
    typedef typename BitChance::Table Table;

private:
    SymbolChance<BitChance,bits> ctx;
    const Table table;
    RAC &rac;

public:
    SimpleSymbolCoder(RAC& racIn, int cut = 2, int alpha = 0xFFFFFFFF / 19) :  ctx(ZERO_CHANCE), table(cut,alpha), rac(racIn) { }

#ifdef HAS_ENCODER
    void write_int(int min, int max, int value);
    void write_int2(int min, int max, int value) {
        if (min>0) write_int(0,max-min,value-min);
        else if (max<0) write_int(min-max,0,value-max);
        else write_int(min,max,value);
    }
    void write_int(int nbits, int value);
#endif

    int read_int(int min, int max) {
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        return reader<bits, SimpleSymbolBitCoder<BitChance, RAC, bits>>(bitCoder, min, max);
    }
    int read_int2(int min, int max) {
        if (min > 0) return read_int(0,max-min)+min;
        else if (max<0) return read_int(min-max,0)+max;
        else return read_int(min,max);
    }
    int read_int(int nbits) {
        assert (nbits <= bits);
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        return reader(bitCoder, nbits);
    }
};

#ifdef HAS_ENCODER
#include "symbol_enc.h"
#endif
