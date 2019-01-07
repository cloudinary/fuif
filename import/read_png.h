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

// TODO: read color profile and add it to the image


Image read_PNG_file(const char *input_image) {

  FILE *fp = fopen(input_image,"rb");
  if (!fp) return Image();

  png_byte header[8];
  int rr = fread(header,1,8,fp);
  int is_png = !png_sig_cmp(header,0,rr);
  if (!is_png) {
    fclose(fp);
    return Image();
  }
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,(png_voidp) NULL,NULL,NULL);

  if (!png_ptr) {
    fclose(fp);
    return Image();
  }
  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_read_struct(&png_ptr,(png_infopp) NULL,(png_infopp) NULL);
    fclose(fp);
    return Image();
  }
  if (setjmp(png_jmpbuf(png_ptr))) {
       png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
       fclose(fp);
       return Image();
  }
  png_init_io(png_ptr,fp);
  png_set_sig_bytes(png_ptr,8);


//  int png_flags = PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_GRAY_TO_RGB;
//  png_read_png(png_ptr,info_ptr, png_flags, NULL);
  png_read_info(png_ptr,info_ptr);

  size_t width = png_get_image_width(png_ptr,info_ptr);
  size_t height = png_get_image_height(png_ptr,info_ptr);
  int bit_depth = png_get_bit_depth(png_ptr,info_ptr);
  int color_type = png_get_color_type(png_ptr,info_ptr);

  unsigned int nbplanes;

  if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

  if (png_get_valid(png_ptr, info_ptr,
        PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

  if (color_type == PNG_COLOR_TYPE_GRAY &&
        bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
/*
  if (bit_depth == 16)
#if PNG_LIBPNG_VER >= 10504
       png_set_scale_16(png_ptr);
#else
       png_set_strip_16(png_ptr);
#endif
*/
  if (bit_depth < 8)
       png_set_packing(png_ptr);
//  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
//       png_set_gray_to_rgb(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  color_type = png_get_color_type(png_ptr,info_ptr);


  if (color_type == PNG_COLOR_TYPE_GRAY) nbplanes=1;
  else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA) nbplanes=2;
  else if (color_type == PNG_COLOR_TYPE_RGB) nbplanes=3;
  else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) nbplanes=4;
  else { png_destroy_read_struct(&png_ptr,(png_infopp) NULL,(png_infopp) NULL); fclose(fp); return Image(); }

  int w = width, h = height, maxval = (bit_depth > 8 ? 65535 : 255);

  int bytes_per_pixel = (bit_depth > 8 ? 2 : 1);
  Image image(w,h,maxval,nbplanes);

  std::vector<unsigned char> one_row(w*nbplanes*bytes_per_pixel,0);
  png_bytep rowptr = &(one_row[0]);
  if (bit_depth <= 8) {
    int y=0;
    while (y < h) {
      png_read_row(png_ptr, rowptr, NULL);
      for(int x=0; x < w ; x++) {
       for (int k=0; k<nbplanes; k++) {
        image.channel[k].data[x+y*w] = one_row[x*nbplanes+k];
       }
      }
      y++;
    }
  } else {
    int y=0;
    while (y < h) {
      png_read_row(png_ptr, rowptr, NULL);
      int index=0;
      for(int x=0; x < w ; x++) {
       for (int k=0; k<nbplanes; k++) {
        image.channel[k].data[x+y*w] = (one_row[index++] << 8);
        image.channel[k].data[x+y*w] += one_row[index++];
       }
      }
      y++;
    }
  }
  png_read_end(png_ptr, info_ptr);
  png_destroy_read_struct(&png_ptr,&info_ptr,(png_infopp) NULL);
  fclose(fp);
  return image;
}
