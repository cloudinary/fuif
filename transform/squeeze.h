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

// Haar-like transform: halves the resolution in one direction
// A B   -> (A+B)>>1              in one channel (average)  -> same range as original channel
//          A-B - tendency        in a new channel ('residual' needed to make the transform reversible)
//                                        -> theoretically range could be 2.5 times larger (2 times without the 'tendency'), but there should be lots of zeroes
// Repeated application (alternating horizontal and vertical squeezes) results in downscaling

// TODO: Chroma subsampling is kind of a special case of this, where the residual is simply dropped
//       Add an option to drop a residual? Could also be simulated by quantizing the residual by maxval so it becomes a trivial all-zeroes channel.

#include "../image/image.h"
#include "stdlib.h"
#include "../config.h"

/*
        int avg=(A+B)>>1;
        int diff=(A-B);
        int rA=(diff+(avg<<1)+(diff&1))>>1;
        int rB=rA-diff;

*/
//         |A B|C D|E F|
//           p   a   n             p=avg(A,B), a=avg(C,D), n=avg(E,F)
//
// Goal: estimate C-D (avoiding ringing artifacts)
// (ensuring that in smooth areas, a zero residual corresponds to a smooth gradient)

// best estimate for C: (B + 2*a)/3
// best estimate for D: (n + 3*a)/4
// best estimate for C-D:  4*B - 3*n - a /12

// avoid ringing by 1) only doing this if B <= a <= n  or  B >= a >= n       (otherwise, this is not a smooth area and we cannot really estimate C-D)
//                  2) making sure that B <= C <= D <= n  or B >= C >= D >= n


inline pixel_type smooth_tendency(pixel_type B, pixel_type a, pixel_type n) {
    pixel_type diff = 0;
    if (B >= a && a >= n) {
        diff = (4*B - 3*n - a + 6)/12;
//      2C = a<<1 + diff - diff&1 <= 2B  so diff - diff&1 <= 2B - 2a
//      2D = a<<1 - diff - diff&1 >= 2n  so diff + diff&1 <= 2a - 2n
        if (diff-(diff&1) > 2*(B-a)) diff = 2*(B-a)+1;
        if (diff+(diff&1) > 2*(a-n)) diff = 2*(a-n);
    } else if (B <= a && a <= n) {
        diff = (4*B - 3*n - a - 6)/12;
//      2C = a<<1 + diff + diff&1 >= 2B  so diff + diff&1 >= 2B - 2a
//      2D = a<<1 - diff + diff&1 <= 2n  so diff - diff&1 >= 2a - 2n
        if (diff+(diff&1) < 2*(B-a)) diff = 2*(B-a)-1;
        if (diff-(diff&1) < 2*(a-n)) diff = 2*(a-n);
    }
    return diff;
}

void inv_hsqueeze(Image &input, int c, int rc) ATTRIBUTE_HOT;

void inv_hsqueeze(Image &input, int c, int rc) {
    const Channel &chin = input.channel[c];
    const Channel &chin_residual = input.channel[rc];
    Channel chout(chin.w + chin_residual.w, chin.h,chin.minval,chin.maxval,chin.q,chin.hshift-1,chin.vshift,chin.hcshift-1,chin.vcshift);
    chout.component = chin.component;
    v_printf(4,"Undoing horizontal squeeze of channel %i using residuals in channel %i (going from width %i to %i)\n",c,rc,chin.w,chout.w);

    for (int y=0; y<chin.h; y++) {
      pixel_type avg = chin.value_nocheck(y,0);
      pixel_type next_avg = (1<chin.w ? next_avg = chin.value_nocheck(y,1) : avg);
      pixel_type tendency=smooth_tendency(avg,avg,next_avg);
      pixel_type diff = chin_residual.value(y,0) + tendency;
      pixel_type A = ((avg<<1)+diff+(diff>0?-(diff&1):(diff&1)))>>1;
      pixel_type B = A-diff;
      chout.value(y,0) = A;
      chout.value(y,1) = B;
      for (int x=1; x<chin_residual.w; x++) {
        pixel_type diff_minus_tendency = chin_residual.value(y,x);
        pixel_type avg = chin.value_nocheck(y,x);
        pixel_type next_avg = (x+1<chin.w ? next_avg = chin.value_nocheck(y,x+1) : avg);
        pixel_type left = chout.value_nocheck(y,(x<<1)-1);
        pixel_type tendency=smooth_tendency(left,avg,next_avg);
        pixel_type diff = diff_minus_tendency + tendency;
//        pixel_type A = ((avg<<1)+diff+(diff&1))>>1;
        pixel_type A = ((avg<<1)+diff+(diff>0?-(diff&1):(diff&1)))>>1;
        chout.value(y,x<<1) = A;
        pixel_type B = A-diff;
        chout.value(y,(x<<1)+1) = B;
//        if (c==2 && chin.w == 1352 && x > 180 && x < 185 && y > 118 && y < 121) printf("Cg = avg=%i, diff=%i=%i+%i, A=%i,B=%i\n",avg,diff,diff_minus_tendency,tendency,A,B);
      }
/*
      for (int x=1; x<chin_residual.w; x++) {
        pixel_type diff_minus_tendency = chin_residual.value(y,x);
        pixel_type avg = chin.value(y,x);

        pixel_type next_avg = (x+1<chin.w ? chin.value(y,x+1) : avg);
        pixel_type leftleft = (x>0 ? chout.value(y,x*2-2) : avg);
        pixel_type left = (x>0 ? chout.value(y,x*2-1) : avg);
        pixel_type tendency=smooth_tendency(leftleft,left,avg,next_avg);

        pixel_type diff = diff_minus_tendency + tendency;

        pixel_type A = ((avg<<1)+diff+(diff&1))>>1;
        pixel_type B = A-diff;
        chout.value(y,x*2) = A;
        chout.value(y,x*2+1) = B;
      }
*/
      if (chout.w & 1) chout.value(y,chout.w-1) = chin.value(y,chin.w-1);
    }
    input.channel[c] = chout;
}


void fwd_hsqueeze(Image &input, int c, int rc) {
    const Channel &chin = input.channel[c];

    v_printf(4,"Doing horizontal squeeze of channel %i to new channel %i\n",c,rc);

    Channel chout((chin.w+1)/2, chin.h, chin.minval, chin.maxval, chin.q, chin.hshift+1,chin.vshift,chin.hcshift+1,chin.vcshift);
    Channel chout_residual(chin.w-chout.w,chout.h,chout.minval-chout.maxval,chout.maxval-chout.minval,1, chin.hshift+1,chin.vshift,chin.hcshift,chin.vcshift);
    chout.component = chin.component;
    chout_residual.component = chin.component;

    for (int y=0; y<chout.h; y++) {
      for (int x=0; x<chout_residual.w; x++) {
        pixel_type A = chin.value(y,x*2);
        pixel_type B = chin.value(y,x*2+1);
        pixel_type avg = (A+B+(A>B))>>1;
        chout.value(y,x) = avg;
        pixel_type diff = A-B;
        pixel_type next_avg = avg;
        if (x+1<chout_residual.w) next_avg = (chin.value(y,x*2+2) + chin.value(y,x*2+3) + (chin.value(y,x*2+2) > chin.value(y,x*2+3))) >> 1;  // which will be chout.value(y,x+1)
        else if (chin.w & 1) next_avg = (chin.value(y,x*2+2));

        pixel_type left = (x>0 ? chin.value(y,x*2-1) : avg);
        pixel_type tendency=smooth_tendency(left,avg,next_avg);

        chout_residual.value(y,x) = diff-tendency;

      }
      if (chin.w & 1) {
        int x = chout.w-1;
        chout.value(y,x) = chin.value(y,x*2);
      }
    }
    input.channel[c] = chout;
//    chout_residual.actual_minmax(&chout_residual.minval, &chout_residual.maxval);
    input.channel.insert(input.channel.begin()+rc, chout_residual);
}

void inv_vsqueeze(Image &input, int c, int rc) ATTRIBUTE_HOT;
void inv_vsqueeze(Image &input, int c, int rc) {
    const Channel &chin = input.channel[c];
    const Channel &chin_residual = input.channel[rc];
    Channel chout(chin.w, chin.h + chin_residual.h,chin.minval,chin.maxval, chin.q, chin.hshift,chin.vshift-1,chin.hcshift,chin.vcshift-1);
    chout.component = chin.component;
    v_printf(4,"Undoing vertical squeeze of channel %i using residuals in channel %i (going from height %i to %i)\n",c,rc,chin.h,chout.h);

      for (int x=0; x<chin.w; x++) {
        pixel_type diff_minus_tendency = chin_residual.value(0,x);
        pixel_type avg = chin.value_nocheck(0,x);

        pixel_type next_avg = avg;
        if (1<chin.h) next_avg = chin.value_nocheck(1,x);

        pixel_type tendency=smooth_tendency(avg,avg,next_avg);

        pixel_type diff = diff_minus_tendency + tendency;

        pixel_type A = ((avg<<1)+diff+(diff>0?-(diff&1):(diff&1)))>>1;
        chout.value(0,x) = A;
        pixel_type B = A-diff;
        chout.value(1,x) = B;
      }

    for (int y=1; y<chin_residual.h; y++) {
      for (int x=0; x<chin.w; x++) {
        pixel_type diff_minus_tendency = chin_residual.value(y,x);
        pixel_type avg = chin.value_nocheck(y,x);

        pixel_type next_avg = avg;
        if (y+1<chin.h) next_avg = chin.value_nocheck(y+1,x);

//        pixel_type top = (y>0 ? chout.value_nocheck((y<<1)-1,x) : avg);
        pixel_type top = chout.value_nocheck((y<<1)-1,x);
        pixel_type tendency=smooth_tendency(top,avg,next_avg);

        pixel_type diff = diff_minus_tendency + tendency;

        pixel_type A = ((avg<<1)+diff+(diff>0?-(diff&1):(diff&1)))>>1;
        chout.value(y<<1,x) = A;
        pixel_type B = A-diff;
        chout.value((y<<1)+1,x) = B;
      }
    }
    if (chout.h & 1) { int y = chin.h-1;
      for (int x=0; x<chin.w; x++) {
        pixel_type avg = chin.value(y,x);
        chout.value(y<<1,x) = avg;
      }
    }
    input.channel[c] = chout;
}


void fwd_vsqueeze(Image &input, int c, int rc) {
    const Channel &chin = input.channel[c];

    v_printf(4,"Doing vertical squeeze of channel %i to new channel %i\n",c,rc);

    Channel chout(chin.w,(chin.h+1)/2, chin.minval, chin.maxval, chin.q, chin.hshift,chin.vshift+1,chin.hcshift,chin.vcshift+1);
    Channel chout_residual(chin.w,chin.h-chout.h,chout.minval-chout.maxval,chout.maxval-chout.minval,1, chin.hshift,chin.vshift+1,chin.hcshift,chin.vcshift);
    chout.component = chin.component;
    chout_residual.component = chin.component;
    for (int y=0; y<chout_residual.h; y++) {
      for (int x=0; x<chout.w; x++) {
        pixel_type A = chin.value(y*2,x);
        pixel_type B = chin.value(y*2+1,x);
        pixel_type avg = (A+B+(A>B))>>1;
        chout.value(y,x) = avg;
        pixel_type diff = A-B;

        pixel_type next_avg = avg;
        if (y+1<chout_residual.h) next_avg = (chin.value(y*2+2,x) + chin.value(y*2+3,x) + (chin.value(y*2+2,x) > chin.value(y*2+3,x))) >> 1; // which will be chout.value(y+1,x)
        else if (chin.h & 1) next_avg = (chin.value(y*2+2,x));

        pixel_type top = (y>0 ? chin.value(y*2-1,x) : avg);
        pixel_type tendency=smooth_tendency(top,avg,next_avg);

        chout_residual.value(y,x) = diff - tendency;
      }
    }
    if (chin.h & 1) { int y = chout.h-1;
      for (int x=0; x<chout.w; x++) {
        pixel_type avg = chin.value(y*2,x);
        chout.value(y,x) = avg;
      }
    }
    input.channel[c] = chout;
//    chout_residual.actual_minmax(&chout_residual.minval, &chout_residual.maxval);
    input.channel.insert(input.channel.begin()+rc, chout_residual);
}


void default_squeeze_parameters(std::vector<int> &parameters, const Image &image) {
    int nb_channels = image.nb_channels;
    // maybe other transforms have been applied before, but let's assume the first nb_channels channels
    // still contain the 'main' data

    parameters.clear();
    int w = image.channel[image.nb_meta_channels].w;
    int h = image.channel[image.nb_meta_channels].h;
    v_printf(7,"\nDefault squeeze parameters for %ix%i image: ",w,h);

    bool wide = (w>h);  // do horizontal first on wide images; vertical first on tall images
    if (nb_channels>2 && image.channel[image.nb_meta_channels+1].w == w && image.channel[image.nb_meta_channels+1].h == h) {
        // assume channels 1 and 2 are chroma, and can be squeezed first for 4:2:0 previews
        v_printf(7,"(4:2:0 chroma), ",w,h);
//        if (!wide) {
//        parameters.push_back(0+2); // vertical chroma squeeze
//        parameters.push_back(image.nb_meta_channels+1);
//        parameters.push_back(image.nb_meta_channels+2);
//        }
        parameters.push_back(1+2); // horizontal chroma squeeze
        parameters.push_back(image.nb_meta_channels+1);
        parameters.push_back(image.nb_meta_channels+2);
//        if (wide) {
        parameters.push_back(0+2); // vertical chroma squeeze
        parameters.push_back(image.nb_meta_channels+1);
        parameters.push_back(image.nb_meta_channels+2);
//        }
    }

    if (!wide) {
            if (h > MAX_FIRST_PREVIEW_SIZE) {
                parameters.push_back(0); // vertical squeeze
                parameters.push_back(image.nb_meta_channels);
                parameters.push_back(image.nb_meta_channels+nb_channels-1);
                h = (h+1)/2;
                v_printf(7,"Vertical (%ix%i), ",w,h);
            }
    }
    while ( w > MAX_FIRST_PREVIEW_SIZE || h > MAX_FIRST_PREVIEW_SIZE ) {
            if (w > MAX_FIRST_PREVIEW_SIZE) {
                parameters.push_back(1); // horizontal squeeze
                parameters.push_back(image.nb_meta_channels);
                parameters.push_back(image.nb_meta_channels+nb_channels-1);
                w = (w+1)/2;
                v_printf(7,"Horizontal (%ix%i), ",w,h);
            }
            if (h > MAX_FIRST_PREVIEW_SIZE) {
                parameters.push_back(0); // vertical squeeze
                parameters.push_back(image.nb_meta_channels);
                parameters.push_back(image.nb_meta_channels+nb_channels-1);
                h = (h+1)/2;
                v_printf(7,"Vertical (%ix%i), ",w,h);
            }
    }
    v_printf(7,"that's it\n");
}

void meta_squeeze(Image &image, std::vector<int> &parameters) {
    if (!parameters.size()) default_squeeze_parameters(parameters,image);

    for (int i=0; i+2<parameters.size(); i+=3) {
        bool horizontal = parameters[i] & 1; // 0=vertical, 1=horizontal
        bool in_place = !(parameters[i] & 2);
        int beginc = parameters[i+1];
        int endc = parameters[i+2];
        int offset;
        if (in_place) offset = endc+1; else offset = image.nb_meta_channels + image.nb_channels;
        int nb_chans = endc-beginc+1;
        for (int c=beginc; c<=endc; c++) {
            Channel dummy;
            dummy.hcshift = image.channel[c].hcshift;
            dummy.vcshift = image.channel[c].vcshift;
            dummy.component = image.channel[c].component;
            if (horizontal) {
                int w = image.channel[c].w;
                image.channel[c].w = (w+1)/2;
                image.channel[c].hshift++;
                image.channel[c].hcshift++;
                dummy.w = w-(w+1)/2;
                dummy.h = image.channel[c].h;
            } else {
                int h = image.channel[c].h;
                image.channel[c].h = (h+1)/2;
                image.channel[c].vshift++;
                image.channel[c].vcshift++;
                dummy.h = h-(h+1)/2;
                dummy.w = image.channel[c].w;
            }
            dummy.hshift = image.channel[c].hshift;
            dummy.vshift = image.channel[c].vshift;

            image.channel.insert(image.channel.begin()+offset+c-beginc, dummy);
        }
    }
}

// [squeezetype] [beginc] [endc]
bool squeeze(Image &input, bool inverse, std::vector<int> &parameters) {
    std::vector<int> adj_params = parameters; // use a copy so empty (default) parameters remain empty
    if (!adj_params.size()) default_squeeze_parameters(adj_params,input);

    if (inverse) {
      for (int i=adj_params.size()-3; i>=0; i-=3) {
        bool horizontal = adj_params[i]&1; // 0=vertical, 1=horizontal
        bool in_place = !(adj_params[i] & 2);
        int beginc = adj_params[i+1];
        int endc = adj_params[i+2];
        if (endc >= input.channel.size()) {
            e_printf("Invalid parameters for squeeze transform: channel %i does not exist\n",endc);
        }
        int offset;
        if (in_place) offset = endc+1; else offset = input.nb_meta_channels + input.nb_channels;
        for (int c=beginc; c<=endc; c++) {
            if (input.channel[offset+c-beginc].data.size() == 0) {
                // stop unsqueezing luma; keep unsqueezing chroma channels
//                if (input.channel[beginc].w == input.channel[c].w && input.channel[beginc].h == input.channel[c].h) continue;
                input.channel[offset+c-beginc].resize();
            }
            if (horizontal) inv_hsqueeze(input, c, offset+c-beginc);
            else inv_vsqueeze(input, c, offset+c-beginc);
        }
        input.channel.erase(input.channel.begin()+offset,input.channel.begin()+offset+(endc-beginc+1));
      }
    } else {
      for (int i=0; i+2<adj_params.size(); i+=3) {
        bool horizontal = adj_params[i]&1; // 0=vertical, 1=horizontal
        bool in_place = !(adj_params[i] & 2);
        int beginc = adj_params[i+1];
        int endc = adj_params[i+2];
        if (endc >= input.channel.size()) {
            e_printf("Invalid parameters for squeeze transform: channel %i does not exist\n",endc);
        }
        int offset;
        if (in_place) offset = endc+1; else offset = input.nb_meta_channels + input.nb_channels;
        int nb_chans = endc-beginc+1;
        for (int c=beginc; c<=endc; c++) {
            if (horizontal) fwd_hsqueeze(input, c, offset+c-beginc);
            else fwd_vsqueeze(input, c, offset+c-beginc);
        }
      }
    }
    return true;
}

