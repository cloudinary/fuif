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

// The forward step of this transformation is very slow and brute-force, which is why this transformation is
// currently not enabled by default

typedef std::pair<int,int> offset;
typedef std::vector<offset> offsets;

/*
    Use a single number as an offset to some 2D location in the (scanline order) past.
    This enumeration generalizes the offsets used in lossless WebP.

               36 35 34 33 32 31 30 29 28
               37 16 17 18 19 20 21 22 27
            .. 38 15 10 9  8  7  6  23 26
            42 39 14 11 2  3  4  5  24 25
            41 40 13 12 1  0


            D D D D D D D D D
            D C C C C C C C D
            D C B B B B B C D
            D C B A A A B C D           (A = layer 0, B = layer 1, C = layer 2, D = layer 3)
            D C B A X
*/

void compute_offset(int code, int &xoffset, int &yoffset) {
    int onion_layer = 0;
    int onion_layer_size = 4;
    int ocode=code;
    while (code > onion_layer_size) { code -= onion_layer_size; onion_layer++; onion_layer_size += 4; }
    if (onion_layer & 1) {
        // odd layer: going right to left
        if (code <= onion_layer) {
            xoffset = 1 + onion_layer;  yoffset = -code;   // right vertical bottom-to-top segment
        } else if (code <= 3+3*onion_layer) {
            xoffset = 2 +2*onion_layer - code;  yoffset = -1-onion_layer;   // horizontal right-to-left segment
        } else {
            xoffset = -1 - onion_layer;  yoffset = -4 -4*onion_layer + code;    // left vertical top-to-bottom segment
        }
    } else {
        // even layer: going left to right
        if (code <= 1+onion_layer) {
            xoffset = -1 - onion_layer;  yoffset = 1-code;   // left vertical bottom-to-top segment
        } else if (code <= 4+3*onion_layer) {
            xoffset = -3 -2*onion_layer + code;  yoffset = -1-onion_layer;   // horizontal left-to-right segment
        } else {
            xoffset = 1 + onion_layer;  yoffset = -5 -4*onion_layer + code;    // right vertical top-to-bottom segment
        }
    }
}

// TODO: hardcode some prefix of this table and generate the rest when needed
void make_offsets_table(offsets &t) {
    for (int i=1; i<t.size(); i++) {
        int xoffset, yoffset;
        compute_offset(i,xoffset,yoffset);
        t[i].first = xoffset;
        t[i].second = yoffset;
    }
}


void default_match_parameters(std::vector<int> &parameters, const Image &image) {
    parameters.clear();
    parameters.push_back(0); // first channel
    parameters.push_back(image.nb_channels - 1); // last channel
    parameters.push_back(0);  // 0 = only exact matches, 1 = soft match (difference gets encoded)
    parameters.push_back(1000000); // max distance (positive = search all, negative = look at corresponding positions in previous frames only)
}

bool inv_match(Image &input, std::vector<int> parameters) {
    if (input.nb_meta_channels < 1) {
        e_printf("Error: match transform without match.\n");
        return false;
    }
    if (!parameters.size()) default_match_parameters(parameters,input);
    if (parameters.size() < 3) {
        e_printf("Error: match transform with incorrect parameters.\n");
        return false;
    }
    const Channel &m = input.channel[0];
    int c0 = input.nb_meta_channels + parameters[0];
    int cn = input.nb_meta_channels + parameters[1];
    if (c0 >= input.channel.size() || cn >= input.channel.size()) {
        e_printf("Error: match transform with incorrect parameters.\n");
        return false;
    }
    bool softmatch = parameters[2];
    int w = input.channel[c0].w;
    int h = input.channel[c0].h;


    if (m.q == 1) {
        offsets offsets_table(m.maxval + 1);
        make_offsets_table(offsets_table);
        if (softmatch)
        for (int y=0; y<h; y++) {
          for (int x=0; x<w; x++) {
            pixel_type z = m.value(y,x);
            if (z) {
                offset o = offsets_table[z];
                for (int c=c0; c<=cn; c++)
                    input.channel[c].value(y,x) += input.channel[c].value(y+o.second, x+o.first);
            }
          }
        }
        else
        for (int y=0; y<h; y++) {
          for (int x=0; x<w; x++) {
            pixel_type z = m.value(y,x);
            if (z) {
                offset o = offsets_table[z];
                for (int c=c0; c<=cn; c++)
                    input.channel[c].value(y,x) = input.channel[c].value(y+o.second, x+o.first);
            }
          }
        }
    } else {
        int fh = h/input.nb_frames;
        int offsetcode = 2*fh*fh + (fh&1); // this simple formula corresponds to the code to go exactly one frame up
        if (m.q == offsetcode) {
            // this is the special case where we are matching with corresponding pixels from previous frames; optimize it a bit...
            if (softmatch)
            for (int y=0; y<h; y++) {
              for (int x=0; x<w; x++) {
                pixel_type z = m.value(y,x);
                if (z) {
                    for (int c=c0; c<=cn; c++)
                        input.channel[c].value(y,x) += input.channel[c].value(y-z*fh, x);
                }
              }
            }
            else
            for (int y=0; y<h; y++) {
              for (int x=0; x<w; x++) {
                pixel_type z = m.value(y,x);
                if (z) {
                    for (int c=c0; c<=cn; c++)
                        input.channel[c].value(y,x) = input.channel[c].value(y-z*fh, x);
                }
              }
            }
        } else {
            e_printf("Error: match transform with unexpected quantization factor. Not implemented.\n");
            return false;
        }
    }
    input.nb_meta_channels--;
    input.channel.erase(input.channel.begin(),input.channel.begin()+1);
    return true;
}

void meta_match(Image &input, std::vector<int> &parameters) {
    if (!parameters.size()) default_match_parameters(parameters,input);
    if (parameters.size() < 3) {
        e_printf("Error: match transform with incorrect parameters.\n");
        input.error = true; return;
    }
    int begin_c = input.nb_meta_channels+parameters[0];
    int end_c = input.nb_meta_channels+parameters[1];
    if (begin_c > end_c || end_c >= input.channel.size()) {
        e_printf("Error: match transform with incorrect parameters.\n");
        input.error = true; return;
    }
    input.nb_meta_channels++;
    Channel mch(input.channel[begin_c].w,input.channel[begin_c].h, 0, 1);
    input.channel.insert(input.channel.begin(),mch);
}

bool matches(Image &img, int c0, int cn, int x, int y, int z, offsets &ot, bool newmatch) {
    int x2 = x+ot[z].first;
    int y2 = y+ot[z].second;
    if (y2<0 || x2<0) return false;
    if (x2 >= img.channel[c0].w) return false;
    if (newmatch && img.channel[0].value(y,x)) return false;
    for (int c=c0; c<=cn; c++)
        if (img.channel[c].value(y,x) != img.channel[c].value(y2,x2)) return false;
    return true;
}
void do_match(Image &img, int c0, int cn, int x, int y, int z, offsets &ot) {
    for (int c=c0; c<=cn; c++)
        img.channel[c].value(y,x) -= img.channel[c].value(y+ot[z].second, x+ot[z].first);
}

// very rudimentary heuristic to find 'good' matches (in the sense that the entropy of the match channel is worth the entropy reduction from eliminating the duplication)
pixel_type find_good_match(Image &img, int c0, int cn, int x, int y, offsets &ot) {
    int max_count=100;   // this is the minimal match size (number of nontrivial matching pixels) to consider
    int best_z=0;
    int w = img.channel[c0].w;
    int h = img.channel[c0].h;
    for (int z=1; z<ot.size(); z++) {
            if (!matches(img,c0,cn,x,y,z,ot,true)) continue;
            int count=0;
            int yy=0;
            for (; y+yy<h; yy++) {
                int xl=0;
                for (; x+xl>=0; xl--) {
                    // if it matches                              // and if it's a nontrivial pixel (not equal to its left neighbor)
                    if (matches(img,c0,cn,x+xl,y+yy,z,ot,true)) { if (!matches(img,c0,cn,x+xl,y+yy,1,ot,true)) count++; }       // then count it
                    else break;
                }
                xl++;
                int xr=1;
                for (; x+xr<w; xr++) {
                    if (matches(img,c0,cn,x+xr,y+yy,z,ot,true)) { if (!matches(img,c0,cn,x+xl,y+yy,1,ot,true)) count++; }
                    else break;
                }
                xr--;
                if (!xl && !xr) break;
                if (xr-xl < 8) break; // minimum 8 pixels wide
            }
            if (yy < 7) break; //minimum 8 pixels high
            if (count>max_count) { max_count=count; best_z=z; }
    }
    if (best_z) v_printf(6,"Found good match at %i,%i with offset %i (x+=%i,y+=%i) and count %i\n",x,y,best_z,ot[best_z].first,ot[best_z].second,max_count);
    return best_z;
}

bool fwd_match(Image &input, std::vector<int> &parameters) {
    std::vector<int> adj_params = parameters; // use a copy so empty (default) parameters remain empty
    meta_match(input, adj_params);
    int c0 = input.nb_meta_channels + adj_params[0];
    int cn = input.nb_meta_channels + adj_params[1];
    if (c0 >= input.channel.size() || cn >= input.channel.size()) {
        e_printf("Error: match transform with incorrect parameters.\n");
        return false;
    }
    int maxdist = 10000, erodes = 1;
    if (adj_params.size() > 3) maxdist = adj_params[3];
    if (adj_params[0] == 0 && adj_params[1] == input.nb_channels-1 && adj_params[2] == 0) parameters.clear(); // we can use default parameters
    if (parameters.size() > 3) parameters.pop_back();
    bool softmatch = adj_params[2];
    Channel &m = input.channel[0];
    int w = input.channel[c0].w;
    int h = input.channel[c0].h;

    if (maxdist > 0) {
      v_printf(5,"Running heuristic to find matches (channels %i-%i)\n",c0,cn);
      offsets ot(maxdist + 1);
      make_offsets_table(ot);
      for (int y=0; y<h; y++) {
        if (y%10 == 0) v_printf(7,"Finding matches at row %i\n",y);
        for (int x=0; x<w; x++) {
            if (m.value(y,x)) continue;
            pixel_type z = find_good_match(input,c0,cn,x,y,ot);
            if (!z) continue;
            int count=0;
            for (int yy=0; y+yy<h; yy++) {
                int xl=0;
                for (; x+xl>=0; xl--) {
                    if (matches(input,c0,cn,x+xl,y+yy,z,ot,false)) {
                        if (m.value(y+yy,x+xl) == 0 || m.value(y+yy,x+xl) > z)
                        { m.value(y+yy,x+xl) = z; count++; }
                    }
                    else break;
                }
                xl++;
                int xr=1;
                for (; x+xr<w; xr++) {
                    if (matches(input,c0,cn,x+xr,y+yy,z,ot,false)) {
                        if (m.value(y+yy,x+xr) == 0 || m.value(y+yy,x+xr) > z)
                        { m.value(y+yy,x+xr) = z; count++; }
                    }
                    else break;
                }
                xr--;
                if (!xl && !xr) break;
            }
            v_printf(8,"Matched %i pixels from seed position (%i,%i)\n",count,x,y);
        }
      }
      for (int y=h-1; y>=0; y--) {
        for (int x=w-1; x>=0; x--) {
            if (m.value(y,x)) do_match(input,c0,cn,x,y,m.value(y,x),ot);
        }
      }
    } else {
      if (input.nb_frames < 2) {
        e_printf("This transform is meant for animations only.\n");
        return false;
      }
      v_printf(5,"Running simple heuristic to find matches from previous frames (channels %i-%i)\n",c0,cn);
      int fh = h / input.nb_frames;
      int offsetcode = 2*fh*fh + (fh&1); // this simple formula corresponds to the code to go exactly one frame up
      m.q = offsetcode;
      int minmatchcount = CLAMP(w/50,5,40); // arbitrary threshold

      for (int n=1; n <= -maxdist; n++)
      for (int y=n*fh; y<h; y++) {
        int matchcount=0;  // to reduce entropy in the match channel, only consider it a match if it matches enough pixels in a row
        bool firstmatch=true;
        for (int x=0; x<w; x++) {
            if (m.value(y,x)) continue;
            bool nomatch = false;
            for (int c=c0; c<=cn; c++)
                if (input.channel[c].value(y,x) != input.channel[c].value(y-n*fh,x)) nomatch=true;
            if (nomatch) { matchcount=0; firstmatch=true; continue; }
            matchcount++;
            if (matchcount >= minmatchcount) {
                if (firstmatch) {
                    for (int prev_x = x-matchcount+1; prev_x < x; prev_x++) {
                        m.value(y,prev_x) = n;
                    }
                }
                firstmatch=false;
                m.value(y,x) = n; // quantization multiplies this by offsetcode;
            }
        }
      }

      // vertical-only erode
/*
      for (int y=fh; y<=h; y++) {
        if (y<h) for (int x=0; x<w; x++) {
            if (m.value(y,x) > 0) {
                if (m.value(y-1,x) == 0
                    && (y+1<h && m.value(y+1,x) == 0)
                ) m.value(y,x) = -m.value(y,x); // if top and bottom are zero, flag this one
            }
        }
        for (int x=0; x<w; x++) {
            if (m.value(y-1,x) < 0) m.value(y-1,x) = 0;
        }
      }
*/

     // simple erode, reduces entropy and helps to avoid lossy artifacts accumulating at the edges

    for (int it=0; it<3; it++)
      for (int y=fh; y<=h; y++) {
        if (y<h) for (int x=0; x<w; x++) {
            if (m.value(y,x) > 0) {
                if (m.value(y-1,x) == 0
                    || (x>0 && m.value(y,x-1) == 0)
                    || (y+1<h && m.value(y+1,x) <= 0)
                    || (x+1<w && m.value(y,x+1) <= 0)
                ) m.value(y,x) = -m.value(y,x); // if any neighbor is zero, flag this one
            }
        }
//        for (int x=0; x<w; x++) {
//            if (m.value(y-1,x) < 0) m.value(y-1,x) = 0;
//        }
      }


      for (int y=fh; y<h; y++) {
        for (int x=0; x<w; x++) {
            if (m.value(y,x) > 0)
                for (int c=c0; c<=cn; c++)
                    input.channel[c].value(y,x) = 0;
            else if (m.value(y,x) < 0) m.value(y,x) *= -1;
        }
      }

    }
    return true;
}



bool match(Image &input, bool inverse, std::vector<int> &parameters) {
    if (inverse) return inv_match(input, parameters);
    else return fwd_match(input, parameters);
}

