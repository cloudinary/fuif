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

// JPEG-style RGB to YCbCr color transformation
#define TRANSFORM_YCbCr 0

// Lossless YCoCg color transformation
#define TRANSFORM_YCoCg 1


// XYB color transformation from PIK
#define TRANSFORM_XYB 11

// Rec. 2100 ICtCp color transformation
// (TO BE IMPLEMENTED)
#define TRANSFORM_ICtCp 2

// JPEG-style (chroma) subsampling. Parameters are: [nb_subsampled_channels], [channel], [sample_ratio_h], [sample_ratio_v], ...
// e.g. 2, 1, 2, 2, 2, 2, 2 corresponds to 4:2:0
#define TRANSFORM_ChromaSubsample 3

// JPEG-style 8x8 DCT
#define TRANSFORM_DCT 4
// some of the internals are needed to do JPEG import (and export, TODO)
extern const int jpeg_zigzag[64];
extern const int dct_cshifts[64];
void default_DCT_scanscript(int nb_components, std::vector<std::vector<int>> & ordering, std::vector<int> & comp, std::vector<int> & coeff);

// JPEG-style quantization. Parameters are quantization factors for each channel (encoded as part of channel metadata).
#define TRANSFORM_QUANTIZE 5

// Color palette. Parameters are: [begin_c] [end_c] [max colors]
#define TRANSFORM_PALETTE 6

// Squeezing (Haar-style)
#define TRANSFORM_SQUEEZE 7

// 2D Matching
#define TRANSFORM_2DMATCH 8

// Channel permutation. Has to be the last transform in the chain if used without parameters.
#define TRANSFORM_PERMUTE 9

// Approximation, i.e. lossless quantization (remainders are put in new channel)
#define TRANSFORM_APPROXIMATE 10

#include <string>


extern const std::vector<std::string> transform_name;

class Transform {
public:
    const int ID;
    std::vector<int> parameters;

    Transform(int id) : ID(id) {}
    bool apply(Image &input, bool inverse);
    void meta_apply(Image &input);
    bool has_parameters() const {
        switch(ID) {
            case TRANSFORM_ChromaSubsample:
            case TRANSFORM_PALETTE: // stores palette itself as meta-channel, but parameter says which channel(s)
            case TRANSFORM_SQUEEZE:
            case TRANSFORM_DCT:
            case TRANSFORM_2DMATCH:
            case TRANSFORM_PERMUTE:  // stores permutation as meta-channel, or in parameters
            case TRANSFORM_APPROXIMATE:
                return true;
            case TRANSFORM_YCbCr: //
            case TRANSFORM_YCoCg: // color transforms always work on channels 0-2
            case TRANSFORM_ICtCp: //
            case TRANSFORM_XYB:   //
            default:
                return false;
        }
    }
    const char * name() const {
        return transform_name[ID].c_str();
    }
};
