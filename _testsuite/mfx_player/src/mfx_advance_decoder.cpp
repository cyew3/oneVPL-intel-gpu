/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"
#include "mfx_advance_decoder.h"

mfxStatus MFXAdvanceDecoder::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts = m_pTarget->QueryIOSurf(par, request);
    if (sts >= MFX_ERR_NONE && request)
    {
        request->NumFrameMin = request->NumFrameMin + (mfxU16)m_OutputBuffering;
        request->NumFrameSuggested = request->NumFrameSuggested + (mfxU16)m_OutputBuffering;
    }
    return sts;
}

mfxStatus MFXAdvanceDecoder::DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    sDecodedFrame fr;

    if (!m_OutputBuffering)
    {
        return m_pTarget->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
    }

    mfxStatus sts = m_pTarget->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
    if (sts == MFX_ERR_NONE)
    {
        fr.surface_out = *surface_out;
        fr.syncp = *syncp;
        IncreaseReference(&fr.surface_out->Data);
        m_OutputBuffer.push_back(fr);
    }

    if (!bs.isNull)
    {
        if (sts != MFX_ERR_NONE)
        {
            return sts;
        }
        if (m_OutputBuffer.size() <= m_OutputBuffering)
        {
            return MFX_ERR_MORE_DATA;
        }
    } else
    {
        if (sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA)
        {
            return sts;
        }
    }

    if (!m_OutputBuffer.size())
    {
        return MFX_ERR_MORE_DATA;
    }

    fr = m_OutputBuffer.front();
    DecreaseReference(&fr.surface_out->Data);
    *surface_out = fr.surface_out;
    *syncp = fr.syncp;
    m_OutputBuffer.erase(m_OutputBuffer.begin());

    return MFX_ERR_NONE;
}
