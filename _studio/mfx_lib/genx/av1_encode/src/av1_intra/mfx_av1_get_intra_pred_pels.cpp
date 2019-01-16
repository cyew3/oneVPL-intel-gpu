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

#include "assert.h"

namespace VP9PP
{

enum { PLANE_Y=0, PLANE_UV=1 };

template <int plane, typename PixType>
void GetPredPelsAV1(const PixType *rec, int pitch, PixType *topPels, PixType *leftPels, int width,
                    int haveTop, int haveLeft, int pixTopRight, int pixBottomLeft)
{
    const PixType baseTopLeft = (plane == PLANE_Y)
        ? (sizeof(PixType) == 1 ? 0x80   : PixType(0x80   << 2))
        : (sizeof(PixType) == 2 ? 0x8080 : PixType(0x8080 << 2));

    const PixType baseTop = (plane == PLANE_Y)
        ? (sizeof(PixType) == 1 ? 0x7f   : PixType(0x7f   << 2))
        : (sizeof(PixType) == 2 ? 0x7f7f : PixType(0x7f7f << 2));

    const PixType baseLeft = (plane == PLANE_Y)
        ? (sizeof(PixType) == 1 ? 0x81   : PixType(0x81   << 2))
        : (sizeof(PixType) == 2 ? 0x8181 : PixType(0x8181 << 2));

    if (haveTop && haveLeft)
        topPels[-1] = *(rec - pitch - 1);
    else if (haveTop)
        topPels[-1] = *(rec - pitch);
    else if (haveLeft)
        topPels[-1] = *(rec - 1);
    else
        topPels[-1] = baseTopLeft;

    leftPels[-1] = topPels[-1];

    if (haveLeft) {
        const PixType *leftRef = rec - 1;
        int i = 0;
        for (; i < width + pixBottomLeft; i++, leftRef += pitch) leftPels[i] = *leftRef;
        for (; i < 2 * width; i++) leftPels[i] = leftPels[i - 1];
    } else {
        if (haveTop)
            for (int i = 0; i < 2 * width; i++) leftPels[i] = *(rec - pitch);
        else
            for (int i = 0; i < 2 * width; i++) leftPels[i] = baseLeft;
    }

    if (haveTop) {
        assert(pixTopRight % 4 == 0);
        const PixType *aboveRef = rec - pitch;
        int i = 0;
        for (; i < width + pixTopRight; i += 4 / sizeof(PixType))
            *(int *)(topPels + i) = *(int *)(aboveRef + i);
        for (; i < 2 * width; i++)
            topPels[i] = topPels[width + pixTopRight - 1];
    } else {
        if (haveLeft)
            for (int i = 0; i < 2 * width; i++) topPels[i] = *(rec - 1);
        else
            for (int i = 0; i < 2 * width; i++) topPels[i] = baseTop;
    }
}

template
void GetPredPelsAV1<PLANE_Y, unsigned char>(const unsigned char *rec, int pitch, unsigned char *topPels,
                                            unsigned char *leftPels, int width, int haveTop, int haveLeft,
                                            int pixTopRight, int pixBottomLeft);
template
void GetPredPelsAV1<PLANE_UV, unsigned short>(const unsigned short *rec, int pitch, unsigned short *topPels,
                                              unsigned short *leftPels, int width, int haveTop, int haveLeft,
                                              int pixTopRight, int pixBottomLeft);
template
void GetPredPelsAV1<PLANE_Y, unsigned short>(const unsigned short *rec, int pitch, unsigned short *topPels,
                                             unsigned short *leftPels, int width, int haveTop, int haveLeft,
                                             int pixTopRight, int pixBottomLeft);
template
void GetPredPelsAV1<PLANE_UV, unsigned int>(const unsigned int *rec, int pitch, unsigned int *topPels,
                                            unsigned int *leftPels, int width, int haveTop, int haveLeft,
                                            int pixTopRight, int pixBottomLeft);

}; // namespace VP9PP
