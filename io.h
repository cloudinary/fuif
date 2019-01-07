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

#pragma once

void e_printf(const char *format, ...);
void v_printf(const int v, const char *format, ...);
void v_printf_tty(const int v, const char *format, ...);
void redirect_stdout_to_stderr();

void increase_verbosity(int how_much=1);
int get_verbosity();

template<class IO>
bool ioget_int_8bit (IO& io, int* result)
{
    int c = io.get_c();
    if (c == io.EOS) {
        e_printf ("Unexpected EOS");
        return false;
    }

    *result = c;
    return true;
}

template<class IO>
bool ioget_int_16bit_bigendian (IO& io, int* result)
{
    int c1;
    int c2;
    if (!(ioget_int_8bit (io, &c1) &&
          ioget_int_8bit (io, &c2)))
        return false;

    *result = (c1 << 8) + c2;
    return true;
}
