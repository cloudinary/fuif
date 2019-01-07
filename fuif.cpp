
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


#include "encoding/encoding.h"

#include "import/read_gif.h"
#include "import/read_png.h"
#include "import/read_jpeg.h"
#include "import/read_pam.h"
#include "import/read_yuv.h"
#include "export/write_png.h"
#include "export/write_pam.h"
#include "export/write_yuv.h"

#include <getopt.h>

#define FUIFVERSIONSTRING "0.0.1"

// DCT default quantization tables
// these are the quantization tables from mozjpeg -quant-table 2
static const uint8_t dct_luma_qtable[64] = {
    12, 17, 20, 21, 30, 34, 56, 63,
    18, 20, 20, 26, 28, 51, 61, 55,
    19, 20, 21, 26, 33, 58, 69, 55,
    26, 26, 26, 30, 46, 87, 86, 66,
    31, 33, 36, 40, 46, 96, 100, 73,
    40, 35, 46, 62, 81, 100, 111, 91,
    46, 66, 76, 86, 102, 121, 120, 101,
    68, 90, 90, 96, 113, 102, 105, 103
  };

static const uint8_t dct_chroma_qtable[64] = {
    8, 12, 15, 15, 86, 96, 96, 98,
    13, 13, 15, 26, 90, 96, 99, 98,
    12, 15, 18, 96, 99, 99, 99, 99,
    17, 16, 90, 96, 99, 99, 99, 99,
    96, 96, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
  };


// Squeeze default quantization factors
// these quantization factors are for -Q 50  (other qualities simply scale the factors; things are rounded down and obviously cannot get below 1)
float squeeze_quality_factor = 0.3; // for easy tweaking of the quality range (decrease this number for higher quality)
float squeeze_luma_factor = 1.2;    // for easy tweaking of the balance between luma (or anything non-chroma) and chroma
                                              // (decrease this number for higher quality luma)

static const float squeeze_luma_qtable[16] =   {163.84,81.92,40.96,20.48,10.24,5.12,2.56,1.28,0.64,0.32,0.16,0.08,0.04,0.02,0.01,0.005};
// for 8-bit input, the range of YCoCg chroma is -255..255 so basically this does 4:2:0 subsampling (two most fine grained layers get quantized away)
static const float squeeze_chroma_qtable[16] = {1024,512,256,128,64,32,16,8,4,2,1,0.5,0.5,0.5,0.5,0.5};

bool file_exists(const char * filename){
        FILE * file = fopen(filename, "rb");
        if (!file) return false;
        fclose(file);
        return true;
}


int main(int argc, char** argv) {

    static struct option optlist[] = {
        {"help", 0, NULL, 'h'},
        {"verbose", 0, NULL, 'v'},
        {"version", 0, NULL, 'V'},
        {"decode", 0, NULL, 'd'},
        {"match-dist", 1, NULL, 'M'},
        {"colorspace", 1, NULL, 'C'},
        {"iterations", 1, NULL, 'I'},
        {"predictor", 1, NULL, 'P'},
        {"extra-context", 1, NULL, 'E'},
        {"quality", 1, NULL, 'Q'},
        {"palette", 1, NULL, 'K'},
        {"pre-compact", 1, NULL, 'X'},
        {"post-compact", 1, NULL, 'Y'},
        {"dct", 0, NULL, 'J'},
        {"responsive", 1, NULL, 'R'},
        {"yuv420p", 1, NULL, 'y'},
        {"uncompressed", 0, NULL, 'U'},
        {"identify", 0, NULL, 'i'},
        {"group", 1, NULL, 'G'},
        {"heatmap", 0, NULL, 'H'},
        {"framerate", 1, NULL, 'F'},
        {"approximate", 1, NULL, 'A'},
        {0,0,0,0}
    };

    bool decode=false;
    bool showhelp = false, showversion = false;
    bool disable_ycocg = false, enable_dct = false, yuv = false, max_dist_set = false;
    int responsive = -1;
    int c,i;
    float quality=100, cquality=101;
    int colorspace = -1;
    float channel_colors = 0.7, channel_colors_pre_transform = 0.7;
    int palette_colors = 256;
    int w,h, bitdepth=8;
    int framerate=-1;
    int approx_k=0, approx_q=3;
    fuif_options options = default_fuif_options;

    while ((c = getopt_long (argc, argv, "hvVdiM:C:I:P:E:Q:JR:K:X:Y:y:UG:HF:A:", optlist, &i)) != -1) {
        switch (c) {
            case 'v': increase_verbosity(); break;
            case 'd': decode = true; break;
            case 'V': showversion = true; break;
            case 'M': options.max_dist = atoi(optarg); max_dist_set = true; break;
            case 'C': colorspace = atoi(optarg); break;
            case 'I': options.nb_repeats = atof(optarg); break;
            case 'P': while (optarg[0]) {if (optarg[0]=='?') options.predictor.push_back(-1); else if(optarg[0]>='0' && optarg[0]<='9') options.predictor.push_back(optarg[0]-'0'); optarg++;} break;
            case 'E': options.max_properties=atoi(optarg); break;
            case 'Q': sscanf(optarg,"%f,%f",&quality,&cquality); break;
            case 'J': enable_dct = true; break;
            case 'R': responsive = atoi(optarg); break;
            case 'i': options.identify = true; decode=true; break;
            case 'K': palette_colors = atoi(optarg); break;
            case 'X': channel_colors = 0.01*atof(optarg); break;
            case 'Y': channel_colors_pre_transform = 0.01*atof(optarg); break;
            case 'y': yuv=true; sscanf(optarg,"%ix%i:%i",&w,&h,&bitdepth); break;
            case 'U': options.compress = false; break;
            case 'G': options.max_group = atoi(optarg); break;
            case 'H': options.debug = true; break;
            case 'h': showhelp=true; break;
            case 'F': framerate=atoi(optarg); break;
            case 'A': sscanf(optarg,"%i,%i",&approx_k,&approx_q); break;
            default: e_printf("Error: unknown option '%s'. Try --help.", argv[optind]); return 3;
        }
    }

    if (showversion) {
        v_printf(3," ___     . ___ \n");
        v_printf(3," )-  (_) | )-  \n");
        v_printf(3,"\n");
        v_printf(1,"FUIF " FUIFVERSIONSTRING "\n");
        v_printf(2,"Free Universal Image Format\n");
        v_printf(2,"Copyright 2018, Jon Sneyers, Cloudinary\n");
        if (showhelp) v_printf(1,"\n");
    }
    if (showhelp || (!showversion && argc-optind < (options.identify?1:2))) {
        v_printf(1,"Usage: %s [encode-options] <input.jpg> [alpha_mask] <output.fuif>\n",argv[0]);
        v_printf(1,"       %s [encode-options] <input.png|input.pnm|input.gif> <output.fuif>\n",argv[0]);
        v_printf(1,"       %s [encode-options] -y WxH[:b] <input.yuv> <output.fuif>\n",argv[0]);
        v_printf(1,"       %s -d [decode-options] <input.fuif> <output.png|output.ppm>\n",argv[0]);
        v_printf(1,"       %s -i <input.fuif>\n",argv[0]);
        v_printf(1,"   -h, --help                  show help (more advanced stuff is shown only with more -v)\n");
        v_printf(1,"   -v, --verbose               increase verbosity (multiple -v for more output)\n");
        v_printf(1,"   -V, --version               print version number\n");
        v_printf(1,"   -i, --identify              decode only the header and print info about a FUIF file\n");
        v_printf(1,"Decode options:\n");
        v_printf(1,"   -R, --responsive=K          partial decode: -1=full image (default), 0=LQIP, 1=(1:16), 2=(1:8), 3=(1:4), 4=(1:2)\n");
        v_printf(1,"Encode options:\n");
        v_printf(1,"   -Q, --quality=K             reduce quality by quantizing stuff\n");
        v_printf(1,"   -R, --responsive=K          0=false, 1=true (default: true)\n");
        v_printf(1,"   -y, --yuv420p=WxH[:b]       interpret input file as YUV420p with dimensions W x H and bit depth b\n");
        v_printf(2,"   -U, --uncompressed          don't use compression at all\n");
        v_printf(2,"   -E, --extra-properties=K    number of extra MANIAC tree properties to use\n",default_fuif_options.max_properties);
        v_printf(2,"   -I, --iterations=K          number of mock encodes to learn MANIAC trees (default=%.2f, try 0 for fast decode)\n",default_fuif_options.nb_repeats);
        v_printf(2,"   -M, --match-dist=K          set maximum match distance (negative numbers to look abs(K) frames back, only at corresponding positions)\n");
        v_printf(2,"                               (default=%i for still images, -1 for animations)\n",default_fuif_options.max_dist);
        v_printf(3,"   -J, --dct                   use JPEG-style DCT instead of Squeeze (lossy)\n");
        v_printf(3,"   -C, --colorspace=K          0=RGB, 1=YCbCr, 2=YCoCg (default: keep for JPEG/YUV input, YCoCg for other input)\n");
        v_printf(3,"   -K, --palette=K             use a palette if image has at most K colors (default: %i)\n",palette_colors);
        v_printf(3,"   -X, --pre-compact=K         compact channels (before color transform) if ratio used/range is below this (default: %.1f%%)\n", 100.0 * channel_colors_pre_transform);
        v_printf(3,"   -Y, --post-compact=K        compact channels (after color transform) if ratio used/range is below this (default: %.1f%%)\n", 100.0 * channel_colors);
        v_printf(4,"   -P, --predictor=K           predictor(s) to use (defaults should be fine)\n");
        v_printf(4,"   -A, --approximate=K,Q       approximate last K scans with quantization Q\n");
        v_printf(4,"   -G, --group=K               don't use channel groups larger than K channels (default: no limit if DCT, 1 otherwise)\n");
        v_printf(5,"   -H, --heatmap               write bit cost heatmap to files heatmap*\n");
        v_printf(1,"To encode animations, you can use printf-style syntax, e.g. %s frame-%%02d.png animation.fuif.\n",argv[0]);
        v_printf(2,"   -F, --framerate=K           frames per second (default: 10)\n");
        return 2;
    }

    if (showversion) return 0;

    argc -= optind;
    argv += optind;


    if (decode) {
        if (responsive < -1 || responsive > 4) {
            e_printf("Invalid value for -R option (range: -1..4)\n");
            return 1;
        }
        options.preview = responsive;
        Image decoded;
        if (fuif_decode_file(argv[0],decoded,options)) {
            if (options.identify) return 0;
            if (!strcasecmp(argv[1],"null:")) {
                decoded.undo_transforms();
                return 0;
            }
            if (!strcasecmp(argv[1],"null_yuv:")) {
                decoded.undo_transforms(2);
                return 0;
            }
            if (!strcasecmp(argv[1],"null_none:")) {
                return 0;
            }

            const char *ext = strrchr(argv[1],'.');

            if (ext && !strcasecmp(ext,".yuv")) {
                decoded.undo_transforms(2);
                write_YUV_file(argv[1],decoded);
            } else {
                decoded.undo_transforms();
                if (ext && !strcasecmp(ext,".png"))
                    write_PNG_file(argv[1],decoded);
                else
                    write_PAM_file(argv[1],decoded);
            }
            return 0;
        } else {
            e_printf("Could not decode %s\n",argv[0]);
            return -1;
        }
    }

    bool has_dct = false;
    if (cquality > 100) cquality = quality;

    Image input_img;
    int image_type = -1; // 0 = JPEG, 1 = PNG/PPM, 2 = YUV, 3 = FUIF, 4 = GIF

    int frame=0;
    char filename[1024];
    bool multiframe=false;
    if (strstr(argv[0],"%")) multiframe=true;
    else {
        input_img = read_GIF_file(argv[0]);
        image_type = 4;
    }
    Image multi_input_img;
    if (!input_img.w)
    while (true) {
      snprintf(filename,1024,argv[0],frame);
      if(!file_exists(filename)) {
        if (!multiframe) {
            e_printf("Error: no such file: %s\n",filename);
            return 1;
        } else {
            if (multi_input_img.w > 0) break;
            if (frame > 100) {
                e_printf("Error: no such files: %s\n",argv[0]);
                return 1;
            }
            frame++; continue;
        }
      }
      if (yuv) {
          input_img = read_YUV_file(filename,w,h,bitdepth);
          if (!input_img.w) {
            e_printf("Error: could not read input file %s (expecting YUV input)\n",filename);
            return 1;
          }
          image_type = 2;
      } else {
        input_img = read_JPEG_file(filename);
        if (input_img.w) {
            image_type = 0;
        } else {
          // Couldn't load the input as JPEG, try loading as PNG
          image_type = 1;
          input_img = read_PNG_file(filename);
          if (!input_img.w) {
            // Couldn't load it as PNG, trying as PPM/PAM
            input_img = read_PAM_file(filename);
            if (!input_img.w) {
              if (fuif_decode_file(filename,input_img,options)) {
                v_printf(2,"Re-encoding existing FUIF file.\n");
                image_type = 3;
              } else {
                e_printf("Error: could not read input file %s (expecting PNM/PAM, PNG, JPEG, GIF or FUIF input)\n",filename);
                return 1;
              }
            }
          }
        }
      }
      if (!multiframe) break;
      frame++;
      if (multi_input_img.h == 0) multi_input_img = std::move(input_img);
      else {
        if (input_img.w != multi_input_img.w || input_img.h != multi_input_img.h / multi_input_img.nb_frames) {
                e_printf("Error: input frame %s has wrong dimensions (%ix%i, expected %ix%i)\n",filename,input_img.w,input_img.h,multi_input_img.w,multi_input_img.h / multi_input_img.nb_frames);
                return 1;
        }
        if (input_img.nb_channels != multi_input_img.nb_channels) {
                e_printf("Error: input frame %s has wrong number of channels\n",filename);
                return 1;
        }
        multi_input_img.h += input_img.h;
        for (int i=0; i<multi_input_img.channel.size(); i++) {
            Channel &ch = multi_input_img.channel[i];
            Channel &ich = input_img.channel[i];
            // todo: check that all channel fields are the same
            int offset = ch.w*ch.h;
            ch.resize(ch.w,ch.h + ich.h);
            for (int j=0; j<ich.data.size(); j++) ch.data[offset+j] = ich.data[j];
        }
        multi_input_img.nb_frames++;
      }
    }
    if (multiframe) {
        input_img = std::move(multi_input_img);
        v_printf(2,"Loaded %i frames from files %s\n",input_img.nb_frames,argv[0]);
    }
    if (framerate>0) input_img.den = framerate;

    input_img.recompute_minmax();
    if (input_img.nb_channels > 3 && input_img.channel[3].minval == input_img.maxval && input_img.channel[3].maxval == input_img.maxval) {
        v_printf(3,"Dropping trivial alpha channel\n");
        input_img.nb_channels--;
        input_img.real_nb_channels--;
        input_img.channel.erase(input_img.channel.begin()+input_img.nb_meta_channels+3,input_img.channel.begin()+input_img.nb_meta_channels+4);
    }

    if (quality < 100 && palette_colors > 0) {
        v_printf(3,"Lossy encode, not doing palette transforms\n");
        channel_colors = 0;
        channel_colors_pre_transform = 0;
        palette_colors = 0;
    }

    if (image_type == 0) {
        // Default options for JPEG input
        has_dct = true;
        if (input_img.real_nb_channels > 1) {
          bool ycbcr = (input_img.transform[0].ID == TRANSFORM_YCbCr);
          if ( (colorspace == 0 && ycbcr) || (colorspace == 1 && !ycbcr) || colorspace > 1) {
              e_printf("Error: cannot change the color space of JPEG input\n");
              return 1;
          }

/*
          // do something like this if you want to use another scan script
            Transform reorder(TRANSFORM_PERMUTE);
            for (int i=0; i<input_img.channel.size(); i++) { reorder.parameters.push_back(input_img.channel.size()-1-i); }
            input_img.do_transform(reorder);
          // can also use TRANSFORM_APPROXIMATE
*/
        }
    } else if (image_type == 1 || image_type == 4) {

        // Default options for PNG/PPM/GIF input

        if (channel_colors_pre_transform > 0 && colorspace != 0 && image_type != 4) {
          // single channel palette (like FLIF's ChannelCompact)
          input_img.recompute_minmax();
          for (int i=0; i<input_img.nb_channels; i++) {
            int colors = (input_img.channel[input_img.nb_meta_channels+i].maxval - input_img.channel[input_img.nb_meta_channels+i].minval + 1);
            if (colors < 256) continue; // only do this for 16-bit PNGs
            v_printf(10,"Channel %i: range=%i..%i\n",i,input_img.channel[input_img.nb_meta_channels+i].minval,input_img.channel[input_img.nb_meta_channels+i].maxval);
            Transform maybe_palette_1(TRANSFORM_PALETTE);
            maybe_palette_1.parameters.push_back(i);
            maybe_palette_1.parameters.push_back(i);
            // simple heuristic: if less than X percent of the values in the range actually occur, it is probably worth it to do a compaction
            maybe_palette_1.parameters.push_back((int) (channel_colors_pre_transform * colors));
            input_img.do_transform(maybe_palette_1);
          }
        }

        input_img.recompute_minmax();

        if (colorspace < 0 || colorspace == 2) {
            input_img.do_transform(Transform(TRANSFORM_YCoCg));
        } else if (colorspace == 1) {
            input_img.do_transform(Transform(TRANSFORM_YCbCr));
        } else if (colorspace == 3) {
            input_img.do_transform(Transform(TRANSFORM_XYB));
        }

        if (palette_colors > 0) {
          // all-channel palette (e.g. RGBA)
          if (input_img.nb_channels > 1) {
            Transform maybe_palette(TRANSFORM_PALETTE);
            maybe_palette.parameters.push_back(0);
            maybe_palette.parameters.push_back(input_img.nb_channels - 1);
            maybe_palette.parameters.push_back(palette_colors);
            input_img.do_transform(maybe_palette);
          }
          // all-minus-one-channel palette (RGB with separate alpha, or CMY with separate K)
          if (input_img.nb_channels > 3) {
            Transform maybe_palette_3(TRANSFORM_PALETTE);
            maybe_palette_3.parameters.push_back(0);
            maybe_palette_3.parameters.push_back(input_img.nb_channels - 2);
            maybe_palette_3.parameters.push_back(palette_colors);
            input_img.do_transform(maybe_palette_3);
          }
        }
        if (channel_colors > 0) {
          // single channel palette (like FLIF's ChannelCompact)
          input_img.recompute_minmax();
          for (int i=0; i<input_img.nb_channels; i++) {
            v_printf(10,"Channel %i: range=%i..%i\n",i,input_img.channel[input_img.nb_meta_channels+i].minval,input_img.channel[input_img.nb_meta_channels+i].maxval);
            Transform maybe_palette_1(TRANSFORM_PALETTE);
            maybe_palette_1.parameters.push_back(i);
            maybe_palette_1.parameters.push_back(i);
            // simple heuristic: if less than X percent of the values in the range actually occur, it is probably worth it to do a compaction
            maybe_palette_1.parameters.push_back((int) (channel_colors * (input_img.channel[input_img.nb_meta_channels+i].maxval - input_img.channel[input_img.nb_meta_channels+i].minval + 1)));
            input_img.do_transform(maybe_palette_1);
          }
        }
    } else if (image_type == 2) {
          // make chroma more important, since .yuv is YCbCr which has a smaller chroma range than YCoCg and also it is already subsampled
          squeeze_luma_factor *= 3.0;
          squeeze_quality_factor /= 3.0;
    }


    if (image_type == 1 || image_type == 2 || image_type == 4) {

        if (input_img.nb_frames > 1 && !max_dist_set) options.max_dist = -1; // use the simple heuristic of matching with corresponding pixels from the previous frame
        if (options.max_dist != 0) {
            Transform match(TRANSFORM_2DMATCH);
            match.parameters.push_back(0);
            match.parameters.push_back(input_img.nb_channels-1);
            match.parameters.push_back(0); // no softmatch until we actually use that feature
            match.parameters.push_back(options.max_dist);
            input_img.do_transform(match);
        }

        if (enable_dct) {
            input_img.do_transform(Transform(TRANSFORM_DCT));
            has_dct = true;
        } else if (responsive && input_img.channel[0].w * input_img.channel[0].h > 20) { // no point squeezing tiny images
            input_img.do_transform(Transform(TRANSFORM_SQUEEZE)); // use default squeezing
            if (options.max_group < 0) options.max_group = 1;
        }
    }

    if ( (quality<100 || cquality<100) && image_type != 3) {
        v_printf(2,"Adding quantization constants corresponding to luma quality %.2f and chroma quality %.2f\n",quality,cquality);
        if (!enable_dct && !responsive) {
            v_printf(1,"Warning: lossy compression without either DCT or Squeeze transform is just color quantization.\n");
            quality = (400 + quality)/5;
            cquality = (400 + cquality)/5;
        }
        Transform quantize(TRANSFORM_QUANTIZE);
        float loss = 100-quality;
        for (int i=0; i<input_img.nb_meta_channels; i++)
            quantize.parameters.push_back(1); // don't quantize metachannels

        // convert 'quality' to quantization scaling factor
        if (quality > 50) quality = 200.0 - quality*2.0;
        else quality = 900.0 - quality*16.0;
        if (cquality > 50) cquality = 200.0 - cquality*2.0;
        else cquality = 900.0 - cquality*16.0;
        quality *= 0.01f;
        cquality *= 0.01f;

        if (has_dct) {
          for (int nbi=0; nbi < 64; nbi++) {
            int bi=0;
            for (; bi<64 ; bi++) if (nbi==jpeg_zigzag[bi]) break;
            for (int ci=0; ci < input_img.nb_channels; ci++) {
                int q;
                if (colorspace != 0 && ci > 0 && ci < 3) q = cquality * dct_chroma_qtable[bi];
                else q = quality * dct_luma_qtable[bi];
                if (q<1) q = 1;
                quantize.parameters.push_back(q);
            }
          }
        } else {
          for (int i=input_img.nb_meta_channels; i<input_img.channel.size(); i++) {
            Channel &ch = input_img.channel[i];
            int shift = ch.hcshift + ch.vcshift; // number of pixel halvings
            if (shift > 15) shift = 15;
            int q;
            if (colorspace != 0 && ch.component > 0 && ch.component < 3) q = cquality * squeeze_quality_factor * squeeze_chroma_qtable[shift];
            else q = quality * squeeze_quality_factor * squeeze_luma_factor * squeeze_luma_qtable[shift];
            if (q<1) q = 1;
            quantize.parameters.push_back(q);
          }
        }
        input_img.do_transform(quantize);
    }
    if (approx_k > 0) {
        Transform approximate(TRANSFORM_APPROXIMATE);
        approximate.parameters.push_back(input_img.channel.size()-approx_k);
        approximate.parameters.push_back(input_img.channel.size()-1);
        approximate.parameters.push_back(approx_q);
        input_img.do_transform(approximate);
    }


    if (argc>2) {
        if (image_type != 0) {
            v_printf(1,"Warning: adding an alpha channel is intended to be used with a JPEG input - this is probably broken for other input formats.\n");
        }
        if (input_img.nb_channels > (input_img.colormodel & 16 ? 4 : 3)) {
            e_printf("Error: input image %s already has alpha, yet an explicit alpha image %s is provided.\n",argv[0],argv[1]);
            return 1;
        }
        argv++; argc--;
        Image input_alpha = read_PNG_file(argv[0]);
        if (!input_alpha.w) {
          input_alpha = read_PAM_file(argv[0]);
          if (!input_alpha.w) {
            e_printf("Error: could not read alpha input file %s (expecting PNG input)\n",argv[0]);
            return 1;
          }
        }
        if (input_alpha.nb_channels != 1) {
            v_printf(2,"Warning: alpha image %s is not grayscale (using just the first channel).\n",argv[0]);
        }
        if (input_alpha.channel[0].w != input_img.w || input_alpha.channel[0].h != input_img.h) {
            e_printf("Error: alpha image has dimensions %ix%i while main image has dimensions %ix%i.\n");
            return 1;
        }
        int position = input_img.nb_meta_channels+input_img.nb_channels;
        input_img.channel.insert(input_img.channel.begin()+position, input_alpha.channel[0]);
        input_img.nb_channels++;
        input_img.real_nb_channels++;
        input_img.channel[position].component = 3;
        for (int i=0; i<input_img.transform.size(); i++) {
            // have to adjust the DCT transform so that it only applies to channels 0-2, not to the alpha channel
            if (input_img.transform[i].ID == TRANSFORM_DCT && input_img.transform[i].parameters.size() == 0) {
                input_img.transform[i].parameters.push_back(0);
                input_img.transform[i].parameters.push_back(input_img.nb_channels-2);
            }
        }
        if (channel_colors > 0) {
          // single channel palette (like FLIF's ChannelCompact)
          input_img.recompute_minmax();
          int i = position;
            v_printf(10,"Channel %i: range=%i..%i\n",i,input_img.channel[input_img.nb_meta_channels+i].minval,input_img.channel[input_img.nb_meta_channels+i].maxval);
            Transform maybe_palette_1(TRANSFORM_PALETTE);
            maybe_palette_1.parameters.push_back(i);
            maybe_palette_1.parameters.push_back(i);
            // simple heuristic: if less than X percent of the values in the range actually occur, it is probably worth it to do a compaction
            maybe_palette_1.parameters.push_back((int) (channel_colors * (input_img.channel[input_img.nb_meta_channels+i].maxval - input_img.channel[input_img.nb_meta_channels+i].minval + 1)));
            if (input_img.do_transform(maybe_palette_1)) position++;
        }


        // squeeze alpha to 1:8 so it matches the dimensions of the DC
        Transform squeeze_alpha(TRANSFORM_SQUEEZE);
        for (int k=0; k<3; k++) {
        squeeze_alpha.parameters.push_back(1);
        squeeze_alpha.parameters.push_back(position);
        squeeze_alpha.parameters.push_back(position);
        squeeze_alpha.parameters.push_back(0);
        squeeze_alpha.parameters.push_back(position);
        squeeze_alpha.parameters.push_back(position);
        }
        input_img.do_transform(squeeze_alpha);
    }
    if (responsive && has_dct && image_type != 3) {
        input_img.do_transform(Transform(TRANSFORM_SQUEEZE)); // use default squeezing
    }

    if (options.predictor.size() == 0) {
        // no explicit predictor(s) given, set a good default
        for (int i=0; i<input_img.nb_meta_channels; i++)
            options.predictor.push_back(3);     // left predictor for the meta channels

        for (int i=0; i<input_img.nb_channels; i++)
            options.predictor.push_back(2);     // median predictor for the DC / squeezed channels
        options.predictor.push_back(0);         // zero predictor for the AC / squeeze residues
    }

    fuif_prepare_encode(input_img,options);


    // this is nice to debug/visualize matching (e.g. using -D1000 -R0)
    if (options.debug && options.max_dist != 0) {
    write_PNG_file("debug-before_encode_after_transforms_c0.png",input_img);
    Image baz = input_img;
    baz.channel[0].data.clear();
    baz.channel[0].resize();
    baz.undo_transforms();
    write_PNG_file("debug-before_encode_rest.png",baz);
    }


#ifdef DEBUG
    Image baz = input_img;
    char name[100];
    for (int i=0; i<input_img.channel.size(); i++) {
        sprintf(name,"transformed-component-%i-coef-%03d.pgm",input_img.channel[i].component,i);
        baz.channel.clear();
        baz.channel.push_back(input_img.channel[i]);
        write_PAM_file(name,baz);
    }
#endif
    v_printf(2,"Encoding %s\n", argv[1]);
    fuif_encode_file(argv[1],input_img,options);

    if (options.debug) {
      Image foo = options.heatmap;
      v_printf(2,"Writing heatmaps to heatmap-*.pgm\n");
      char name[100];
      for (int i=0; i<options.heatmap.channel.size(); i++) {
        sprintf(name,"heatmap-%i-channel-%03d.pgm",options.heatmap.channel[i].component,i);
        foo.channel.clear();
        foo.channel.push_back(options.heatmap.channel[i]);
        write_PAM_file(name,foo);
      }
      foo.real_nb_channels = 1;
      v_printf(2,"Writing transformed image to transformed-*.pgm\n");
      for (int i=0; i<input_img.channel.size(); i++) {
        sprintf(name,"transformed-%i-channel-%03d.pgm",input_img.channel[i].component,i);
        foo.channel.clear();
        foo.channel.push_back(input_img.channel[i]);
        write_PAM_file(name,foo);
      }
    }

#ifdef DEBUG
    v_printf(2,"Decoding %s\n", argv[1]);
    Image decoded_img;
    fuif_decode_file(argv[1],decoded_img);
    decoded_img.undo_transforms();
    v_printf(2,"Writing encoded/decoded image to debug.png\n");
    write_PNG_file("debug.png",decoded_img);
#endif


    return 0;

}