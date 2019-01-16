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

namespace AV1PP
{
    #define MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
    #define MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )
    #define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

    void adds_nv12_px(unsigned char *dst, int pitchDst, const unsigned char *src1, int pitch1, const short *src2u, const short *src2v, int pitch2, int size)
    {
        const int MAXVAL = 255;
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                dst[2*j]   = Saturate(0, MAXVAL, src1[2*j]   + src2u[j]);
                dst[2*j+1] = Saturate(0, MAXVAL, src1[2*j+1] + src2v[j]);
            }
            src1  += pitch1;
            src2u += pitch2;
            src2v += pitch2;
            dst   += pitchDst;
        }
    }
}; // namespace AV1PP
