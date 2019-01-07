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

// JPEG-style (chroma) subsampling. Parameters are: [begin_channel], [end_channel], [sample_ratio_h], [sample_ratio_v], ...
// e.g. 1, 2, 2 corresponds to 4:2:0

void check_subsample_parameters(std::vector<int> &parameters) {
    if (parameters.size() == 1) {
        // special case: abbreviated parameters for some common cases
        switch (parameters[0]) {
            case 0:     // 4:2:0
                parameters[0] = 1;
                parameters.push_back(2);
                parameters.push_back(2);
                parameters.push_back(2);
                break;
            case 1:     // 4:2:2
                parameters[0] = 1;
                parameters.push_back(2);
                parameters.push_back(2);
                parameters.push_back(1);
                break;
            case 2:     // 4:4:0
                parameters[0] = 1;
                parameters.push_back(2);
                parameters.push_back(1);
                parameters.push_back(2);
                break;
            case 3:     // 4:1:1
                parameters[0] = 1;
                parameters.push_back(2);
                parameters.push_back(4);
                parameters.push_back(1);
                break;
            default:
                break;
        }
    }
    if (parameters.size() % 4) {
        e_printf("Error: invalid parameters for subsampling.\n");
        parameters.clear();
    }
}


// simple upscaling; TODO: fancy upscaling
bool inv_subsample(Image &input, std::vector<int> parameters) {
    check_subsample_parameters(parameters);

    for (int i=0; i<parameters.size(); i+=4) {
        int c1 = parameters[i+0];
        int c2 = parameters[i+1];
        int srh = parameters[i+2];
        int srv = parameters[i+3];
        for (int c=c1; c<=c2; c++) {
         int ow = input.channel[c].w;
         int oh = input.channel[c].h;
         if (ow >= input.channel[input.nb_meta_channels].w && oh >= input.channel[input.nb_meta_channels].h) {
            // this can happen in case of LQIP and 1:16 scale decodes
            v_printf(5,"Skipping upscaling of channel %i because it is already as large as channel %i.\n",c,input.nb_meta_channels);
            continue;
         }
         Channel channel(ow*srh,oh*srv,input.channel[c].minval,input.channel[c].maxval);
         if (srv <= 2 && srh <= 2) {
          // 'fancy' horizontal upscale
          if (srh == 2) {
           for (int y=0; y<oh; y++) {
            for (int x=0; x<ow; x++) {
             channel.value(y*srv,x*srh) =   (3*input.channel[c].value(y,x) + input.channel[c].value(y,(x?x-1:0)) + 1)>>2;
             channel.value(y*srv,x*srh+1) = (3*input.channel[c].value(y,x) + input.channel[c].value(y,(x+1<ow?x+1:x)) + 2)>>2;
            }
           }
          } else {
           for (int y=0; y<oh; y++) {
            for (int x=0; x<ow; x++) {
             channel.value(y*srv,x) = input.channel[c].value(y,x);
            }
           }
          }
          if (srv == 2) {
           Channel orig = channel;
           for (int y=0; y<oh; y++) {
            for (int x=0; x<ow*srh; x++) {
             channel.value(y*srv,x) =   (3*orig.value(y*srv,x) + orig.value((y?(y-1)*srv:0),x) + 1)>>2;
             channel.value(y*srv+1,x) = (3*orig.value(y*srv,x) + orig.value((y+1<oh?(y+1)*srv:y*srv),x) + 2)>>2;
            }
           }
          }
         } else {
         // simple box filter upscaling. fancier upscaling could be done too...
          for (int y=0; y<oh*srv; y++) {
           for (int x=0; x<ow*srh; x++) {
             channel.value(y,x) = input.channel[c].value(y/srv,x/srh);
           }
          }
         }
         v_printf(5,"Upscaled channel %i from %ix%i to %ix%i\n",c,input.channel[c].w,input.channel[c].h,channel.w,channel.h);
         input.channel[c] = channel;
        }
    }
    return true;
}

bool fwd_subsample(Image &input, const std::vector<int> &parameters) {
    return false; // TODO (not really needed though; subsampling is useful if the input data is a JPEG or YUV, but then the transform is already done)
                    // for non-subsampled input it's probably better to just stick to 4:4:4 (and quantize most of the chroma details away)
}

void meta_subsample(Image &input, std::vector<int> parameters) {
    check_subsample_parameters(parameters);
    if (parameters.size())
    for (int i=0; i<parameters.size(); i+=4) {
        int c1 = parameters[i+0];
        int c2 = parameters[i+1];
        int srh = parameters[i+2];
        int srv = parameters[i+3];
        assert (srh == 1 || srh == 2);
        assert (srv == 1 || srv == 2);
        int srhshift = (srh==1?0:1);
        int srvshift = (srv==1?0:1);
        for (int c=c1; c<=c2; c++) {
          input.channel[c].w += srh-1;
          input.channel[c].w /= srh;
          input.channel[c].h += srv-1;
          input.channel[c].h /= srv;
          input.channel[c].hshift += srhshift;
          input.channel[c].vshift += srvshift;
        }
    }
}


bool subsample(Image &input, bool inverse, const std::vector<int> &parameters) {
    if (inverse) return inv_subsample(input, parameters);
    else return fwd_subsample(input, parameters);
}

