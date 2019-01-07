/*//////////////////////////////////////////////////////////////////////////////////////////////////////

FUIF -  FREE UNIVERSAL IMAGE FORMAT
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

#include <stdio.h>
#include <string>
#include <string.h>

class FileIO
{
private:
    FILE *file;
    const char *name;
public:
    // prevent copy
    FileIO(const FileIO&) = delete;
    void operator=(const FileIO&) = delete;
    // prevent move, for now
    FileIO(FileIO&&) = delete;
    void operator=(FileIO&&) = delete;

    const int EOS = EOF;

    FileIO(FILE* fil, const char *aname) : file(fil), name(aname) { }
    ~FileIO() {
        if (file) fclose(file);
    }
    void flush() {
        fflush(file);
    }
    bool isEOF() {
      return feof(file);
    }
    long ftell() {
      return ::ftell(file);
    }
    int get_c() {
      return fgetc(file);
    }
    char * gets(char *buf, int n) {
      return fgets(buf, n, file);
    }
    int fputs(const char *s) {
      return ::fputs(s, file);
    }
    int fputc(int c) {
      return ::fputc(c, file);
    }
    void fseek(long offset, int where) {
      ::fseek(file, offset,where);
    }
    const char* getName() const {
      return name;
    }
};

/*!
 * Read-only IO interface for a constant memory block
 */
class BlobReader
{
private:
    const uint8_t* data;
    size_t data_array_size;
    size_t seek_pos;
public:
    const int EOS = -1;

    BlobReader(const uint8_t* _data, size_t _data_array_size)
    : data(_data)
    , data_array_size(_data_array_size)
    , seek_pos(0)
    {
    }

    bool isEOF() const {
        return seek_pos >= data_array_size;
    }
    long ftell() const {
        return seek_pos;
    }
    int get_c() {
        if(seek_pos >= data_array_size)
            return EOS;
        return data[seek_pos++];
    }
    char * gets(char *buf, int n) {
        int i = 0;
        const int max_write = n-1;
        while(seek_pos < data_array_size && i < max_write)
            buf[i++] = data[seek_pos++];
        buf[n-1] = '\0';

        if(i < max_write)
            return 0;
        else
            return buf;
    }
    int fputc(int c) {
      // cannot write on const memory
      return EOS;
    }
    void fseek(long offset, int where) {
        switch(where) {
        case SEEK_SET:
            seek_pos = offset;
            break;
        case SEEK_CUR:
            seek_pos += offset;
            break;
        case SEEK_END:
            seek_pos = long(data_array_size) + offset;
            break;
        }
    }
    static const char* getName() {
        return "BlobReader";
    }
};

/*!
 * IO interface for a growable memory block
 */
class BlobIO
{
private:
    uint8_t* data;
    // keeps track how large the array really is
    // HINT: should only be used in the grow() function
    size_t data_array_size;
    // keeps track how many bytes were written
    size_t bytes_used;
    size_t seek_pos;

    void grow(size_t necessary_size) {
        if(necessary_size < data_array_size)
            return;

        size_t new_size = necessary_size;
        // start with reasonably large array
        if(new_size < 4096)
            new_size = 4096;

        if(new_size < data_array_size * 3 / 2)
            new_size = data_array_size * 3 / 2;
        uint8_t* new_data = new uint8_t[new_size];

        memcpy(new_data, data, bytes_used);
        // if seek_pos has been moved beyond the written bytes,
        // fill the empty space with zeroes
        for(size_t i = bytes_used; i < seek_pos; ++i)
            new_data[i] = 0;
        delete [] data;
        data = new_data;
        std::swap(data_array_size, new_size);
    }
public:
    const int EOS = -1;

    BlobIO()
    : data(0)
    , data_array_size(0)
    , bytes_used(0)
    , seek_pos(0)
    {
    }

    ~BlobIO() {
        delete [] data;
    }

    uint8_t* release(size_t* array_size)
    {
        uint8_t* ptr = data;
        *array_size = bytes_used;

        data = 0;
        data_array_size = 0;
        bytes_used = 0;
        seek_pos = 0;
        return ptr;
    }

    static void flush() {
        // nothing to do
    }
    bool isEOF() const {
        return seek_pos >= bytes_used;
    }
    int ftell() const {
        return seek_pos;
    }
    int get_c() {
        if(seek_pos >= bytes_used)
            return EOS;
        return data[seek_pos++];
    }
    char * gets(char *buf, int n) {
        int i = 0;
        const int max_write = n-1;
        while(seek_pos < bytes_used && i < max_write)
            buf[i++] = data[seek_pos++];
        buf[n-1] = '\0';

        if(i < max_write)
            return 0;
        else
            return buf;
    }
    int fputs(const char *s) {
        size_t i = 0;
        // null-terminated string
        while(s[i])
        {
            grow(seek_pos + 1);
            data[seek_pos++] = s[i++];
            if(bytes_used < seek_pos)
                bytes_used = seek_pos+1;
        }
        return 0;
    }
    int fputc(int c) {
        grow(seek_pos + 1);

        data[seek_pos++] = static_cast<uint8_t>(c);
        if(bytes_used < seek_pos)
            bytes_used = seek_pos+1;
        return c;
    }
    void fseek(long offset, int where) {
        switch(where) {
        case SEEK_SET:
            seek_pos = offset;
            break;
        case SEEK_CUR:
            seek_pos += offset;
            break;
        case SEEK_END:
            seek_pos = long(bytes_used) + offset;
            break;
        }
    }
    static const char* getName() {
        return "BlobIO";
    }
};

/*!
 * Dummy IO
 */
class DummyIO
{
public:
    const int EOS = -1;

    DummyIO() {}

    static void flush() {
        // nothing to do
    }
    bool isEOF() const {
        return false;
    }
    int ftell() const {
        return -1;
    }
    int get_c() {
        return 0;
    }
    char * gets(char *buf, int n) {
        return 0;
    }
    int fputs(const char *s) {
        return 0;
    }
    int fputc(int c) {
        return c;
    }
    void fseek(long offset, int where) {
    }
    static const char* getName() {
        return "DummyIO";
    }
};

