/*//////////////////////////////////////////////////////////////////////////////////////////////////////

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

#include "../image/image.h"
#include "../io.h"



bool inv_quantize(Image &input, const std::vector<int> &parameters) {
    for (int c=input.nb_meta_channels; c<input.channel.size(); c++) {
        Channel &ch = input.channel[c];
        if (ch.data.size() == 0) continue;
        int q = ch.q;
        if (q == 1) continue;
        v_printf(3,"De-quantizing channel %i with quantization constant %i\n",c,q);
        for (int y=0; y<ch.h; y++) {
          for (int x=0; x<ch.w; x++) {
            ch.value(y,x) *= q;
          }
        }
        ch.minval *= q;
        ch.maxval *= q;
        ch.q = 1;
    }
    return true;
}



//inline int rounded_div(const int n, const int d) { return ((n < 0) ^ (d < 0)) ? ((n - d/2)/d) : ((n + d/2)/d); }
inline int rounded_div(const int n, const int d) { return n/d; }

bool fwd_quantize(Image &input, std::vector<int> &parameters) {
    for (int c=input.nb_meta_channels; c<input.channel.size(); c++) {
        Channel &ch = input.channel[c];
        int q = (c < parameters.size() ? parameters[c] : parameters.back());
        v_printf(3,"Quantizing channel %i with quantization constant %i\n",c,q);
        for (int y=0; y<ch.h; y++) {
          for (int x=0; x<ch.w; x++) {
            ch.value(y,x) = rounded_div(ch.value(y,x),q);
          }
        }
        ch.minval /= q;
        ch.maxval /= q;
        ch.q = q;
    }
    return true;
}

bool quantize(Image &input, bool inverse, std::vector<int> &parameters) {
    if (inverse) return inv_quantize(input, parameters);
    else return fwd_quantize(input, parameters);
}

