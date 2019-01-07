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

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <assert.h>

#include "../util.h"
#include <stdio.h>

typedef int16_t pixel_type; // enough for up to 14-bit with YCoCg/Squeeze (I think); enough for up to 10-bit if DCT is added (I think)
#define LARGEST_VAL 0x7FFF
#define SMALLEST_VAL 0x8001

//typedef int32_t pixel_type;  // can use int16_t if it's only for 8-bit images. Need some wiggle room for YCoCg / DCT / Squeeze etc

// largest possible pixel value (2147483647)
//#define LARGEST_VAL 0x7FFFFFFF

// smallest possible pixel value (-2147483647)
//#define SMALLEST_VAL 0x80000001



#define RESPONSIVE_SIZE(n)  ((32 >> n))


// TODO: add two pixels of padding on the left and top, and one pixel on the right and bottom,
//       and get rid of all edge cases in the context properties / predictors / etc
class Channel {
public:
    std::vector<pixel_type> data;
    int w, h;
    pixel_type minval, maxval;  // range
    mutable pixel_type zero;    // should be zero if zero is a valid value for this channel; the valid value closest to zero otherwise
    int q;                      // quantization factor
    int hshift, vshift;         // w ~= image.w >> hshift;  h ~= image.h >> vshift
    int hcshift, vcshift;       // cumulative, i.e. when decoding up to this point, we have data available with these shifts (for this component)
    int component;              // this channel contains (some) data of this component  (-1 = bug/unknown)
    Channel(int iw, int ih, pixel_type iminval, pixel_type imaxval, int qf=1, int hsh=0, int vsh=0, int hcsh=0, int vcsh=0) :
            w(iw), h(ih), minval(iminval), maxval(imaxval), data(iw*ih,0), q(qf), hshift(hsh), vshift(vsh), hcshift(hcsh), vcshift(vcsh), component(-1) { setzero(); }
    Channel() : w(0), h(0), minval(0), maxval(0), zero(0), q(1), hshift(0), vshift(0), hcshift(0), vcshift(0), component(-1) { }

    void setzero() const {
        if (minval > 0) zero = minval;
        else if (maxval < 0) zero = maxval;
        else zero = 0;
    }
    void resize() {
        data.resize(w*h,zero);
    }
    void resize(int nw, int nh) {
        w=nw; h=nh;
        resize();
    }

    pixel_type value_nocheck(int r, int c) const { return data[r*w+c]; }
    pixel_type value(int r, int c) const { if (r*w+c >= data.size()) return zero;
        assert(r*w+c >=0); assert(r*w+c < data.size()); return data[r*w+c]; }
    pixel_type &value(int r, int c) { if (r*w+c >= data.size()) return zero;
        assert(r*w+c >=0); assert(r*w+c < data.size()); return data[r*w+c]; }
    pixel_type repeating_edge_value(int r, int c) const { return value( (r<0?0:(r>=h?h-1:r)) , (c<0?0:(c>=w?w-1:c)) ); }
    pixel_type value(size_t i) const { assert(i < data.size()); return data[i]; }
    pixel_type &value(size_t i) { assert(i < data.size()); return data[i]; }

    void actual_minmax(pixel_type *min, pixel_type *max) const;
};

class Transform;

const char * colormodel_name(int colormodel, int nb_channels);
const char * colorprofile_name(int colormodel);

class Image {
public:
    std::vector<Channel> channel; // image data, transforms can dramatically change the number of channels and their semantics
    std::vector<Transform> transform;  // keeps track of the transforms that have been applied (and that have to be undone when rendering the image)

    int w,h; // actual dimensions of the image (channels may have different dimensions due to transforms like chroma subsampling and DCT)
    int nb_frames; // number of frames; multi-frame images are stored as a vertical filmstrip
    int den;  // frames per second (denominator of frame duration in seconds; default numerator is 1)
    std::vector<int> num; // numerator of frame duration
    int loops; // 0=forever
    int minval,maxval; // actual (largest) range of the channels (actual ranges might be different due to transforms; after undoing transforms, might still be different due to lossy)
    int nb_channels; // actual number of distinct channels (after undoing all transforms except Palette; can be different from channel.size())
    int real_nb_channels; // real number of channels (after undoing all transforms)
    int nb_meta_channels; // first few channels might contain things like palettes or compaction data that are not yet real image data
    int colormodel; // indicates one of the standard color models/spaces/profiles, or a custom one (in which case ICC profile should be provided)
    std::vector<unsigned char> icc_profile; // icc profile blob
    int downscales[6]; //   LQIP, 1:16, 1:8, 1:4, 1:2, 1:1
    bool error; // true if a fatal error occurred, false otherwise

    Image(int iw, int ih, int maxval, int nb_chans, int cm=0) :
        channel(nb_chans,Channel(iw,ih,0,maxval)),
        w(iw), h(ih), nb_frames(1), den(10), loops(0), minval(0), maxval(maxval), nb_channels(nb_chans), real_nb_channels(nb_chans), nb_meta_channels(0), colormodel(cm), error(false) {
        for (int i=0; i<nb_chans; i++) channel[i].component=i;
        for (int i=0; i<6; i++) downscales[i] = nb_chans-1;
    }

    Image() : w(0), h(0), nb_frames(1), den(10), loops(0), minval(0), maxval(255), nb_channels(0), real_nb_channels(0), nb_meta_channels(0), colormodel(0), error(true) { }
    bool do_transform(const Transform &t);
    void undo_transforms(int keep=0); // undo all except the first 'keep' transforms
    void recompute_minmax() { for (int i=0; i<channel.size(); i++) channel[i].actual_minmax(&channel[i].minval, &channel[i].maxval); }
    void recompute_downscales();
};

#include "../transform/transform.h"