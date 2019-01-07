/*//////////////////////////////////////////////////////////////////////////////////////////////////////

Copyright 2010-2016, Jon Sneyers & Pieter Wuille
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

#include <stdio.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "io.h"

void e_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fflush(stderr);
    va_end(args);
}

#ifdef DEBUG
static int verbosity = 5;
#else
static int verbosity = 1;
#endif
static FILE * my_stdout = stdout;
void increase_verbosity(int how_much) {
    verbosity += how_much;
}

int get_verbosity() {
    return verbosity;
}

void v_printf(const int v, const char *format, ...) {
    if (verbosity < v) return;
    va_list args;
    va_start(args, format);
    vfprintf(my_stdout, format, args);
    fflush(my_stdout);
    va_end(args);
}

void v_printf_tty(const int v, const char *format, ...) {
    if (verbosity < v) return;
#ifdef _WIN32
    if(!_isatty(_fileno(my_stdout))) return;
#else
    if(!isatty(fileno(my_stdout))) return;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(my_stdout, format, args);
    fflush(my_stdout);
    va_end(args);
}

void redirect_stdout_to_stderr() {
    my_stdout = stderr;
}
