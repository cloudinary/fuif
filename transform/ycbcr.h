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


bool inv_YCbCr(Image &input) ATTRIBUTE_HOT;
bool inv_YCbCr(Image &input) {
    int nb_channels = input.channel.size();
    if (nb_channels < 3) {
        e_printf("Invalid number of channels to apply inverse YCbCr.\n");
        return false;
    }
    int w = input.channel[0].w;
    int h = input.channel[0].h;
    if (input.channel[1].w < w
     || input.channel[1].h < h
     || input.channel[2].w < w
     || input.channel[2].h < h) {
        e_printf("Invalid channel dimensions to apply inverse YCbCr (maybe chroma is subsampled?).\n");
        return false;
    }

    float half = (input.maxval+1)/2;
    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x++) {
        float yy = input.channel[0].value(y,x);
        float cb = input.channel[1].value(y,x) - half;
        float cr = input.channel[2].value(y,x) - half;

        input.channel[0].value(y,x) = CLAMP(yy + 1.402*cr + 0.5, input.minval, input.maxval);
        input.channel[1].value(y,x) = CLAMP(yy - 0.344136*cb - 0.714136*cr  + 0.5, input.minval, input.maxval);
        input.channel[2].value(y,x) = CLAMP(yy + 1.772*cb  + 0.5, input.minval, input.maxval);
      }
    }

    return true;
}

bool fwd_YCbCr(Image &input) {
    int nb_channels = input.channel.size();
    if (nb_channels < 3) {
        e_printf("Invalid number of channels to apply YCbCr.\n");
        return false;
    }
    int w = input.channel[0].w;
    int h = input.channel[0].h;
    if (input.channel[1].w < w
     || input.channel[1].h < h
     || input.channel[2].w < w
     || input.channel[2].h < h) {
        e_printf("Invalid channel dimensions to apply YCbCr.\n");
        return false;
    }

    float half = (input.maxval+1)/2;
    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x++) {
        float r = input.channel[0].value(y,x);
        float g = input.channel[1].value(y,x);
        float b = input.channel[2].value(y,x);

        input.channel[0].value(y,x) = CLAMP(0.299*r + 0.587*g + 0.114*b, input.minval, input.maxval);
        input.channel[1].value(y,x) = CLAMP(half - 0.168736*r - 0.331264*g + 0.5*b, input.minval, input.maxval);
        input.channel[2].value(y,x) = CLAMP(half + 0.5*r - 0.418688*g - 0.081312*b, input.minval, input.maxval);
      }
    }

    return true;
}


bool YCbCr(Image &input, bool inverse) {
    if (inverse) return inv_YCbCr(input);
    else return fwd_YCbCr(input);
}

