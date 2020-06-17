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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "av1ehw_base.h"
#include "av1ehw_ddi.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_iddi_packer.h"
#include "ehw_utils_ddi.h"
#include <array>

namespace MfxEncodeHW
{
    template<> struct DDICbInfo <ENCODE_SET_SEQUENCE_PARAMETERS_AV1>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_SPSDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA };
    };

    template<> struct DDICbInfo <ENCODE_SET_PICTURE_PARAMETERS_AV1>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_PPSDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA };
    };

    template<> struct DDICbInfo <ENCODE_SET_TILE_GROUP_HEADER_AV1>
    {
        static constexpr D3DFORMAT Type[] =
        { D3DDDIFMT_INTELENCODE_SLICEDATA, (D3DFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA };
    };
}

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
using namespace AV1EHW::Base;
using namespace MfxEncodeHW;

class DDIPacker
    : public IDDIPacker
    , protected DDIParPacker<ENCODE_QUERY_STATUS_PARAMS_AV1>
{
public:
    DDIPacker(mfxU32 FeatureId)
        : IDDIPacker(FeatureId)
    {
        SetTraceName("Base_DDIPacker");
        SetStartCodeLen(3, 4);
    }

protected:
    static const mfxU32 MAX_HEADER_BUFFERS = 3; //ivf, sps, pps

    ENCODE_SET_SEQUENCE_PARAMETERS_AV1              m_sps = {};
    ENCODE_SET_PICTURE_PARAMETERS_AV1               m_pps = {};
    std::vector<ENCODE_SET_TILE_GROUP_HEADER_AV1>   m_tile_groups_global;
    std::vector<ENCODE_SET_TILE_GROUP_HEADER_AV1>   m_tile_groups_task;
    std::vector<UCHAR>                              m_segment_map;

    D3DFORMAT
          ID_SPSDATA          = D3DDDIFMT_INTELENCODE_SPSDATA
        , ID_PPSDATA          = D3DDDIFMT_INTELENCODE_PPSDATA
        , ID_TILEGROUPDATA    = D3DDDIFMT_INTELENCODE_SLICEDATA
        , ID_SEGMENTMAP       = D3DDDIFMT_INTELENCODE_MBSEGMENTMAP
        , ID_BITSTREAMDATA    = D3DDDIFMT_INTELENCODE_BITSTREAMDATA
        , ID_PACKEDHEADERDATA = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA
        ;

    eMFXHWType      m_hwType            = MFX_HW_UNKNOWN;

    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;

    void FillSpsBuffer(
        const ExtBuffer::Param<mfxVideoParam>& par
        , const SH& bs_sh
        , ENCODE_SET_SEQUENCE_PARAMETERS_AV1& sps);

    void FillPpsBuffer(
        const ExtBuffer::Param<mfxVideoParam>& par
        , const FH& bs_fh
        , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps);

    void FillPpsBuffer(
        const TaskCommonPar& task
        , const SH& bs_sh
        , const FH& bs_fh
        , ENCODE_SET_PICTURE_PARAMETERS_AV1& pps);

    void FillTileGroupBuffer(
        const TileGroupInfos& infos
        , std::vector<ENCODE_SET_TILE_GROUP_HEADER_AV1>& tile_groups);
};

} //Base
} //Windows
} //namespace AV1EHW

#endif
