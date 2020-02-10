// Copyright (c) 2014-2020 Intel Corporation
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

#include "mfx_lp_lookahead.h"
#include "mfx_h265_encode_hw.h"

#if defined (MFX_ENABLE_LP_LOOKAHEAD)

mfxStatus MfxLpLookAhead::Init(mfxVideoParam* param)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(param);

    if (m_bInitialized)
        return MFX_ERR_NONE;

    m_pEnc = new MFXVideoENCODEH265_HW(m_core, &mfxRes);
    MFX_CHECK_NULL_PTR1(m_pEnc);

    // following configuration comes from HW recommendation
    mfxVideoParam par         = *param;
    par.AsyncDepth            = 1;
    par.mfx.CodecId           = MFX_CODEC_HEVC;
    par.mfx.LowPower          = MFX_CODINGOPTION_ON;
    par.mfx.NumRefFrame       = 1;
    par.mfx.TargetUsage       = 7;
    par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    par.mfx.CodecProfile      = MFX_PROFILE_HEVC_MAIN;
    par.mfx.CodecLevel        = MFX_LEVEL_HEVC_52;
    par.mfx.QPI               = 30;
    par.mfx.QPP               = 32;
    par.mfx.QPB               = 32;
    par.mfx.NumSlice          = 1;

    //create the bitstream buffer
    memset(&m_bitstream, 0, sizeof(mfxBitstream));
    mfxU32 bufferSize = param->mfx.FrameInfo.Width * param->mfx.FrameInfo.Height * 3/2;
    m_bitstream.Data = new mfxU8[bufferSize];
    m_bitstream.MaxLength = bufferSize;

    mfxRes = m_pEnc->InitInternal(&par);
    m_bInitialized = true;

    return mfxRes;
}

mfxStatus MfxLpLookAhead::Reset(mfxVideoParam* param)
{
    (void*)param;
    // TODO: will implement it later
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MfxLpLookAhead::Close()
{
    if (!m_bInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (m_bitstream.Data)
    {
        delete[]  m_bitstream.Data;
        m_bitstream.Data = nullptr;
    }
    if (m_pEnc)
    {
        delete m_pEnc;
        m_pEnc = nullptr;
    }

    m_bInitialized = false;
    return MFX_ERR_NONE;
}

mfxStatus MfxLpLookAhead::Submit(mfxFrameSurface1 * surface)
{
    mfxStatus mfxRes;
    MFX_CHECK_NULL_PTR1(surface);

    if (!m_bInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    m_bitstream.DataLength = 0;
    m_bitstream.DataOffset = 0;

    mfxFrameSurface1 *reordered_surface = nullptr;
    mfxEncodeInternalParams internal_params;
    MFX_ENTRY_POINT entryPoint = {};

    mfxRes = m_pEnc->EncodeFrameCheck(
        nullptr,
        surface,
        &m_bitstream,
        &reordered_surface,
        &internal_params,
        &entryPoint);
    MFX_CHECK_STS(mfxRes);

    mfxRes = entryPoint.pRoutine(entryPoint.pState, entryPoint.pParam, 0, 0);
    if (mfxRes == MFX_ERR_NONE)
    {
        MfxHwH265Encode::Task * task = (MfxHwH265Encode::Task*)entryPoint.pParam;
        if (task->m_cqmHint != CQM_HINT_INVALID)
            m_cqmHint.push_back(task->m_cqmHint);
    }
    //else return to h264 encoder pipeline and leverage external scheduler for task scheduling

    return mfxRes;
}

mfxStatus MfxLpLookAhead::Query(mfxU8 *cqmHint)
{
    MFX_CHECK_NULL_PTR1(cqmHint);

    if (!m_bInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (m_cqmHint.empty())
    {
        return MFX_ERR_NOT_FOUND;
    }

    *cqmHint = m_cqmHint.front();
    m_cqmHint.pop_front();

    return MFX_ERR_NONE;
}

#endif