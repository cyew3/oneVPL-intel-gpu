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

    template<class T>
    bool PackCBD(D3DFORMAT id, std::vector<T>& src)
    {
        m_cbd.push_back({});

        auto& dst                = m_cbd.back();
        dst.CompressedBufferType = id;
        dst.DataSize             = UINT(sizeof(T) * src.size());
        dst.pCompBuffer          = src.data();

        return true;
    }

    template<class T>
    bool PackCBD(D3DFORMAT id, T& src)
    {
        m_cbd.push_back({});

        auto& dst                = m_cbd.back();
        dst.CompressedBufferType = id;
        dst.DataSize             = (UINT)sizeof(T);
        dst.pCompBuffer          = &src;

        return true;
    }

    ENCODE_EXECUTE_PARAMS* EndPicture()
    {
        m_execPar.NumCompBuffers     = UINT(m_cbd.size());
        m_execPar.pCompressedBuffers = m_cbd.data();
        return &m_execPar;
    }

    static mfxStatus QueryStatus(Device& dev, DDIFeedback& fb, mfxU32 id);

protected:
    std::map<mfxU32, std::vector<mfxHDLPair>> m_resources       = {};
    std::vector<mfxU32>                       m_resId           = std::vector<mfxU32>(NUM_RES, 0);
    ENCODE_EXECUTE_PARAMS                     m_execPar         = {};
    ENCODE_QUERY_STATUS_PARAMS_DESCR          m_queryPar        = {};
    ENCODE_INPUT_DESC                         m_inputDesc       = {};
    mfxU32                                    m_startCodeLen[2] = {0, 0};
    mfxU32                                    m_maxBsSize       = mfxU32(-1);
    FeedbackStorage                           m_feedback        = FeedbackStorage();
    eMFXVAType                                m_vaType          = MFX_HW_NO;
    std::vector<mfxHDL>                       m_taskRes;
    std::vector<ENCODE_COMPBUFFERDESC>        m_cbd;
    std::list<ENCODE_PACKEDHEADER_DATA>       m_phBuf;
};

} //namespace MfxEncodeHW