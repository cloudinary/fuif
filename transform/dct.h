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

// float DCT, code taken from guetzli

// This transform corresponds to JPEG's 8x8 DCT applied to all channels (by default) or some channels (given by the optional parameters).
// It replaces each channel by 64 new channels (with dimensions 1/8), one for each DCT coefficient.
// These new channels are ordered using a fixed 'scan script'; if you want a different order, you can use an explicit TRANSFORM_PERMUTE


/*
 * Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "../image/image.h"



#include <algorithm>
#include <cmath>


// kDCTMatrix[8*u+x] = 0.5*alpha(u)*cos((2*x+1)*u*M_PI/16),
// where alpha(0) = 1/sqrt(2) and alpha(u) = 1 for u > 0.
static const double kDCTMatrix[64] = {
  0.3535533906,  0.3535533906,  0.3535533906,  0.3535533906,
  0.3535533906,  0.3535533906,  0.3535533906,  0.3535533906,
  0.4903926402,  0.4157348062,  0.2777851165,  0.0975451610,
 -0.0975451610, -0.2777851165, -0.4157348062, -0.4903926402,
  0.4619397663,  0.1913417162, -0.1913417162, -0.4619397663,
 -0.4619397663, -0.1913417162,  0.1913417162,  0.4619397663,
  0.4157348062, -0.0975451610, -0.4903926402, -0.2777851165,
  0.2777851165,  0.4903926402,  0.0975451610, -0.4157348062,
  0.3535533906, -0.3535533906, -0.3535533906,  0.3535533906,
  0.3535533906, -0.3535533906, -0.3535533906,  0.3535533906,
  0.2777851165, -0.4903926402,  0.0975451610,  0.4157348062,
 -0.4157348062, -0.0975451610,  0.4903926402, -0.2777851165,
  0.1913417162, -0.4619397663,  0.4619397663, -0.1913417162,
 -0.1913417162,  0.4619397663, -0.4619397663,  0.1913417162,
  0.0975451610, -0.2777851165,  0.4157348062, -0.4903926402,
  0.4903926402, -0.4157348062,  0.2777851165, -0.0975451610,
};

void DCT1d(const double* in, int stride, double* out) {
  for (int x = 0; x < 8; ++x) {
    out[x * stride] = 0.0;
    for (int u = 0; u < 8; ++u) {
      out[x * stride] += kDCTMatrix[8 * x + u] * in[u * stride];
    }
  }
}

void IDCT1d(const double* in, int stride, double* out) {
  for (int x = 0; x < 8; ++x) {
    out[x * stride] = 0.0;
    for (int u = 0; u < 8; ++u) {
      out[x * stride] += kDCTMatrix[8 * u + x] * in[u * stride];
    }
  }
}

typedef void (*Transform1d)(const double* in, int stride, double* out);

void TransformBlock(double block[64], Transform1d f) {
  double tmp[64];
  for (int x = 0; x < 8; ++x) {
    f(&block[x], 8, &tmp[x]);
  }
  for (int y = 0; y < 8; ++y) {
    f(&tmp[8 * y], 1, &block[8 * y]);
  }
}


void ComputeBlockDCTDouble(double block[64]) {
  TransformBlock(block, DCT1d);
}

void ComputeBlockIDCTDouble(double block[64]) {
  TransformBlock(block, IDCT1d);
}


// we use a variant
const int jpeg_zigzag[64] = {

   0,  1,  4, 15, 16, 35, 36, 63,
   2,  3,  5, 14, 17, 34, 37, 62,
   8,  7,  6, 13, 18, 33, 38, 61,
   9, 10, 11, 12, 19, 32, 39, 60,
  24, 23, 22, 21, 20, 31, 40, 59,
  25, 26, 27, 28, 29, 30, 41, 58,
  48, 47, 46, 45, 44, 43, 42, 57,
  49, 50, 51, 52, 53, 54, 55, 56
/*
   0,  1,  5,  6, 14, 15, 27, 28,
   2,  3,  7, 13, 16, 26, 29, 42,
   4,  8, 12, 17, 25, 30, 41, 43,
   9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63
*/
};

/*
// This is the actual zigzag order of JPEG
const int jpeg_zigzag[64] = {
   0,  1,  5,  6, 14, 15, 27, 28,
   2,  4,  7, 13, 16, 26, 29, 42,
   3,  8, 12, 17, 25, 30, 41, 43,
   9, 11, 18, 24, 31, 40, 44, 53,
  10, 19, 23, 32, 39, 45, 52, 54,
  20, 22, 33, 38, 46, 51, 55, 60,
  21, 34, 37, 47, 50, 56, 59, 61,
  35, 36, 48, 49, 57, 58, 62, 63
};
*/

// 1:8 needs only DC
// 1:4 needs coefficients 0,1,2,3 (actually more, but this is OK-ish)
// 1:2 needs coefficients 0-16 (enough?)
const int dct_cshifts[64] = {
   3, 2, 2, 2, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,

   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
//   1, 1, 1, 1, 1, 1, 1, 1,
//   1, 1, 1, 1, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,
};

void default_DCT_scanscript(int nb_components, std::vector<std::vector<int>> & ordering, std::vector<int> & comp, std::vector<int> & coeff) {
   ordering.clear();
   for(int i=0; i<nb_components; i++)
       ordering.push_back(std::vector<int>(64));
   comp.clear();
   coeff.clear();
   std::vector<int> pc(nb_components,0);
   std::vector<int> cshifts(nb_components,3);
   int cc=0;
   int m=0;
   for (int p=0; p<nb_components*64; p++) {
     ordering[cc][pc[cc]] = p;
     comp.push_back(cc);
     coeff.push_back(pc[cc]);
//     v_printf(10,"Position %i: component %i, coefficient %i (cshift=%i)\n",p,cc,pc[cc],cshifts[cc]);
     pc[cc]++;

/*
     if (pc[cc] > 63) {cc++; m=0;}
     else if (dct_cshifts[pc[cc]] < cshifts[cc]) { cshifts[cc]--; cc++; m=0;}
     else if (cshifts[cc] == 2 && ++m == 1) {cc++; m=0;}
     else if (cshifts[cc] == 1 && ++m == 1) {cc++; m=0;}
     else if (cshifts[cc] == 0 && ++m == 2) {cc++; m=0;}
*/

// tried this one, but doesn't seem to be better than doing the simpler thing
//     if (pc[cc] > 63) cc++;
//     else if (dct_cshifts[pc[cc]] < cshifts[cc]) { cshifts[cc]--; cc++; }

// simple scan script: go one coefficient at a time, from low freq to high freq, alternating between the components
     cc++;
     if (cc == nb_components) { cc=0; }

   }
}

void default_DCT_parameters(std::vector<int> &parameters, const Image &image) {
    parameters.clear();
    parameters.push_back(0);
    parameters.push_back(image.nb_channels - 1);
}

void meta_DCT(Image &image, std::vector<int> &parameters) {
    if (!parameters.size()) default_DCT_parameters(parameters,image);
    int beginc = image.nb_meta_channels + parameters[0];
    int endc = image.nb_meta_channels + parameters[1];
    int nb_channels = endc-beginc+1;
    std::vector<std::vector<int>> ordering;
    std::vector<int> comp;
    std::vector<int> coeff;
    default_DCT_scanscript(nb_channels,ordering,comp,coeff);
    for (int c=beginc; c<=endc; c++) {
        int bw = (image.channel[c].w+7)/8;
        int bh = (image.channel[c].h+7)/8;
        image.channel[c].w = bw;
        image.channel[c].h = bh;
        image.channel[c].hshift += 3;
        image.channel[c].vshift += 3;
        image.channel[c].hcshift += 3;
        image.channel[c].vcshift += 3;
    }
    for (int i=nb_channels; i<64*nb_channels; i++) {
          Channel dummy;
          int c = beginc+comp[i];
          dummy.w=image.channel[c].w; dummy.h=image.channel[c].h;
          dummy.hshift = image.channel[c].hshift;
          dummy.vshift = image.channel[c].vshift;
          dummy.hcshift = dct_cshifts[coeff[i]]+image.channel[c].hcshift-3;
          dummy.vcshift = dct_cshifts[coeff[i]]+image.channel[c].vcshift-3;
          dummy.component = image.channel[c].component;
          assert(dummy.component == comp[i]);
          image.channel.push_back(dummy);
    }
}

// TODO: deal with partial inverse DCT for a 1:2 and 1:4 scale decode (it currently produces a 1:1 image)
bool inv_DCT(Image &input, std::vector<int> &parameters) {
    if (!parameters.size()) default_DCT_parameters(parameters,input);
    int beginc = input.nb_meta_channels + parameters[0];
    int endc = input.nb_meta_channels + parameters[1];
    int nb_channels = endc-beginc+1;
    int offset = input.channel.size() - 63*nb_channels;

    if (offset <= endc) {
        e_printf("Invalid number of channels to apply inverse DCT.\n");
        return false;
    }
    v_printf(3,"Undoing DCT on channels %i..%i with AC coefficients in channels %i..%i\n",beginc,endc,offset,offset+63*nb_channels-1);


    std::vector<std::vector<int>> ordering;
    std::vector<int> comp;
    std::vector<int> coeff;
    default_DCT_scanscript(nb_channels,ordering,comp,coeff);

    for (int c=beginc; c<=endc; c++) {
        int bw = input.channel[c-beginc+offset].w; // consider first AC, in case we did repeated DCT (TODO: try repeated DCT to see if it even makes sense)
        int bh = input.channel[c-beginc+offset].h;
        if (input.channel[c].w < bw) bw = input.channel[c].w;
        if (input.channel[c].h < bh) bh = input.channel[c].h;

        v_printf(3,"  Channel %i : %ix%i image from %ix%i blocks\n",c,bw*8,bh*8,bw,bh);
        Channel outch(bw*8,bh*8,0,0);
        outch.component = input.channel[c].component;
        outch.hshift = input.channel[c].hshift - 3;
        outch.vshift = input.channel[c].vshift - 3;
        outch.hcshift = input.channel[c].hcshift - 3;
        outch.vcshift = input.channel[c].hcshift - 3;
        float DCoffset = (input.maxval + 1.0) * 4.0;
        for (int by=0; by<bh; by++) {
          for (int bx=0; bx<bw; bx++) {
            double block[64];
            block[0] = input.channel[c].value(by,bx) + DCoffset;
            for (int i=1; i<64; i++) block[i] = input.channel[offset-nb_channels+ ordering[c-beginc][jpeg_zigzag[i]]].value(by,bx);
            ComputeBlockIDCTDouble(block);
            for (int y=0; y<8; y++)
            for (int x=0; x<8; x++) outch.value(by*8+y, bx*8+x) = round(block[y*8+x]);
          }
        }
        input.channel[c] = outch;
    }
    input.channel.erase(input.channel.begin()+offset,input.channel.begin()+offset+nb_channels*63);
    return true;
}

bool fwd_DCT(Image &input, std::vector<int> &parameters) {
    Image tmp=input; // TODO: implement without copy
    std::vector<int> adj_params = parameters; // use a copy so empty (default) parameters remain empty

    int beginc = input.nb_meta_channels + adj_params[0];
    int endc = input.nb_meta_channels + adj_params[1];
    int nb_channels = endc-beginc+1;
    int offset = input.channel.size();
    meta_DCT(input, adj_params);

    v_printf(3,"Doing DCT on channels %i..%i with AC coefficients in channels %i..%i\n",beginc,endc,offset,offset+63*nb_channels-1);

    std::vector<std::vector<int>> ordering;
    std::vector<int> comp;
    std::vector<int> coeff;
    default_DCT_scanscript(nb_channels,ordering,comp,coeff);

    float DCoffset = (input.maxval + 1.0) * 4.0;

    for (int c=beginc; c<offset+63*nb_channels; c++) {
            input.channel[c].resize();
    }
    for (int c=beginc; c<=endc; c++) {
        int bw=input.channel[c].w;
        int bh=input.channel[c].h;
        v_printf(3,"  Channel %i : %ix%i image to %ix%i blocks\n",c,tmp.channel[c].w,tmp.channel[c].h,bw,bh);
        for (int by=0; by<bh; by++) {
          for (int bx=0; bx<bw; bx++) {
            double block[64];
            for (int i=0; i<64; i++) block[i] = tmp.channel[c].repeating_edge_value(by*8+(i>>3),bx*8+(i&7));
            ComputeBlockDCTDouble(block);
            input.channel[c].value(by,bx) = round(block[0]) - DCoffset;
            for (int i=1; i<64; i++) input.channel[offset-nb_channels+ ordering[c-beginc][jpeg_zigzag[i]]].value(by,bx) = round(block[i]);
          }
        }
    }

    return true;
}



bool DCT(Image &input, bool inverse, std::vector<int> &parameters) {
    if (inverse) return inv_DCT(input,parameters);
    else return fwd_DCT(input,parameters);
}

