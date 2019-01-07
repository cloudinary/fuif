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


bool write_YUV_file(const char *output_image, const Image &image) {

  int nb_channels = image.channel.size();
  if (nb_channels != 3 || image.transform.size() != 2 || image.transform[0].ID != TRANSFORM_YCbCr || image.transform[1].ID != TRANSFORM_ChromaSubsample
      || image.channel[1].hshift != 1 || image.channel[2].hshift != 1) {
    e_printf("Image is not YCbCr 4:2:0, not saving as .yuv.\n");
    return false;
  }

  FILE *fp = fopen(output_image,"wb");
  if (!fp) {
    return false;
  }
  if (image.colormodel != 0) {
    v_printf(1,"Warning: saving as .yuv, even though the image is not sRGB.\n");
  }

  for (int c = 0; c < 3; c++) {
    for (int y = 0; y < image.channel[c].h; y++) {
     for (int x = 0; x < image.channel[c].w; x++) {
        fputc(CLAMP(image.channel[c].value(y,x),image.minval,image.maxval) & 0xFF,fp);
        if (image.maxval > 255)
            fputc((CLAMP(image.channel[c].value(y,x),image.minval,image.maxval)>>8) & 0xFF,fp);
     }
    }
  }

  fclose(fp);
  return 0;
}
