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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "hevcehw_base.h"
#include "hevcehw_ddi.h"
#include "hevcehw_g9_data.h"
#include "hevcehw_g9_iddi_packer.h"
#include "ehw_utils_ddi.h"
#include <array>

namespace MfxEncodeHW
{
    template<> struct DDICbInfo <ENCODE_SET_SEQUENCE_PARAMETERS_HEVC>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_SPSDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA };
    };
    template<> struct DDICbInfo <ENCODE_SET_PICTURE_PARAMETERS_HEVC>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_PPSDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA };
    };
    template<> struct DDICbInfo <ENCODE_SET_SLICE_HEADER_HEVC>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_SLICEDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA };
    };
    template<> struct DDICbInfo <ENCODE_SET_QMATRIX_HEVC>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_QUANTDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA };
    };
}

namespace HEVCEHW
{
namespace Windows
{
namespace Gen9
{
using namespace HEVCEHW::Gen9;

using namespace MfxEncodeHW;

class DDIPacker
    : public IDDIPacker
    , protected DDIParPacker
{
public:
    DDIPacker(mfxU32 FeatureId)
        : IDDIPacker(FeatureId)
    {
        SetTraceName("G9_DDIPacker");
        SetStartCodeLen(3, 4);
    }

    struct CallChains
        : Storable
    {
        using TInitSPS = CallChain<void, const StorageR&, ENCODE_SET_SEQUENCE_PARAMETERS_HEVC&>;
        TInitSPS InitSPS;

        using TInitPPS = CallChain<void, const StorageR&, ENCODE_SET_PICTURE_PARAMETERS_HEVC&>;
        TInitPPS InitPPS;

        using TUpdatePPS = CallChain<void
            , const StorageR& //global
            , const StorageR& //task
            , const ENCODE_SET_SEQUENCE_PARAMETERS_HEVC&
            , ENCODE_SET_PICTURE_PARAMETERS_HEVC&>;
        TUpdatePPS UpdatePPS;

        using TInitFeedback = CallChain<void
            , const StorageR&
            , mfxU16 //cacheSize
            , ENCODE_QUERY_STATUS_PARAM_TYPE //fbType
            , mfxU32>; //maxSlices
        TInitFeedback InitFeedback;

        using TReadFeedback = CallChain<mfxStatus
            , const StorageR&
            , StorageW& //task
            , const void* //pFeedback
            , mfxU32>; //size
        TReadFeedback ReadFeedback;
    };

    using CC = StorageVar<Gen9::Glob::ReservedKey0, CallChains>;

protected:
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC             m_sps       = {};
    ENCODE_SET_PICTURE_PARAMETERS_HEVC              m_pps       = {};
    ENCODE_SET_QMATRIX_HEVC                         m_qMatrix   = {};
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC>       m_slices;

    D3DFORMAT
          ID_SPSDATA          = D3DDDIFMT_INTELENCODE_SPSDATA
        , ID_PPSDATA          = D3DDDIFMT_INTELENCODE_PPSDATA
        , ID_SLICEDATA        = D3DDDIFMT_INTELENCODE_SLICEDATA
        , ID_BITSTREAMDATA    = D3DDDIFMT_INTELENCODE_BITSTREAMDATA
        , ID_MBQPDATA         = D3DDDIFMT_INTELENCODE_MBQPDATA
        , ID_PACKEDHEADERDATA = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA
        , ID_PACKEDSLICEDATA  = D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA
        , ID_QUANTDATA        = D3DDDIFMT_INTELENCODE_QUANTDATA
        ;
    bool            m_bResetBRC         = false;
    mfxU32          m_numSkipFrames     = 0;
    mfxU32          m_sizeSkipFrames    = 0;
    eMFXHWType      m_hwType            = MFX_HW_UNKNOWN;

    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;


    void FillSpsBuffer(
        const ExtBuffer::Param<mfxVideoParam>& par
        , const SPS& bs_sps
        , ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps);

    void FillPpsBuffer(
        const ExtBuffer::Param<mfxVideoParam>& par
        , const PPS& bs_pps
        , ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

    void FillSliceBuffer(
        const std::vector<SliceInfo>& sinfo
        , std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

    void FillPpsBuffer(
        const TaskCommonPar & task
        , const Slice & sh
        , const ENCODE_CAPS_HEVC & caps
        , ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

    void FillSliceBuffer(
        const Slice & sh
        , const mfxU8(&rpl)[2][15]
        , bool bLast
        , ENCODE_SET_SLICE_HEADER_HEVC & cs);

    void FillSliceBuffer(
        const Slice & sh
        , const mfxU8(&rpl)[2][15]
        , std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
    {
        for (mfxU16 i = 0; i < slice.size(); i++)
            FillSliceBuffer(sh, rpl, (i == slice.size() - 1), slice[i]);
    }
};

} //Gen9
} //Windows
} //namespace HEVCEHW

#endif
