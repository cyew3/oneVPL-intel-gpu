// Copyright (c) 2019 Intel Corporation
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
#include <array>

namespace HEVCEHW
{
namespace Windows
{
namespace Gen9
{
using namespace HEVCEHW::Gen9;

enum eDDIParType
{
    DDIPar_In = 0
    , DDIPar_Out
    , DDIPar_Resource
};

template<class T> struct DDICbInfo {};
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
    typedef ENCODE_QUERY_STATUS_PARAMS Feedback;

    FeedbackStorage()
        : m_pool_size(0)
        , m_fb_size(0)
        , m_type(QUERY_STATUS_PARAM_FRAME)
    {
    }

    inline ENCODE_QUERY_STATUS_PARAMS& operator[] (size_t i) const
    {
        return *(ENCODE_QUERY_STATUS_PARAMS*)&m_buf[i * m_fb_size];
    }

    inline size_t size() const
    {
        return m_pool_size;
    }

    inline mfxU32 feedback_size()
    {
        return m_fb_size;
    }

    void Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType, mfxU32 maxSlices);

    const Feedback* Get(mfxU32 feedbackNumber) const;

    mfxStatus Update();

    inline void CacheFeedback(Feedback *fb_dst, Feedback *fb_src);

    mfxStatus Remove(mfxU32 feedbackNumber);

private:
    std::vector<mfxU8> m_buf;
    std::vector<mfxU8> m_buf_cache;
    std::vector<mfxU16> m_ssizes;
    std::vector<mfxU16> m_ssizes_cache;
    mfxU32 m_fb_size;
    mfxU16 m_pool_size;
    ENCODE_QUERY_STATUS_PARAM_TYPE m_type;

    class FBIter
        : public IterStepWrapper<mfxU8*>
    {
    public:
        FBIter(const IterStepWrapper<mfxU8*>& other)
            : IterStepWrapper<mfxU8*>(other)
        {
        }
        Feedback* operator->() { return (Feedback*)IterStepWrapper<mfxU8*>::operator->(); }
        Feedback& operator*() { return *operator->(); }
    };
    FBIter m_cacheBegin = MakeStepIter<mfxU8*>(nullptr);
    FBIter m_cacheEnd   = MakeStepIter<mfxU8*>(nullptr);
    FBIter m_bufBegin   = MakeStepIter<mfxU8*>(nullptr);
    FBIter m_bufEnd     = MakeStepIter<mfxU8*>(nullptr);

    struct IsSameId
    {
        IsSameId(mfxU32 id) : m_id(id) {}
        bool operator()(const Feedback& fb) { return fb.StatusReportFeedbackNumber == m_id; };
        mfxU32 m_id;
    };
    struct IsNotAvailable
    {
        bool operator()(const Feedback& fb) { return fb.bStatus == ENCODE_NOTAVAILABLE; };
    };
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
        SetTraceName("G9_DDIPacker");
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
    static const mfxU32 MAX_DDI_BUFFERS = 4; //sps, pps, slice, bs
    enum eResId
    {
          RES_BS = 0
        , RES_RAW
        , RES_REF
        , RES_CUQP
        , NUM_RES
    };
    std::vector<ENCODE_PACKEDHEADER_DATA>           m_buf;
    std::vector<ENCODE_PACKEDHEADER_DATA>::iterator m_cur;
    std::vector<ENCODE_COMPBUFFERDESC>              m_cbd;
    std::array<mfxU32, NUM_RES>                     m_resId;
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC             m_sps;
    ENCODE_SET_PICTURE_PARAMETERS_HEVC              m_pps;
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC>       m_slices;
    ENCODE_INPUT_DESC                               m_inputDesc;
    ENCODE_EXECUTE_PARAMS                           m_execPar;
    ENCODE_QUERY_STATUS_PARAMS_DESCR                m_query;
    std::map<mfxU32, std::vector<mfxHDLPair>>       m_resources;
    std::vector<mfxHDL>                             m_taskRes;

    D3DFORMAT
          ID_SPSDATA          = D3DDDIFMT_INTELENCODE_SPSDATA
        , ID_PPSDATA          = D3DDDIFMT_INTELENCODE_PPSDATA
        , ID_SLICEDATA        = D3DDDIFMT_INTELENCODE_SLICEDATA
        , ID_BITSTREAMDATA    = D3DDDIFMT_INTELENCODE_BITSTREAMDATA
        , ID_MBQPDATA         = D3DDDIFMT_INTELENCODE_MBQPDATA
        , ID_PACKEDHEADERDATA = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA
        , ID_PACKEDSLICEDATA  = D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA
        ;
    bool            m_bResetBRC         = false;
    mfxU32          m_numSkipFrames     = 0;
    mfxU32          m_sizeSkipFrames    = 0;
    eMFXVAType      m_vaType            = MFX_HW_NO;
    eMFXHWType      m_hwType            = MFX_HW_UNKNOWN;
    FeedbackStorage m_feedback;

    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
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

    void NewHeader();
    void Reset(size_t MaxSlices);

    ENCODE_PACKEDHEADER_DATA& PackHeader(const PackedData& d);
    ENCODE_PACKEDHEADER_DATA& PackSliceHeader(const PackedData& d);
};
#pragma warning(pop)

} //Gen9
} //Windows
} //namespace HEVCEHW

#endif
