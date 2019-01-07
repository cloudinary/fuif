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

#include "encoding.h"
#include "context_predict.h"




template <typename IO>
void write_big_endian_varint(IO& io, size_t number, bool done = true) {
    if (number < 128) {
        if (done) io.fputc(number);
        else io.fputc(number + 128);
    } else {
        size_t lsb = (number & 127);
        number >>= 7;
        write_big_endian_varint(io, number, false);
        write_big_endian_varint(io, lsb, done);
    }
}

template <typename IO>
int read_big_endian_varint(IO& io) {
    int result = 0;
    int bytes_read = 0;
    while (bytes_read++ < 10) {
      int number = io.get_c();
      if (number < 0) break;
      if (number < 128) return result+number;
      number -= 128;
      result += number;
      result <<= 7;
    }
    if (!io.isEOF()) e_printf("Invalid number encountered!\n");
    return -1;
}

bool check_bit_depth(pixel_type minv, pixel_type maxv, int predictor) {
    pixel_type maxav = abs(maxv);
    if (-minv > maxav) maxav = -minv;
    if (predictor>0 && maxv-minv > maxav) maxav = maxv-minv;
    if (predictor>0 && abs(minv-maxv) > maxav) maxav = abs(minv-maxv);
    if (maniac::util::ilog2(maxav)+1 > MAX_BIT_DEPTH) {
        e_printf("Erorr: this FUIF is compiled for a maximum bit depth of %i, while %i bits are needed to encode this channel (range=%i..%i, predictor=%i)\n",
                MAX_BIT_DEPTH, maniac::util::ilog2(maxav)+1, minv, maxv, predictor);
        return false;
    }
    return true;
}

template <typename IO, typename Rac, typename Coder, bool learn, bool compress>
bool fuif_encode_channels(IO& io, Tree &tree, fuif_options &options, int predictor, int beginc, int endc, const Image &image, size_t &header_pos) {
  assert(endc >= beginc);
  write_big_endian_varint(io, ((endc-beginc) << 4) + (predictor << 1) + (compress?1:0));
  if (endc>beginc) v_printf(5,"Encoding%s channels %i-%i\n", (compress?"":" uncompressed"), beginc, endc);

  pixel_type global_minv=LARGEST_VAL;
  pixel_type global_maxv=SMALLEST_VAL;
  for (int i=beginc; i<=endc; i++) {
    const Channel &channel = image.channel[i];
    if (channel.w * channel.h <= 0) continue; // zero pixel channel? could happen
    pixel_type minv = channel.minval;
    pixel_type maxv = channel.maxval;
    if (minv<global_minv) global_minv=minv;
    if (maxv>global_maxv) global_maxv=maxv;
  }
  if (global_minv <= 0) {
    write_big_endian_varint(io, 1-global_minv);
  } else {
    write_big_endian_varint(io, 0);
    write_big_endian_varint(io, global_minv);
  }
  write_big_endian_varint(io, global_maxv-global_minv);


  int firstrealc = beginc;
  for (int i=beginc; i<=endc; i++) {
    const Channel &channel = image.channel[i];
    if (channel.w * channel.h <= 0) continue; // zero pixel channel? could happen

    pixel_type minv = channel.minval;
    pixel_type maxv = channel.maxval;

    if (endc > beginc && global_minv < global_maxv) {
      write_big_endian_varint(io, minv-global_minv);
      write_big_endian_varint(io, maxv-minv);
    }
    if (minv == maxv) { firstrealc++; }

    if (!check_bit_depth(minv,maxv,predictor)) return false;

    if (minv == 0 && maxv == 0) continue; // don't need quantization factor if it's all zeroes anyway

    write_big_endian_varint(io, channel.q);

    if (!learn) v_printf(6,"Encoding %ix%i channel %i with range %i..%i (predictor %i, quantization %i), (shift=%i,%i, cshift=%i,%i)\n", channel.w, channel.h, i, minv, maxv, predictor, channel.q, channel.hshift, channel.vshift, channel.hcshift, channel.vcshift);
    else  v_printf(7,"Learning %ix%i channel %i with range %i..%i (predictor %i)\n", channel.w, channel.h, i, minv, maxv, predictor);

    channel.setzero();
  }

  header_pos = io.ftell();

  if (firstrealc > endc) return true; // all trivial


  Ranges propRanges;
  init_properties(propRanges, image, beginc, endc, options);

  int predictability = 2048; // 50%
  {
    const Channel &channel = image.channel[firstrealc];
    if (predictor == 0 && compress) {
        uint64_t zeroes = 0;
        uint64_t pixels = channel.h*channel.w;
        for (int y=0; y<channel.h; y++)
          for (int x=0; x<channel.w; x++)
            if ( channel.value(y,x) == 0) zeroes++;
        int rounded = zeroes * 128 / pixels;
        if (rounded < 1) rounded = 1;
        if (rounded > 127) rounded = 127;
        write_big_endian_varint(io, rounded);
        predictability = rounded * 32;
        v_printf(5,"Found %i zeroes in %i pixels (zero chance=%i/4096)\n",zeroes,pixels,predictability);
    }
  }

  Rac rac(io);

  if (!compress) {
    UniformSymbolCoder<Rac> coder(rac);
    for (int i=beginc; i<=endc; i++) {
        const Channel &channel = image.channel[i];
        for (int y=0; y<channel.h; y++) {
            for (int x=0; x<channel.w; x++) {
                coder.write_int(channel.minval,channel.maxval,channel.value(y,x)); // TODO: use predictor? (only makes sense if the predictor residues have a smaller range)
                if (!learn && options.debug)
                    options.heatmap.channel[i].value(y,x) = (x);
            }
        }
    }
  } else {
    if (!learn) {
        // encode tree here
        MetaPropertySymbolCoder<FUIFBitChanceTree, Rac> metacoder(rac, propRanges);
        metacoder.write_tree(tree);
    }
    Coder coder(rac, propRanges, tree, predictability, CONTEXT_TREE_SPLIT_THRESHOLD, options.maniac_cutoff, options.maniac_alpha);
    Properties properties(propRanges.size());


    // planar
    for (int i=beginc; i<=endc; i++) {
        const Channel &channel = image.channel[i];
        pixel_type minv = channel.minval;
        pixel_type maxv = channel.maxval;
        if (minv==maxv) continue;
        int rowslearned=0;
        Channel references(properties.size() - NB_NONREF_PROPERTIES, channel.w, 0, 0);
        for (int y=0; y<channel.h; y++) {
            if (learn) { if (++rowslearned > options.nb_repeats*channel.h) break; }
            if (learn) y=rand()%channel.h; // try random rows, to avoid giving priority to the top of the image (because then the y property cannot be learned)
            precompute_references(channel, y, image, beginc, options, references);
            for (int x=0; x<channel.w; x++) {
                pixel_type guess;
        //        guess = predict_and_compute_properties_with_reference(properties, channel, x, y, predictor, image, beginc, options);
                guess = predict_and_compute_properties_with_precomputed_reference(properties, channel, x, y, predictor, image, beginc, options, references);
                pixel_type diff = channel.value(y,x)-guess;
                if (!learn && options.debug) {
                    int estimate = coder.estimate_int(properties, minv-guess, maxv-guess, diff);
                    options.heatmap.channel[i].value(y,x) = estimate;
                }
                coder.write_int(properties, minv-guess, maxv-guess, diff);
            }
            if (learn) { y=0; } // set to zero in case random row was channel.h-1
        }
    }
    coder.simplify();
  }

  rac.flush();
  io.flush();
  return true;
}

template <typename IO>
bool corrupt_or_truncated(IO& io, Channel &channel, size_t bytes_to_load) {
    if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) {
        v_printf(3,"Premature end-of-file detected.\n");
        channel.data = std::vector<pixel_type>(channel.w * channel.h, 0);
        return true;
    } else {
        v_printf(3,"Corruption detected.\n");
        return false;
    }
}

// this is just for verbose output
const char * ch_describe(const Image &image, int i) {
    if (i < image.nb_meta_channels) return "Meta";
    int c=image.channel[i].component;
    bool rgb=true, palette=false;
    for (const Transform &t : image.transform) {
        if (t.ID == TRANSFORM_YCoCg) rgb=false;
        if (t.ID == TRANSFORM_YCbCr) rgb=false;
        if (t.ID == TRANSFORM_PALETTE && t.parameters[0] != t.parameters[1]) { palette=true;}
    }
    if (image.real_nb_channels < 3) {
        if (c==0) return "Gray";
        if (c==1) return "Alpha";
    }
    if (palette && c==0) return "Palette";
    if ((image.colormodel & 48) == 0) {
        if (rgb && c==0) return "Red";
        if (rgb && c==1) return "Green";
        if (rgb && c==2) return "Blue";
    } else {
        if (rgb && c==0) return "Cyan";
        if (rgb && c==1) return "Magenta";
        if (rgb && c==2) return "Yellow";
        if (c==3) return "Key";
        if (c==4) return "Alpha";
        if (c==5) return "Depth";
    }
    if (c==0) return "Luma";
    if (c==1) return "Chroma1";
    if (c==2) return "Chroma2";
    if (c==3) return "Alpha";
    if (c==4) return "Depth";
    if (c==5) return "Unknown_5";
    if (c==6) return "Unknown_6";
    return "Unknown";
}


template <typename IO, typename Coder>
bool fuif_decode_channel(IO& io, fuif_options &options, int &beginc, Image &image, size_t bytes_to_load) {

  long unsigned filepos = io.ftell();
  if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) return true;
  int firstbyte = read_big_endian_varint(io);
  if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) return true;
  int endc = beginc + (firstbyte >> 4);
  bool compress = (firstbyte & 1);
  int predictor = (firstbyte & 14)>>1;
  pixel_type global_minv = 1-read_big_endian_varint(io);
  if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) return true;
  if (global_minv == 1) global_minv = read_big_endian_varint(io);
  if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) return true;
  pixel_type global_maxv = global_minv + read_big_endian_varint(io);
  if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) return true;
  if (endc>beginc) v_printf(3,"[File position %lu] Decoding%s channels %i-%i with range %i..%i\n", filepos, (compress?"":" uncompressed"), beginc, endc, global_minv, global_maxv);
  if (endc >= image.channel.size() || endc < beginc) {
    e_printf("Wrong channel range: %i-%i while we have only %i channels.\n",beginc,endc,image.channel.size());
    return false;
  }

  int firstrealc = beginc;
  for (int i=beginc; i<=endc; i++) {
    Channel &channel = image.channel[i];

    if (channel.w * channel.h <= 0) continue; //return true; // zero pixel channel? could happen

    channel.minval = global_minv;
    channel.maxval = global_maxv;
    if (endc>beginc && global_minv<global_maxv) {
        channel.minval += read_big_endian_varint(io);
        channel.maxval = channel.minval + read_big_endian_varint(io);
    }
    if (channel.minval == channel.maxval) {
        channel.data = std::vector<pixel_type>(channel.w * channel.h, channel.minval);
        v_printf(3,"[File position %lu] Decoding channel %i: %ix%i %s, constant %i\n", filepos, i, channel.w, channel.h, ch_describe(image,i), channel.minval);
        firstrealc++;
    }

    if (channel.minval == 0 && channel.maxval == 0) continue; // don't need quantization factor if it's all zeroes anyway

    channel.q = read_big_endian_varint(io);

    if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) return corrupt_or_truncated(io,channel,bytes_to_load);

    v_printf(3,"[File position %lu] Decoding%s channel %i: %ix%i %s with range %i..%i (qf=%i), predictor %i", filepos, (compress?"":" uncompressed"), i, channel.w, channel.h, ch_describe(image,i), channel.minval, channel.maxval, channel.q, predictor);
    v_printf(6," (shift=%i,%i, cshift=%i,%i)", channel.hshift, channel.vshift, channel.hcshift, channel.vcshift);
    v_printf(3,"\n");

    if (compress && !check_bit_depth(channel.minval,channel.maxval,predictor)) return false;
  }

  if (firstrealc > endc) { beginc=endc; return true; } // all trivial

  Ranges propRanges;
  init_properties(propRanges, image, beginc, endc, options);

  int predictability = 2048;

  if (predictor == 0 && compress) {
        Channel &channel = image.channel[firstrealc];
        uint64_t pixels = channel.h*channel.w;
        int rounded = read_big_endian_varint(io);
        if (rounded < 1 || rounded > 127) {
            return corrupt_or_truncated(io, channel, bytes_to_load);
        }
        predictability = rounded * 32;
        uint64_t zeroes = pixels*predictability/4096;
        v_printf(5,"Estimating %i zeroes in %i pixels (zero chance=%i/4096)\n",zeroes,pixels,predictability);
  }

  // decode trees
  RacIn<IO> rac(io);

  if (!compress) {
    UniformSymbolCoder<RacIn<IO>> coder(rac);
    for (int i=beginc; i<=endc; i++) {
        Channel &channel = image.channel[i];
        if (channel.minval==channel.maxval) continue;
        channel.setzero();
        channel.resize(channel.w, channel.h);
        for (int y=0; y<channel.h; y++) {
          if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) {
             v_printf(3,"Premature end-of-file at row %i.\n",y);
             break;
          }
          for (int x=0; x<channel.w; x++) {
             channel.value(y,x) = coder.read_int(channel.minval,channel.maxval-channel.minval);
          }
        }
        if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) break;
    }
    beginc = endc;
    return true;
  }

  Tree tree;
  MetaPropertySymbolCoder<FUIFBitChanceTree, RacIn<IO>> metacoder(rac, propRanges);
  if (!metacoder.read_tree(tree)) return corrupt_or_truncated(io, image.channel[beginc], bytes_to_load);

  Coder coder(rac, propRanges, tree, predictability, CONTEXT_TREE_SPLIT_THRESHOLD, options.maniac_cutoff, options.maniac_alpha);
  Properties properties(propRanges.size());


  // planar
  for (int i=beginc; i<=endc; i++) {
    Channel &channel = image.channel[i];
    if (channel.minval==channel.maxval) continue;
    channel.setzero();
    channel.resize(channel.w, channel.h);

    if (tree.size() == 1 && predictor == 0 && channel.zero == 0) {
      // special optimized case: no meta-adaptation, no predictor, so no need to compute properties
      v_printf(5,"Fast track.\n");
      for (int y=0; y<channel.h; y++) {
        if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) {
         v_printf(3,"Premature end-of-file at row %i of channel %i.\n",y,i);
         beginc = i;
         break;
        }
       for (int x=0; x<channel.w; x++) {
        channel.value(y,x) = coder.read_int(properties, channel.minval, channel.maxval);
       }
      }
    } else {

      v_printf(5,"Slow track.\n");

    Channel references(properties.size() - NB_NONREF_PROPERTIES, channel.w, 0, 0);
    for (int y=0; y<channel.h; y++) {
      if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) {
         v_printf(3,"Premature end-of-file at row %i of channel %i.\n",y,i);
         beginc = i;
         break;
      }
      precompute_references(channel, y, image, beginc, options, references);
      if (y <= 1 || predictor) {
       for (int x=0; x<channel.w; x++) {
        pixel_type guess;
        guess = predict_and_compute_properties_with_precomputed_reference(properties, channel, x, y, predictor, image, beginc, options, references);
        pixel_type diff = coder.read_int(properties, channel.minval-guess, channel.maxval-guess);
        channel.value(y,x) = diff + guess;
       }
      } else {
       int x=0;
       pixel_type guess, diff;
       for (; x<channel.w && x<2; x++) {
        guess = predict_and_compute_properties_with_precomputed_reference(properties, channel, x, y, predictor, image, beginc, options, references);
        diff = coder.read_int(properties, channel.minval-guess, channel.maxval-guess);
        channel.value(y,x) = diff + guess;
       }
       for (; x<channel.w-1; x++) {
        guess = predict_and_compute_properties_with_precomputed_reference_no_edge_case(properties, channel, x, y, references);
        diff = coder.read_int(properties, channel.minval-guess, channel.maxval-guess);
        channel.value(y,x) = diff + guess;
       }
       for (; x<channel.w; x++) {
        guess = predict_and_compute_properties_with_precomputed_reference(properties, channel, x, y, predictor, image, beginc, options, references);
        diff = coder.read_int(properties, channel.minval-guess, channel.maxval-guess);
        channel.value(y,x) = diff + guess;
       }
      }
    }
    }
    if (io.isEOF() || (bytes_to_load && io.ftell() >= bytes_to_load)) break;
  }

  beginc = endc;
  return true;
}
/*
int find_best_predictor(const Channel &channel) {
    Properties properties; // empty, not used here
    channel.setzero();
    int64_t best = -1;
    int best_predictor = 0;
    for (int predictor=0; predictor < NB_PREDICTORS; predictor++) {
        int64_t total_diff = 0;
        for (int y=0; y<channel.h; y++) {
          for (int x=0; x<channel.w; x++) {
            pixel_type guess = predict_and_compute_properties(properties, channel, x, y, predictor); // broken now
            total_diff += abs(channel.value(y,x) - guess);
          }
        }
        if (total_diff < best || best < 0) { best_predictor = predictor; best = total_diff; }
        v_printf(8,"Avg diff for predictor %i: %f  (total: %lli)\n",predictor, total_diff*1.0d/(channel.w*channel.h),total_diff);
    }
    v_printf(6,"Best predictor is %i: avg diff of %f\n", best_predictor, best*1.0d/(channel.w*channel.h));
    return best_predictor;
}
*/

const int responsive_sizes[5] = {0, 16, 8, 4, 2};


template <typename IO>
bool fuif_encode(IO& realio, const Image &image, fuif_options &options) {
    if (image.error) return false;
    if (image.nb_frames < 2) realio.fputs("FUIF");  // bytes 1-4 are fixed magic
    else realio.fputs("FUAF");                      // animation has different magic
    int nb_channels = image.real_nb_channels;
    write_big_endian_varint(realio, nb_channels + '0');
    int bit_depth = 1, maxval = 1;
    while (maxval < image.maxval) { bit_depth++; maxval = maxval*2 + 1; }
    write_big_endian_varint(realio, bit_depth + '&');
    write_big_endian_varint(realio, image.w-1);
    write_big_endian_varint(realio, image.h-1);
    if (image.nb_frames > 1) {
        write_big_endian_varint(realio, image.nb_frames-2);
        write_big_endian_varint(realio, image.den-1);
        if (image.num.size() == 0) write_big_endian_varint(realio, 0);
        else for (int i=0; i<image.num.size(); i++) write_big_endian_varint(realio, image.num[i]);
        write_big_endian_varint(realio, image.loops);
    }
    write_big_endian_varint(realio, image.colormodel);

    write_big_endian_varint(realio, options.max_properties);

    v_printf(2,"Encoding %i-channel, %i-bit, %ix%i %s%s image.\n", nb_channels, bit_depth, image.w, image.h, colormodel_name(image.colormodel,nb_channels), colorprofile_name(image.colormodel));

    BlobIO io;

    if (nb_channels < 1) return true; // is there any use for a zero-channel image?


    // encode transforms
    int nb_transforms = image.transform.size();
    write_big_endian_varint(io,nb_transforms);
    for (int i=0; i<nb_transforms; i++) {
        int id = image.transform[i].ID;
        int nb_params = (image.transform[i].has_parameters() ? image.transform[i].parameters.size() : 0);
        write_big_endian_varint(io, (nb_params << 4) + id);
        for (int j=0; j<nb_params; j++) write_big_endian_varint(io, image.transform[i].parameters[j]);
    }

    nb_channels = image.channel.size();

    int responsive_offsets[5] = {-1, -1, -1, -1, -1}; //   LQIP, 1/16 1/8 1/4 1/2

    // encode channel data
    for (int i=0; i<nb_channels; i++) {
        if (! image.channel[i].w || ! image.channel[i].h ) continue; // skip empty channels
        int predictor = 0;
        if (options.predictor.size() > i) predictor = options.predictor[i]; else if (options.predictor.size() > 0) predictor = options.predictor.back(); else predictor = 0;
//        if (predictor < 0) predictor = find_best_predictor(image.channel[i]);

        int j=i;
        Tree tree;
        size_t header_pos;

        if (!options.compress) {
            if (!fuif_encode_channels<BlobIO, RacOut<BlobIO>, FinalPropertySymbolCoder<FUIFBitChancePass2, RacOut<BlobIO>, MAX_BIT_DEPTH>, false, false >(io, tree, options, predictor, i, j, image, header_pos)) return false;
            continue;
        }

        // do several channels at a time (keep going until we hit a new downscale truncation point)
        for (int s=1; s<5; s++) if (j > image.downscales[s] && j < image.downscales[s+1]) j = image.downscales[s+1];

        // needed for interleaved (which we don't use), and maybe a good idea in any case: only clump same-dimension channels
        for (int k=i+1; k<=j; k++) if (image.channel[i].w != image.channel[k].w || image.channel[i].h != image.channel[k].h) { j=k-1; break; }

        if (options.max_group > 0 && j > i + options.max_group - 1) j = i + options.max_group - 1;

        DummyIO dummyio;
        if (!fuif_encode_channels<DummyIO, RacDummy<DummyIO>, PropertySymbolCoder<FUIFBitChancePass1, RacDummy<DummyIO>, MAX_BIT_DEPTH>, true, true >(dummyio, tree, options, predictor, i, j, image, header_pos)) return false;
        size_t before=io.ftell();
        if (!fuif_encode_channels<BlobIO, RacOut<BlobIO>, FinalPropertySymbolCoder<FUIFBitChancePass2, RacOut<BlobIO>, MAX_BIT_DEPTH>, false, true >(io, tree, options, predictor, i, j, image, header_pos)) return false;
        size_t after=io.ftell();
        float bits = (after-header_pos)*8.0;
        float pixels = 0.0;
        float ubits = 0.0;
        for (int k=i; k<=j; k++) {
            float chpixels = image.channel[k].w*image.channel[k].h;
            float uncompressed_bpp = maniac::util::ilog2(image.channel[k].maxval-image.channel[k].minval)+1;
            pixels += chpixels;
            if (image.channel[k].maxval > image.channel[k].minval)
                ubits += chpixels*uncompressed_bpp;
        }
        if (ubits > 0.0) ubits += 16; // rac flush might add 2 bytes
        float bpp=bits/pixels;
        float ubpp=ubits/pixels;

        v_printf(4,"Encoded channel %i-%i (%ix%i %s, range %i..%i), %i+%i bytes [%i-%i] (%f bpp; uncompressed estimate: %f bpp; %.2f%% reduction)\n", i,j, image.channel[i].w, image.channel[i].h, ch_describe(image,i), image.channel[i].minval, image.channel[i].maxval, after-header_pos, header_pos-before, before,after,
                bpp, ubpp, 100.0-100.0*bpp/ubpp);

        if ( bits >= ubits ) {
            io.fseek(before,SEEK_SET);
            if (!fuif_encode_channels<BlobIO, RacOut<BlobIO>, FinalPropertySymbolCoder<FUIFBitChancePass2, RacOut<BlobIO>, MAX_BIT_DEPTH>, false, false >(io, tree, options, predictor, i, j, image, header_pos)) return false;
            after=io.ftell();
            float bpp=(after-header_pos)*8.0/pixels;
            v_printf(4,"Rolled back. Encoded channel %i-%i UNCOMPRESSED (%ix%i %s, range %i..%i), %i+%i bytes (%f bpp)\n", i, j, image.channel[i].w, image.channel[i].h, ch_describe(image,i), image.channel[i].minval, image.channel[i].maxval, after-header_pos, header_pos-before, bpp);
        }
        for (int s=0; s<5; s++) {
            if (image.downscales[s] >= i && image.downscales[s] <= j) responsive_offsets[s] = after;
        }
        i=j;
    }

    int relative_offset = 0;
    for (int s=0; s<5; s++) {
        v_printf(3,"Responsive truncation point for size 1/%i after channel %i, at post-header position %i\n",responsive_sizes[s],image.downscales[s],responsive_offsets[s]);
        if (responsive_offsets[s] < 0) responsive_offsets[s] = io.ftell();
        int offset = responsive_offsets[s] - relative_offset;
        write_big_endian_varint(realio, (offset+TRUNCATION_OFFSET_RESOLUTION-1)/TRUNCATION_OFFSET_RESOLUTION);
        relative_offset = responsive_offsets[s];
    }

    if (options.debug) options.heatmap.recompute_minmax();

    io.fseek(0,SEEK_SET);
    int c;
    while ( (c=io.get_c()) != io.EOS ) realio.fputc(c); // TODO: copy the memory buffer more efficiently
    return true;
}

// Permutation is a special transform since it has to be done on the channel metadata as soon as possible if it has no parameters
void inv_permute_meta(Image &input) {
    v_printf(5,"Permutation (Meta): ");
    std::vector<Channel> inchannel = input.channel;
    for (int i=0; i<input.channel[0].w; i++) {
        int c = input.channel[0].value(0,i);
        if (c < 0 || c >= input.channel[0].w) {
            e_printf("Invalid permutation: %i is not a channel number\n",c);
            input.error=true;
            return;
        }
        for (int j=0; j<i; j++) if (input.channel[0].value(0,i) == input.channel[0].value(0,j)) {
            e_printf("Invalid permutation: both %i and %i map from channel number %i\n",i,j,input.channel[0].value(0,i));
            input.error=true;
            return;
        }
        input.channel[input.nb_meta_channels+c] = inchannel[input.nb_meta_channels+i];
        v_printf(5,"[%i -> %i] ",i,c);
    }
    v_printf(5,"\n");
    return;
}


template<typename IO>
bool fuif_decode(IO& io, Image &image, fuif_options options) {
    char buff[5];
    if (!io.gets(buff,5)) { e_printf("Could not read header from file: %s\n",io.getName()); return false; }
    bool multi_frame = false;
    if (!strcmp(buff,"FUAF")) { multi_frame = true; }
    else if (strcmp(buff,"FUIF")) { e_printf("%s is not a FUIF file\n",io.getName()); return false; }
    int nb_channels = read_big_endian_varint(io) - '0';
    int bit_depth = read_big_endian_varint(io) - '&';
    int w = read_big_endian_varint(io)+1;
    int h = read_big_endian_varint(io)+1;
    int nb_frames = 1;
    int loops = 0;
    int den = 0;
    std::vector<int> num;
    if (multi_frame) {
        nb_frames = read_big_endian_varint(io)+2;
        den = read_big_endian_varint(io)+1;
        int numerator = read_big_endian_varint(io);
        if (numerator) {
            num.push_back(numerator);
            for (int i=1; i<nb_frames; i++) num.push_back(read_big_endian_varint(io));
        }
        loops = read_big_endian_varint(io);
    }
    int colormodel = read_big_endian_varint(io);
    if (options.identify) {
        v_printf(1,"%s: %i-channel, %i-bit, ", io.getName(), nb_channels, bit_depth);
        if (multi_frame) v_printf(1,"%ix%i %s%s animation (%i frames)\n", w, h/nb_frames, colormodel_name(colormodel,nb_channels), colorprofile_name(colormodel), nb_frames);
        else v_printf(1,"%ix%i %s%s image\n", w, h, colormodel_name(colormodel,nb_channels), colorprofile_name(colormodel));
    }
    options.max_properties = read_big_endian_varint(io);

    v_printf(4,"Global option: up to %i back-referencing MANIAC properties.\n", options.max_properties);

    v_printf(7,"First part of header decoded (basic info). Read %i bytes so far.\n",io.ftell());

    if (!options.identify) {
        image = Image(w, h, (1<<bit_depth)-1, nb_channels, colormodel);
        image.nb_frames = nb_frames;
        image.den = den;
        image.num = num;
        image.loops = loops;
    }

    if (nb_channels < 1) return true; // is there any use for a zero-channel image?

    int responsive_offsets[5];
    int relative_offset = 0;
    for (int s=0; s<5; s++) {
        responsive_offsets[s] = read_big_endian_varint(io)*TRUNCATION_OFFSET_RESOLUTION + relative_offset;
        relative_offset = responsive_offsets[s];
    }
    relative_offset = io.ftell();
    for (int s=0; s<5; s++) {
        responsive_offsets[s] += relative_offset;
        if (s) v_printf(3,"Responsive truncation point for size 1/%i at position %i\n",responsive_sizes[s],responsive_offsets[s]);
        else v_printf(3,"Responsive truncation point for LQIP at position %i\n",responsive_offsets[s]);
    }



    if (!options.identify) {
    if (multi_frame) v_printf(2,"Decoding %i-channel, %i-bit, %ix%i %s%s animation (%i frames)", nb_channels, bit_depth, w, h/nb_frames, colormodel_name(colormodel,nb_channels), colorprofile_name(colormodel), nb_frames);
    else v_printf(2,"Decoding %i-channel, %i-bit, %ix%i %s%s image", nb_channels, bit_depth, w, h, colormodel_name(colormodel,nb_channels), colorprofile_name(colormodel));
    if (options.preview == 0) v_printf(2, " as a low-quality image placeholder");
    else if (options.preview > 0) v_printf(2," at scale 1:%i",responsive_sizes[options.preview]);
    v_printf(2,".\n");
    }


    v_printf(7,"Second part of header decoded (responsive offsets and global parameters; before transforms). Read %i bytes so far.\n",io.ftell());

    // decode transforms
    int nb_transforms = read_big_endian_varint(io);
    v_printf(2,"Image data underwent %i transformations: ",nb_transforms);
    for (int i=0; i<nb_transforms; i++) {
        int id_and_nb_params = read_big_endian_varint(io);
        Transform t(id_and_nb_params & 0xf);
        if (t.has_parameters()) {
            int nb_params = (id_and_nb_params >> 4);
            for (int j=0; j<nb_params; j++) t.parameters.push_back(read_big_endian_varint(io));
        }
        if (!options.identify) {
            t.meta_apply(image);
            image.transform.push_back(t);
        }
        if (i) v_printf(2,", ");
        v_printf(2,"%s",t.name());
        if (t.ID == TRANSFORM_PALETTE) {
            if (t.parameters[0] == t.parameters[1]) v_printf(3,"[Compact channel %i to ",t.parameters[0]);
            else v_printf(3,"[channels %i-%i with ",t.parameters[0],t.parameters[1]);
            v_printf(3,"%i colors]",t.parameters[2]);
        }
    }
    v_printf(2,"\n");
    v_printf(6,"Header decoded. Read %i bytes so far.\n",io.ftell());
    if (options.identify) return true;
    if (image.error) {
        e_printf("Corrupt file. Aborting.\n");
        return false;
    }

    nb_channels = image.channel.size();

    size_t bytes_to_load = 0;
    if (options.preview >= 0) bytes_to_load = responsive_offsets[options.preview];

    // decode channel data
    for (int i=0; i<nb_channels; i++) {
        if ((options.preview < 0 || io.ftell() < bytes_to_load) && !io.isEOF()) {
            if (! image.channel[i].w || ! image.channel[i].h ) continue; // skip empty channels
            if (!fuif_decode_channel<IO, FinalPropertySymbolCoder<FUIFBitChancePass2, RacIn<IO>, MAX_BIT_DEPTH> >(io, options, i, image, bytes_to_load)) return false;
            if (image.transform.size() > 0 && image.transform.back().ID == TRANSFORM_PERMUTE && image.transform.back().parameters.size() == 0 && i==0) inv_permute_meta(image);
        } else {
            v_printf(3,"Skipping decode of channels %i-%i.\n",i,nb_channels-1);
            break;
        }
    }
    v_printf(3,"Done decoding. Read %i bytes.\n",io.ftell());
    return true;
}

template bool fuif_encode(FileIO& io, const Image &image, fuif_options &options);
template bool fuif_encode(BlobIO& io, const Image &image, fuif_options &options);
template bool fuif_decode(FileIO& io, Image &image, fuif_options options);
template bool fuif_decode(BlobReader& io, Image &image, fuif_options options);

bool fuif_encode_file(const char * filename, const Image &image, fuif_options &options) {
    FILE *file = NULL;
    if (!strcmp(filename,"-")) file = stdout;
    else file = fopen(filename,"wb");
    if (!file) return false;
    FileIO fio(file, (file == stdout? "to standard output" : filename));
    bool result = fuif_encode(fio, image, options);
    return result;
}

void fuif_prepare_encode(Image &image, fuif_options &options) {
    // ensure that the ranges are correct and tight
    image.recompute_minmax();
    // inspect the cumulative channel hshift/vshift to find the right spots to put the truncation offsets
    image.recompute_downscales();
    if (options.debug) {options.heatmap.channel = image.channel; options.heatmap.w = image.w; options.heatmap.h = image.h;}
}

bool fuif_decode_file(const char * filename, Image &image, fuif_options options) {
    FILE *file = NULL;
    if (!strcmp(filename,"-")) file = stdin;
    else file = fopen(filename,"rb");
    if(!file) return false;
    FileIO fio(file, (file==stdin ? "from standard input" : filename));
    bool result = fuif_decode(fio, image, options);
    return result;
}
