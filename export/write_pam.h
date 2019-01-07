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


bool write_PAM_file(const char *output_image, const Image &image) {

  int nb_channels = image.channel.size();
  bool debugstuff = false;

  if (nb_channels > ((image.colormodel & 48) == 16 ? 5 : 4)) {
    e_printf("Too many channels. Saving just the first 3 channels (instead of %i)\n",nb_channels);
    nb_channels = 3; // just use the first 3. debug stuff
    debugstuff = true;
  }
  if (nb_channels == 0) {
    e_printf("Cannot save a zero-channel 'image' as PAM.\n");
    return false;
  }

  int w = image.channel[0].w;
  int h = image.channel[0].h;
  if (image.w < w) w = image.w;
  if (image.h < h) h = image.h;
  for (int i=1; i<nb_channels; i++) {
    if (image.channel[i].w < w) nb_channels = 1;
    if (image.channel[i].h < h) nb_channels = 1;
    if (nb_channels == 1) e_printf("Not saving channel %i since dimensions (%ix%i) do not correspond with the image dimensions (%ix%i).\n",i,image.channel[i].w,image.channel[i].h,w,h);
  }

  if (nb_channels > image.nb_channels) {debugstuff=true; nb_channels=1;}

  FILE *fp = fopen(output_image,"wb");
  if (!fp) {
    return false;
  }


  int components=3;
  bool cmyk = false;
  if ((image.colormodel & 48) == 0) { // RGB
    if (nb_channels == 4) components=4;
  } else if ((image.colormodel & 48) == 16) { // CMYK
    if (nb_channels == 5) components=4;
    if (nb_channels > 3) cmyk = true;
  }
  if (image.colormodel != 0 && image.colormodel != 16) {
    v_printf(1,"Warning: saving raw pixel data without a color profile, even though the image is not sRGB.\n");
  }
  if (nb_channels == 1) components = 1;
  if (nb_channels == 2) components = 2;

  int bit_depth = 8, bytes_per_value=1;
  if (image.channel[0].minval < 0) {bit_depth = 8; bytes_per_value=1;} // this is just for debugging things


  if (image.maxval > 255) {bit_depth = 16; bytes_per_value=2;}
  if (image.maxval > 65535) { e_printf("Warning: cannot save as PAM since bit depth is higher than 16-bit. Doing it anyway.\n");}

  if (image.nb_channels == 0) {
    // visualize signed stuff
    int range = image.channel[0].maxval - image.channel[0].minval;
    v_printf(3,"Visualizing %ix%i channel with range %i..%i\n",w,h,image.channel[0].minval,image.channel[0].maxval);
    if (range > 255) {bit_depth = 16; bytes_per_value=2;}
    if (image.real_nb_channels == 0) {
    fprintf(fp,"P6\n%u %u\n%i\n", w, h, 255);
    for (int y = 0; y < h; y++) {
     for (int x = 0; x < w; x++) {
      for (int c=0; c<components; c++) {
        // 5461 is one bit
        fputc(CLAMP(image.channel[c].value(y,x)*255/5461, 0, 255),fp);
        fputc(CLAMP(image.channel[c].value(y,x)*50/5461, 0, 255),fp);
        fputc(CLAMP(image.channel[c].value(y,x)*12/5461, 0, 255),fp);
      }
     }
    }
    } else {
    fprintf(fp,"P5\n%u %u\n%i\n", w, h, range);
    for (int y = 0; y < h; y++) {
     for (int x = 0; x < w; x++) {
      for (int c=0; c<components; c++) {
        if (bytes_per_value > 1) fputc((image.channel[c].value(y,x) - image.channel[0].minval) >> 8,fp);
        fputc((image.channel[c].value(y,x) - image.channel[0].minval) & 0xFF,fp);
      }
     }
    }
    }
    fclose(fp);
    return 0;
  }

  if (components == 1) fprintf(fp,"P5\n%u %u\n%i\n", w, h, image.maxval);
  if (components == 2) fprintf(fp,"P7\nWIDTH %u\nHEIGHT %u\nDEPTH 2\nMAXVAL %i\nTUPLTYPE GRAYSCALE_ALPHA\nENDHDR\n", w, h, image.maxval);
  if (components == 3) fprintf(fp,"P6\n%u %u\n%i\n", w, h, image.maxval);
  if (components == 4) fprintf(fp,"P7\nWIDTH %u\nHEIGHT %u\nDEPTH 4\nMAXVAL %i\nTUPLTYPE RGB_ALPHA\nENDHDR\n", w, h, image.maxval);
  v_printf(10,"Writing %i-channel PAM file (range %i..%i)\n",components,image.minval,image.maxval);

  if (cmyk) {
    v_printf(1,"Warning: converting CMYK image to RGB in order to save it as PAM.\n");
    for (int y = 0; y < h; y++) {
     for (int x = 0; x < w; x++) {
      for (int c=0; c<3; c++) {
        if (bytes_per_value > 1) fputc(CLAMP( (image.maxval-image.channel[c].value_nocheck(y,x))*image.channel[3].value_nocheck(y,x)/image.maxval,image.minval,image.maxval) >> 8,fp);
        fputc(CLAMP( (image.maxval-image.channel[c].value_nocheck(y,x))*image.channel[3].value_nocheck(y,x)/image.maxval,image.minval,image.maxval) & 0xFF,fp);
      }
      if (nb_channels == 5) {
        if (bytes_per_value > 1) fputc(CLAMP(image.channel[4].value_nocheck(y,x),image.minval,image.maxval) >> 8,fp);
        fputc(CLAMP(image.channel[4].value_nocheck(y,x),image.minval,image.maxval) & 0xFF,fp);
      }
     }
    }
  } else {
    if (bytes_per_value > 1) {
    for (int y = 0; y < h; y++) {
     for (int x = 0; x < w; x++) {
      for (int c=0; c<components; c++) {
        fputc(CLAMP(image.channel[c].value_nocheck(y,x),image.minval,image.maxval) >> 8,fp);
        fputc(CLAMP(image.channel[c].value_nocheck(y,x),image.minval,image.maxval) & 0xFF,fp);
      }
     }
    }
    } else {
      if (components == 3) {
        for (int y = 0; y < h; y++) {
         for (int x = 0; x < w; x++) {
            fputc(CLAMP(image.channel[0].value_nocheck(y,x),image.minval,image.maxval) & 0xFF,fp);
            fputc(CLAMP(image.channel[1].value_nocheck(y,x),image.minval,image.maxval) & 0xFF,fp);
            fputc(CLAMP(image.channel[2].value_nocheck(y,x),image.minval,image.maxval) & 0xFF,fp);
         }
        }
      } else {
        for (int y = 0; y < h; y++) {
         for (int x = 0; x < w; x++) {
          for (int c=0; c<components; c++) {
            fputc(CLAMP(image.channel[c].value_nocheck(y,x),image.minval,image.maxval) & 0xFF,fp);
          }
         }
        }
      }
    }
  }

  fclose(fp);
  return 0;
}
