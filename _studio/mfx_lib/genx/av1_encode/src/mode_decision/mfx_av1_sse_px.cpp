// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "ippdefs.h"
#include <stdint.h>

namespace VP9PP
{
    namespace details
    {
        template <typename PixType>
        int sse_px(const PixType *p1, int pitch1, const PixType *p2, int pitch2, int width, int height) {
            int s = 0;
            for (int j = 0; j < height; j++, p1 += pitch1, p2 += pitch2) {
                for (int i = 0; i < width; i++) {
                    int diff = p1[i] - p2[i];
                    s += (diff * diff);
                }
            }
            return s;
        }
    }; // namespace details

    template<int w, int h> int sse_px(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2) {
        return details::sse_px<unsigned char>(p1, pitch1, p2, pitch2, w, h);
    }
    template int sse_px< 4, 4>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px< 4, 8>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px< 8, 4>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px< 8, 8>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px< 8,16>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<16, 4>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<16, 8>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<16,16>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<16,32>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<32, 8>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<32,16>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<32,32>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<32,64>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<64,16>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<64,32>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);
    template int sse_px<64,64>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2);

    template<int w, int h> int sse_p64_pw_px(const unsigned char *p1, const unsigned char *p2) {
        return details::sse_px<unsigned char>(p1, 64, p2, w, w, h);
    }
    template int sse_p64_pw_px< 4, 4>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px< 4, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px< 8, 4>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px< 8, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px< 8,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<16, 4>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<16, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<16,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<16,32>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<32, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<32,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<32,32>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<32,64>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<64,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<64,32>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_pw_px<64,64>(const unsigned char *p1, const unsigned char *p2);

    template<int w, int h> int sse_p64_p64_px(const unsigned char *p1, const unsigned char *p2) {
        return details::sse_px<unsigned char>(p1, 64, p2, 64, w, h);
    }
    template int sse_p64_p64_px< 4, 4>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px< 4, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px< 8, 4>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px< 8, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px< 8,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<16, 4>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<16, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<16,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<16,32>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<32, 8>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<32,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<32,32>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<32,64>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<64,16>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<64,32>(const unsigned char *p1, const unsigned char *p2);
    template int sse_p64_p64_px<64,64>(const unsigned char *p1, const unsigned char *p2);

    template<int w> int sse_px(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2, int h) {
        return details::sse_px<unsigned char>(p1, pitch1, p2, pitch2, w, h);
    }
    template int sse_px< 4>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2, int h);
    template int sse_px< 8>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2, int h);
    template int sse_px<16>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2, int h);
    template int sse_px<32>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2, int h);
    template int sse_px<64>(const unsigned char *p1, int pitch1, const unsigned char *p2, int pitch2, int h);

    template<int size> int sse_noshift_px(const unsigned short *p1, int pitch1, const unsigned short *p2, int pitch2) {
        int s = 0;
        for (int j = 0; j < size; j++, p1 += pitch1, p2 += pitch2) {
            for (int i = 0; i < size; i++) {
                int diff = p1[i] - p2[i];
                s += (diff * diff);
            }
        }
        return s;
    }
    template int sse_noshift_px<4>(const unsigned short *p1, int pitch1, const unsigned short *p2, int pitch2);
    template int sse_noshift_px<8>(const unsigned short *p1, int pitch1, const unsigned short *p2, int pitch2);
    int64_t sse_cont_px(const int16_t *src1, const int16_t *src2, int len)
    {
        int s = 0;
        for (int j = 0; j < len; j++) {
            int diff = src1[j] - src2[j];
            s += (diff * diff);
        }
        return s;
    }
}
