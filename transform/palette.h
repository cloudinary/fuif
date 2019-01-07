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

// TODO: allow this on metachannels too (useful for 2dmatch)

#pragma once

#include "../image/image.h"
#include <set>

bool inv_palette(Image &input, std::vector<int> parameters) {
    if (input.nb_meta_channels < 1) {
        e_printf("Error: Palette transform without palette.\n");
        return false;
    }
    if (parameters.size() != 3) {
        e_printf("Error: Palette transform with incorrect parameters.\n");
        return false;
    }
    int nb = input.channel[0].h;
    int c0 = input.nb_meta_channels + parameters[0];
    if (c0 >= input.channel.size()) {
        e_printf("Error: Palette transform with incorrect parameters.\n");
        return false;
    }
    int w = input.channel[c0].w;
    int h = input.channel[c0].h;
    // might be false in case of lossy
    //assert(input.channel[c0].minval == 0);
    //assert(input.channel[c0].maxval == palette.w-1);
    for (int i=1; i<nb; i++) {
        input.channel.insert(input.channel.begin()+c0+1, Channel(w,h,0,1));
        input.channel[c0+i].component = parameters[0]+i;
    }
    const Channel &palette = input.channel[0];
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            int index = CLAMP(input.channel[c0].value(y,x),0,palette.w-1);
            for (int c=0; c<nb; c++)
                input.channel[c0+c].value(y,x) = palette.value(c,index);
        }
    }
    input.nb_channels += nb-1;
    input.nb_meta_channels--;
    input.channel.erase(input.channel.begin(),input.channel.begin()+1);
    return true;
}

void meta_palette(Image &input, std::vector<int> parameters) {
    if (parameters.size() != 3) {
        e_printf("Error: Palette transform with incorrect parameters.\n");
        input.error = true; return;
    }
    int begin_c = input.nb_meta_channels+parameters[0];
    int end_c = input.nb_meta_channels+parameters[1];
    if (begin_c > end_c || end_c >= input.channel.size()) {
        e_printf("Error: Palette transform with incorrect parameters.\n");
        input.error = true; return;
    }
    int nb = end_c - begin_c + 1;
    int &nb_colors = parameters[2];
    input.nb_meta_channels++;
    input.nb_channels -= nb-1;
    input.channel.erase(input.channel.begin()+begin_c+1,input.channel.begin()+end_c+1);
    Channel pch(nb_colors,nb, 0, 1);
    pch.hshift = -1;
    input.channel.insert(input.channel.begin(),pch);
}


bool fwd_palette(Image &input, std::vector<int> &parameters) {
    assert(parameters.size() == 3);
    int begin_c = input.nb_meta_channels+parameters[0];
    int end_c = input.nb_meta_channels+parameters[1];
    int &nb_colors = parameters[2];
    int nb = end_c - begin_c + 1;

    int w = input.channel[begin_c].w;
    int h = input.channel[begin_c].h;

    v_printf(8,"Trying to represent channels %i-%i using at most a %i-color palette.\n",begin_c,end_c,nb_colors);
    std::set< std::vector<pixel_type> > candidate_palette;
    std::vector<pixel_type> color(nb);
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            for (int c=0; c<nb; c++) color[c] = input.channel[begin_c+c].value(y,x);
            candidate_palette.insert(color);
            if (candidate_palette.size() > nb_colors) return false; // too many colors
        }
    }
    nb_colors = candidate_palette.size();
    v_printf(6,"Channels %i-%i can be represented using a %i-color palette.\n",begin_c,end_c,nb_colors);

    Channel pch(nb_colors,nb, 0, 1);
    pch.hshift = -1;
    int x=0;
    for (auto pcol : candidate_palette) {
        v_printf(9,"Color %i :  ",x);
        for (int i=0; i<nb; i++) pch.value(i,x) = pcol[i];
        for (int i=0; i<nb; i++) v_printf(9,"%i ", pcol[i]);
        v_printf(9,"\n");
        x++;
    }
    for (int y=0; y<h; y++) {
        for (int x=0; x<w; x++) {
            for (int c=0; c<nb; c++) color[c] = input.channel[begin_c+c].value(y,x);
            int index=0;
            for ( ; index<nb_colors ; index++ ) {
                bool found=true;
                for (int c=0; c<nb; c++) if (color[c] != pch.value(c,index)) {found = false; break;}
                if (found) break;
            }
            input.channel[begin_c].value(y,x) = index;
        }
    }
    input.nb_meta_channels++;
    input.nb_channels -= nb-1;
    input.channel.erase(input.channel.begin()+begin_c+1,input.channel.begin()+end_c+1);
    input.channel.insert(input.channel.begin(),pch);
    return true;
}



bool palette(Image &input, bool inverse, std::vector<int> &parameters) {
    if (inverse) return inv_palette(input, parameters);
    else return fwd_palette(input, parameters);
}

