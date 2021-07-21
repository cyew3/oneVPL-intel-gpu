// Copyright (c) 2019-2021 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base.h"
#include "av1ehw_base_data.h"

namespace AV1EHW
{

inline bool IsSegmentationEnabled(const mfxExtAV1AuxData* pAuxPar)
{
    return pAuxPar && pAuxPar->SegmentationMode != MFX_AV1_SEGMENT_DISABLED;
}

inline bool IsAutoSegmentationEnabled(const mfxExtAV1AuxData* pAuxPar)
{
    return pAuxPar && pAuxPar->SegmentationMode == MFX_AV1_SEGMENT_AUTO;
}

inline bool IsForceSegmentationEnabled(const mfxExtAV1AuxData* pAuxPar)
{
    return pAuxPar && pAuxPar->SegmentationMode == MFX_AV1_SEGMENT_MANUAL;
}

inline bool IsSegmentationSwitchedOff(const mfxExtAV1Segmentation* pPar)
{
    return pPar && pPar->NumSegments == 0;
}

inline bool IsFeatureSupported(
    const ENCODE_CAPS_AV1& caps
    , Base::SEG_LVL_FEATURES feature)
{
    return (caps.SegmentFeatureSupport & (1 << feature)) != 0;
}

inline bool IsFeatureEnabled(
    mfxU16 featureEnabled
    , Base::SEG_LVL_FEATURES feature)
{
    return (featureEnabled & (1 << feature)) != 0;
}

inline void DisableFeature(
    mfxU16& featureEnabled
    , Base::SEG_LVL_FEATURES feature)
{
    featureEnabled &= ~(1 << feature);
}

inline void EnableFeature(
    mfxU16& featureEnabled
    , Base::SEG_LVL_FEATURES feature)
{
    featureEnabled |= (1 << feature);
}

namespace Base
{
    class Segmentation
        : public FeatureBase
    {
    public:
#define DECL_BLOCK_LIST\
        DECL_BLOCK(CheckAndFix)\
        DECL_BLOCK(SetDefaults)\
        DECL_BLOCK(AllocTask)\
        DECL_BLOCK(InitTask)\
        DECL_BLOCK(ConfigureTask)
#define DECL_FEATURE_NAME "G12_Segmentation"
#include "av1ehw_decl_blocks.h"

        Segmentation(mfxU32 FeatureId)
            : FeatureBase(FeatureId)
        {}

        using SegDpbType = std::vector<std::shared_ptr<mfxExtAV1Segmentation>>;

    protected:
        virtual void SetSupported(ParamSupport& par) override;
        virtual void Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push) override;
        virtual void SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push) override;
        virtual void AllocTask(const FeatureBlocks& /*blocks*/, TPushAT Push) override;
        virtual void InitTask(const FeatureBlocks& blocks, TPushIT Push) override;
        virtual void PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push) override;
        virtual void SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) override {};

        mfxStatus UpdateFrameHeader(
            const mfxExtAV1Segmentation& segPar
            , const FH& fh
            , SegmentationParams& seg) const;

        SegDpbType dpb;
    };

} //Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)