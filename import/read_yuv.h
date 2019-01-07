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


Image read_YUV_file(const char *filename, int w, int h, int bitdepth) {
    FILE *fp = NULL;
    if (!strcmp(filename,"-")) fp = stdin;
    else fp = fopen(filename,"rb");

    v_printf(7,"Loading %s as a %ix%i %i-bit YUV file\n",filename,w,h,bitdepth);
    Image image(w, h, (1<<bitdepth) -1, 3);
    int shift = bitdepth-8;
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            image.channel[0].value(y,x) = fgetc(fp);
            if (bitdepth > 8) { image.channel[0].value(y,x) += (fgetc(fp)<<8); }
        }
    }

    for (int c=1; c<3; c++) {
      image.channel[c].w = (w+1)/2;
      image.channel[c].h = (h+1)/2;
      image.channel[c].hshift = 1;
      image.channel[c].vshift = 1;
      image.channel[c].resize();
      for (int y=0; y<(h+1)/2; y++) {
        for (int x=0; x<(w+1)/2; x++) {
            image.channel[c].value(y,x) = fgetc(fp);
            if (bitdepth > 8) { image.channel[c].value(y,x) += (fgetc(fp)<<8); }
        }
      }
    }
    image.transform.push_back(Transform(TRANSFORM_YCbCr));
    Transform subsampling(TRANSFORM_ChromaSubsample);
    subsampling.parameters.push_back(0); // 4:2:0
    image.transform.push_back(subsampling);
    if (fp != stdin) fclose(fp);
    return image;
}
