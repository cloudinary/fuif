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

#include "../image/image.h"
#include "transform.h"

// transforms needed to represent legacy JPEG
#include "ycbcr.h"
#include "subsample.h"
#include "dct.h"
#include "quantize.h"

// new transforms
#include "ycocg.h"
#include "palette.h"
#include "squeeze.h"
#include "2dmatch.h"
#include "permute.h"
#include "approximate.h"

//#include "xyb.h"


const std::vector<std::string> transform_name = {"YCbCr", "YCoCg", "ICtCp [TODO]", "ChromaSubsampling", "DCT", "Quantization", "Palette", "Squeeze", "Matching", "Permutation", "Approximation", "XYB"};


bool Transform::apply(Image &input, bool inverse) {
    switch(ID) {
        case TRANSFORM_YCbCr: return YCbCr(input, inverse);
        case TRANSFORM_ChromaSubsample: return subsample(input, inverse, parameters);
        case TRANSFORM_DCT: return DCT(input, inverse, parameters);
        case TRANSFORM_QUANTIZE: return quantize(input, inverse, parameters);
        case TRANSFORM_YCoCg: return YCoCg(input, inverse);
//        case TRANSFORM_XYB: return XYB(input, inverse);
        case TRANSFORM_SQUEEZE: return squeeze(input, inverse, parameters);
        case TRANSFORM_PALETTE: return palette(input, inverse, parameters);
        case TRANSFORM_2DMATCH: return match(input, inverse, parameters);
        case TRANSFORM_PERMUTE: return permute(input, inverse, parameters);
        case TRANSFORM_APPROXIMATE: return approximate(input, inverse, parameters);
        default: e_printf("Unknown transformation (ID=%i)\n",ID); return false;
    }
}


void Transform::meta_apply(Image &input) {
    switch(ID) {
        case TRANSFORM_YCbCr: return;
        case TRANSFORM_YCoCg: return;
//        case TRANSFORM_XYB: return;
        case TRANSFORM_ChromaSubsample: meta_subsample(input, parameters); return;
        case TRANSFORM_DCT: meta_DCT(input, parameters); return;
        case TRANSFORM_QUANTIZE: return;
        case TRANSFORM_SQUEEZE: meta_squeeze(input, parameters); return;
        case TRANSFORM_PALETTE: meta_palette(input, parameters); return;
        case TRANSFORM_2DMATCH: meta_match(input, parameters); return;
        case TRANSFORM_PERMUTE: meta_permute(input, parameters); return;
        case TRANSFORM_APPROXIMATE: meta_approximate(input, parameters); return;
        default: e_printf("Unknown transformation (ID=%i)\n",ID); return;
    }
}
