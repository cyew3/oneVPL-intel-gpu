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

#include "stdio.h"
#include "assert.h"
#include "string.h"
#include "ipps.h"
#include "mfx_av1_opts_intrin.h"

namespace AV1PP
{
    enum { PLANE_Y = 0, PLANE_UV = 1 };

void Copy(const void *src_, void *dst_, int numBytes)
{
    assert((numBytes & 3) == 0);
    const uint8_t *src = (const uint8_t *)src_;
    uint8_t *dst = (uint8_t *)dst_;

    for (; numBytes >= 16; numBytes -= 16, src += 16, dst += 16)
        storea_si128(dst, loadu_si128(src));
    if (numBytes >= 8) {
        storel_epi64(dst, loadl_epi64(src));
        numBytes -= 8;
    }
    if (numBytes == 4)
        storel_si32(dst, loadl_si32(src));
}

template <typename PixType> void Set(PixType *dst, PixType val, int numValues);

template <> void Set<uint8_t>(uint8_t *dst, uint8_t val, int numValues)
{
    assert((numValues & 3) == 0);
    __m128i v = _mm_cvtsi32_si128(val);
    v = _mm_unpacklo_epi8(v, v);
    v = _mm_unpacklo_epi16(v, v);
    do {
        storel_si32(dst, v);
        numValues -= 4;
        dst += 4;
    } while (numValues >= 4);
}

template <> void Set<uint16_t>(uint16_t *dst, uint16_t val, int numValues)
{
    assert((numValues & 3) == 0);
    __m128i v = _mm_cvtsi32_si128(val);
    v = _mm_unpacklo_epi16(v, v);
    v = _mm_unpacklo_epi32(v, v);
    do {
        storel_epi64(dst, v);
        numValues -= 4;
        dst += 4;
    } while (numValues >= 4);
}

template <> void Set<uint32_t>(uint32_t *dst, uint32_t val, int numValues)
{
    assert(numValues > 0);
    assert((numValues & 3) == 0);
    __m128i v = _mm_cvtsi32_si128(val);
    v = _mm_unpacklo_epi32(v, v);
    v = _mm_unpacklo_epi64(v, v);
    do {
        storea_si128(dst, v);
        numValues -= 4;
        dst += 4;
    } while (numValues >= 4);
}

template <int plane, typename PixType>
void GetPredPelsAV1(const PixType *rec, int pitch, PixType *topPels, PixType *leftPels, int width,
                    int haveTop, int haveLeft, int pixTopRight, int pixBottomLeft)
{
    assert((width & 3) == 0);
    assert((pixTopRight & 3) == 0);
    assert((pixBottomLeft & 3) == 0);

    const PixType baseTopLeft = (plane == PLANE_Y)
        ? (sizeof(PixType) == 1 ? 0x80   : PixType(0x80   << 2))
        : (sizeof(PixType) == 2 ? 0x8080 : PixType(PixType(0x80 << 2) + (PixType(0x80 << 2)<<16))); //PixType(0x8080 << 2));

    const PixType baseTop = (plane == PLANE_Y)
        ? (sizeof(PixType) == 1 ? 0x7f   : PixType((0x80 << 2)-1)) //PixType(0x7f   << 2))
        : (sizeof(PixType) == 2 ? 0x7f7f : PixType(PixType((0x80 << 2)-1) + (PixType((0x80 << 2)-1) << 16)));//PixType(0x7f7f << 2));

    const PixType baseLeft = (plane == PLANE_Y)
        ? (sizeof(PixType) == 1 ? 0x81   : PixType((0x80 << 2) + 1))//PixType(0x81   << 2))
        : (sizeof(PixType) == 2 ? 0x8181 : PixType(PixType((0x80 << 2)+1) + (PixType((0x80 << 2)+1) << 16)));//PixType(0x8181 << 2));

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
        for (int i = 0; i < width + pixBottomLeft; i += 4) {
            leftPels[i + 0] = *leftRef; leftRef += pitch;
            leftPels[i + 1] = *leftRef; leftRef += pitch;
            leftPels[i + 2] = *leftRef; leftRef += pitch;
            leftPels[i + 3] = *leftRef; leftRef += pitch;
        }
        if (pixBottomLeft < width) {
            const PixType pel = leftPels[width + pixBottomLeft - 1];
            Set(leftPels + width + pixBottomLeft, pel, width - pixBottomLeft);
        }
    } else {
        const PixType pel = haveTop ? *(rec - pitch) : baseLeft;
        Set(leftPels, pel, 2 * width);
    }

    if (haveTop) {
        Copy((const uint8_t *)(rec - pitch), (uint8_t *)topPels, (width + pixTopRight) * sizeof(PixType));
        if (pixTopRight < width) {
            const PixType pel = topPels[width + pixTopRight - 1];
            Set(topPels + width + pixTopRight, pel, width - pixTopRight);
        }
    } else {
        const PixType pel = haveLeft ? *(rec - 1) : baseTop;
        Set(topPels, pel, 2 * width);
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

    template <typename PixType>
    void GetPredPelsLuma(const PixType *rec, int32_t pitch, PixType *predPels, int32_t width,
                         PixType halfRangeM1, PixType halfRangeP1, int32_t haveAbove, int32_t haveLeft,
                         int32_t notOnRight)
    {
        PixType *aboveRow = predPels + 1;
        PixType *leftCol  = aboveRow + 2*width;

        const PixType *above = rec - pitch;
        if (haveAbove && notOnRight && width == 4) {
            for (int32_t i = 0; i < 2*width; i+=8/sizeof(PixType))
                *(uint64_t*)(aboveRow+i) = *(uint64_t*)(above+i);
        } else if (haveAbove) {
            for (int32_t i = 0; i < width; i+=4/sizeof(PixType))
                *(uint32_t*)(aboveRow+i) = *(uint32_t*)(above+i);
            for (int32_t i = 0; i < width; i++)
                aboveRow[width+i] = above[width-1];
        } else {
            for (int32_t i = 0; i < 2*width; i++)
                aboveRow[i] = halfRangeM1;
        }

        if (haveAbove && haveLeft)
            aboveRow[-1] = above[-1];
        else if (haveAbove)
            aboveRow[-1] = halfRangeP1;
        else
            aboveRow[-1] = halfRangeM1;

        if (haveLeft) {
            const PixType *left = rec - 1;
            for (int32_t i = 0; i < width; i++, left+=pitch)
                leftCol[i] = *left;
        } else {
            for (int32_t i = 0; i < width; i++)
                leftCol[i] = halfRangeP1;
        }
    }

    template
    void GetPredPelsLuma<unsigned char>(const unsigned char *rec, int32_t pitch, unsigned char *predPels, int32_t width,
                                        unsigned char halfRangeM1, unsigned char halfRangeP1, int32_t haveAbove,
                                        int32_t haveLeft, int32_t notOnRight);
    template
    void GetPredPelsLuma<unsigned short>(const unsigned short *rec, int32_t pitch, unsigned short *predPels, int32_t width,
                                         unsigned short halfRangeM1, unsigned short halfRangeP1, int32_t haveAbove,
                                         int32_t haveLeft, int32_t notOnRight);
    template
    void GetPredPelsLuma<unsigned int>(const unsigned int *rec, int32_t pitch, unsigned int *predPels, int32_t width,
                                       unsigned int halfRangeM1, unsigned int halfRangeP1, int32_t haveAbove,
                                       int32_t haveLeft, int32_t notOnRight);

}; // namespace AV1PP
