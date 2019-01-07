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
#include "../transform/transform.h"
#include <jpeglib.h>

// TODO: read color profile and add it to the image (currently profiles are just silently stripped, which is of course wrong)

struct jpegErrorManager {
    /* "public" fields */
    struct jpeg_error_mgr pub;
    /* for return to caller */
    jmp_buf setjmp_buffer;
};
char jpegLastErrorMsg[JMSG_LENGTH_MAX];
void jpegErrorExit (j_common_ptr cinfo)
{
    /* cinfo->err actually points to a jpegErrorManager struct */
    jpegErrorManager* myerr = (jpegErrorManager*) cinfo->err;
    /* note : *(cinfo->err) is now equivalent to myerr->pub */

    /* output_message is a method to print an error message */
    /*(* (cinfo->err->output_message) ) (cinfo);*/      

    /* Create the message */
    ( *(cinfo->err->format_message) ) (cinfo, jpegLastErrorMsg);

    /* Jump to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);

}

Image read_JPEG_file(const char *input_image) {

  FILE *fp = fopen(input_image,"rb");
  if (!fp) return Image();

  struct jpeg_decompress_struct cinfo;

  jpegErrorManager jerr;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpegErrorExit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error. */
    v_printf(10, "libjpeg: %s\n",jpegLastErrorMsg);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);
    return Image();
  }

  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, fp);
  (void) jpeg_read_header(&cinfo, TRUE);


  jvirt_barray_ptr *coeffs_array = jpeg_read_coefficients(&cinfo);

  int nbcomp = cinfo.num_components;
  int w = cinfo.image_width;
  int h = cinfo.image_height;
  int bitdepth = cinfo.data_precision;

  Image image;
  image.w = w;
  image.h = h;
  image.nb_channels = nbcomp;
  image.real_nb_channels = nbcomp;
  image.minval = 0;
  image.maxval = (1<<bitdepth)-1;

  if (nbcomp == 4) image.colormodel = 16; // CMYK

  Transform subsampling(TRANSFORM_ChromaSubsample);
  int max_factor_h = 1;
  int max_factor_v = 1;
  for (int ci=0; ci < nbcomp; ci++) {
      jpeg_component_info* compptr = cinfo.comp_info + ci;
      int srh = compptr->h_samp_factor;
      int srv = compptr->v_samp_factor;
      if (srh > max_factor_h) max_factor_h = srh;
      if (srv > max_factor_v) max_factor_v = srv;
  }
  std::vector<int> & p = subsampling.parameters;
  for (int ci=0; ci < nbcomp; ci++) {
      jpeg_component_info* compptr = cinfo.comp_info + ci;
      int srh = compptr->h_samp_factor;
      int srv = compptr->v_samp_factor;
      if (srh < max_factor_h || srv < max_factor_v) {
         p.push_back(ci);
         p.push_back(ci);
         p.push_back(max_factor_h/srh);
         p.push_back(max_factor_v/srv);
      }
  }
  if (p.size() >= 8) {
    if (p[2] == p[6] && p[3] == p[7] && p[4] == p[1]+1) {
        p[1] = p[4];
        p.erase(p.begin()+4, p.end());
    }
    if (p.size() == 4 && p[0] == 1 && p[1] == 2) {
        if (p[2] == 2 && p[3] == 2) { p[0] = 0; p.erase(p.begin()+1, p.end()); } // 4:2:0
    }
  }

  if (cinfo.jpeg_color_space == JCS_YCbCr ||
      cinfo.jpeg_color_space == JCS_YCCK) image.transform.push_back(Transform(TRANSFORM_YCbCr));

  if (p.size()) image.transform.push_back(subsampling);

  image.transform.push_back(Transform(TRANSFORM_DCT));

  image.transform.push_back(Transform(TRANSFORM_QUANTIZE));

  std::vector<std::vector<int>> ordering;
  std::vector<int> comp;
  std::vector<int> coeff;
  default_DCT_scanscript(nbcomp,ordering,comp,coeff);

  for (int i=0; i < 64*nbcomp; i++) {
    int bi=0;
    for (; bi<64 ; bi++) if (coeff[i]==jpeg_zigzag[bi]) break;
    int ci = comp[i];
    jpeg_component_info* compptr = cinfo.comp_info + ci;
    int hib = compptr->height_in_blocks;
    int wib = compptr->width_in_blocks;
    int q = compptr->quant_table->quantval[bi];
    v_printf(5,"Quantization for component %i, coefficient %i: %i\n",comp[i],coeff[i],q);

    Channel c(wib,hib,LARGEST_VAL,SMALLEST_VAL);
    c.q = q;
    c.component = ci;
    c.hshift = 3 + (compptr->h_samp_factor < max_factor_h ? 1 : 0);
    c.vshift = 3 + (compptr->v_samp_factor < max_factor_v ? 1 : 0);
    c.hcshift = dct_cshifts[coeff[i]];
    c.vcshift = dct_cshifts[coeff[i]];
    int zeroes=0;
    int nonzeroes=0;
    for (int by=0; by < hib; by++) {
      JBLOCKARRAY buffer = (cinfo.mem->access_virt_barray)((j_common_ptr)&cinfo, coeffs_array[ci], by, (JDIMENSION) 1, FALSE);
      for (int bx=0; bx < wib; bx++) {
        JCOEFPTR blockptr = buffer[0][bx];
        c.value(by,bx) = blockptr[bi];
        if (c.value(by,bx) > c.maxval) c.maxval = c.value(by,bx);
        if (c.value(by,bx) < c.minval) c.minval = c.value(by,bx);
        if (c.value(by,bx)) nonzeroes++; else zeroes++;
      }
    }
    v_printf(10,"Comp %i, coeff %i (natural %i) : %f %% zeroes (%i zeroes, %i nonzeroes)\n",ci,coeff[i],bi,100.0*zeroes/(zeroes+nonzeroes),zeroes,nonzeroes);
    image.channel.push_back(c);
  }

  jpeg_finish_decompress( &cinfo );
  jpeg_destroy_decompress( &cinfo );
  fclose(fp);

  image.error = false;


  return image;
}
