
#pragma once

#include "../image/image.h"
#include "../io.h"
#include "../config.h"

#define SRGB_TO_LINEAR(c)  ((c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)))
#define LINEAR_TO_SRGB(c)  ((c < 0.0031308 ? 12.92 * c : 1.055 * pow(c,1.0/2.4) - 0.055))
#define X_PRECISION 50.0
#define Y_PRECISION 2.0
#define B_PRECISION 2.0

static const float kOpsinAbsorbanceMatrix[9] = {
  0.29758629764381878,  0.63479886329551205,  0.088129079251512379,
  0.22671198744507498,  0.6936230820580469,  0.098933489737625696,
  0.19161912544122028,  0.082898111024512638,  0.53811869403330603
};

static const float kOpsinAbsorbanceBias[3] = {  0.22909219828963151,  0.22754880685562834,  0.1426315625332778 };

static const float kOpsinAbsorbanceInvMatrix[9] = {
  11.006452774923291,      -10.079060487634042,     0.050487148542225385,
  -3.106722218699964,      4.319048293702604,       -0.2852641120243108,
  -3.4407008566370068,     2.923704060800807,       1.8842934914233993
};


bool inv_XYB(Image &input) ATTRIBUTE_HOT;
bool inv_XYB(Image &input) {
    int nb_channels = input.channel.size();
    if (nb_channels < 3) {
        e_printf("Invalid number of channels to apply inverse XYB.\n");
        return false;
    }
    int w = input.channel[0].w;
    int h = input.channel[0].h;
    if (input.channel[1].w < w
     || input.channel[1].h < h
     || input.channel[2].w < w
     || input.channel[2].h < h) {
        e_printf("Invalid channel dimensions to apply inverse XYB (maybe chroma is subsampled?).\n");
        return false;
    }

    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x++) {
        float X = input.channel[1].value(y,x);
        float Y = input.channel[0].value(y,x);
        float B = input.channel[2].value(y,x);


        X /= input.maxval * X_PRECISION;
        Y /= input.maxval * Y_PRECISION;
        B /= input.maxval * B_PRECISION;

        float L = Y+X;
        float M = Y-X;
        float S = B;

        L = L*L*L;
        M = M*M*M;
        S = S*S*S;

        const float* bias = &kOpsinAbsorbanceBias[0];
        L -= bias[0];
        M -= bias[1];
        S -= bias[2];

        const float* mix = &kOpsinAbsorbanceInvMatrix[0];
        float r = mix[0] * L + mix[1] * M + mix[2] * S;
        float g = mix[3] * L + mix[4] * M + mix[5] * S;
        float b = mix[6] * L + mix[7] * M + mix[8] * S;

        r = LINEAR_TO_SRGB(r);
        g = LINEAR_TO_SRGB(g);
        b = LINEAR_TO_SRGB(b);

        r *= input.maxval;
        g *= input.maxval;
        b *= input.maxval;

        input.channel[0].value(y,x) = CLAMP(r + 0.5, input.minval, input.maxval);
        input.channel[1].value(y,x) = CLAMP(g + 0.5, input.minval, input.maxval);
        input.channel[2].value(y,x) = CLAMP(b + 0.5, input.minval, input.maxval);
      }
    }

    return true;
}



bool fwd_XYB(Image &input) {
    int nb_channels = input.channel.size();
    if (nb_channels < 3) {
        e_printf("Invalid number of channels to apply XYB.\n");
        return false;
    }
    int w = input.channel[0].w;
    int h = input.channel[0].h;
    if (input.channel[1].w < w
     || input.channel[1].h < h
     || input.channel[2].w < w
     || input.channel[2].h < h) {
        e_printf("Invalid channel dimensions to apply XYB.\n");
        return false;
    }

    for (int y=0; y<h; y++) {
      for (int x=0; x<w; x++) {
        float r = input.channel[0].value(y,x);
        float g = input.channel[1].value(y,x);
        float b = input.channel[2].value(y,x);

        r /= input.maxval;
        g /= input.maxval;
        b /= input.maxval;

        r = SRGB_TO_LINEAR(r);
        g = SRGB_TO_LINEAR(g);
        b = SRGB_TO_LINEAR(b);

        const float* mix = &kOpsinAbsorbanceMatrix[0];
        const float* bias = &kOpsinAbsorbanceBias[0];
        float L = mix[0] * r + mix[1] * g + mix[2] * b + bias[0];
        float M = mix[3] * r + mix[4] * g + mix[5] * b + bias[1];
        float S = mix[6] * r + mix[7] * g + mix[8] * b + bias[2];

        L = pow(L, 0.333333333333333f);
        M = pow(M, 0.333333333333333f);
        S = pow(S, 0.333333333333333f);

        float X = (L - M) * 0.5f;
        float Y = (L + M) * 0.5f;
        float B = S;

        X *= input.maxval * X_PRECISION;
        Y *= input.maxval * Y_PRECISION;
        B *= input.maxval * B_PRECISION;

        input.channel[0].value(y,x) = Y + 0.5;
        input.channel[1].value(y,x) = X;
        input.channel[2].value(y,x) = B + 0.5;
      }
    }

    return true;
}


bool XYB(Image &input, bool inverse) {
    if (inverse) return inv_XYB(input);
    else return fwd_XYB(input);
}

