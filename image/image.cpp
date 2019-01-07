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

#include "image.h"
#include "../transform/transform.h"
#include "../io.h"

// colormodel: 6-bit number abcdefg
// a (64) : custom profile? (TODO: add some mechanism to include an ICC profile)
// bc (32/16) : 00 = RGB, 01 = CMYK, 10 = CIE, 11 = reserved
// defg : 4-bit code for some standard/common color profile

// TODO: add standard color profiles, write them in image export and detect them in image import

const char * colormodel_name(int colormodel, int nb_channels) {
    if (nb_channels == 1) return "Grayscale";
    else if (nb_channels == 2) return "Grayscale+alpha";

    switch (colormodel>>4) {
        case 7: return "Custom other";
        case 6: return "Custom CIE";
        case 5: return "Custom CMYK";
        case 4: return "Custom RGB";
        case 3: return "[RESERVED]";
        case 2:
            if (nb_channels == 3) {
                if (colormodel & 1) return "CIEXYZ";
                else return "CIELAB";
            } else if (nb_channels == 4) {
                if (colormodel & 1) return "CIEXYZ+alpha";
                else return "CIELAB+alpha";
            }
        case 1:
            if (nb_channels == 3) return "CMY";
            else if (nb_channels == 4) return "CMYK";
            else if (nb_channels == 5) return "CMYK+alpha";
            else return "CMYK+";

        case 0:
        default:
            if (nb_channels == 3) return "RGB";
            else if (nb_channels == 4) return "RGBA";
            else return "RGB+";
    }
}
const char * colorprofile_name(int colormodel) {
    if (colormodel>>4) {
        return ""; // TODO: define some common non-RGB profiles
    } else {
        // common RGB profiles
        switch (colormodel & 0xF) {
            case 1: return " (DCI-P3)";
            case 2: return " (Rec.2020)";
            case 3: return " (Adobe RGB 1998)";
            case 4: return " (ProPhoto)";
            case 0:
            default: return " (sRGB)";
        }
    }
}

void Channel::actual_minmax(pixel_type *min, pixel_type *max) const {
    pixel_type realmin=LARGEST_VAL;
    pixel_type realmax=SMALLEST_VAL;
    for (int i=0; i<data.size(); i++) {
        if (data[i] < realmin) realmin = data[i];
        if (data[i] > realmax) realmax = data[i];
    }
    *min = realmin;
    *max = realmax;
}

void Image::undo_transforms(int keep) {
    while ( transform.size() > keep ) {
            Transform t = transform.back();
            v_printf(4,"Undoing transform %s\n",t.name());
            bool result = t.apply(*this, true);
            if (result == false) {
                e_printf("Error while undoing transform %s.\n",t.name());
                error = true;
                return;
            }
            v_printf(8,"Undoing transform %s: done\n",t.name());
            transform.pop_back();
    }
    if (!keep) { // clamp the values to the valid range (lossy compression can produce values outside the range)
        for (int i=0; i<channel.size(); i++) {
            for (int j=0; j<channel[i].data.size(); j++) {
                channel[i].data[j] = CLAMP(channel[i].data[j], minval, maxval);
            }
        }
    }
//    recompute_minmax();
}

bool Image::do_transform(const Transform &tr) {
    Transform t = tr;
    bool did_it = t.apply(*this, false);
    if (did_it) transform.push_back(t);
    return did_it;
}

void Image::recompute_downscales() {
    downscales[0] = nb_meta_channels + nb_channels -1; // we have an LQIP when each component has had its first scan
    for (int s=1; s<6; s++) {
        downscales[s] = channel.size()-1;
        for (int k=downscales[s-1]; k<channel.size(); k++) {
            // has to be possible to decode downscaled image with a buffer the size of the final image, so once we need a larger buffer, we went too far
            if ((1<<channel[k].hcshift) < RESPONSIVE_SIZE(s) || (1<<channel[k].vcshift) < RESPONSIVE_SIZE(s)) break;

            // all channels should ideally be available in the required size (though chroma subsampling may cause this to be not true)
            if ((1<<channel[k].hcshift) == RESPONSIVE_SIZE(s) && (1<<channel[k].vcshift) == RESPONSIVE_SIZE(s)) downscales[s] = k;
        }
    }
}

