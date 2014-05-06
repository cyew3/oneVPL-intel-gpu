/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"
#include "..\pacm\utilities.h"
#include "mfxvideo++int.h"

#define GPUPATH

DeinterlaceFilter* pdeinterlaceFilter;

PTIR_ProcessorCM::PTIR_ProcessorCM(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply, eMFXHWType _HWType)
{
    uiDeinterlacingMode = INTL_MODE_SMART_CHECK;
    uiDeinterlacingMeasure = 16;
    dTimeStamp = 0.0;
    dBaseTime = 0.0;
    dOutBaseTime = 0.0;
    dBaseTimeSw = 0.0;
    dDeIntTime = 0.0;
    //liTime[sizeof(cOperations) / sizeof(const char *) + 1] = {0},
    liFreq;
    liFileSize;
    fTCodeOut = NULL;

    FrameQueue_Initialize(&fqIn);
    Pattern_init(&mainPattern);
    uiCount = MFX_INFINITE / 2; // /2 to avoid overflow. TODO: fix
    uiSupBuf = BUFMINSIZE;

    b_firstFrameProceed = false;
    uiCur = 0;
    uiStart = 0;
    m_pmfxCore = mfxCore;
    frmSupply = _frmSupply;
    HWType = _HWType;
}

PTIR_ProcessorCM::~PTIR_ProcessorCM()
{
    //if (b_inited)
    //{
        Close();
    //}
}

mfxStatus PTIR_ProcessorCM::Init(mfxVideoParam *par)
{
    uiInWidth  = uiWidth  = par->vpp.In.Width;
    uiInHeight = uiHeight = par->vpp.In.Height;
    //uiInWidth  = uiWidth  = par->vpp.In.CropW;
    //uiInHeight = uiHeight = par->vpp.In.CropH;
    if(par->vpp.In.FrameRateExtN && par->vpp.In.FrameRateExtD)
        dFrameRate = par->vpp.In.FrameRateExtN / par->vpp.In.FrameRateExtD;
    else
        dFrameRate = 30.0;

    //bisInterlaced = false;
    uiisInterlaced = 0;
    uiTeleCount = 0;
    uiCurTemp = 0;
    bFullFrameRate = false;
    uiLastFrameNumber = 0;
    if(MFX_PICSTRUCT_UNKNOWN == par->vpp.In.PicStruct &&
       0 == par->vpp.In.FrameRateExtN && 0 == par->vpp.Out.FrameRateExtN)
    {
        //auto-detection mode, currently equal to reverse telecine mode
        uiisInterlaced = 0;
        bFullFrameRate = false;
    }
    else if((MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
             MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct) &&
       (4 * par->vpp.In.FrameRateExtN * par->vpp.In.FrameRateExtD ==
        5 * par->vpp.Out.FrameRateExtN * par->vpp.Out.FrameRateExtD))
    {
        //reverse telecine mode
        uiisInterlaced = 2;
        bFullFrameRate = false;
    }
    else if(MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
           MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct)
    {
        //Deinterlace only mode
        uiisInterlaced = 1;

        if(2 * par->vpp.In.FrameRateExtN * par->vpp.Out.FrameRateExtD ==
            par->vpp.In.FrameRateExtD * par->vpp.Out.FrameRateExtN)
        {
            //30i -> 60p mode
            bFullFrameRate = true;
        }
        else if(par->vpp.In.FrameRateExtN * par->vpp.Out.FrameRateExtD ==
            par->vpp.In.FrameRateExtD * par->vpp.Out.FrameRateExtN)
        {
            //30i -> 30p mode
            bFullFrameRate = false;
        }
        else
        {
            //any additional frame rate conversions are unsupported
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
    else
    {
        //reverse telecine + additional frame rate conversion are unsupported
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    //PTIR's frames init
    try //try is useless here since frames allocated by malloc, but probably in future it could be changed to new
    {
        mfxHandleType mfxDeviceType;
        mfxHDL mfxDeviceHdl;
        mfxCoreParam mfxCorePar;
        mfxStatus mfxSts = MFX_ERR_NONE;
        m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
        if(mfxSts) return mfxSts;
        if(mfxCorePar.Impl & MFX_IMPL_VIA_D3D9)
            mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
        else if(mfxCorePar.Impl & MFX_IMPL_VIA_D3D11)
            mfxDeviceType = MFX_HANDLE_D3D11_DEVICE;
        else if(mfxCorePar.Impl & MFX_IMPL_VIA_VAAPI)
            mfxDeviceType = MFX_HANDLE_VA_DISPLAY;
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
        mfxSts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, mfxDeviceType, &mfxDeviceHdl);
        if(mfxSts) return mfxSts;

        //const char * pIsaFileNames[] = { "Deinterlace_genx.isa" };
        deinterlaceFilter = new DeinterlaceFilter(HWType, uiInWidth, uiInHeight, mfxDeviceType, mfxDeviceHdl);
        pdeinterlaceFilter = deinterlaceFilter;
        for(i = 0; i < LASTFRAME; i++)
        {
            Frame_CreateCM(&frmIO[i], uiInWidth, uiInHeight, uiInWidth / 2, uiInHeight / 2, 0);
            frmBuffer[i] = frmIO + i;
            //if(!frmIO[i].ucMem)
            //{
            //    Close();
            //    return MFX_ERR_MEMORY_ALLOC;
            //}
        }
        Frame_CreateCM(&frmIO[LASTFRAME], uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);
        //if(!frmIO[LASTFRAME].ucMem)
        //{
        //    Close();
        //    return MFX_ERR_MEMORY_ALLOC;
        //}
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...) 
    { 
        return MFX_ERR_UNKNOWN;
    }

    frmSupply->SetDevice(&deinterlaceFilter->DeviceEx());
    frmSupply->SetMap(&CmToMfxSurfmap);
    work_par = *par;
    bInited = true;

    return MFX_ERR_NONE;
}


mfxStatus PTIR_ProcessorCM::Close()
{
    if(bInited)
    {
        try
        {
            for (mfxU32 i = 0; i <= LASTFRAME; ++i)
                Frame_Release(&frmIO[i]);
            bInited = false;

            return MFX_ERR_NONE;
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;;
        }
        catch(...) 
        { 
            return MFX_ERR_UNKNOWN;; 
        }
    }
    else
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
}

mfxStatus PTIR_ProcessorCM::Process(mfxFrameSurface1 *surface_in, mfxFrameSurface1 **surface_out, mfxCoreInterface *mfx_core, mfxFrameSurface1 **surface_outt)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;

    if(NULL != surface_in && NULL != surface_out && NULL != (*surface_out))
    {
        bool isUnlockReq = false;

        CmSurface2D *pInCmSurface2D = 0;
        CmSurface2D *pOutCmSurface2D = 0;

        if((surface_in->Data.MemId) && (work_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || work_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            //MemId should be a video memory surface
            CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
            int result = -1;
            result = pCMdevice->CreateSurface2D((IDirect3DSurface9 *) surface_in->Data.MemId, pInCmSurface2D);
            CmToMfxSurfmap[pInCmSurface2D] = surface_in;
            result = pCMdevice->CreateSurface2D((IDirect3DSurface9 *) (*surface_out)->Data.MemId, pOutCmSurface2D);
            CmToMfxSurfmap[pOutCmSurface2D] = (*surface_out);
            //mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            //mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }
        if((surface_in->Data.MemId) && (work_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY || work_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        {
            //MemId should be a video memory surface
            mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;


        mfxSts = PTIR_ProcessFrame( pInCmSurface2D, pOutCmSurface2D, surface_outt);

        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }

        *surface_out = 0;
        return mfxSts;
    }
    else if(NULL != surface_in && NULL != surface_out && NULL == (*surface_out))
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
        work420_surface.Info.FourCC = MFX_FOURCC_I420;
        mfxU16& w = work420_surface.Info.CropW;
        mfxU16& h = work420_surface.Info.CropH;
        work420_surface.Data.Y = frmBuffer[uiSupBuf]->ucMem;
        work420_surface.Data.U = work420_surface.Data.Y+w*h;
        work420_surface.Data.V = work420_surface.Data.U+w*h/4;
        work420_surface.Data.Pitch = w;

        if(surface_in->Data.MemId)
        {
            mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = ColorSpaceConversionWCopy(surface_in, &work420_surface, work420_surface.Info.FourCC);
        assert(!mfxCCSts);
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;

        //mfxSts = PTIR_ProcessFrame( &work420_surface, 0 );

        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }

        *surface_out = 0;
        return mfxSts;
    }
    else if(0 == fqIn.iCount && 0 == uiCur)
    {
        return MFX_ERR_MORE_DATA;
    }
    else
    {
        bool isUnlockReq = false;

        CmSurface2D *pOutCmSurface2D = 0;

        if(((*surface_out)->Data.MemId) && (work_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || work_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            //MemId should be a video memory surface
            CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
            int result = -1;
            result = pCMdevice->CreateSurface2D((IDirect3DSurface9 *) (*surface_out)->Data.MemId, pOutCmSurface2D);
            CmToMfxSurfmap[pOutCmSurface2D] = (*surface_out);
            isUnlockReq = false;
        }
        if(((*surface_out)->Data.MemId) && (work_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY || work_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        {
            //MemId should be a video memory surface
            mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            isUnlockReq = true;
        }
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;


        mfxSts = PTIR_ProcessFrame( 0, pOutCmSurface2D, surface_outt);

        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            isUnlockReq = false;
        }

        *surface_out = 0;
        return mfxSts;
    }

    return mfxSts;
}

mfxStatus PTIR_ProcessorCM::PTIR_ProcessFrame(CmSurface2D *surf_in, CmSurface2D *surf_out,  mfxFrameSurface1 **surf_outt)
{
    //----------------------------------------
    // Loading first frame
    //----------------------------------------
    //LoadNextFrameInI420(hIn, pcFormat, frmBuffer[0], uiSize, pucIO);

    //frmBuffer[0]->outSurf = surf_out;

    if(!b_firstFrameProceed)
    {
        CmDeviceEx& device = pdeinterlaceFilter->DeviceEx();
        if(static_cast<CmSurface2DEx*>(frmBuffer[0]->inSurf)->pCmSurface2D && !CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmBuffer[0]->inSurf)->pCmSurface2D])
            device->DestroySurface(static_cast<CmSurface2DEx*>(frmBuffer[0]->inSurf)->pCmSurface2D);
        if(static_cast<CmSurface2DEx*>(frmBuffer[0]->outSurf)->pCmSurface2D && !CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmBuffer[0]->outSurf)->pCmSurface2D])
            device->DestroySurface(static_cast<CmSurface2DEx*>(frmBuffer[0]->outSurf)->pCmSurface2D);

        if(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D && !CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D])
        {
            device->DestroySurface(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D);
            static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
        }

        if(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D && !CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D])
        {
            device->DestroySurface(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D);
            static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
        }

        static_cast<CmSurface2DEx*>(frmBuffer[0]->inSurf)->pCmSurface2D = surf_in;
        static_cast<CmSurface2DEx*>(frmBuffer[0]->outSurf)->pCmSurface2D = surf_out;

        m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &CmToMfxSurfmap[surf_in]->Data);
        m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &CmToMfxSurfmap[surf_out]->Data);

        frmBuffer[0]->frmProperties.tindex = uiStart + 1;

        deinterlaceFilter->CalculateSADRs(frmBuffer[0], frmBuffer[0]);

        Artifacts_Detection_frame(frmBuffer, 0, TRUE);
        frmBuffer[0]->frmProperties.processed = FALSE;

        frmBuffer[0]->plaY.ucStats.ucSAD[0] = 99.999;
        frmBuffer[0]->plaY.ucStats.ucSAD[1] = 99.999;
        frmIn = NULL;

        //hOut = CreateFileA(pcFileOut, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        //GetConsoleScreenBufferInfo(hStd, &sbInfo);

        uiCur = 1;
        uiCurTemp = uiCur;
        uiBufferCount = 0;
        patternFound = FALSE;
        uiDispatch = 0;
        uiStart = 1;
        mainPattern.blendedCount = 0.0;
        mainPattern.uiIFlush = 0;
        mainPattern.uiPFlush = 0;

        dBaseTime = (1 / dFrameRate) * 1000;
        dDeIntTime = dBaseTime / 2;
        frmBuffer[0]->frmProperties.timestamp = dTimeStamp;
        if (uiisInterlaced == 2)
            dBaseTimeSw = (dBaseTime * 5 / 4);
        else
            dBaseTimeSw = dBaseTime;

        uiFrame = uiStart;

        b_firstFrameProceed = true;

        return MFX_ERR_MORE_DATA;
    }

    if(surf_in)
    {
    //for (uiFrame = uiStart, uiFrameOut = 0, uiCur = 1; uiFrame < uiCount; ++uiFrame)
    //{
        uiNext = uiCur - 1;

        uiProgress = 0;
        uiTimer = 0;

        if (uiFrame < uiStart + uiCount)
        {
            //QueryPerformanceCounter(&liTime[uiTimer++]);
            //----------------------------------------
            // Loading
            //----------------------------------------
            //LoadNextFrameInI420(hIn, pcFormat, frmBuffer[uiCur], uiSize, pucIO);
            int result = -1;
            CmDeviceEx& device = pdeinterlaceFilter->DeviceEx();
            if(static_cast<CmSurface2DEx*>(frmBuffer[uiCur]->inSurf)->pCmSurface2D)
                result = device->DestroySurface(static_cast<CmSurface2DEx*>(frmBuffer[uiCur]->inSurf)->pCmSurface2D);
            //if(static_cast<CmSurface2DEx*>(frmBuffer[uiCur]->outSurf)->pCmSurface2D)
            //    result = device->DestroySurface(static_cast<CmSurface2DEx*>(frmBuffer[uiCur]->outSurf)->pCmSurface2D);
            static_cast<CmSurface2DEx*>(frmBuffer[uiCur]->inSurf)->pCmSurface2D = surf_in;
            static_cast<CmSurface2DEx*>(frmBuffer[uiCur]->outSurf)->pCmSurface2D = surf_out;
            m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &CmToMfxSurfmap[surf_in]->Data);
            m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &CmToMfxSurfmap[surf_out]->Data);
            if(static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D == 0)
            {
                static_cast<CmSurface2DEx*>(frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
            }

            //QueryPerformanceCounter(&liTime[uiTimer++]);
            //----------------------------------------
            // Picture Format Conversion
            //----------------------------------------
            frmBuffer[uiCur]->frmProperties.tindex = uiFrame + 1;
            deinterlaceFilter->CalculateSADRs(frmBuffer[uiCur], frmBuffer[uiNext]);
            frmBuffer[uiCur]->frmProperties.processed = TRUE;
            dSAD = frmBuffer[uiCur]->plaY.ucStats.ucSAD;
            dRs = frmBuffer[uiCur]->plaY.ucStats.ucRs;

            if ((uiCur == BUFMINSIZE - 1) || (uiFrame == (uiCount - 1)))
            {                
                Analyze_Buffer_Stats_CM(frmBuffer, &mainPattern, &uiDispatch, &uiisInterlaced);
                if(mainPattern.ucPatternFound && uiisInterlaced != 1)
                {
                    if (mainPattern.ucPatternType == 0 && uiisInterlaced != 2)
                        dOutBaseTime = dBaseTime;
                    else
                        dOutBaseTime = (dBaseTime * 5 / 4);

                    for (i = 0; i < min(uiDispatch, uiCur + 1); i++)
                    {
                        if (!frmBuffer[i]->frmProperties.drop)
                        {
                            CheckGenFrameCM(frmBuffer, i, mainPattern.ucPatternType);
                            Prepare_frame_for_queueCM(&frmIn, frmBuffer[i], uiWidth, uiHeight);
                            memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                            //Timestamp
                            frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dOutBaseTime;
                            frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                            FrameQueue_Add(&fqIn, frmIn);
                        }
                        else
                        {
                            frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;
                            frmBuffer[i]->frmProperties.drop = FALSE;
                            frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(frmBuffer[i]->inSurf)->pCmSurface2D);
                            frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D);
                            if(0 == surf_in)
                                std::cout << "DEEEE\n";
                            static_cast<CmSurface2DEx*>(frmBuffer[i]->inSurf)->pCmSurface2D  = 0;
                            static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D = 0;

                        }
                    }
                    Rotate_Buffer_borders(frmBuffer, uiDispatch);
                    uiCur -= min(uiDispatch, uiCur);
                    dBaseTimeSw = dOutBaseTime;
                }
                else
                {
                    uiCurTemp = uiCur;
                    if (uiisInterlaced == 2)
                        dBaseTimeSw = (dBaseTime * 5 / 4);
                    for (i = 0; i < min(uiDispatch, uiCurTemp + 1); i++)
                    {
                        if (!frmBuffer[0]->frmProperties.drop)
                        {
                            uiBufferCount++;
                            if ((uiisInterlaced == 0 && !(mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1))) || uiisInterlaced == 2)
                            {
                                if ((uiisInterlaced == 2 && uiBufferCount < (BUFMINSIZE - 1)) || uiisInterlaced == 0)
                                {
                                    if (frmBuffer[0]->frmProperties.interlaced)
                                    {
                                        deinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, 0, BUFMINSIZE);
                                    }

                                    Prepare_frame_for_queueCM(&frmIn, frmBuffer[0], uiWidth, uiHeight);
                                    memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[0]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                    //Timestamp
                                    frmBuffer[1]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dBaseTimeSw;
                                    frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                    FrameQueue_Add(&fqIn, frmIn);
                                }
                                else
                                {
                                    frmBuffer[1]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;// +dBaseTimeSw;
                                    uiBufferCount = 0;
                                }
                                Rotate_Buffer(frmBuffer);
                            }
                            else
                            {
                                deinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, 0, BUFMINSIZE);

                                Prepare_frame_for_queueCM(&frmIn, frmBuffer[0], uiWidth, uiHeight); // Go to input frame rate
                                memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[0]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                //Timestamp
                                frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[0]->frmProperties.tindex;
                                frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dDeIntTime;
                                frmBuffer[1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                                frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                FrameQueue_Add(&fqIn, frmIn);
                                if (bFullFrameRate && uiisInterlaced == 1)
                                {
                                    Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight, frmSupply); // Go to double frame rate
                                    memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                    //Timestamp
                                    frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

                                    FrameQueue_Add(&fqIn, frmIn);
                                }
                                Rotate_Buffer_deinterlaced(frmBuffer);
                            }
                        }
                        else
                        {
                            uiBufferCount = 0;
                            frmBuffer[0]->frmProperties.drop = FALSE;
                        }
                        uiCur--;
                    }
                }

            }
            uiCur++;
        }
        else
        {
            //QueryPerformanceCounter(&liTime[uiTimer++]);
            //QueryPerformanceCounter(&liTime[uiTimer++]);
        }

        frmIn = NULL;
        //QueryPerformanceCounter(&liTime[uiTimer++]);
        if (fqIn.iCount >= 1 && !mainPattern.ucPatternFound)
        {
            uiFrameCount = fqIn.iCount;
            for (i = 0; i < uiFrameCount; i++)
            {
                uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                frmIn = FrameQueue_Get(&fqIn);
                //----------------------------------------
                // Saving Test Output
                //----------------------------------------
                if (frmIn)
                {
                    ferror = FALSE;
#ifndef GPUPATH
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                    fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
                    ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
#else
                    frmIn; //it is a output
                    mfxFrameSurface1* output = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D];
                    mfxFrameSurface1* input = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D];
                    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &output->Data);

                    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
                    it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
                    CmToMfxSurfmap.erase(it);
                    if(input)
                    {
                        m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &input->Data);
                        it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);
                        CmToMfxSurfmap.erase(it);
                    }

                    *surf_outt = output;
                    CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
                    int result = -1;
                    result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
                    result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);

                    Frame_ReleaseCM(frmIn);
                    free(frmIn);
                    frmSupply->AddOutputSurf(output);
                            if(0 == surf_in)
                                std::cout << "DEEEE\n";

#endif
#if PRINTX == 1
                    dSAD = frmIn->plaY.ucStats.ucSAD;
                    dRs = frmIn->plaY.ucStats.ucRs;
                    printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%6.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", frmIn->frmProperties.tindex, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[4], dRs[5]);
#endif
                    //free(frmIn);

                    uiFrameOut++;
                }
            }
        }
        else if (mainPattern.ucPatternFound)
        {
            mainPattern.ucPatternFound = FALSE;
            uiFrameCount = fqIn.iCount;
            for (i = 0; i < uiFrameCount; i++)
            {
                ferror = FALSE;
                uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                frmIn = FrameQueue_Get(&fqIn);
                if (frmIn)
                {
//#ifndef GPUPATH
//                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
//                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
//                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);
//
//                    fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
//                    ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
//#else
//                    void* ucMem = malloc(frmIn->uiSize);
//                    pdeinterlaceFilter->ReadRAWI420FromGPUNV12(frmIn, ucMem);
//                    fprintf(fTCodeOut, "%4.3lf\n", frmIn->frmProperties.timestamp);
//                    ferror = WriteFile(hOut, ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
//                    free(ucMem);
//#endif
//
//#if PRINTX == 1
//                    dSAD = frmIn->plaY.ucStats.ucSAD;
//                    dRs = frmIn->plaY.ucStats.ucRs;
//                    printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%6.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", frmIn->frmProperties.tindex, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[4], dRs[5]);
//#endif
                    frmIn; //it is a output
                    mfxFrameSurface1* output = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D];
                    mfxFrameSurface1* input = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D];

                    if(!output)
                        std::cout << "SMTH BAD HAPPENED!!!\n";
                    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &output->Data);

                    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
                    it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
                    CmToMfxSurfmap.erase(it);
                    if(input)
                    {
                        m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &input->Data);
                        it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);
                        CmToMfxSurfmap.erase(it);
                    }
                    if(frmIn->outState == Frame::OUT_PROCESSED)
                        std::cout << "OUT_PROCESSED\n";
                    if(frmIn->outState == Frame::OUT_UNCHANGED)
                    {
                        frmSupply->CMCopyGPUToGpu(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D, static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);
                        std::cout << "OUT_UNCHANGED\n";
                    }
                    if(frmIn->outState == Frame::OUT_DROPPED)
                        std::cout << "OUT_DROPPED\n";

                    if(frmIn->frmProperties.drop)
                        std::cout << "DDDROP!!!!\n";


                    *surf_outt = output;
                    CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
                    int result = -1;
                    result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
                    result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);
                    //if(frmIn->outState == Frame::OUT_UNCHANGED)

                    Frame_ReleaseCM(frmIn);
                    free(frmIn);
                    frmSupply->AddOutputSurf(output);

                            if(0 == surf_in)
                                std::cout << "DEEEE\n";


                    uiFrameOut++;
                }
            }
        }
        //QueryPerformanceCounter(&liTime[uiTimer++]);

        // Update Time Counters
        //for (i = 0; i < uiTimer - 1; ++i)
        //    dTime[i] += (double)(liTime[i + 1].QuadPart - liTime[i].QuadPart) / liFreq.QuadPart;

    //}
    }
    if (uiCur && !surf_in)
    {
        for(i = 0; i < uiCur; i++)
        {
            if (!frmBuffer[i]->frmProperties.drop /*&& (frmBuffer[i]->frmProperties.tindex > uiLastFrameNumber)*/)
            {
                if (uiisInterlaced == 1)
                {
                    deinterlaceFilter->DeinterlaceMedianFilterCM(frmBuffer, i, BUFMINSIZE);

                    Prepare_frame_for_queueCM(&frmIn, frmBuffer[i], uiWidth, uiHeight); // Go to input frame rate
                    memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                    //Timestamp
                    frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[i]->frmProperties.tindex;
                    frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dDeIntTime;
                    frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                    frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                    FrameQueue_Add(&fqIn, frmIn);
                    if (bFullFrameRate)
                    {
                        Prepare_frame_for_queueCM(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight, frmSupply); // Go to double frame rate
                        memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[BUFMINSIZE]->plaY.ucStats.ucRs, sizeof(double)* 10);

                        //Timestamp
                        frmIn->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp;

                        FrameQueue_Add(&fqIn, frmIn);
                    }
                }
                else
                {
                    uiBufferCount++;
                    if (uiisInterlaced != 2 || (uiisInterlaced == 2 && uiBufferCount < 5))
                    {
                        CheckGenFrameCM(frmBuffer, i, mainPattern.ucPatternType);
                        Prepare_frame_for_queueCM(&frmIn, frmBuffer[i], uiWidth, uiHeight);
                        memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                        //timestamp
                        frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + (dBaseTime * 5 / 4);
                        frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                        FrameQueue_Add(&fqIn, frmIn);
                    }
                    else
                        uiBufferCount = 0;
                }
            
            }
        }
        uiCur = 0;
        
        uiFrameCount = fqIn.iCount;
        for (i = 0; i < uiFrameCount; i++)
        {
            ferror = FALSE;
            frmIn = FrameQueue_Get(&fqIn);
            if (frmIn)
            {
                frmIn; //it is a output
                mfxFrameSurface1* output = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D];
                mfxFrameSurface1* input = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D];

                m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &output->Data);

                std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
                it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
                CmToMfxSurfmap.erase(it);
                if(input)
                {
                    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &input->Data);
                    it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);
                    CmToMfxSurfmap.erase(it);
                }


                *surf_outt = output;
                CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
                int result = -1;
                result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
                result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);

                Frame_ReleaseCM(frmIn);
                free(frmIn);
                frmSupply->AddOutputSurf(output);


                uiFrameOut++;
            }
        }
    }
    if (fqIn.iCount)
    {
        frmIn = FrameQueue_Get(&fqIn);
        if (frmIn)
        {
            frmIn; //it is a output
            mfxFrameSurface1* output = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D];
            mfxFrameSurface1* input = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D];
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &input->Data);
            m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &output->Data);

            std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
            it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
            CmToMfxSurfmap.erase(it);
            it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);
            CmToMfxSurfmap.erase(it);

            *surf_outt = output;
            CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
            int result = -1;
            result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->outSurf)->pCmSurface2D);
            result = pCMdevice->DestroySurface(static_cast<CmSurface2DEx*>(frmIn->inSurf)->pCmSurface2D);

            Frame_ReleaseCM(frmIn);
            free(frmIn);
            return MFX_ERR_NONE;
        }
    }



    return MFX_ERR_MORE_DATA;
}