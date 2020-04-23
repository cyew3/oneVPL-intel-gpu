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

#include "ehw_utils_ddi.h"

namespace MfxEncodeHW
{
void FeedbackStorage::Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType, mfxU32 maxSlices)
{
    assert(fbType == QUERY_STATUS_PARAM_FRAME || fbType == QUERY_STATUS_PARAM_SLICE);

    m_type      = fbType;
    m_pool_size = cacheSize;
    m_fb_size   =
          (m_type == QUERY_STATUS_PARAM_SLICE)
        ? sizeof(ENCODE_QUERY_STATUS_SLICE_PARAMS)
        : sizeof(ENCODE_QUERY_STATUS_PARAMS);

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

const FeedbackStorage::Feedback* FeedbackStorage::Get(mfxU32 id) const
{
    auto it = std::find_if(m_cacheBegin, m_cacheEnd, IsSameId(id));

    if (it == m_cacheEnd)
        return nullptr;

    return &*it;
}

mfxStatus FeedbackStorage::Update()
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

inline void FeedbackStorage::CacheFeedback(Feedback *fb_dst, Feedback *fb_src)
{
    *fb_dst = *fb_src;

    if (m_type == QUERY_STATUS_PARAM_SLICE)
    {
        auto pdst = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)fb_dst;
        auto psrc = (ENCODE_QUERY_STATUS_SLICE_PARAMS *)fb_src;
        std::copy_n(psrc->pSliceSizes, psrc->FrameLevelStatus.NumberSlices, pdst->pSliceSizes);
    }
}

mfxStatus FeedbackStorage::Remove(mfxU32 id)
{
    auto it = std::find_if(m_cacheBegin, m_cacheEnd, IsSameId(id));

    MFX_CHECK(it != m_cacheEnd, MFX_ERR_UNDEFINED_BEHAVIOR);

    it->bStatus = ENCODE_NOTAVAILABLE;

    return MFX_ERR_NONE;
}

mfxStatus DDIParPacker::Register(
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

ENCODE_INPUT_DESC* DDIParPacker::SetTaskResources(
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

ENCODE_PACKEDHEADER_DATA& DDIParPacker::PackHeader(const PackedData& d, bool bLenInBytes)
{
    m_phBuf.push_back({});

    auto& phd = m_phBuf.back();

    phd.pData                  = d.pData;
    phd.BufferSize             = (d.BitLen + 7) / 8;
    phd.DataLength             = d.BitLen * !bLenInBytes + phd.BufferSize * bLenInBytes;
    phd.SkipEmulationByteCount = m_startCodeLen[!!d.bLongSC];

    return phd;
}

mfxStatus DDIParPacker::ReadFeedback(const void* pFB, mfxU32 fbSize, mfxU32& bsSize)
{
    MFX_CHECK(pFB, MFX_ERR_DEVICE_FAILED);

    if (fbSize < sizeof(ENCODE_QUERY_STATUS_PARAMS))
        throw std::logic_error("Invalid feedback");

    auto pFeedback = (const ENCODE_QUERY_STATUS_PARAMS*)pFB;
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

void DDIParPacker::GetFeedbackInterface(DDIFeedback& fb)
{
    fb = DDIFeedback();

    m_queryPar = {};
    m_queryPar.StatusParamType = m_feedback.GetType();
    m_queryPar.SizeOfStatusParamStruct = m_feedback.feedback_size();

    DDIExecParam xPar;
    xPar.Function   = ENCODE_QUERY_STATUS_ID;
    xPar.In.pData   = &m_queryPar;
    xPar.In.Size    = sizeof(m_queryPar);
    xPar.Out.pData  = &m_feedback[0];
    xPar.Out.Size   = (mfxU32)m_feedback.size() * m_feedback.feedback_size();

    fb.ExecParam.emplace_back(std::move(xPar));

    fb.Get.Push([this](DDIFeedback::TGet::TExt, mfxU32 id) { return m_feedback.Get(id); });
    fb.Update.Push([this](DDIFeedback::TUpdate::TExt, mfxU32) { return m_feedback.Update(); });
    fb.Remove.Push([this](DDIFeedback::TUpdate::TExt, mfxU32 id) { return m_feedback.Remove(id); });
}

mfxStatus DDIParPacker::QueryStatus(Device& dev, DDIFeedback& ddiFB, mfxU32 id)
{
    auto pFB = (const ENCODE_QUERY_STATUS_PARAMS*)ddiFB.Get(id);

    if (pFB && pFB->bStatus == ENCODE_OK)
        return MFX_ERR_NONE;

    for (auto& xPar : ddiFB.ExecParam)
    {
        auto sts = dev.Execute(xPar);
        ddiFB.bNotReady = (dev.GetLastErr() == D3DERR_WASSTILLDRAWING);
        MFX_CHECK(!ddiFB.bNotReady, MFX_ERR_NONE);
        MFX_CHECK_STS(sts);
    }

    return ddiFB.Update(id);
}

} //namespace MfxEncodeHW