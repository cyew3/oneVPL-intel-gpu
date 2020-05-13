// Copyright (c) 2019-2020 Intel Corporation
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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "hevcehw_base.h"
#include "base/hevcehw_base_legacy.h"
#include "base/hevcehw_base_interlace.h"

namespace hevce { namespace tests
{
    struct FeatureBlocksInterlace
        : testing::Test
    {
        HEVCEHW::FeatureBlocks    blocks{};
        HEVCEHW::Base::Interlace interlace;
        HEVCEHW::Base::Legacy    legacy;
        HEVCEHW::StorageRW        storage;
        mfxFrameAllocRequest      request{};

        FeatureBlocksInterlace()
            : legacy(HEVCEHW::Base::FEATURE_LEGACY)
            , interlace(HEVCEHW::Base::FEATURE_INTERLACE)
        {}

        void SetUp() override
        {
            legacy.Init(HEVCEHW::QUERY_IO_SURF, blocks);
            interlace.Init(HEVCEHW::QUERY_IO_SURF, blocks);

            auto& vp = HEVCEHW::Base::Glob::VideoParam::GetOrConstruct(storage);
            vp.AsyncDepth = 3;
            vp.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            vp.mfx.GopRefDist = 16;

            auto& queueQIS = HEVCEHW::FeatureBlocks::BQ<HEVCEHW::FeatureBlocks::BQ_QueryIOSurf>::Get(blocks);
            auto block = HEVCEHW::FeatureBlocks::Get(queueQIS, { HEVCEHW::Base::FEATURE_LEGACY, HEVCEHW::Base::Legacy::BLK_SetFrameAllocRequest });

            ASSERT_EQ(
                block->Call(vp, request, storage),
                MFX_ERR_NONE
            );
        }
    };

    TEST_F(FeatureBlocksInterlace, QueryIOSurfNoInterlace)
    {
        auto& queueQIS = HEVCEHW::FeatureBlocks::BQ<HEVCEHW::FeatureBlocks::BQ_QueryIOSurf>::Get(blocks);
        auto block = HEVCEHW::FeatureBlocks::Get(queueQIS, { HEVCEHW::Base::FEATURE_INTERLACE, HEVCEHW::Base::Interlace::BLK_QueryIOSurf });

        mfxFrameAllocRequest result{};
        result.NumFrameMin = request.NumFrameMin;

        ASSERT_EQ(
            block->Call(mfxVideoParam{}, result, storage),
            MFX_ERR_NONE
        );

        //Shouldn't change if PicStruct != MFX_PICSTRUCT_FIELD
        EXPECT_EQ(
            result.NumFrameMin, request.NumFrameMin
        );
    }

    TEST_F(FeatureBlocksInterlace, QueryIOSurf)
    {
        auto& queueQIS = HEVCEHW::FeatureBlocks::BQ<HEVCEHW::FeatureBlocks::BQ_QueryIOSurf>::Get(blocks);
        auto block = HEVCEHW::FeatureBlocks::Get(queueQIS, { HEVCEHW::Base::FEATURE_INTERLACE, HEVCEHW::Base::Interlace::BLK_QueryIOSurf });

        mfxFrameAllocRequest result{};
        result.NumFrameMin = request.NumFrameMin;

        auto& vp = HEVCEHW::Base::Glob::VideoParam::Get(storage);
        vp.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_SINGLE;

        ASSERT_EQ(
            block->Call(mfxVideoParam{}, result, storage),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            result.NumFrameMin, request.NumFrameMin + (vp.mfx.GopRefDist - 1)
        );
    }

} }