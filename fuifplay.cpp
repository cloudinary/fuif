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


// somewhat based on https://github.com/lecram/gifdec/blob/master/example.c

#include <stdio.h>

#include <SDL.h>

#include "config.h"
#include "image/image.h"
#include "encoding/encoding.h"
#include <getopt.h>

int main(int argc, char *argv[]) {

    static struct option optlist[] = {
        {"help", 0, NULL, 'h'},
        {"verbose", 0, NULL, 'v'},
        {"responsive", 1, NULL, 'R'},
        {0,0,0,0}
    };

    bool showhelp=false;
    int responsive=-1;
    int c,i;
    while ((c = getopt_long (argc, argv, "hvR:", optlist, &i)) != -1) {
        switch (c) {
            case 'v': increase_verbosity(); break;
            case 'R': responsive = atoi(optarg); break;
            case 'h':
            default: showhelp=true; break;
        }
    }
    if (argc-optind < 1 || showhelp) {
        v_printf(1,"Usage:  %s <input.fuif> [options]\n", argv[0]);
        v_printf(1,"   -h, --help                  show help\n");
        v_printf(1,"   -v, --verbose               increase verbosity (multiple -v for more output)\n");
        v_printf(1,"   -R, --responsive=K          -1=full image (default), 0=LQIP, 1=(1:16), 2=(1:8), 3=(1:4), 4=(1:2)\n");
        return 1;
    }
    argc -= optind;
    argv += optind;

    fuif_options options = default_fuif_options;
    if (responsive < -1 || responsive > 4) {
        e_printf("Invalid value for -R option (range: -1..4)\n");
        return 1;
    }
    options.preview = responsive;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Event event;

    char title[32] = {0};

    char * sp;
    Uint32 pixel;
    int ret, paused, quit;
    Uint32 t0, t1, delay, delta;


    Image decoded;
    if (fuif_decode_file(argv[0],decoded,options)) {
        decoded.undo_transforms();
    } else {
        e_printf("Could not decode %s\n",argv[0]);
        return -1;
    }

    int w = decoded.w;
    int h = decoded.h / decoded.nb_frames;
    int frames = decoded.nb_frames;
    int cf = 0;

    printf("Decoded %ix%i FUIF with %i frames, %i fps\n",w,h,frames,decoded.den);

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }
    SDL_EventState(SDL_MOUSEMOTION,SDL_IGNORE);

    snprintf(title, sizeof(title) - 1, "FUIF (%dx%d, %d frames)", w, h, frames);
    window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!window || !renderer) {
        SDL_Log("Couldn't create window or renderer: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x00);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    surface = SDL_CreateRGBSurface(0, w, h, 32, 0x000000FF,0x0000FF00,0x00FF0000,0xFF000000);
    if (!surface) {
        SDL_Log("SDL_CreateRGBSurface() failed: %s", SDL_GetError());
        return 1;
    }
    paused = 0;
    quit = 0;
    bool alpha=(decoded.nb_channels > 3);
    if (decoded.den == 0 || frames < 2) decoded.den = 2; // update twice per second if it's not an animation
    while (1) {
        while (SDL_PollEvent(&event) && !quit) {
            if (event.type == SDL_QUIT)
                quit = 1;
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_q)
                    quit = 1;
                else if (event.key.keysym.sym == SDLK_SPACE)
                    paused = !paused;
            }
        }
        if (quit) break;
        if (paused) {
            SDL_Delay(10);
            continue;
        }
        t0 = SDL_GetTicks();
        SDL_LockSurface(surface);
        sp = (char*) surface->pixels;
        if (alpha)
        for (i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                int a = decoded.channel[3].value(i+h*cf,j);
                int b = ( (i/10 + j/10)&1 ? 0x60 : 0xA0 ); // checkerboard pattern
                sp[4*j+0] = (decoded.channel[0].value(i+h*cf,j) * a + b * (255-a))>>8;
                sp[4*j+1] = (decoded.channel[1].value(i+h*cf,j) * a + b * (255-a))>>8;
                sp[4*j+2] = (decoded.channel[2].value(i+h*cf,j) * a + b * (255-a))>>8;
                sp[4*j+3] = 255;
            }
            sp += surface->pitch;
        }
        else
        for (i = 0; i < h; i++) {
            for (int j = 0; j < w; j++) {
                sp[4*j+0] = decoded.channel[0].value(i+h*cf,j);
                sp[4*j+1] = decoded.channel[1].value(i+h*cf,j);
                sp[4*j+2] = decoded.channel[2].value(i+h*cf,j);
                sp[4*j+3] = 255;
            }
            sp += surface->pitch;
        }
        SDL_UnlockSurface(surface);
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_DestroyTexture(texture);
        SDL_RenderPresent(renderer);
        t1 = SDL_GetTicks();
        delta = t1 - t0;
        delay = (decoded.num.size() ? decoded.num[cf] : 1) * 1000 / decoded.den;
        delay = delay > delta ? delay - delta : 1;
        SDL_Delay(delay);
        cf++;
        if (cf >= frames) cf = 0;
    }
    SDL_FreeSurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
