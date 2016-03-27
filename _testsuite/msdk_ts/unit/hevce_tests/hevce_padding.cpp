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

namespace H265Enc {
    template <class PixType> void PadOnePix(PixType *frame, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s padW, Ipp32s padH);
};

using H265Enc::FrameData;
using H265Enc::Frame;
using namespace H265Enc::MfxEnumShortAliases;

const Ipp32s CtuSize = 64;
const Ipp32s Width = 3 * CtuSize - 8;
const Ipp32s Height = 3 * CtuSize - 8;
const Ipp32s Padding = 96;

namespace PaddingTest {
    template <class T> void ReferencePadding(T *frame, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padw, Ipp32s padh)
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
                SetValue(frame[0], -padw, -padh, padw, padh);
            for (Ipp32s x = rectx; x < rectx + rectw; x++) // top
                SetValue(frame[x], x, -padh, 1, padh);
            if (rectx + rectw == width) // top-right corner
                SetValue(frame[width - 1], width, -padh,  padw, padh);
        }

        // pad right edge
        if (rectx + rectw == width)
            for (Ipp32s y = recty; y < recty + recth; y++) // right
                SetValue(frame[y * pitch + width - 1], width, y, padw, 1);

        // pad bottom edge
        if (recty + recth == height) {
            if (rectx == 0) // bottom-left corner
                SetValue(frame[(height - 1) * pitch], -padw, height, padw, padh);
            for (Ipp32s x = rectx; x < rectx + rectw; x++) // bottom
                SetValue(frame[(height - 1) * pitch + x], x, height, 1, padh);
            if (rectx + rectw == width) // bottom-right corner
                SetValue(frame[(height - 1) * pitch + width - 1], width, height, padw, padh);
        }

        // pad left edge
        if (rectx == 0)
            for (Ipp32s y = recty; y < recty + recth; y++) // left
                SetValue(frame[y * pitch], -padw, y, padw, 1);
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

    template <Ipp32u fourcc> void Test()
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

                        H265Enc::PadRect(ytst, pitchL, widthL, heightL, x*64, y*64, w*64, h*64, padwL, padhL);
                        H265Enc::PadRect((DoublePixT *)uvtst, pitchC/2, widthC/2, heightC, x*ctuwC, y*ctuhC, w*ctuwC, h*ctuhC, padwC/2, padhC);

                        ReferencePadding(yref, pitchL, widthL, heightL, x*64, y*64, w*64, h*64, padwL, padhL);
                        ReferencePadding((DoublePixT *)uvref, pitchC/2, widthC/2, heightC, x*ctuwC, y*ctuhC, w*ctuwC, h*ctuhC, padwC/2, padhC);
                        auto pos = Mismatch(scratchPadRef, scratchPadTst, pitchL);
                        EXPECT_EQ(INT_MAX, pos.x);
                        EXPECT_EQ(INT_MAX, pos.y);
                    }
                }
            }
        }
    }
};

TEST(PaddingTest, NV12) {
    PaddingTest::Test<NV12>();
}

TEST(PaddingTest, NV16) {
    PaddingTest::Test<NV16>();
}

TEST(PaddingTest, P010) {
    PaddingTest::Test<P010>();
}

TEST(PaddingTest, P210) {
    PaddingTest::Test<P210>();
}
