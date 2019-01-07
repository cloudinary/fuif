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
#include "png.h"


bool write_PNG_file(const char *output_image, const Image &image) {

  int nb_channels = image.channel.size();
  bool debugstuff = false;

  if (nb_channels > ((image.colormodel & 48) == 16 ? 5 : 4)) {
    e_printf("Too many channels. Saving just the first 3 channels (instead of %i)\n",nb_channels);
    nb_channels = 3; // just use the first 3. debug stuff
    debugstuff = true;
  }
  if (nb_channels == 0) {
    e_printf("Cannot save a zero-channel 'image' as PNG.\n");
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

  if (w > 0x7fffffffL || h > 0x7fffffffL) {
    printf("Image too large to be saved as PNG.\n");
    return false;
  }

  FILE *fp = fopen(output_image,"wb");
  if (!fp) {
    return false;
  }
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,(png_voidp) NULL,NULL,NULL);
  if (!png_ptr) {
    fclose(fp);
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr,(png_infopp) NULL);
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr,fp);
  png_set_user_limits(png_ptr, 0x7fffffffL, 0x7fffffffL);
//  png_set_filter(png_ptr,0,PNG_FILTER_PAETH);
//  png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);


  int colortype=PNG_COLOR_TYPE_RGB;
  bool cmyk = false;
  if ((image.colormodel & 48) == 0) { // RGB
    if (nb_channels == 4) colortype=PNG_COLOR_TYPE_RGB_ALPHA;
  } else if ((image.colormodel & 48) == 16) { // CMYK
    if (nb_channels == 5) colortype=PNG_COLOR_TYPE_RGB_ALPHA;
    if (nb_channels > 3) cmyk = true;
  }

  if (nb_channels == 1) colortype=PNG_COLOR_TYPE_GRAY;
  if (nb_channels == 2) colortype=PNG_COLOR_TYPE_GRAY_ALPHA;
  int bit_depth = 8, bytes_per_value=1;
  if (image.channel[0].minval < 0) {bit_depth = 8; bytes_per_value=1;} // this is just for debugging things

  if (image.maxval > 255) {bit_depth = 16; bytes_per_value=2;}
  if (image.maxval > 65535) { e_printf("Cannot save as PNG since bit depth is higher than 16-bit.\n");    return false; }

  if (image.channel[0].maxval > 255 && debugstuff && nb_channels == 1) {colortype=PNG_COLOR_TYPE_RGB; nb_channels=3;}

  int shift = 0;
//  if (image.channel[0].maxval == 2048) shift = 5;  // could use something like this to debug DCT values

  png_set_IHDR(png_ptr,info_ptr,w,h,bit_depth,colortype,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr,info_ptr);

  png_bytep row = (png_bytep) png_malloc(png_ptr,nb_channels * bytes_per_value * w);

  v_printf(10,"Writing PNG file (range %i..%i)\n",image.minval,image.maxval);
  if (image.channel[0].maxval > 255 && debugstuff) {
    v_printf(10,"Writing PNG file with false colors for debug purposes\n");
    for (size_t r = 0; r < (size_t) h; r++) {
     for (size_t c = 0; c < (size_t) w; c++) {
        row[c * 3 + 0] = (png_byte) (CLAMP((image.channel[0].value(r,c)%10)*26,0,255));
        row[c * 3 + 1] = (png_byte) (CLAMP(image.channel[0].value(r,c)/10,0,255));
        row[c * 3 + 2] = (png_byte) (CLAMP(image.channel[0].value(r,c)/500,0,255));
     }
     png_write_row(png_ptr,row);
    }
  } else if (false && image.channel[0].minval < 0) {
    v_printf(10,"Writing PNG file with negative pixels\n");
   // visualize the negative pixels somehow, for debugging.
   for (size_t r = 0; r < (size_t) h; r++) {
     for (size_t c = 0; c < (size_t) w; c++) {
      for (int p=0; p<nb_channels; p++) {
        row[c * nb_channels + p] = (png_byte) (CLAMP(127+(image.channel[p].value(r,c)),0,255));
      }
     }
    png_write_row(png_ptr,row);
    }
  } else if (cmyk) {
   v_printf(1,"Warning: converting CMYK image to RGB in order to save it as PNG.\n");
   for (size_t r = 0; r < (size_t) h; r++) {
     for (size_t c = 0; c < (size_t) w; c++) {
      for (int p=0; p<3; p++) {
        row[c * (nb_channels-1) + p] = (png_byte) (CLAMP( (255-image.channel[p].value(r,c))*(image.channel[3].value(r,c))/255,0,255));
      }
      if (nb_channels == 5) row[c * (nb_channels-1) + 3] = (png_byte) (CLAMP(image.channel[4].value(r,c),0,255)); // cmyk+alpha

     }
    png_write_row(png_ptr,row);
    }
  } else for (size_t r = 0; r < (size_t) h; r++) {
    if (bytes_per_value == 1) {
     for (size_t c = 0; c < (size_t) w; c++) {
      for (int p=0; p<nb_channels; p++) {
        row[c * nb_channels + p] = (png_byte) (CLAMP(image.channel[p].value(r,c),image.minval,image.maxval));
      }
     }
    } else {
     if (!debugstuff) {
     for (size_t c = 0; c < (size_t) w; c++) {
      for (int p=0; p<nb_channels; p++) {
        row[c * nb_channels * 2 + 2*p] = (png_byte) ((CLAMP(image.channel[p].value(r,c),image.minval,image.maxval)<<shift) >> 8);
        row[c * nb_channels * 2 + 2*p + 1] = (png_byte) ((CLAMP(image.channel[p].value(r,c),image.minval,image.maxval)<<shift) & 0xff);
      }
     }
     } else {
     for (size_t c = 0; c < (size_t) w; c++) {
      for (int p=0; p<nb_channels; p++) {
        row[c * nb_channels * 2 + 2*p] = (png_byte) ((image.channel[p].value(r,c)) >> 8);
        row[c * nb_channels * 2 + 2*p + 1] = (png_byte) ((image.channel[p].value(r,c)) & 0xff);
      }
     }
     }
    }
    png_write_row(png_ptr,row);
  }

  png_free(png_ptr,row);
  png_write_end(png_ptr,info_ptr);
  png_destroy_write_struct(&png_ptr,&info_ptr);
  fclose(fp);
  return 0;
}
