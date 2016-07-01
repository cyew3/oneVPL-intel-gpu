//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2016 Intel Corporation. All Rights Reserved.
//

#include "gtest/gtest.h"
#include <stdlib.h>
#include <limits.h>
#include <vector>
#include <algorithm>

#define MFX_ENABLE_H265_VIDEO_ENCODE
#include "mfx_h265_frame.h"

using H265Enc::FrameData;
using H265Enc::Frame;
using namespace H265Enc::MfxEnumShortAliases;

namespace H265Enc {
    template <class PixType> void CopyAndPad(const mfxFrameSurface1 &src, FrameData &dst, Ipp32u fourcc);
    template <class T> void PadRect(T *plane, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padL, Ipp32s padR, Ipp32s padh);
};

template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    size_t mask = ~(size_t(alignment) - 1);
    return T((size_t(value) + alignment - 1) & mask);
}

namespace PaddingTest {
    template <class T> void ReferencePadding(T *frame, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padl, Ipp32s padr, Ipp32s padh)
    {
        rectx = IPP_MAX(rectx, 0);
        recty = IPP_MAX(recty, 0);
        rectw = IPP_MIN(IPP_MAX(rectw, 1), width-rectx);
        recth = IPP_MIN(IPP_MAX(recth, 1), height-recty);

        auto SetValue = [frame, pitch](T value, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth) {
            for (Ipp32s y = recty; y < recty + recth; y++)
                for (Ipp32s x = rectx; x < rectx + rectw; x++)
                    frame[y * pitch + x] = value;
        };

        Ipp32s x, y;

        // pad top edge
        if (recty == 0) {
            if (rectx == 0) // top-left corner
                SetValue(frame[0], -padl, -padh, padl, padh);
            for (Ipp32s x = rectx; x < rectx + rectw; x++) // top
                SetValue(frame[x], x, -padh, 1, padh);
            if (rectx + rectw == width) // top-right corner
                SetValue(frame[width - 1], width, -padh,  padr, padh);
        }

        // pad right edge
        if (rectx + rectw == width)
            for (Ipp32s y = recty; y < recty + recth; y++) // right
                SetValue(frame[y * pitch + width - 1], width, y, padr, 1);

        // pad bottom edge
        if (recty + recth == height) {
            if (rectx == 0) // bottom-left corner
                SetValue(frame[(height - 1) * pitch], -padl, height, padl, padh);
            for (Ipp32s x = rectx; x < rectx + rectw; x++) // bottom
                SetValue(frame[(height - 1) * pitch + x], x, height, 1, padh);
            if (rectx + rectw == width) // bottom-right corner
                SetValue(frame[(height - 1) * pitch + width - 1], width, height, padr, padh);
        }

        // pad left edge
        if (rectx == 0)
            for (Ipp32s y = recty; y < recty + recth; y++) // left
                SetValue(frame[y * pitch], -padl, y, padl, 1);
    }

    struct Pos2D { Ipp32s x, y; };

    template <class T> Pos2D Mismatch(const std::vector<T> &ref, const std::vector<T> &tst, Ipp32s pitch) {
        Pos2D pos = { INT_MAX, INT_MAX };
        auto res = std::mismatch(ref.begin(), ref.end(), tst.begin());
        if (res.first != ref.end())
            pos.x = (res.first - ref.begin()) % pitch, pos.y = (res.first - ref.begin()) / pitch;
        return pos;
    }

    template <Ipp32u fourcc> struct TestTraits;
    template <> struct TestTraits<NV12> { typedef Ipp8u  PixT; typedef Ipp16u DoublePixT; enum { ChromaHeightDiv = 2, ChromaFormat = YUV420, BitDepth = 8 }; };
    template <> struct TestTraits<NV16> { typedef Ipp8u  PixT; typedef Ipp16u DoublePixT; enum { ChromaHeightDiv = 1, ChromaFormat = YUV422, BitDepth = 8 }; };
    template <> struct TestTraits<P010> { typedef Ipp16u PixT; typedef Ipp32u DoublePixT; enum { ChromaHeightDiv = 2, ChromaFormat = YUV420, BitDepth = 10}; };
    template <> struct TestTraits<P210> { typedef Ipp16u PixT; typedef Ipp32u DoublePixT; enum { ChromaHeightDiv = 1, ChromaFormat = YUV422, BitDepth = 10}; };

    template <Ipp32u fourcc> void Test(Ipp32s Width, Ipp32s Height, Ipp32s Padding)
    {
        typedef typename TestTraits<fourcc>::PixT PixT;
        typedef typename TestTraits<fourcc>::DoublePixT DoublePixT;
        Ipp32s chromaHeightDiv = TestTraits<fourcc>::ChromaHeightDiv;
        Ipp32u chromaFormat = TestTraits<fourcc>::ChromaFormat;
        Ipp32s ctuwC = 32;
        Ipp32s ctuhC = 64 / chromaHeightDiv;
        Ipp32s bitDepth = TestTraits<fourcc>::BitDepth;
        Ipp32s widthL = Width;
        Ipp32s heightL = Height;
        Ipp32s widthC = Width;
        Ipp32s heightC = Height / chromaHeightDiv;
        Ipp32s padwL = Padding;
        Ipp32s padhL = Padding;
        Ipp32s padwC = Padding;
        Ipp32s padhC = Padding / chromaHeightDiv;
        Ipp32s pitchL = widthL + 2 * padwL + 32;
        Ipp32s pitchC = widthC + 2 * padwC + 32;
        Ipp32s widthInCtu = (widthL + 64) / 64;
        Ipp32s heightInCtu = (heightL + 64) / 64;

        std::vector<PixT> scratchPadRef(pitchL * (heightL + 2 * padhL) + pitchC * (heightC + 2 * padhC));
        std::vector<PixT> scratchPadTst(pitchL * (heightL + 2 * padhL) + pitchC * (heightC + 2 * padhC));

        PixT *yref = scratchPadRef.data() + padwL + padhL * pitchL;
        PixT *ytst = scratchPadTst.data() + padwL + padhL * pitchL;
        PixT *uvref = scratchPadRef.data() + pitchL * (heightL + 2 * padhL) + padwC + padhC * pitchC;
        PixT *uvtst = scratchPadTst.data() + pitchL * (heightL + 2 * padhL) + padwC + padhC * pitchC;

        for (Ipp32s h = 1; h <= heightInCtu; h++) {
            for (Ipp32s w = 1; w <= widthInCtu; w++) {
                for (Ipp32s y = 0; y <= 3 - h; y++) {
                    for (Ipp32s x = 0; x <= 3 - w; x++) {
                        std::stringstream s;
                        s << "testing rect { x: " << x << ", y: " << y << ", w: " << w << ", h: " << h << " }";
                        SCOPED_TRACE(s.str());
                        std::generate(scratchPadRef.begin(), scratchPadRef.end(), rand);
                        std::copy(scratchPadRef.begin(), scratchPadRef.end(), scratchPadTst.begin());

                        H265Enc::PadRect(ytst, pitchL, widthL, heightL, x*64, y*64, w*64, h*64, padwL, padwL, padhL);
                        H265Enc::PadRect((DoublePixT *)uvtst, pitchC/2, widthC/2, heightC, x*ctuwC, y*ctuhC, w*ctuwC, h*ctuhC, padwC/2, padwC/2, padhC);

                        ReferencePadding(yref, pitchL, widthL, heightL, x*64, y*64, w*64, h*64, padwL, padwL, padhL);
                        ReferencePadding((DoublePixT *)uvref, pitchC/2, widthC/2, heightC, x*ctuwC, y*ctuhC, w*ctuwC, h*ctuhC, padwC/2, padwC/2, padhC);
                        auto pos = Mismatch(scratchPadRef, scratchPadTst, pitchL);
                        EXPECT_EQ(INT_MAX, pos.x);
                        EXPECT_EQ(INT_MAX, pos.y);
                    }
                }
            }
        }
    }

    template <Ipp32u fourcc> void Test2(Ipp32s Width, Ipp32s Height, Ipp32s Padding, bool gacc)
    {
        typedef typename TestTraits<fourcc>::PixT PixT;
        typedef typename TestTraits<fourcc>::DoublePixT DoublePixT;
        Ipp32s chromaHeightDiv = TestTraits<fourcc>::ChromaHeightDiv;
        Ipp32u chromaFormat = TestTraits<fourcc>::ChromaFormat;
        Ipp32s bpp = sizeof(PixT);
        Ipp32s ctuwC = 32;
        Ipp32s ctuhC = 64 / chromaHeightDiv;
        Ipp32s bitDepth = TestTraits<fourcc>::BitDepth;
        Ipp32s widthL = Width;
        Ipp32s heightL = Height;
        Ipp32s widthC = Width;
        Ipp32s heightC = Height / chromaHeightDiv;
        Ipp32s padLumaL = Padding;
        Ipp32s padLumaR = Padding;
        Ipp32s padLumaH = Padding;
        Ipp32s padChromaL = Padding;
        Ipp32s padChromaR = Padding;
        Ipp32s padChromaH = Padding / chromaHeightDiv;
        Ipp32s pitchL = AlignValue(widthL + AlignValue(padLumaL*bpp, 64)/bpp + padLumaR, 64);
        Ipp32s pitchC = AlignValue(widthC + AlignValue(padChromaL*bpp, 64)/bpp + padChromaR, 64);
        Ipp32s lumaSize = pitchL * (heightL + 2 * padLumaH);
        Ipp32s chromaSize = pitchC * (heightC + 2 * padChromaH);

        if (gacc) {
            padLumaL = AlignValue(padLumaL*bpp, 64)/bpp;
            padLumaR = pitchL - widthL - padLumaL;
            padChromaL = AlignValue(padChromaL*bpp, 64)/bpp;
            padChromaR = pitchC - widthC - padChromaL;
        }

        std::vector<PixT> scratchPadRef(lumaSize + chromaSize + 2*16);
        std::vector<PixT> scratchPadTst(lumaSize + chromaSize + 2*64);

        PixT *yref = AlignValue(scratchPadRef.data() + padLumaL + padLumaH * pitchL, 16);
        PixT *ytst = AlignValue(scratchPadTst.data() + padLumaL + padLumaH * pitchL, 64);
        PixT *uvref = AlignValue(scratchPadRef.data() + lumaSize + 16 + padChromaL + padChromaH * pitchC, 16);
        PixT *uvtst = AlignValue(scratchPadTst.data() + lumaSize + 64 + padChromaL + padChromaH * pitchC, 64);

        std::generate(scratchPadRef.begin(), scratchPadRef.end(), rand);

        mfxFrameSurface1 src;
        src.Data.Y = (Ipp8u*)yref;
        src.Data.UV = (Ipp8u*)uvref;
        src.Data.Pitch = pitchL * sizeof(PixT);
        FrameData dst;
        dst.y = (Ipp8u*)ytst;
        dst.uv = (Ipp8u*)uvtst;
        dst.width = widthL;
        dst.height = heightL;
        dst.pitch_luma_pix = pitchL;
        dst.pitch_chroma_pix = pitchC;
        dst.padding = Padding;
        dst.m_handle = (gacc ? (void*)0x12345600 : nullptr);

        H265Enc::CopyAndPad<PixT>(src, dst, fourcc);

        ReferencePadding(yref, pitchL, widthL, heightL, 0, 0, widthL, heightL, padLumaL, padLumaR, padLumaH);
        for (Ipp32s y = -padLumaH; y < heightL + padLumaH; y++)
            for (Ipp32s x = -padLumaL; x < widthL + padLumaR; x++)
                ASSERT_EQ(yref[y*pitchL+x], ytst[y*pitchL+x]);
        for (Ipp32s y = 0; y < heightC; y++)
            for (Ipp32s x = 0; x < widthC; x++)
                ASSERT_EQ(uvref[y*pitchC+x], uvtst[y*pitchC+x]);
    }
};

const Ipp32s CtuSize = 64;
const Ipp32s Width = 3 * CtuSize - 8;
const Ipp32s Height = 3 * CtuSize - 8;
const Ipp32s Padding = 96;

TEST(PaddingTest, NV12) {
    PaddingTest::Test<NV12>(Width, Height, Padding);
}

TEST(PaddingTest, NV16) {
    PaddingTest::Test<NV16>(Width, Height, Padding);
}

TEST(PaddingTest, P010) {
    PaddingTest::Test<P010>(Width, Height, Padding);
}

TEST(PaddingTest, P210) {
    PaddingTest::Test<P210>(Width, Height, Padding);
}

TEST(CopyAndPadTest, NV12) {
    PaddingTest::Test2<NV12>(64+16, 64, 80, true);
    PaddingTest::Test2<NV12>(64+16, 64, 80, false);
    PaddingTest::Test2<NV12>(64+24, 64, 80, true);
    PaddingTest::Test2<NV12>(64+24, 64, 80, false);
}

TEST(CopyAndPadTest, NV16) {
    PaddingTest::Test2<NV16>(64+16, 64, 80, true);
    PaddingTest::Test2<NV16>(64+16, 64, 80, false);
    PaddingTest::Test2<NV16>(64+24, 64, 80, true);
    PaddingTest::Test2<NV16>(64+24, 64, 80, false);
}

TEST(CopyAndPadTest, P010) {
    PaddingTest::Test2<P010>(64+16, 64, 80, true);
    PaddingTest::Test2<P010>(64+16, 64, 80, false);
    PaddingTest::Test2<P010>(64+24, 64, 80, true);
    PaddingTest::Test2<P010>(64+24, 64, 80, false);
}

TEST(CopyAndPadTest, P210) {
    PaddingTest::Test2<P210>(64+16, 64, 80, true);
    PaddingTest::Test2<P210>(64+16, 64, 80, false);
    PaddingTest::Test2<P210>(64+24, 64, 80, true);
    PaddingTest::Test2<P210>(64+24, 64, 80, false);
}
