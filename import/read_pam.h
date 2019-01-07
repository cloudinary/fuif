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

#define PPMREADBUFLEN 256


// auxiliary function to read a non-zero number from a PNM header, ignoring whitespace and comments
unsigned int read_pnm_int(FILE *fp, char* buf, char** t) {
    long result = strtol(*t,t,10);
    if (result == 0) {
        // no valid int found here, try next line
        do {
          *t = fgets(buf, PPMREADBUFLEN, fp);
          if ( *t == NULL ) {return false;}
        } while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
        result = strtol(*t,t,10);
        if (result == 0) { e_printf("Invalid PNM file.\n"); fclose(fp); }
    }
    return result;
}

Image read_PAM_file(const char *filename) {
    FILE *fp = NULL;
    if (!strcmp(filename,"-")) fp = stdin;
    else fp = fopen(filename,"rb");

    char buf[PPMREADBUFLEN], *t;
    if (!fp) {
        e_printf("Could not open file: %s\n", filename);
        return Image();
    }
    unsigned int width=0,height=0;
    unsigned int maxval=0,nbchans=0;
    do {
        /* # comments before the first line */
        t = fgets(buf, PPMREADBUFLEN, fp);
        if ( t == NULL ) { fclose(fp); return Image(); }
    } while ( strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0);
    int type=0;
    if ( (!strncmp(buf, "P4", 2)) ) type=4;
    if ( (!strncmp(buf, "P5", 2)) ) type=5;
    if ( (!strncmp(buf, "P6", 2)) ) type=6;
    if ( (!strncmp(buf, "P7", 2)) ) type=7;
    if (type==0) {
        if (buf[0] == 'P') e_printf("PNM file is not of type P4, P5, P6 or P7, cannot read other types.\n");
        else v_printf(8,"This does not look like a PNM file.\n");
        fclose(fp);
        return Image();
    }
    t += 2;

    v_printf(7,"Trying to load a PNM/PAM file of type P%i\n",type);
    if (type < 7) {
      if (!(width = read_pnm_int(fp,buf,&t))) return Image();
      if (!(height = read_pnm_int(fp,buf,&t))) return Image();
      if (type>4) {
        if (!(maxval = read_pnm_int(fp,buf,&t))) return Image();
        if ( maxval > 0xffff ) {
          e_printf("Invalid PNM file (more than 16-bit?)\n");
          fclose(fp);
          return Image();
        }
      } else maxval=1;
      nbchans=(type==6?3:1);
    } else {
      int maxlines = 100;
      do {
        t = fgets(buf, PPMREADBUFLEN, fp);
        if ( t == NULL ) {
            fclose(fp);
            return Image();
        }
        if (strncmp(buf, "#", 1) == 0 || strncmp(buf, "\n", 1) == 0)
            continue;

        sscanf(buf, "WIDTH %u\n", &width);
        sscanf(buf, "HEIGHT %u\n", &height);
        sscanf(buf, "DEPTH %u\n", &nbchans);
        sscanf(buf, "MAXVAL %u\n", &maxval);

        if (maxlines-- < 1) {
            e_printf("Problem while parsing PAM header.\n");
            fclose(fp);
            return Image();
        }
      } while ( strncmp(buf, "ENDHDR", 6) != 0 );
      if (nbchans>4 || nbchans <1 || width <1 || height < 1 || maxval<1 || maxval > 0xffff) {
        e_printf("Couldn't parse PAM header, or unsupported kind of PAM file.\n");
        fclose(fp);
        return Image();
      }
    }

    v_printf(7,"Loading %ix%i PNM/PAM file with %i channels and maxval=%i\n",width,height,nbchans,maxval);
    Image image(width,height, maxval, nbchans);
    if (type==4) {
      char byte=0;
      for (unsigned int y=0; y<height; y++) {
        for (unsigned int x=0; x<width; x++) {
                if (x%8 == 0) byte = fgetc(fp);
                image.channel[0].value(y,x) = (byte & (128>>(x%8)) ? 0 : 1);
        }
      }
    } else {
      if (maxval > 0xff) {
        for (unsigned int y=0; y<height; y++) {
          for (unsigned int x=0; x<width; x++) {
            for (unsigned int c=0; c<nbchans; c++) {
                pixel_type pixel= (fgetc(fp) << 8);
                pixel += fgetc(fp);
                if (pixel > (int)maxval) {
                    fclose(fp);
                    e_printf("Invalid PNM/PAM file: value %i is larger than declared maxval %u\n", pixel, maxval);
                    return Image();
                }
                image.channel[c].value(y,x) = pixel;
            }
          }
        }
      } else {
        for (unsigned int y=0; y<height; y++) {
          for (unsigned int x=0; x<width; x++) {
            for (unsigned int c=0; c<nbchans; c++) {
                image.channel[c].value(y,x) = fgetc(fp);
            }
          }
        }
      }
    }
    if (fp != stdin) fclose(fp);
    return image;
}
