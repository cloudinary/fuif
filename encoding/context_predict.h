/*//////////////////////////////////////////////////////////////////////////////////////////////////////

FUIF -  FREE UNIVERSAL IMAGE FORMAT
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

#include "../maniac/util.h"


// should only be called with positive values
// kind of like the ilog2, just a bit more resolution at the low values
// ilog2 0,1 = 0
// ilog2 2,3 = 1
// ilog2 4,5,6,7 = 2
// ilog2 8-15 = 3
// ilog2 16-31 = 4
// ilog2 32-63 = 5

inline pixel_type foolog(pixel_type x) ATTRIBUTE_HOT;
inline pixel_type foolog(pixel_type x) {
    return maniac::util::ilog2(x<<1);
/*
    if (x > 31) return maniac::util::ilog2(x) + 4;  // ilog2(32) == 5
    static const pixel_type foolog_table[32] =
                             { 0, 1, 2, 3, 3, 4, 4, 4,
                               5, 5, 5, 5, 6, 6, 6, 6,
                               6, 7, 7, 7, 7, 7, 7, 7,
                               8, 8, 8, 8, 8, 8, 8, 8 };
    return foolog_table[x];
*/
}

// signed log-like thing
inline pixel_type slog(pixel_type x) ATTRIBUTE_HOT;
inline pixel_type slog(pixel_type x) {
//    pixel_type a = foolog(abs(x));
//    if (x >= 0) return a; else return -a;
//    printf("value %i, foolog %i\n",x,foolog(abs(x)));
    if (x == 0) return 0;
    else if (x > 0) return (sizeof(unsigned int) * 8 - __builtin_clz(x));
    else return -(sizeof(unsigned int) * 8 - __builtin_clz(-x));
}
// absolute value (wrapped into a function to make it easier to experiment with other functions)
pixel_type fooabs(pixel_type x) {
    return abs(x);
}

void init_properties(Ranges &pr, const Image &image, int beginc, int endc, fuif_options &options) {
    int offset=0;
//    for (int j=beginc-1; j>=image.nb_meta_channels && offset < options.max_properties; j--) {
    for (int j=beginc-1; j>=0 && offset < options.max_properties; j--) {
        if (image.channel[j].minval == image.channel[j].maxval) continue;
        if (image.channel[j].hshift < 0) continue;
        int minval = image.channel[j].minval;
        if (minval > 0) minval = 0;
        int maxval = image.channel[j].maxval;
        if (maxval < 0) maxval = 0;
        pr.push_back(std::pair<PropertyVal,PropertyVal>(0,fooabs((maxval > -minval ? maxval : minval))));
        offset++;
        pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval),slog(maxval)));
        offset++;
//        pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval-maxval),slog(maxval-minval)));
//        offset++;
    }

    pixel_type minval = LARGEST_VAL;
    pixel_type maxval = SMALLEST_VAL;
    int maxh = 0;
    int maxw = 0;
    for (int j=beginc; j<=endc; j++) {
        if (image.channel[j].minval < minval) minval = image.channel[j].minval;
        if (image.channel[j].maxval > maxval) maxval = image.channel[j].maxval;
        if (image.channel[j].h > maxh) maxh = image.channel[j].h;
        if (image.channel[j].w > maxw) maxw = image.channel[j].w;
    }
    if (minval > 0) minval = 0;
    if (maxval < 0) maxval = 0;

//    printf("range = %i..%i\n",minval,maxval);
    // neighbors
    pr.push_back(std::pair<PropertyVal,PropertyVal>(0,MAX(fooabs(minval),fooabs(maxval))));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(0,MAX(fooabs(minval),fooabs(maxval))));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval),slog(maxval)));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval),slog(maxval)));

    // location
    pr.push_back(std::pair<PropertyVal,PropertyVal>(0,maxh-1));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(0,maxw-1));

    // blah
    pr.push_back(std::pair<PropertyVal,PropertyVal>(minval+minval-maxval,maxval+maxval-minval));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(minval+minval-maxval,maxval+maxval-minval));

    // FFV1
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval-maxval),slog(maxval-minval)));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval-maxval),slog(maxval-minval)));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval-maxval),slog(maxval-minval)));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval-maxval),slog(maxval-minval)));
    pr.push_back(std::pair<PropertyVal,PropertyVal>(slog(minval-maxval),slog(maxval-minval)));

}



pixel_type predict_and_compute_properties(Properties &p, const Channel &ch, int x, int y, int predictor, int offset=0) ATTRIBUTE_HOT;
pixel_type predict_and_compute_properties(Properties &p, const Channel &ch, int x, int y, int predictor, int offset) {
    pixel_type left = (x ? ch.value_nocheck(y,x-1) : ch.zero);
    pixel_type top = (y ? ch.value_nocheck(y-1,x) : ch.zero);
    pixel_type topleft = (x && y ? ch.value_nocheck(y-1,x-1) : left);
    pixel_type topright = (x+1<ch.w && y ? ch.value_nocheck(y-1,x+1) : top);

//    if (p.size()) {
      pixel_type leftleft = (x>1 ? ch.value_nocheck(y,x-2) : left);
      pixel_type toptop = (y>1 ? ch.value_nocheck(y-2,x) : top);

      // neighbors
      p[offset++] = fooabs(top);
      p[offset++] = fooabs(left);
      p[offset++] = slog(top);
      p[offset++] = slog(left);

      // location
      p[offset++] = y;
      p[offset++] = x;

      // local gradients
      p[offset++] = left+top-topleft;
      p[offset++] = topleft+topright-top;

      // FFV1 context properties
      p[offset++] = slog(left-topleft);
      p[offset++] = slog(topleft-top);
      p[offset++] = slog(top-topright);
      p[offset++] = slog(top-toptop);
      p[offset++] = slog(left-leftleft);
//    }

    switch (predictor) {
        case 0: return ch.zero;
        case 1: return (left+top)/2;
        case 2: return median3((pixel_type) (left+top-topleft), left, top);
        case 3: return left;
        case 4: return top;
        case 5: return (left+topleft+top+topright)/4;
        case 6: return CLAMP(left+top-topleft, ch.minval, ch.maxval);
        default: return median3((pixel_type) (left+top-topleft), left, top);
    }

}

pixel_type predict_and_compute_properties_no_edge_case(Properties &p, const Channel &ch, int x, int y, int offset=0) ATTRIBUTE_HOT;
pixel_type predict_and_compute_properties_no_edge_case(Properties &p, const Channel &ch, int x, int y, int offset) {
    assert(x>1);
    assert(y>1);
    assert(x+1<ch.w);

    pixel_type left = ch.value_nocheck(y,x-1);
    pixel_type top = ch.value_nocheck(y-1,x);
    pixel_type topleft = ch.value_nocheck(y-1,x-1);
    pixel_type topright = ch.value_nocheck(y-1,x+1);

      pixel_type leftleft = ch.value_nocheck(y,x-2);
      pixel_type toptop = ch.value_nocheck(y-2,x);

      // neighbors
      p[offset++] = fooabs(top);
      p[offset++] = fooabs(left);
      p[offset++] = slog(top);
      p[offset++] = slog(left);

      // location
      p[offset++] = y;
      p[offset++] = x;

      // local gradients
      p[offset++] = left+top-topleft;
      p[offset++] = topleft+topright-top;

      // FFV1 context properties
      p[offset++] = slog(left-topleft);
      p[offset++] = slog(topleft-top);
      p[offset++] = slog(top-topright);
      p[offset++] = slog(top-toptop);
      p[offset++] = slog(left-leftleft);

    return ch.zero;
}


#define NB_PREDICTORS 7
#define NB_NONREF_PROPERTIES 13
/*
pixel_type predict_and_compute_properties_with_reference(Properties &p, const Channel &ch, int x, int y, int predictor, const Image &image, int i, fuif_options &options) ATTRIBUTE_HOT;
pixel_type predict_and_compute_properties_with_reference(Properties &p, const Channel &ch, int x, int y, int predictor, const Image &image, int i, fuif_options &options) {
    int offset=0;
    int ox = x << ch.hshift;
    int oy = y << ch.vshift;

    for (int j=i-1; j>=image.nb_meta_channels && offset < options.max_properties; j--) {
        if (image.channel[j].minval == image.channel[j].maxval) continue;
        int rx = ox >> image.channel[j].hshift;
        int ry = oy >> image.channel[j].vshift;
        if (rx >= image.channel[j].w) rx = image.channel[j].w-1;
        if (ry >= image.channel[j].h) ry = image.channel[j].h-1;
        pixel_type v = image.channel[j].value_nocheck(ry,rx);
        p[offset++] = fooabs(v);
        p[offset++] = slog(v);
//        p[offset++] = slog(image.channel[j].value(ry,rx) - image.channel[j].value(ry,(rx?rx-1:1)));
    }
    return predict_and_compute_properties(p,ch,x,y,predictor,offset);
}
*/

pixel_type precompute_references(const Channel &ch, int y, const Image &image, int i, fuif_options &options, Channel &references) {
    int offset=0;
    int oy = y << ch.vshift;
//    for (int j=i-1; j>=image.nb_meta_channels && offset < options.max_properties; j--) {
    for (int j=i-1; j>=0 && offset < options.max_properties; j--) {
        if (image.channel[j].minval == image.channel[j].maxval) continue;
        if (image.channel[j].hshift < 0) continue;
        int ry = oy >> image.channel[j].vshift;
        if (ry >= image.channel[j].h) ry = image.channel[j].h-1;
        if (ch.hshift == image.channel[j].hshift && ch.w <= image.channel[j].w)
        for (int x=0; x<ch.w; x++) {
            pixel_type v = image.channel[j].value_nocheck(ry,x);
            references.value(x,offset) = fooabs(v);
            references.value(x,offset+1) = slog(v);
        }
        else if (ch.hshift < image.channel[j].hshift) {
          int stepsize = (1 << image.channel[j].hshift) >> ch.hshift;
          int x=0, rx=0;
          pixel_type v;
          if (stepsize == 2)
            for (; rx<image.channel[j].w-1; rx++) {
              v = image.channel[j].value_nocheck(ry,rx);
              references.value(x,offset) = fooabs(v);
              references.value(x,offset+1) = slog(v);
              x++;
              references.value(x,offset) = fooabs(v);
              references.value(x,offset+1) = slog(v);
              x++;
            }
          else
            for (; rx<image.channel[j].w-1; rx++) {
              v = image.channel[j].value_nocheck(ry,rx);
              for (int s=0; s<stepsize; s++, x++) {
                references.value(x,offset) = fooabs(v);
                references.value(x,offset+1) = slog(v);
              }
            }
          assert (x-1 < ch.w);
          v = image.channel[j].value_nocheck(ry,rx);
          while (x<ch.w) {
              references.value(x,offset) = fooabs(v);
              references.value(x,offset+1) = slog(v);
              x++;
          }
        } else
        for (int x=0; x<ch.w; x++) {
            int ox = x << ch.hshift;
            int rx = ox >> image.channel[j].hshift;
            if (rx >= image.channel[j].w) rx = image.channel[j].w-1;
            pixel_type v = image.channel[j].value_nocheck(ry,rx);
            references.value(x,offset) = fooabs(v);
            references.value(x,offset+1) = slog(v);
        }

        offset += 2;
    }
}


inline pixel_type predict_and_compute_properties_with_precomputed_reference(Properties &p, const Channel &ch, int x, int y, int predictor, const Image &image, int i, fuif_options &options, const Channel &references) ATTRIBUTE_HOT;
inline pixel_type predict_and_compute_properties_with_precomputed_reference(Properties &p, const Channel &ch, int x, int y, int predictor, const Image &image, int i, fuif_options &options, const Channel &references) {
    int offset;
    for (offset=0; offset<references.w; ) {
        p[offset] = references.value_nocheck(x,offset); offset++;
        p[offset] = references.value_nocheck(x,offset); offset++;
    }
    return predict_and_compute_properties(p,ch,x,y,predictor,offset);
}

inline pixel_type predict_and_compute_properties_with_precomputed_reference_no_edge_case(Properties &p, const Channel &ch, int x, int y, const Channel &references) ATTRIBUTE_HOT;
inline pixel_type predict_and_compute_properties_with_precomputed_reference_no_edge_case(Properties &p, const Channel &ch, int x, int y, const Channel &references) {
    int offset;
    for (offset=0; offset<references.w; ) {
        p[offset] = references.value_nocheck(x,offset); offset++;
        p[offset] = references.value_nocheck(x,offset); offset++;
    }
    return predict_and_compute_properties_no_edge_case(p,ch,x,y,offset);
}
