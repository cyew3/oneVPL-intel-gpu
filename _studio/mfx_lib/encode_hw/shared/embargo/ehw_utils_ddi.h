// Copyright (c) 2020 Intel Corporation
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
#include "ehw_device.h"
#include "encoding_ddi.h"
#include <algorithm>

namespace MfxEncodeHW
{

enum eDDIParType
{
    DDIPar_In = 0
    , DDIPar_Out
    , DDIPar_Resource
};

template<class T> struct DDICbInfo {};

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
    auto  pCBD = GetDDICB(DDICbInfo<T>::Type[vaType == MFX_HW_D3D11], par);

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

    auto& eep           = *(ENCODE_EXECUTE_PARAMS*)par.pData;
    auto  IsRequestedCB = [cbType](ENCODE_COMPBUFFERDESC& cb) { return cb.CompressedBufferType == cbType; };
    auto  pEnd          = eep.pCompressedBuffers + eep.NumCompBuffers;
    auto  pCBD          = std::find_if(eep.pCompressedBuffers, pEnd, IsRequestedCB);

    if (pCBD != pEnd)
        return pCBD;

    return nullptr;
}

template<class T>
class FeedbackStorage
{
public:
    typedef T Feedback;

    FeedbackStorage()
        : m_pool_size(0)
        , m_fb_size(0)
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

    inline mfxU32 GetFeedBackSize()
    {
        return m_fb_size;
    }

    void Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType, mfxU32 maxSlices);

    const Feedback* Get(mfxU32 feedbackNumber) const;

    mfxStatus Update();

    inline void CacheFeedback(Feedback *fb_dst, Feedback *fb_src);

    mfxStatus Remove(mfxU32 feedbackNumber);

    ENCODE_QUERY_STATUS_PARAM_TYPE GetType() { return m_type; }

private:
    std::vector<mfxU8>             m_buf;
    std::vector<mfxU8>             m_buf_cache;
    std::vector<mfxU16>            m_ssizes;
    std::vector<mfxU16>            m_ssizes_cache;
    mfxU32                         m_fb_size;
    mfxU16                         m_pool_size;
    ENCODE_QUERY_STATUS_PARAM_TYPE m_type;

    class FBIter
        : public mfx::IterStepWrapper<mfxU8*>
    {
    public:
        FBIter(const IterStepWrapper<mfxU8*>& other)
            : IterStepWrapper<mfxU8*>(other)
        {
        }
        Feedback* operator->() { return (Feedback*)IterStepWrapper<mfxU8*>::operator->(); }
        Feedback& operator*() { return *operator->(); }
    };
    FBIter m_cacheBegin = mfx::MakeStepIter<mfxU8*>(nullptr);
    FBIter m_cacheEnd   = mfx::MakeStepIter<mfxU8*>(nullptr);
    FBIter m_bufBegin   = mfx::MakeStepIter<mfxU8*>(nullptr);
    FBIter m_bufEnd     = mfx::MakeStepIter<mfxU8*>(nullptr);

    struct IsSameId
    {
        IsSameId(mfxU32 id) : m_id(id) {}
        bool operator()(const Feedback& fb) { return fb.StatusReportFeedbackNumber == m_id; };
        mfxU32 m_id;
    };
    template<UCHAR STS>
    static bool IsStatus(const Feedback& fb) { return fb.bStatus == STS; }
};

template<class T>
class DDIParPacker
{
public:
    enum eResId
    {
        RES_BS = 0
        , RES_RAW
        , RES_REF
        , RES_CUQP
        , NUM_RES
    };

    void SetMaxBs(mfxU32 sizeInBytes)
    {
        m_maxBsSize = sizeInBytes;
    }

    void SetStartCodeLen(mfxU32 scShortBytes, mfxU32 scLongBytes)
    {
        m_startCodeLen[0] = scShortBytes;
        m_startCodeLen[1] = scLongBytes;
    }

    void SetVaType(eMFXVAType type)
    {
        m_vaType = type;
    }

    mfxStatus Register(
        VideoCORE& core
        , const mfxFrameAllocResponse& response
        , mfxU32 type);

    mfxU32& GetResId(mfxU32 type) { return m_resId.at(type); }

    void InitFeedback(
        mfxU16 cacheSize
        , ENCODE_QUERY_STATUS_PARAM_TYPE fbType
        , mfxU32 maxSlices)
    {
        m_feedback.Reset(cacheSize, fbType, maxSlices);
    }

    mfxStatus ReadFeedback(const void* pFB, mfxU32 fbSize, mfxU32& bsSize);

    void GetFeedbackInterface(DDIFeedback& fb);

    void BeginPicture()
    {
        m_cbd.clear();
        m_phBuf.clear();
        m_execPar = {};
    }

    ENCODE_INPUT_DESC* SetTaskResources(
        DDIExecParam::Param& resource
        , mfxHDLPair hdlRaw
        , mfxU32 idxBS
        , mfxU32 idxRec
        , mfxU32 idxCUQP = mfxU32(-1));

    ENCODE_PACKEDHEADER_DATA& PackHeader(const PackedData& d, bool bLenInBytes);

    template<class TT>
    bool PackCBD(D3DFORMAT id, std::vector<TT>& src)
    {
        m_cbd.push_back({});

        auto& dst                = m_cbd.back();
        dst.CompressedBufferType = id;
        dst.DataSize             = UINT(sizeof(TT) * src.size());
        dst.pCompBuffer          = src.data();

        return true;
    }

    template<class TT>
    bool PackCBD(D3DFORMAT id, TT& src)
    {
        m_cbd.push_back({});

        auto& dst                = m_cbd.back();
        dst.CompressedBufferType = id;
        dst.DataSize             = (UINT)sizeof(TT);
        dst.pCompBuffer          = &src;

        return true;
    }

    ENCODE_EXECUTE_PARAMS* EndPicture()
    {
        m_execPar.NumCompBuffers     = UINT(m_cbd.size());
        m_execPar.pCompressedBuffers = m_cbd.data();
        return &m_execPar;
    }

protected:
    std::map<mfxU32, std::vector<mfxHDLPair>> m_resources       = {};
    std::vector<mfxU32>                       m_resId           = std::vector<mfxU32>(NUM_RES, 0);
    ENCODE_EXECUTE_PARAMS                     m_execPar         = {};
    ENCODE_QUERY_STATUS_PARAMS_DESCR          m_queryPar        = {};
    ENCODE_INPUT_DESC                         m_inputDesc       = {};
    mfxU32                                    m_startCodeLen[2] = {0, 0};
    mfxU32                                    m_maxBsSize       = mfxU32(-1);
    FeedbackStorage<T>                        m_feedback        = FeedbackStorage<T>();
    eMFXVAType                                m_vaType          = MFX_HW_NO;
    std::vector<mfxHDL>                       m_taskRes;
    std::vector<ENCODE_COMPBUFFERDESC>        m_cbd;
    std::list<ENCODE_PACKEDHEADER_DATA>       m_phBuf;
};

template<class T>
void FeedbackStorage<T>::Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType, mfxU32 maxSlices)
{
    assert(fbType == QUERY_STATUS_PARAM_FRAME || fbType == QUERY_STATUS_PARAM_SLICE);

    m_type      = fbType;
    m_pool_size = cacheSize;
    if (m_type == QUERY_STATUS_PARAM_SLICE)
        m_fb_size = sizeof(ENCODE_QUERY_STATUS_SLICE_PARAMS);
    else
        m_fb_size = sizeof(T);

    m_buf.resize(m_fb_size * m_pool_size);
    m_bufBegin = mfx::MakeStepIter(m_buf.data(), m_fb_size);
    m_bufEnd   = mfx::MakeStepIter(m_buf.data() + m_buf.size(), m_fb_size);

    m_buf_cache.resize(m_fb_size * m_pool_size);
    m_cacheBegin = mfx::MakeStepIter(m_buf_cache.data(), m_fb_size);
    m_cacheEnd   = mfx::MakeStepIter(m_buf_cache.data() + m_buf_cache.size(), m_fb_size);

    auto MarkNotAvailable = [](Feedback& fb) { fb.bStatus = ENCODE_NOTAVAILABLE; };
    std::for_each(m_cacheBegin, m_cacheEnd, MarkNotAvailable);

    if (m_type == QUERY_STATUS_PARAM_SLICE)
    {
        mfxU16* pSSize = nullptr;
        auto SetSSize = [&](Feedback& r)
        {
            auto& fb = *(ENCODE_QUERY_STATUS_SLICE_PARAMS*)&r;
            fb.SizeOfSliceSizesBuffer   = maxSlices;
            fb.pSliceSizes              = pSSize;
            pSSize += maxSlices;
        };

        m_ssizes.resize(maxSlices * m_pool_size);
        pSSize = m_ssizes.data();
        std::for_each(m_bufBegin, m_bufEnd, SetSSize);

        m_ssizes_cache.resize(maxSlices * m_pool_size);
        pSSize = m_ssizes_cache.data();
        std::for_each(m_cacheBegin, m_cacheEnd, SetSSize);
    }
}

template<class T>
const T* FeedbackStorage<T>::Get(mfxU32 id) const
{
    auto it = std::find_if(m_cacheBegin, m_cacheEnd, IsSameId(id));

    if (it == m_cacheEnd)
        return nullptr;

    return &*it;
}

template<class T>
mfxStatus FeedbackStorage<T>::Update()
{
    auto CacheUpdate = [this](Feedback& fb)
    {
        auto it = find_if(m_cacheBegin, m_cacheEnd, IsSameId(fb.StatusReportFeedbackNumber));
        if (it == m_cacheEnd)
        {
            it = find_if(m_cacheBegin, m_cacheEnd, IsStatus<ENCODE_NOTAVAILABLE>);
        }
        if (it == m_cacheEnd)
            throw std::logic_error("Feedback cache overflow");
        CacheFeedback(&*it, &fb);
    };

    std::list<std::reference_wrapper<Feedback>> toUpdate(m_bufBegin, m_bufEnd);

    toUpdate.remove_if(IsStatus<ENCODE_NOTAVAILABLE>);
    std::for_each(toUpdate.begin(), toUpdate.end(), CacheUpdate);

    return MFX_ERR_NONE;
}

template<class T>
inline void FeedbackStorage<T>::CacheFeedback(Feedback *fb_dst, Feedback *fb_src)
{
    *fb_dst = *fb_src;

    if (m_type == QUERY_STATUS_PARAM_SLICE)
    {
        auto pdst = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)fb_dst;
        auto psrc = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)fb_src;
        std::copy_n(psrc->pSliceSizes, psrc->FrameLevelStatus.NumberSlices, pdst->pSliceSizes);
    }
}

template<class T>
mfxStatus FeedbackStorage<T>::Remove(mfxU32 id)
{
    auto it = std::find_if(m_cacheBegin, m_cacheEnd, IsSameId(id));

    MFX_CHECK(it != m_cacheEnd, MFX_ERR_UNDEFINED_BEHAVIOR);

    it->bStatus = ENCODE_NOTAVAILABLE;

    return MFX_ERR_NONE;
}

template<class T>
mfxStatus DDIParPacker<T>::Register(
    VideoCORE& core
    , const mfxFrameAllocResponse& response
    , mfxU32 type)
{
    auto& res = m_resources[type];

    res.resize(response.NumFrameActual, {});

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = core.GetFrameHDL(response.mids[i], (mfxHDL*)&res[i]);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

template<class T>
ENCODE_INPUT_DESC* DDIParPacker<T>::SetTaskResources(
    DDIExecParam::Param& resource
    , mfxHDLPair hdlRaw
    , mfxU32 idxBS
    , mfxU32 idxRec
    , mfxU32 idxCUQP)
{
    m_resId[RES_BS]   = idxBS;
    m_resId[RES_CUQP] = idxCUQP;

    if (m_vaType != MFX_HW_D3D11)
        return nullptr;

    bool   bCUQPMap = m_resources.count(RES_CUQP) && m_resources.at(RES_CUQP).size() > idxCUQP;
    auto&  ref      = m_resources.at(RES_REF);
    auto   GetFirst = [](mfxHDLPair pair) { return pair.first; };

    m_taskRes.clear();

    m_resId[RES_BS] = mfxU32(m_taskRes.size());
    m_taskRes.push_back(m_resources.at(RES_BS).at(idxBS).first);

    m_resId[RES_RAW] = mfxU32(m_taskRes.size());
    m_taskRes.push_back(hdlRaw.first);

    m_resId[RES_REF] = mfxU32(m_taskRes.size());
    std::transform(ref.begin(), ref.end(), std::back_inserter(m_taskRes), GetFirst);

    if (bCUQPMap)
    {
        m_resId[RES_CUQP] = mfxU32(m_taskRes.size());
        m_taskRes.push_back(m_resources.at(RES_CUQP).at(idxCUQP).first);
    }

    resource.pData = m_taskRes.data();
    resource.Size  = mfxU32(m_taskRes.size());

    m_inputDesc = {};
    m_inputDesc.IndexOriginal       = m_resId[RES_RAW];
    m_inputDesc.ArraSliceOriginal   = (UINT)(UINT_PTR)(hdlRaw.second);
    m_inputDesc.IndexRecon          = m_resId[RES_REF] + idxRec;
    m_inputDesc.ArraySliceRecon     = (UINT)(UINT_PTR)(ref.at(idxRec).second);

    return &m_inputDesc;
}

template<class T>
ENCODE_PACKEDHEADER_DATA& DDIParPacker<T>::PackHeader(const PackedData& d, bool bLenInBytes)
{
    m_phBuf.push_back({});

    auto& phd = m_phBuf.back();

    phd.pData                  = d.pData;
    phd.BufferSize             = (d.BitLen + 7) / 8;
    phd.DataLength             = d.BitLen * !bLenInBytes + phd.BufferSize * bLenInBytes;
    phd.SkipEmulationByteCount = m_startCodeLen[!!d.bLongSC];

    return phd;
}

template<class T>
mfxStatus DDIParPacker<T>::ReadFeedback(const void* pFB, mfxU32 fbSize, mfxU32& bsSize)
{
    MFX_CHECK(pFB, MFX_ERR_DEVICE_FAILED);

    if (fbSize < sizeof(T))
        throw std::logic_error("Invalid feedback");

    auto pFeedback = (const T*)pFB;
    bool bFeedbackValid =
        ((pFeedback->bStatus == ENCODE_OK) || (pFeedback->bStatus == ENCODE_OK_WITH_MISMATCH))
        && (m_maxBsSize >= pFeedback->bitstreamSize)
        && (pFeedback->bitstreamSize > 0);

    assert(pFeedback->bStatus != ENCODE_OK_WITH_MISMATCH); //slice sizes buffer is too small
    MFX_CHECK(pFeedback->bStatus != ENCODE_NOTREADY, MFX_TASK_BUSY);
    assert(bFeedbackValid); //bad feedback status

    bsSize = pFeedback->bitstreamSize;

    MFX_CHECK(bFeedbackValid, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

template<class T>
void DDIParPacker<T>::GetFeedbackInterface(DDIFeedback& fb)
{
    fb = DDIFeedback();

    m_queryPar = {};
    m_queryPar.StatusParamType = m_feedback.GetType();
    m_queryPar.SizeOfStatusParamStruct = m_feedback.GetFeedBackSize();

    DDIExecParam xPar;
    xPar.Function   = ENCODE_QUERY_STATUS_ID;
    xPar.In.pData   = &m_queryPar;
    xPar.In.Size    = sizeof(m_queryPar);
    xPar.Out.pData  = &m_feedback[0];
    xPar.Out.Size   = (mfxU32)m_feedback.size() * m_feedback.GetFeedBackSize();

    fb.ExecParam.emplace_back(std::move(xPar));

    fb.Get.Push([this](DDIFeedback::TGet::TExt, mfxU32 id) { return m_feedback.Get(id); });
    fb.CheckStatus.Push([this](DDIFeedback::TCheckStatus::TExt, mfxU32 id)
        {
            auto pFB = static_cast<const T*> (m_feedback.Get(id));
            if (pFB && pFB->bStatus == ENCODE_OK)
                return true;
            else
                return false;
    });
    fb.Update.Push([this](DDIFeedback::TUpdate::TExt, mfxU32) { return m_feedback.Update(); });
    fb.Remove.Push([this](DDIFeedback::TUpdate::TExt, mfxU32 id) { return m_feedback.Remove(id); });
}

mfxStatus QueryFeedbackStatus(Device& dev, DDIFeedback& ddiFB, mfxU32 id);

} //namespace MfxEncodeHW