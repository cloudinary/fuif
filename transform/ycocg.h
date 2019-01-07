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
#include "../config.h"


bool inv_YCoCg(Image &input) ATTRIBUTE_HOT;
bool inv_YCoCg(Image &input) {
    int m = input.nb_meta_channels;
    int nb_channels = input.nb_channels;
    if (nb_channels < 3) {
        e_printf("Invalid number of channels to apply inverse YCoCg.\n");
        return false;
    }
    int w = input.channel[m+0].w;
    int h = input.channel[m+0].h;
    if (input.channel[m+1].w < w
     || input.channel[m+1].h < h
     || input.channel[m+2].w < w
     || input.channel[m+2].h < h) {
        e_printf("Invalid channel dimensions to apply inverse YCoCg (maybe chroma is subsampled?).\n");
        return false;
    }
    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x++) {
        int Y = CLAMP(input.channel[m+0].value(y,x), 0, input.maxval);
        int Co = input.channel[m+1].value(y,x);
        int Cg = input.channel[m+2].value(y,x);
        int G = CLAMP(Y - ((-Cg)>>1), 0, input.maxval);
        int B = CLAMP(Y + ((1-Cg)>>1) - (Co>>1), 0, input.maxval);
        int R = CLAMP(Co + B, 0, input.maxval);
        input.channel[m+0].value(y,x) = R;
        input.channel[m+1].value(y,x) = G;
        input.channel[m+2].value(y,x) = B;
      }
    }
    return true;
}

bool fwd_YCoCg(Image &input) {
    int nb_channels = input.nb_channels;
    if (nb_channels < 3) {
        //e_printf("Invalid number of channels to apply YCoCg.\n");
        return false;
    }
    int m = input.nb_meta_channels;
    int w = input.channel[m+0].w;
    int h = input.channel[m+0].h;
    if (input.channel[m+1].w < w
     || input.channel[m+1].h < h
     || input.channel[m+2].w < w
     || input.channel[m+2].h < h) {
        e_printf("Invalid channel dimensions to apply YCoCg.\n");
        return false;
    }
    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x++) {
        int R = input.channel[m+0].value(y,x);
        int G = input.channel[m+1].value(y,x);
        int B = input.channel[m+2].value(y,x);
        int Y = (((R + B)>>1) + G)>>1;
        int Co = R - B;
        int Cg = G - ((R + B)>>1);
        input.channel[m+0].value(y,x) = Y;
        input.channel[m+1].value(y,x) = Co;
        input.channel[m+2].value(y,x) = Cg;
      }
    }
    return true;
}


bool YCoCg(Image &input, bool inverse) {
    if (inverse) return inv_YCoCg(input);
    else return fwd_YCoCg(input);
}
