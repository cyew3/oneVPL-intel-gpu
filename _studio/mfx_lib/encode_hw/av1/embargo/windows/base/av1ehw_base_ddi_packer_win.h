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
#include <array>

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
using namespace AV1EHW::Base;

enum eDDIParType
{
    DDIPar_In = 0
    , DDIPar_Out
    , DDIPar_Resource
};

template<class T> struct DDICbInfo {};

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

template<class T, class A>
T* GetDDICB(
    mfxU32 func
    , eDDIParType parType
    , eMFXVAType vaType
    , const A& par)
{
    auto it = std::find_if(
        std::begin(par)
        , std::end(par)
        , [func](decltype(*std::begin(par)) ep) { return (ep.Function == func); });

    if (it == std::end(par))
        return nullptr;

    return GetDDICB<T>(parType, vaType, *it);
}

template<class T>
T* GetDDICB(
    eDDIParType parType
    , eMFXVAType vaType
    , const DDIExecParam& ep)
{
    auto& par = (&ep.In)[parType];
    auto pCBD = GetDDICB(DDICbInfo<T>::Type[vaType == MFX_HW_D3D11], par);

    if (!pCBD)
        return nullptr;

    if (pCBD->DataSize < sizeof(T))
        throw std::logic_error("incompatible CompressedBuffer data");

    return (T*)(pCBD->pCompBuffer);
}

inline ENCODE_COMPBUFFERDESC* GetDDICB(
    D3DFORMAT cbType
    , const DDIExecParam::Param& par)
{
    if (par.Size != sizeof(ENCODE_EXECUTE_PARAMS))
        throw std::logic_error("unexpected DDIExecParam::Param format");

    auto& eep = *(ENCODE_EXECUTE_PARAMS*)par.pData;
    auto pEnd = eep.pCompressedBuffers + eep.NumCompBuffers;
    auto pCBD = std::find_if(eep.pCompressedBuffers, pEnd
        , [cbType](ENCODE_COMPBUFFERDESC& cb) { return cb.CompressedBufferType == cbType; });

    if (pCBD != pEnd)
        return pCBD;

    return nullptr;
}

class FeedbackStorage
{
public:
    typedef ENCODE_QUERY_STATUS_PARAMS_AV1 Feedback;

    FeedbackStorage()
        : m_pool_size(0)
        , m_fb_size(sizeof(Feedback))
        , m_type(QUERY_STATUS_PARAM_FRAME)
    {
    }

    inline Feedback& operator[] (size_t i) const
    {
        return *(Feedback*)&m_buf[i * m_fb_size];
    }

    inline size_t size() const
    {
        return m_pool_size;
    }

    inline mfxU32 feedback_size()
    {
        return m_fb_size;
    }

    void Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType);

    const Feedback* Get(mfxU32 feedbackNumber) const;

    mfxStatus Update();

    inline void CacheFeedback(Feedback *fb_dst, Feedback *fb_src);

    mfxStatus Remove(mfxU32 feedbackNumber);

private:
    std::vector<mfxU8>  m_buf;
    std::vector<mfxU8>  m_buf_cache;
    std::vector<mfxU16> m_ssizes;
    std::vector<mfxU16> m_ssizes_cache;
    mfxU32 m_fb_size;
    mfxU16 m_pool_size;
    ENCODE_QUERY_STATUS_PARAM_TYPE m_type;
};

#pragma warning(push)
#pragma warning(disable:4250) //inherits via dominance
class DDIPacker
    : public virtual FeatureBase
    , protected IDDIPacker
{
public:
    DDIPacker(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
        , IDDIPacker(FeatureId)
    {
        SetTraceName("G12_DDIPacker");
    }

protected:
    static const mfxU32 MAX_DDI_BUFFERS = 5; //sps, pps, tile group, segmentation, bs
    static const mfxU32 MAX_HEADER_BUFFERS = 3; //ivf, sps, pps
    enum eResId
    {
          RES_BS = 0
        , RES_RAW
        , RES_REF
        , NUM_RES
    };
    std::vector<ENCODE_PACKEDHEADER_DATA>           m_buf;
    std::vector<ENCODE_PACKEDHEADER_DATA>::iterator m_cur;
    std::vector<ENCODE_COMPBUFFERDESC>              m_cbd;
    std::array<mfxU32, NUM_RES>                     m_resId;
    ENCODE_SET_SEQUENCE_PARAMETERS_AV1              m_sps = {};
    ENCODE_SET_PICTURE_PARAMETERS_AV1               m_pps = {};
    std::vector<ENCODE_SET_TILE_GROUP_HEADER_AV1>   m_tile_groups_global;
    std::vector<ENCODE_SET_TILE_GROUP_HEADER_AV1>   m_tile_groups_task;
    std::vector<UCHAR>                              m_segment_map;
    ENCODE_INPUT_DESC                               m_inputDesc = {};
    ENCODE_EXECUTE_PARAMS                           m_execPar   = {};
    ENCODE_QUERY_STATUS_PARAMS_DESCR                m_query     = {};
    std::map<mfxU32, std::vector<mfxHDLPair>>       m_resources;
    std::vector<mfxHDL>                             m_taskRes;

    D3DFORMAT
          ID_SPSDATA          = D3DDDIFMT_INTELENCODE_SPSDATA
        , ID_PPSDATA          = D3DDDIFMT_INTELENCODE_PPSDATA
        , ID_TILEGROUPDATA    = D3DDDIFMT_INTELENCODE_SLICEDATA
        , ID_SEGMENTMAP       = D3DDDIFMT_INTELENCODE_MBSEGMENTMAP
        , ID_BITSTREAMDATA    = D3DDDIFMT_INTELENCODE_BITSTREAMDATA
        , ID_PACKEDHEADERDATA = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA
        ;

    eMFXVAType      m_vaType            = MFX_HW_NO;
    eMFXHWType      m_hwType            = MFX_HW_UNKNOWN;
    FeedbackStorage m_feedback;

    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;

    mfxStatus Register(
        VideoCORE& core
        , const mfxFrameAllocResponse& response
        , mfxU32 type);

    mfxStatus SetTaskResources(
        Task::Common::TRef& task
        , Glob::DDI_SubmitParam::TRef& submit);

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

    void NewHeader();
    void ResetPackHeaderBuffer();

    ENCODE_PACKEDHEADER_DATA& PackHeader(const PackedData& d);
};
#pragma warning(pop)

} //Base
} //Windows
} //namespace AV1EHW

#endif
