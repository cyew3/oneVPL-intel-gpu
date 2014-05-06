/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_wrapper.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"

mfxStatus MFX_PTIR_Plugin::Process(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;

    if(NULL != surface_in && NULL != surface_out)
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        work420_surface.Info.FourCC = MFX_FOURCC_I420;
        mfxU16& w = work420_surface.Info.CropW;
        mfxU16& h = work420_surface.Info.CropH;
        work420_surface.Data.Y = ptir->frmBuffer[ptir->uiSupBuf]->ucMem;
        work420_surface.Data.U = work420_surface.Data.Y+w*h;
        work420_surface.Data.V = work420_surface.Data.U+w*h/4;
        work420_surface.Data.Pitch = w;

        mfxFrameSurface1 surf_out;
        memset(&surf_out, 0, sizeof(mfxFrameSurface1));
        memcpy(&(surf_out.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        surf_out.Info.FourCC = MFX_FOURCC_I420;

        surf_out.Data.Y = ptir->frmBuffer[ptir->uiSupBuf]->ucMem;
        surf_out.Data.U = surf_out.Data.Y+w*h;
        surf_out.Data.V = surf_out.Data.U+w*h/4;
        surf_out.Data.Pitch = w;

        if(surface_in->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = ColorSpaceConversionWCopy(surface_in, &work420_surface, work420_surface.Info.FourCC);
        assert(!mfxCCSts);
        ptir->frmBuffer[ptir->uiSupBuf]->frmProperties.fr = ptir->dFrameRate;


        mfxSts = ptir->PTIR_ProcessFrame( &work420_surface, &surf_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            mfxCCSts = ColorSpaceConversionWCopy(&surf_out, surface_out, surface_out->Info.FourCC);
            assert(!mfxCCSts);

        }
        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }
        if(MFX_ERR_MORE_SURFACE == mfxSts)
            bMoreOutFrames = true;
        else
            bMoreOutFrames = false;

        return mfxSts;
    }
    else if(NULL != surface_in && NULL == surface_out)
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        work420_surface.Info.FourCC = MFX_FOURCC_I420;
        mfxU16& w = work420_surface.Info.CropW;
        mfxU16& h = work420_surface.Info.CropH;
        work420_surface.Data.Y = ptir->frmBuffer[ptir->uiSupBuf]->ucMem;
        work420_surface.Data.U = work420_surface.Data.Y+w*h;
        work420_surface.Data.V = work420_surface.Data.U+w*h/4;
        work420_surface.Data.Pitch = w;

        if(surface_in->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = ColorSpaceConversionWCopy(surface_in, &work420_surface, work420_surface.Info.FourCC);
        assert(!mfxCCSts);
        ptir->frmBuffer[ptir->uiSupBuf]->frmProperties.fr = ptir->dFrameRate;

        mfxSts = ptir->PTIR_ProcessFrame( &work420_surface, 0 );

        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }

        return mfxSts;
    }
    else if(0 == ptir->fqIn.iCount && 0 == ptir->uiCur)
    {
        return MFX_ERR_MORE_DATA;
    }
    else
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 surf_out;
        memset(&surf_out, 0, sizeof(mfxFrameSurface1));
        memcpy(&(surf_out.Info), &(surface_out->Info), sizeof(mfxFrameInfo));
        mfxU16& w = surf_out.Info.CropW;
        mfxU16& h = surf_out.Info.CropH;
        surf_out.Info.FourCC = MFX_FOURCC_I420;

        surf_out.Data.Y = ptir->frmBuffer[ptir->uiSupBuf]->ucMem;
        surf_out.Data.U = surf_out.Data.Y+w*h;
        surf_out.Data.V = surf_out.Data.U+w*h/4;
        surf_out.Data.Pitch = w;

        if(surface_out->Data.MemId)
        {
            m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            isUnlockReq = true;
        }

        mfxSts = ptir->PTIR_ProcessFrame( 0, &surf_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            mfxCCSts = ColorSpaceConversionWCopy(&surf_out, surface_out, surface_out->Info.FourCC);
            assert(!mfxCCSts);
        }
        if(isUnlockReq)
        {
            m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_out->Data.MemId, &(surface_out->Data));
            isUnlockReq = false;
        }
        if(MFX_ERR_MORE_SURFACE == mfxSts)
            bMoreOutFrames = true;
        else
            bMoreOutFrames = false;
        return mfxSts;
    }

    return mfxSts;
}