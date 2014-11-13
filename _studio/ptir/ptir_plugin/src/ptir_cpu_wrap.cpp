/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: ptir_vpp_plugin.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"

PTIR_ProcessorCPU::PTIR_ProcessorCPU(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply)
{
    dTimeStamp = 0.0;
    dBaseTime = 0.0;
    dOutBaseTime = 0.0;
    dBaseTimeSw = 0.0;
    dDeIntTime = 0.0;
    dTimePerFrame = 0.0;
    liFreq;
    liFileSize;
    fTCodeOut = NULL;
    uiInterlaceParity = 0;

    uiCur = 1;
    uiBufferCount = 0;
    patternFound = false;
    uiDispatch = 0;
    //uiStart = 1;
    mainPattern.blendedCount = 0.0;
    mainPattern.uiIFlush = 0;
    mainPattern.uiPFlush = 0;
    mainPattern.uiOverrideHalfFrameRate = false;
    mainPattern.uiCountFullyInterlacedBuffers = 0;
    mainPattern.uiInterlacedFramesNum = 0;

    dBaseTime = (1 / dFrameRate) * 1000;
    dDeIntTime = dBaseTime / 2;


    FrameQueue_Initialize(&fqIn);
    Pattern_init(&mainPattern);
    //uiCount = MFX_INFINITE / 2; // /2 to avoid overflow. TODO: fix
    uiSupBuf = BUFMINSIZE;

    b_firstFrameProceed = false;
    uiCur = 0;
    uiStart = 0;
    m_pmfxCore = mfxCore;
    frmSupply = _frmSupply;
    isHW = false;
}

PTIR_ProcessorCPU::~PTIR_ProcessorCPU()
{
    //if (b_inited)
    //{
        Close();
    //}
}

mfxStatus PTIR_ProcessorCPU::Init(mfxVideoParam *par)
{
    //maximize b2b with reference app
    uiInWidth  = uiWidth  = par->vpp.In.CropX + par->vpp.In.CropW;
    uiInHeight = uiHeight = par->vpp.In.CropY + par->vpp.In.CropH;
    if(par->vpp.In.FrameRateExtN && par->vpp.In.FrameRateExtD)
        dFrameRate = (double) par->vpp.In.FrameRateExtN / par->vpp.In.FrameRateExtD;
    else
        dFrameRate = 30.0;

    //bisInterlaced = false;
    uiisInterlaced = 0;
    uiTeleCount = 0;
    //uiCurTemp = 0;
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
        if(MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct)
            uiInterlaceParity = 0;
        else
            uiInterlaceParity = 1;

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


    if (uiisInterlaced == 2)
        dBaseTimeSw = (dBaseTime * 5 / 4);
    else
        dBaseTimeSw = dBaseTime;

    //PTIR's frames init
    try //try is useless here since frames allocated by malloc, but probably in future it could be changed to new
    {
        for(i = 0; i < LASTFRAME; i++)
        {
            Frame_Create(&frmIO[i], uiInWidth, uiInHeight, uiInWidth / 2, uiInHeight / 2, 0);
            frmBuffer[i] = frmIO + i;
            if(!frmIO[i].ucMem)
            {
                Close();
                return MFX_ERR_MEMORY_ALLOC;
            }
            frmIO[i].frmProperties.timestamp = -1;
        }
        Frame_Create(&frmIO[LASTFRAME], uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);
        if(!frmIO[LASTFRAME].ucMem)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
        frmIO[i].frmProperties.timestamp = -1;
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...) 
    { 
        return MFX_ERR_UNKNOWN;
    }

    work_par = *par;
    bInited = true;

    return MFX_ERR_NONE;
}


mfxStatus PTIR_ProcessorCPU::Close()
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

mfxStatus PTIR_ProcessorCPU::Process(mfxFrameSurface1 *surface_in, mfxFrameSurface1 **surface_out, mfxCoreInterface *mfx_core, mfxFrameSurface1**, bool beof, mfxFrameSurface1 *exp_surf)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;

    if(0 != surface_in && 0 != surface_out && 0 != (*surface_out))
    {
        bool isUnlockReq = false;

        mfxFrameSurface1* realSurf = surface_in;
        if(work_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            mfxSts = mfx_core->GetRealSurface(mfx_core->pthis, surface_in, &realSurf);
            if(mfxSts)
                return mfxSts;
        }

        if(realSurf->Data.MemId)
        {
            mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, realSurf->Data.MemId, &(realSurf->Data));
            isUnlockReq = true;
        }
        mfxCCSts = MfxNv12toPtir420(realSurf, frmBuffer[uiSupBuf]->ucMem);
        assert(!mfxCCSts);
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;
        frmBuffer[uiSupBuf]->frmProperties.timestamp = (long double) surface_in->Data.TimeStamp;
        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, realSurf->Data.MemId, &(realSurf->Data));
            isUnlockReq = false;
        }
        mfxSts = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surface_in->Data);
        if(mfxSts)
            return mfxSts;

        mfxSts = PTIR_ProcessFrame( surface_in, *surface_out, beof);
        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
            (*surface_out) = 0;
        return mfxSts;
    }
    else if(NULL != surface_in && (NULL == surface_out || NULL == (*surface_out)))
    {
        bool isUnlockReq = false;
        mfxFrameSurface1 work420_surface;
        memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
        ptir_memcpy(&(work420_surface.Info), &(surface_in->Info), sizeof(mfxFrameInfo));
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

        mfxSts = PTIR_ProcessFrame( &work420_surface, 0, beof);

        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }

        return mfxSts;
    }
    else if(0 == fqIn.iCount && 0 == uiCur)
    {
        return MFX_ERR_MORE_DATA;
    }
    else
    {
        bool isUnlockReq = false;

        mfxSts = PTIR_ProcessFrame( 0, *surface_out, beof);

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            //mfxCCSts = ColorSpaceConversionWCopy(&surf_out, (*surface_out), (*surface_out)->Info.FourCC);
            //assert(!mfxCCSts);
            //frmSupply->AddOutputSurf((*surface_out));
        }
        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            isUnlockReq = false;
        }
        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
            (*surface_out) = 0;
        return mfxSts;
    }

    return mfxSts;
}

mfxStatus PTIR_ProcessorCPU::PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, bool beof, mfxFrameSurface1 *)
{

    if(!b_firstFrameProceed)
    {
        uiStart = 1;

        frmBuffer[0]->frmProperties.processed = false;

        Frame_Prep_and_Analysis(frmBuffer, /*pcFormat*/0, dFrameRate, 0, 0, 0);
        frmBuffer[0]->frmProperties.timestamp = dTimeStamp;

        b_firstFrameProceed = true;

        return MFX_ERR_MORE_DATA;
    }

    //for (uiFrame = uiStart, uiFrameOut = 0, uiCur = 1; uiFrame < uiCount; ++uiFrame)
    //{
        if(surf_in)
        {
            uiNext = uiCur - 1;

            uiProgress = 0;
            uiTimer = 0;

            if (true/*uiFrame < uiStart + uiCount*/)
            {
                ////QueryPerformanceCounter(&liTime[uiTimer++]);
                //----------------------------------------
                // Loading
                //----------------------------------------
                //ferror = ReadFile(hIn, pucIO, uiSize, &uiBytesRead, NULL);

                ////QueryPerformanceCounter(&liTime[uiTimer++]);
                //----------------------------------------
                // Picture Format Conversion
                //----------------------------------------
                //Convert_to_I420(pucIO, frmBuffer[uiSupBuf], pcFormat, dFrameRate);

                Convert_to_I420(pucIO, frmBuffer[BUFMINSIZE], /*pcFormat*/0, dFrameRate);
                Frame_Prep_and_Analysis(frmBuffer, /*pcFormat*/0, dFrameRate, uiCur, uiNext, uiFrame);
           
                if ((uiCur == BUFMINSIZE - 1) || beof/*|| (uiFrame == (uiCount - 1))*/)
                {
                    if(beof)
                        b_firstFrameProceed = false;
                    Analyze_Buffer_Stats(frmBuffer, &mainPattern, &uiDispatch, &uiisInterlaced);
                    if(mainPattern.ucPatternFound && uiisInterlaced != 1)
                    {
                        if (uiisInterlaced != 3)
                            dTimePerFrame = Calculate_Resulting_timestamps(frmBuffer, uiDispatch, uiCur, dBaseTime, &uiNumFramesToDispatch, mainPattern.ucPatternType);
                        else
                        {
                            uiNumFramesToDispatch = min(uiDispatch, uiCur + 1);
                            dTimePerFrame = dBaseTime;
                        }

                        for (i = 0; i < uiNumFramesToDispatch; i++)
                        {
                            if (!frmBuffer[i]->frmProperties.drop)
                                Update_Frame_Buffer(frmBuffer, i, dTimePerFrame, uiisInterlaced, uiInterlaceParity, bFullFrameRate, frmIn, &fqIn);
                            else
                            {
                                if (uiisInterlaced != 3)
                                {
                                    frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;
                                    frmBuffer[i]->frmProperties.drop = false;
                                }
                                else
                                {
                                    frmBuffer[i]->frmProperties.candidate = true;
                                    Update_Frame_Buffer(frmBuffer, i, dTimePerFrame, uiisInterlaced, uiInterlaceParity, bFullFrameRate, frmIn, &fqIn);
                                }
                            }
                        }
                        Rotate_Buffer_borders(frmBuffer, uiDispatch);
                        uiCur -= min(uiDispatch,uiCur);
                        dBaseTimeSw = dTimePerFrame;
                    }
                    else
                    {
                        uiDeinterlace = (uiisInterlaced == 1 || mainPattern.uiOverrideHalfFrameRate);
                        if (uiisInterlaced == 2)
                            dBaseTimeSw = (dBaseTime * 5 / 4);
                        else
                            dTimePerFrame = Calculate_Resulting_timestamps(frmBuffer, uiDispatch, uiCur, dBaseTime, &uiNumFramesToDispatch, mainPattern.ucPatternType);
                        dBaseTimeSw = dTimePerFrame / (1 + (bFullFrameRate && uiDeinterlace));
                        for(i = 0; i < min(uiDispatch,uiCur + 1); i++)
                        {
                            FillBaseLinesIYUV(frmBuffer[0], frmBuffer[BUFMINSIZE], uiInterlaceParity, uiInterlaceParity);
                            if (!frmBuffer[0]->frmProperties.drop)
                            {
                                uiBufferCount++;
                                if ((uiisInterlaced == 0 && !(mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1))) || uiisInterlaced == 2)
                                {
                                    if ((uiisInterlaced == 2 && uiBufferCount < (BUFMINSIZE - 1)) || uiisInterlaced == 0)
                                        Update_Frame_Buffer(frmBuffer, 0, dBaseTimeSw, uiisInterlaced, uiInterlaceParity, bFullFrameRate, frmIn, &fqIn);
                                    else
                                    {
                                        frmBuffer[1]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dBaseTimeSw;
                                        uiBufferCount = 0;
                                    }
                                    Rotate_Buffer(frmBuffer);
                                }
                                else
                                {
                                    Update_Frame_Buffer(frmBuffer, 0, dBaseTimeSw, uiDeinterlace, uiInterlaceParity, bFullFrameRate, frmIn, &fqIn);
                                    Rotate_Buffer_deinterlaced(frmBuffer);
                                }
                            }
                            else
                            {
                                uiBufferCount = 0;
                                frmBuffer[0]->frmProperties.drop = false;
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
        }
    //}

    if(surf_out)
    {
        frmIn = NULL;
        //QueryPerformanceCounter(&liTime[uiTimer++]);

        while(frmIn = FrameQueue_Get(&fqIn))
        {
            Frame* frmOut = &frmIO[LASTFRAME];

            uiLastFrameNumber = frmIn->frmProperties.tindex;
            ferror = false;
            TrimBorders(&frmIn->plaY, &frmOut->plaY);
            TrimBorders(&frmIn->plaU, &frmOut->plaU);
            TrimBorders(&frmIn->plaV, &frmOut->plaV);

            //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
            //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
            if(!surf_out){
                surf_out = frmSupply->GetWorkSurfaceMfx();
            }
            surf_out->Data.TimeStamp = (mfxU64) frmIO[LASTFRAME].frmProperties.timestamp;
            frmSupply->AddCPUPtirOutSurf(frmIO[LASTFRAME].ucMem, surf_out);

            Frame_Release(frmIn);
            free(frmIn);
            surf_out = 0;

            uiFrameOut++;
        }

        //QueryPerformanceCounter(&liTime[uiTimer++]);

        //// Update Time Counters
        //for (i = 0; i < uiTimer - 1; ++i)
        //    dTime[i] += (double)(liTime[i+1].QuadPart - liTime[i].QuadPart) / liFreq.QuadPart;
    //}

        uiFrame++;
        if(uiCur && beof)
        {
            for(mfxU32 i = 0; i < uiCur; i++)
            {
                if (!frmBuffer[i]->frmProperties.drop && (frmBuffer[i]->frmProperties.tindex > uiLastFrameNumber))
                {
                    if (uiisInterlaced == 1)
                    {
                        FillBaseLinesIYUV(frmBuffer[i], frmBuffer[BUFMINSIZE], false, false);
                        Update_Frame_Buffer(frmBuffer, i, dBaseTimeSw, uiisInterlaced, uiInterlaceParity, bFullFrameRate, frmIn, &fqIn);
                    }
                    else
                    {
                        uiBufferCount++;
                        if (uiisInterlaced != 2 || (uiisInterlaced == 2 && uiBufferCount < 5))
                            Update_Frame_Buffer(frmBuffer, i, dTimePerFrame, uiisInterlaced, uiInterlaceParity, bFullFrameRate, frmIn, &fqIn);
                    }
                }
            }

            uiCur = 0;

            //uiFrameCount = fqIn.iCount;
            while(frmIn = FrameQueue_Get(&fqIn))
            {
                Frame* frmOut = &frmIO[LASTFRAME];

                uiLastFrameNumber = frmIn->frmProperties.tindex;
                ferror = false;
                TrimBorders(&frmIn->plaY, &frmOut->plaY);
                TrimBorders(&frmIn->plaU, &frmOut->plaU);
                TrimBorders(&frmIn->plaV, &frmOut->plaV);

                //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                if(!surf_out){
                    surf_out = frmSupply->GetWorkSurfaceMfx();
                }
                surf_out->Data.TimeStamp = (mfxU64) frmIO[LASTFRAME].frmProperties.timestamp;
                frmSupply->AddCPUPtirOutSurf(frmIO[LASTFRAME].ucMem, surf_out);

                Frame_Release(frmIn);
                free(frmIn);
                surf_out = 0;

                uiFrameOut++;
            }
        }
    }
    if(!surf_out)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_MORE_DATA;
}
