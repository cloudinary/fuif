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



bool inv_approximate(Image &input, const std::vector<int> &parameters) {
    int beginc = parameters[0];
    int endc = parameters[1];
    int offset = input.channel.size() - (endc - beginc + 1);
    for (int c=beginc; c<=endc; c++) {
        int q = (c+2-beginc < parameters.size() ? parameters[c+2-beginc] : parameters.back());
        if (!q) offset++;
    }
    v_printf(3,"Reconstructing approximated channels %i-%i using remainder channels %i-%i.\n",beginc,endc,offset,input.channel.size()-1);
    int i=0;
    for (int c=beginc; c<=endc; c++) {
        int q = (c+2-beginc < parameters.size() ? parameters[c+2-beginc] : parameters.back());
        q += 1;
        if (q == 1) continue;
        Channel &ch = input.channel[c];
        const Channel &chr = input.channel[offset+i];
        i++;
        if (chr.data.size() == 0) v_printf(3,"Remainder channel is not available.\n");
        else ch.q = chr.q; // usually these are the same, except when approximated channel became all zeroes
        for (int y=0; y<ch.h; y++) {
          for (int x=0; x<ch.w; x++) {
            ch.value(y,x) *= q;
//            ch.value(y,x) += (chr.data.size() == 0 ? q/2 : chr.value(y,x));
            ch.value(y,x) += (chr.data.size() == 0 ? 0 : chr.value(y,x));
          }
        }
    }
    input.channel.erase(input.channel.begin() + offset, input.channel.end());
    return true;
}

void meta_approximate(Image &image, std::vector<int> &parameters) {
    if (parameters.size() < 3) {
        e_printf("Incorrect number of parameters for Approximation transform.\n");
        image.error = true;
        return;
    }
    int nb = parameters[1] - parameters[0] + 1;
    if (nb < 1 || parameters[0] < 0 || parameters[1] >= image.channel.size()) {
        e_printf("Incorrect parameters for Approximation transform.\n");
        image.error = true;
        return;
    }
    for (int c = parameters[0]; c <= parameters[1]; c++) {
        int q = (c+2-parameters[0] < parameters.size() ? parameters[c+2-parameters[0]] : parameters.back());
        if (q) image.channel.push_back(image.channel[c]);
    }
}


bool fwd_approximate(Image &input, std::vector<int> &parameters) {
    int offset = input.channel.size();
    meta_approximate(input,parameters);
    int beginc = parameters[0];
    int endc = parameters[1];
    v_printf(3,"Approximating channels %i-%i\n",beginc,endc);
    int i=0;
    for (int c=beginc; c<=endc; c++) {
        int q = (c+2-beginc < parameters.size() ? parameters[c+2-beginc] : parameters.back());
        q += 1;
        if (q == 1) continue;
        Channel &ch = input.channel[c];
        Channel &chr = input.channel[offset+i];
        i++;
        for (int y=0; y<ch.h; y++) {
          for (int x=0; x<ch.w; x++) {
            pixel_type p = ch.value(y,x);
            pixel_type quotient = p/q;
            pixel_type r = p%q;
            if (r < 0) { quotient--; r+=q; }
            ch.value(y,x) = quotient;
            chr.value(y,x) = r;
          }
        }
        ch.minval /= q;
        ch.maxval /= q;
        chr.minval = 0;
        chr.maxval = q-1;
        chr.q = ch.q; // need to duplicate the quantization factor in case approximated channel becomes all zeroes
    }
    return true;
}

bool approximate(Image &input, bool inverse, std::vector<int> &parameters) {
    if (inverse) return inv_approximate(input, parameters);
    else return fwd_approximate(input, parameters);
}

