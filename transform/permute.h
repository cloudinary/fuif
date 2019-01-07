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
#include "../io.h"


bool inv_permute(Image &input, const std::vector<int> &parameters) {
    Image tmp=input;
    bool use_channel = (parameters.size()==0);
    int perm_length;
    if (use_channel) perm_length = input.channel[0].w;
    else perm_length = parameters.size();

    v_printf(5,"Permutation: ");
    for (int i=0; i<perm_length; i++) {
        int c;
        if (use_channel) c = input.channel[0].value(0,i);
        else c = parameters[i];
        // permutation sanity is already checked in meta-step, so no need to double check
        input.channel[input.nb_meta_channels+i] = tmp.channel[tmp.nb_meta_channels+c];
        v_printf(5,"[%i <- %i] ",i,c);
    }
    if (use_channel) {
     input.nb_meta_channels--;
     input.channel.erase(input.channel.begin(),input.channel.begin()+1);
     assert(input.channel[0].w == input.channel.size() - input.nb_meta_channels);
    }
    v_printf(5,"\n");
    return true;
}

void meta_permute(Image &input, std::vector<int> &parameters, bool use_channel=false) {
    int nb = input.channel.size() - input.nb_meta_channels;
    if (parameters.size() == 0 || use_channel) {
      input.nb_meta_channels++;
      Channel pch(nb, 1, 0, nb-1);
      pch.hshift = -1;
      input.channel.insert(input.channel.begin(),pch);
    } else if (parameters.size() <= nb) {
      std::vector<Channel> inchannel = input.channel;
      for (int i=0; i<parameters.size(); i++) {
        int c = parameters[i];
        if (c < 0 || c >= parameters.size()) {
            e_printf("Invalid permutation: channel %i is lost\n",c);
            input.error=true;
            return;
        }
        for (int j=0; j<i; j++) if (parameters[i] == parameters[j]) {
            e_printf("Invalid permutation: both %i and %i map from channel number %i\n",i,j,parameters[i]);
            input.error=true;
            return;
        }
        input.channel[input.nb_meta_channels+c] = inchannel[input.nb_meta_channels+i];
        v_printf(5,"[%i -> %i] ",i,c);
      }
    } else {
        e_printf("Incorrect number of parameters in Permute transform.\n");
        input.error=true;
    }
}

bool fwd_permute(Image &input, std::vector<int> &parameters) {
    // TODO: implement this without copying stuff
    Image tmp = input;
    if (parameters.size() < 3) {
        e_printf("Invalid permutation: not enough parameters\n");
        return false;
    }
    bool use_channel = true;
    if (parameters[0] == -1) {
        parameters.erase(parameters.begin(), parameters.begin()+1);
        use_channel = false;
    }
    meta_permute(input, parameters, use_channel);
    if (!use_channel) return true;

    assert(input.channel[0].w == input.channel.size() - input.nb_meta_channels);
    if (parameters.size() != input.channel[0].w) {
        e_printf("Invalid permutation: need to specify %i new channels, gave only %i\n",input.channel[0].w,parameters.size());
        return false;
    }
    v_printf(5,"Permutation: ");
    for (int i=0; i<input.channel[0].w; i++) {
        input.channel[0].value(0,i) = parameters[i];
        int c = input.channel[0].value(0,i);
        if (c < 0 || c >= input.channel[0].w) {
            e_printf("Invalid permutation: %i is not a channel number\n",c);
            return false;
        }
        for (int j=0; j<i; j++) if (input.channel[0].value(0,i) == input.channel[0].value(0,j)) {
            e_printf("Invalid permutation: both %i and %i map to channel number %i\n",i,j,input.channel[0].value(0,i));
            return false;
        }
        input.channel[input.nb_meta_channels+c] = tmp.channel[tmp.nb_meta_channels+i];
        v_printf(5,"[%i -> %i] ",i,c);
    }
    v_printf(5,"\n");
    return true;
}

bool permute(Image &input, bool inverse, std::vector<int> &parameters) {
    if (inverse) return inv_permute(input, parameters);
    else return fwd_permute(input, parameters);
}
