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

template <typename RAC>
void UniformSymbolCoder<RAC>::write_int(int min, int max, int val) {
        assert(max >= min);
        if (min != 0) {
            max -= min;
            val -= min;
        }
        if (max == 0) return;

        // split in [0..med] [med+1..max]
        int med = max/2;
        if (val > med) {
            rac.write_bit(true);
            write_int(med+1, max, val);
        } else {
            rac.write_bit(false);
            write_int(0, med, val);
        }
        return;
}


template <typename SymbolCoder> void writer(SymbolCoder& coder, int bits, int value) {
  int pos=0;
  while (pos++ < bits) {
    coder.write(value&1, BIT_MANT, pos);
    value >>= 1;
  }
}

template <int bits, typename SymbolCoder> void writer(SymbolCoder& coder, int min, int max, int value) {
    assert(min<=max);
    assert(value>=min);
    assert(value<=max);

    // avoid doing anything if the value is already known
    if (min == max) return;

    if (value == 0) { // value is zero
        coder.write(true, BIT_ZERO);
        return;
    }

    assert(min <= 0 && max >= 0); // should always be the case, because guess should always be in valid range

    coder.write(false,BIT_ZERO);
    int sign = (value > 0 ? 1 : 0);
    if (max > 0 && min < 0) {
        // only output sign bit if value can be both pos and neg
        coder.write(sign,BIT_SIGN);
    }
    const int a = abs(value);
    const int e = maniac::util::ilog2(a);
    int amax = sign ? abs(max) : abs(min);

    int emax = maniac::util::ilog2(amax);

    int i = 0; //maniac::util::ilog2(amin);
    while (i < emax) {
        // if exponent >i is impossible, we are done
        if ((1 << (i+1)) > amax) break;
        // if exponent i is possible, output the exponent bit
        coder.write(i==e, BIT_EXP, i);
        if (i==e) break;
        i++;
    }

    int have = (1 << e);
    int left = have-1;
    for (int pos = e; pos>0;) {
        int bit = 1;
        left ^= (1 << (--pos));
        int minabs1 = have | (1<<pos);
        if (minabs1 > amax) { // 1-bit is impossible
            bit = 0;
        } else {
            bit = (a >> pos) & 1;
            coder.write(bit, BIT_MANT, pos);
        }
        have |= (bit << pos);
    }
}

template <int bits, typename SymbolCoder> int estimate_writer(SymbolCoder& coder, int min, int max, int value) {
    assert(min<=max);
    assert(value>=min);
    assert(value<=max);

    uint64_t total=0;

    // avoid doing anything if the value is already known
    if (min == max) return total;

    if (value == 0) { // value is zero
        coder.estimate(true, BIT_ZERO,0,total);
        return total;
    }

    assert(min <= 0 && max >= 0); // should always be the case, because guess should always be in valid range

    coder.estimate(false,BIT_ZERO,0,total);
    int sign = (value > 0 ? 1 : 0);
    if (max > 0 && min < 0) {
        // only output sign bit if value can be both pos and neg
        coder.estimate(sign,BIT_SIGN,0,total);
    }
    if (sign) min = 1;
    if (!sign) max = -1;
    const int a = abs(value);
    const int e = maniac::util::ilog2(a);
    int amin = sign ? abs(min) : abs(max);
    int amax = sign ? abs(max) : abs(min);

    int emax = maniac::util::ilog2(amax);
    int i = maniac::util::ilog2(amin);

    while (i < emax) {
        // if exponent >i is impossible, we are done
        if ((1 << (i+1)) > amax) break;
        // if exponent i is possible, output the exponent bit
        coder.estimate(i==e, BIT_EXP, i,total);
        if (i==e) break;
        i++;
    }

    int have = (1 << e);
    int left = have-1;
    for (int pos = e; pos>0;) {
        int bit = 1;
        left ^= (1 << (--pos));
        int minabs1 = have | (1<<pos);
        if (minabs1 > amax) { // 1-bit is impossible
            bit = 0;
        } else {
            bit = (a >> pos) & 1;
            coder.estimate(bit, BIT_MANT, pos,total);
        }
        have |= (bit << pos);
    }
//    printf("Total to write %i in %i..%i: %lu\n",value,min,max,total);
    return total;
}


template <typename BitChance, typename RAC, int bits>
void SimpleSymbolBitCoder<BitChance,RAC,bits>::write(bool bit, SymbolChanceBitType typ, int i) {
        BitChance& bch = ctx.bit(typ, i);
        rac.write_12bit_chance(bch.get_12bit(), bit);
        bch.put(bit, table);
}


template <typename BitChance, typename RAC, int bits>
void SimpleSymbolCoder<BitChance,RAC,bits>::write_int(int min, int max, int value) {
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        writer<bits, SimpleSymbolBitCoder<BitChance, RAC, bits> >(bitCoder, min, max, value);
}
template <typename BitChance, typename RAC, int bits>
void SimpleSymbolCoder<BitChance,RAC,bits>::write_int(int nbits, int value) {
        assert (nbits <= bits);
        SimpleSymbolBitCoder<BitChance, RAC, bits> bitCoder(table, ctx, rac);
        writer(bitCoder, nbits, value);
}
