/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_loop_decoder.h"
//#include "mfx_io_utils.h"

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
#ifndef _WIN32_WCE
#include <process.h>
#endif /* _WIN32_WCE */
#include <windows.h>
#endif

MFXLoopDecoder::MFXLoopDecoder( mfxI32 nNumFramesInLoop, std::unique_ptr<IYUVSource> &&target )
    : base(std::move(target))
    , m_CurrSurfaceIndex()
    , m_syncPoint((mfxSyncPoint)0x101) //some random constant to distinguish from zero
{
    m_Surfaces.reserve(nNumFramesInLoop);
    
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    SetThreadPriority(GetCurrentThread(), 4 /*THREAD_PRIORITY_HIGHEST*/);
#endif
}

mfxStatus MFXLoopDecoder::QueryIOSurf(mfxVideoParam * par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_POINTER(request);

    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_STS(sts = base::QueryIOSurf(par, request));

    request->NumFrameSuggested = (mfxU16)(request->NumFrameSuggested + m_Surfaces.capacity());
    request->NumFrameMin       = (mfxU16)(request->NumFrameMin       + m_Surfaces.capacity());

    return sts;
}

mfxStatus MFXLoopDecoder::DecodeFrameAsync( mfxBitstream2 & bs2
                                          , mfxFrameSurface1 * surface_work
                                          , mfxFrameSurface1 **surface_out
                                          , mfxSyncPoint *syncp)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    //not enough frames buffered yet
    if (m_Surfaces.capacity() > m_Surfaces.size())
    {
        MPA_TRACE("LoopDecoder::Buffering");
        MFX_CHECK_STS_SKIP(sts = base::DecodeFrameAsync(bs2, surface_work, surface_out, syncp)
            , MFX_ERR_MORE_DATA
            , MFX_ERR_MORE_SURFACE
            , MFX_WRN_DEVICE_BUSY);
    
        //handled by upstream
        if (NULL == *surface_out)
            return sts;

        IncreaseReference(&(*surface_out)->Data);

        m_Surfaces.push_back(*surface_out);

        MFX_CHECK_STS(sts = base::SyncOperation(*syncp, TimeoutVal<>::val()));

        sts = MFX_ERR_MORE_DATA;
    }else
    {
        *surface_out = m_Surfaces[m_CurrSurfaceIndex];
        *syncp = m_syncPoint;

        // get just next surface in the list
        m_CurrSurfaceIndex = (m_CurrSurfaceIndex + 1u) % (mfxU16)m_Surfaces.size();
    }

    return sts;
}

mfxStatus MFXLoopDecoder::SyncOperation(mfxSyncPoint syncp, mfxU32 /*wait*/)
{
    //sync own syncpoint only
    MFX_CHECK_WITH_ERR(m_syncPoint == syncp, MFX_ERR_INVALID_HANDLE);
    
    return MFX_ERR_NONE;
}
