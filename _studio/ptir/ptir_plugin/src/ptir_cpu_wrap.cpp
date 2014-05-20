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
    //uiCount = MFX_INFINITE / 2; // /2 to avoid overflow. TODO: fix
    uiSupBuf = BUFMINSIZE;

    b_firstFrameProceed = false;
    uiCur = 0;
    uiStart = 0;
    m_pmfxCore = mfxCore;
    frmSupply = _frmSupply;
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
    //uiInWidth  = uiWidth  = par->vpp.In.Width;
    //uiInHeight = uiHeight = par->vpp.In.Height;
    uiInWidth  = uiWidth  = par->vpp.In.CropW;
    uiInHeight = uiHeight = par->vpp.In.CropH;
    if(par->vpp.In.FrameRateExtN && par->vpp.In.FrameRateExtD)
        dFrameRate = (double) par->vpp.In.FrameRateExtN / par->vpp.In.FrameRateExtD;
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
        for(i = 0; i < LASTFRAME; i++)
        {
            Frame_Create(&frmIO[i], uiInWidth, uiInHeight, uiInWidth / 2, uiInHeight / 2, 0);
            frmBuffer[i] = frmIO + i;
            if(!frmIO[i].ucMem)
            {
                Close();
                return MFX_ERR_MEMORY_ALLOC;
            }
        }
        Frame_Create(&frmIO[LASTFRAME], uiWidth, uiHeight, uiWidth / 2, uiHeight / 2, 0);
        if(!frmIO[LASTFRAME].ucMem)
        {
            Close();
            return MFX_ERR_MEMORY_ALLOC;
        }
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

mfxStatus PTIR_ProcessorCPU::Process(mfxFrameSurface1 *surface_in, mfxFrameSurface1 **surface_out, mfxCoreInterface *mfx_core, mfxFrameSurface1**, bool beof)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;

    if(0 != surface_in && 0 != surface_out && 0 != (*surface_out))
    {
        bool isUnlockReq = false;

        if(surface_in->Data.MemId)
        {
            mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = true;
        }
        mfxCCSts = MfxNv12toPtir420(surface_in, frmBuffer[uiSupBuf]->ucMem);
        assert(!mfxCCSts);
        frmBuffer[uiSupBuf]->frmProperties.fr = dFrameRate;

        mfxSts = PTIR_ProcessFrame( surface_in, *surface_out );

        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
        {
            if((*surface_out)->Data.MemId)
            {
                mfx_core->FrameAllocator.Lock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
                isUnlockReq = true;
            }
            //mfxCCSts = Ptir420toMfxNv12(frmBuffer[uiSupBuf]->ucMem, surface_in);
            //assert(!mfxCCSts);
            //frmSupply->AddOutputSurf((*surface_out));
        }
        if(isUnlockReq)
        {
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, (*surface_out)->Data.MemId, &((*surface_out)->Data));
            mfx_core->FrameAllocator.Unlock(mfx_core->FrameAllocator.pthis, surface_in->Data.MemId, &(surface_in->Data));
            isUnlockReq = false;
        }
        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
            (*surface_out) = 0;
        return mfxSts;
    }
    else if(NULL != surface_in && (NULL == surface_out || NULL == (*surface_out)))
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

        mfxSts = PTIR_ProcessFrame( &work420_surface, 0 );

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

        mfxSts = PTIR_ProcessFrame( 0, *surface_out );

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

mfxStatus PTIR_ProcessorCPU::PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out)
{

    if(!b_firstFrameProceed)
    {
        uiStart = 1;
        memcpy(frmBuffer[0]->ucMem, frmBuffer[uiSupBuf]->ucMem, frmBuffer[uiSupBuf]->uiSize);
        frmBuffer[0]->frmProperties.tindex = uiStart + 1;
        sadCalc_I420_frame(frmBuffer[0],frmBuffer[0]);
        Rs_measurement(frmBuffer[0]);
        Artifacts_Detection_frame(frmBuffer, 0, TRUE);
        frmBuffer[0]->frmProperties.processed = FALSE;

        frmBuffer[0]->plaY.ucStats.ucSAD[0] = 99.999;
        frmBuffer[0]->plaY.ucStats.ucSAD[1] = 99.999;

        uiCur = 1;
        uiCurTemp = uiCur;
        uiBufferCount = 0;
        patternFound = FALSE;
        //bFullFrameRate = FALSE;
        uiDispatch = 0;
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
        b_firstFrameProceed = true;

        uiFrame = uiStart;

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

                memcpy(frmBuffer[uiCur]->ucMem, frmBuffer[uiSupBuf]->ucMem, frmBuffer[uiSupBuf]->uiSize);
                frmBuffer[uiCur]->frmProperties.tindex = uiFrame + 1;
                sadCalc_I420_frame(frmBuffer[uiCur], frmBuffer[uiNext]);
                Rs_measurement(frmBuffer[uiCur]);
                frmBuffer[uiCur]->frmProperties.processed = TRUE;
                dSAD = frmBuffer[uiCur]->plaY.ucStats.ucSAD;
                dRs = frmBuffer[uiCur]->plaY.ucStats.ucRs;
    //#if PRINTX == 2
    //            printf("%i\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.0lf\t%4.3lf\t%4.3lf\t%4.3lf\t%4.3lf\n", uiFrame + 1, dSAD[0], dSAD[1], dSAD[2], dSAD[3], dSAD[4], dRs[0], dRs[1], dRs[2], dRs[3], dRs[4], dRs[5] / dPicSize * 1000, dRs[6], dRs[7] / dPicSize * 1000, dRs[8], dRs[9], dRs[10]);
    //#endif
                if ((uiCur == BUFMINSIZE - 1) /*|| (uiFrame == (uiCount - 1))*/)
                {
                    Analyze_Buffer_Stats(frmBuffer, &mainPattern, &uiDispatch, &uiisInterlaced);
                    if(mainPattern.ucPatternFound && uiisInterlaced != 1)
                    {
                        if (mainPattern.ucPatternType == 0 && uiisInterlaced != 2)
                            dOutBaseTime = dBaseTime;
                        else
                            dOutBaseTime = (dBaseTime * 5 / 4);

                        for(mfxU32 i = 0; i < min(uiDispatch,uiCur + 1); i++)
                        {
                            if(!frmBuffer[i]->frmProperties.drop)
                            {
                                CheckGenFrame(frmBuffer, i, mainPattern.ucPatternType);
                                Prepare_frame_for_queue(&frmIn,frmBuffer[i], uiWidth, uiHeight);
                                memcpy(frmIn->plaY.ucStats.ucRs,frmBuffer[i]->plaY.ucStats.ucRs,sizeof(double) * 10);

                                //Timestamp
                                frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dOutBaseTime;
                                frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                                FrameQueue_Add(&fqIn, frmIn);
                            }
                            else
                            {
                                frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;
                                frmBuffer[i]->frmProperties.drop = FALSE;
                            }
                        }
                        Rotate_Buffer_borders(frmBuffer, uiDispatch);
                        uiCur -= min(uiDispatch,uiCur);
                        dBaseTimeSw = dOutBaseTime;
                    }
                    else
                    {
                        uiCurTemp = uiCur;
                        if (uiisInterlaced == 2)
                            dBaseTimeSw = (dBaseTime * 5 / 4);
                        for(i = 0; i < min(uiDispatch,uiCurTemp + 1); i++)
                        {
                            FillBaseLinesIYUV(frmBuffer[0], frmBuffer[BUFMINSIZE], FALSE, FALSE);
                            if (!frmBuffer[0]->frmProperties.drop)
                            {
                                uiBufferCount++;
                                if ((uiisInterlaced == 0 && !(mainPattern.uiInterlacedFramesNum == (BUFMINSIZE - 1))) || uiisInterlaced == 2)
                                {
                                    if ((uiisInterlaced == 2 && uiBufferCount < (BUFMINSIZE - 1)) || uiisInterlaced == 0)
                                    {
                                        if (frmBuffer[0]->frmProperties.interlaced)
                                        {
                                            //First field
                                            DeinterlaceMedianFilter(frmBuffer, 0, 0);
                                            //Second field
                                            DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, 1);
                                            Prepare_frame_for_queue(&frmIn, frmBuffer[0], uiWidth, uiHeight);
                                        }
                                        else
                                            Prepare_frame_for_queue(&frmIn, frmBuffer[0], uiWidth, uiHeight);
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
                                    //First field
                                    DeinterlaceMedianFilter(frmBuffer, 0, 0);
                                    //Second field
                                    DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, 1);

                                    Prepare_frame_for_queue(&frmIn, frmBuffer[0], uiWidth, uiHeight); // Go to input frame rate
                                    memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[0]->plaY.ucStats.ucRs, sizeof(double)* 10);

                                    //Timestamp
                                    frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[0]->frmProperties.tindex;
                                    frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp + dDeIntTime;
                                    frmBuffer[1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                                    frmIn->frmProperties.timestamp = frmBuffer[0]->frmProperties.timestamp;

                                    FrameQueue_Add(&fqIn, frmIn);
                                    if (bFullFrameRate && uiisInterlaced == 1)
                                    {
                                        Prepare_frame_for_queue(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight); // Go to double frame rate
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
        }

    if(surf_out)
    {
        frmIn = NULL;
        //QueryPerformanceCounter(&liTime[uiTimer++]);
        if(fqIn.iCount >= 1 && !mainPattern.ucPatternFound)
        {
            uiFrameCount = fqIn.iCount;
            for(mfxU32 i = 0; i < uiFrameCount; i++)
            {
                if(!surf_out && !frmSupply->countFreeWorkSurfs())
                    break;
                uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                frmIn = FrameQueue_Get(&fqIn);
                //----------------------------------------
                // Saving Test Output
                //----------------------------------------
                if (frmIn)
                {
                    ferror = FALSE;
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                    //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                    //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                    if(!surf_out){
                        surf_out = frmSupply->GetWorkSurfaceMfx();
                    }
                    frmSupply->AddCPUPtirOutSurf(frmIO[LASTFRAME].ucMem, surf_out);
                    Frame_Release(frmIn);
                    free(frmIn);
                    surf_out = 0;

                    uiFrameOut++;
                }
            }
        }
        else if(mainPattern.ucPatternFound)
        {
            mainPattern.ucPatternFound = FALSE;
            uiFrameCount = fqIn.iCount;

            for(mfxU32 i = 0; i < uiFrameCount; i++)
            {
                if(!surf_out && !frmSupply->countFreeWorkSurfs())
                    break;
                ferror = FALSE;
                uiLastFrameNumber = fqIn.pfrmHead->pfrmItem->frmProperties.tindex;
                frmIn = FrameQueue_Get(&fqIn);
                if (frmIn)
                {
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                    //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                    //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                    if(!surf_out){
                        surf_out = frmSupply->GetWorkSurfaceMfx();
                    }
                    frmSupply->AddCPUPtirOutSurf(frmIO[LASTFRAME].ucMem, surf_out);
                    //memcpy(surf_out->Data.Y, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize);
                    Frame_Release(frmIn);
                    free(frmIn);
                    surf_out = 0;

                    uiFrameOut++;
                }
            }
        }
        //QueryPerformanceCounter(&liTime[uiTimer++]);

        //// Update Time Counters
        //for (i = 0; i < uiTimer - 1; ++i)
        //    dTime[i] += (double)(liTime[i+1].QuadPart - liTime[i].QuadPart) / liFreq.QuadPart;
    //}

        if(uiCur && !surf_in)
        {
            for(mfxU32 i = 0; i < uiCur; i++)
            {
                if (!frmBuffer[i]->frmProperties.drop /*&& (frmBuffer[i]->frmProperties.tindex > uiLastFrameNumber)*/)
                {
                    if (uiisInterlaced == 1)
                    {
                        FillBaseLinesIYUV(frmBuffer[i], frmBuffer[BUFMINSIZE], FALSE, FALSE);
                        DeinterlaceMedianFilter(frmBuffer, i, 0);
                        //Second field
                        DeinterlaceMedianFilter(frmBuffer, BUFMINSIZE, 1);

                        Prepare_frame_for_queue(&frmIn, frmBuffer[i], uiWidth, uiHeight); // Go to input frame rate
                        memcpy(frmIn->plaY.ucStats.ucRs, frmBuffer[i]->plaY.ucStats.ucRs, sizeof(double)* 10);

                        //Timestamp
                        frmBuffer[BUFMINSIZE]->frmProperties.tindex = frmBuffer[i]->frmProperties.tindex;
                        frmBuffer[BUFMINSIZE]->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp + dDeIntTime;
                        frmBuffer[i + 1]->frmProperties.timestamp = frmBuffer[BUFMINSIZE]->frmProperties.timestamp + dDeIntTime;
                        frmIn->frmProperties.timestamp = frmBuffer[i]->frmProperties.timestamp;

                        FrameQueue_Add(&fqIn, frmIn);
                        if (bFullFrameRate)
                        {
                            Prepare_frame_for_queue(&frmIn, frmBuffer[BUFMINSIZE], uiWidth, uiHeight); // Go to double frame rate
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
                            CheckGenFrame(frmBuffer, i, mainPattern.ucPatternType);
                            Prepare_frame_for_queue(&frmIn, frmBuffer[i], uiWidth, uiHeight);
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
            for(mfxU32 i = 0; i < uiFrameCount; i++)
            {
                if(!surf_out && !frmSupply->countFreeWorkSurfs())
                    break;
                ferror = FALSE;
                frmIn = FrameQueue_Get(&fqIn);
                if (frmIn)
                {
                    TrimBorders(&frmIn->plaY, &frmIO[LASTFRAME].plaY);
                    TrimBorders(&frmIn->plaU, &frmIO[LASTFRAME].plaU);
                    TrimBorders(&frmIn->plaV, &frmIO[LASTFRAME].plaV);

                        //fprintf(fTCodeOut,"%4.3lf\n",frmIn->frmProperties.timestamp);
                        //ferror = WriteFile(hOut, frmIO[LASTFRAME].ucMem, frmIO[LASTFRAME].uiSize, &uiBytesRead, NULL);
                    if(!surf_out){
                        surf_out = frmSupply->GetWorkSurfaceMfx();
                    }
                    frmSupply->AddCPUPtirOutSurf(frmIO[LASTFRAME].ucMem, surf_out);
                    Frame_Release(frmIn);
                    free(frmIn);
                    surf_out = 0;

                    uiFrameOut++;
                }
            }
        }
    }
    return MFX_ERR_MORE_DATA;
}