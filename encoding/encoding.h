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

#include "../config.h"
#include "../image/image.h"
#include "../maniac/compound.h"
#include "../fileio.h"

struct fuif_options {
// decoding options
    int preview;                // -1 : all, 0 : LQIP, 1: 1/16, 2: 1/8, 3: 1/4, 4: 1/2
    bool identify;              // don't decode image data, just decode header
// encoding options (some of which are needed during decoding too)
    float nb_repeats;            // number of iterations to do to learn a MANIAC tree (does not have to be an integer)
    int max_dist;                // maximum distance to look for matches
    int max_properties;          // maximum number of (previous channel) properties to use in the MANIAC trees
    int maniac_cutoff;  // TODO: put this in the bitstream
    int maniac_alpha;   // TODO: put this in the bitstream
    bool compress;
    int max_group;
    bool debug;
    std::vector<int> predictor;
    Image heatmap;
};
const struct fuif_options default_fuif_options {
    .preview = -1,
    .identify = false,
    .nb_repeats = 0.5,
    .max_dist = 0,
    .max_properties = 12,
    .maniac_cutoff = 6,
    .maniac_alpha = 0x0d000000,
    .compress = true,
    .max_group = -1,
    .debug = false,
};

void fuif_prepare_encode(Image &image, fuif_options &options);

template <typename IO>
bool fuif_encode(IO& io, const Image &image, fuif_options &options);

bool fuif_encode_file(const char * filename, const Image &image, fuif_options &options);

template <typename IO>
bool fuif_decode(IO& io, Image &image, fuif_options options=default_fuif_options);

bool fuif_decode_file(const char * filename, Image &image, fuif_options options=default_fuif_options);
