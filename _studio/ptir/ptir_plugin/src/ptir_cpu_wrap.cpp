//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "ptir_vpp_plugin.h"

PTIR_ProcessorCPU::PTIR_ProcessorCPU(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply)
{
    i;
    _uiInWidth;
    _uiInHeight;
    uiTimer;
    uiOpMode;
    uiProgress;
    uiStart;
    uiCount;
    uiLastFrameNumber = 0;
    _uiInterlaceParity = 0;
     pucIO;
    dOutBaseTime = 0.0;
    _dFrameRate = 0.0;
    dTime= 0.0;
    dTimeTotal = 0.0;
    pts = 0.0;
    frame_duration = 0.0;//1000 / _dFrameRate;


    b_firstFrameProceed = false;
    m_pmfxCore = mfxCore;
    frmSupply = _frmSupply;
    isHW = false;
    bFullFrameRate = false;
}

PTIR_ProcessorCPU::~PTIR_ProcessorCPU()
{
    //if (b_inited)
    //{
        Close();
    //}
}

mfxStatus PTIR_ProcessorCPU::Init(mfxVideoParam *par, mfxExtVPPDeinterlacing* pDeinter)
{
    //maximize b2b with reference app
    _uiInWidth  = par->vpp.In.CropX + par->vpp.In.CropW;
    _uiInHeight = par->vpp.In.CropY + par->vpp.In.CropH;

    if(par->vpp.In.FrameRateExtN && par->vpp.In.FrameRateExtD)
        _dFrameRate = (double) par->vpp.In.FrameRateExtN / par->vpp.In.FrameRateExtD;
    else
        _dFrameRate = 30.0;

    _uiInterlaceParity = 0;
    Env.control.uiPatternTypeNumber = 0;
    Env.control.uiPatternTypeInit = 0;

    bool buffer_mode = pDeinter ? true : false;

    if(!buffer_mode)
    {
        if(MFX_PICSTRUCT_UNKNOWN == par->vpp.In.PicStruct &&
           0 == par->vpp.In.FrameRateExtN &&
           0 != par->vpp.Out.FrameRateExtD &&
           30 == (par->vpp.Out.FrameRateExtN / par->vpp.Out.FrameRateExtD)
           )
        {
            uiOpMode = PTIR_AUTO_SINGLE;
        }
        else if (MFX_PICSTRUCT_UNKNOWN == par->vpp.In.PicStruct &&
           0 == par->vpp.In.FrameRateExtN &&
           0 != par->vpp.Out.FrameRateExtD &&
           60 == (par->vpp.Out.FrameRateExtN / par->vpp.Out.FrameRateExtD)
           )
        {
            uiOpMode = PTIR_AUTO_DOUBLE;
            bFullFrameRate = true;
        }
        else if((MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
                 MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct) &&
              ((4 * par->vpp.In.FrameRateExtN * par->vpp.In.FrameRateExtD ==
                5 * par->vpp.Out.FrameRateExtN * par->vpp.Out.FrameRateExtD) ||
               (30000 == par->vpp.In.FrameRateExtN && 1001 == par->vpp.In.FrameRateExtD &&
                24000 == par->vpp.Out.FrameRateExtN && 1001 == par->vpp.Out.FrameRateExtD)
                ))
        {
            if(MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct)
                _uiInterlaceParity = 0;
            else
                _uiInterlaceParity = 1;
            uiOpMode = PTIR_OUT24FPS_FIXED;
        }
        else if(MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct ||
               MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct)
        {
            //Deinterlace only mode
            if(MFX_PICSTRUCT_FIELD_TFF == par->vpp.In.PicStruct)
                _uiInterlaceParity = 0;
            else
                _uiInterlaceParity = 1;

            if(2 * par->vpp.In.FrameRateExtN * par->vpp.Out.FrameRateExtD ==
                par->vpp.In.FrameRateExtD * par->vpp.Out.FrameRateExtN)
            {
                //30i -> 60p mode
                uiOpMode = PTIR_DEINTERLACE_FULL;
                bFullFrameRate = true;
            }
            else if(par->vpp.In.FrameRateExtN * par->vpp.Out.FrameRateExtD ==
                par->vpp.In.FrameRateExtD * par->vpp.Out.FrameRateExtN)
            {
                //30i -> 30p mode
                uiOpMode = PTIR_DEINTERLACE_HALF;
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
    }
    else
    {
        if(!pDeinter)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        switch(pDeinter->Mode)
        {
        case MFX_DEINTERLACING_AUTO_DOUBLE:
            uiOpMode = PTIR_AUTO_DOUBLE;
            bFullFrameRate = true;
            break;
        case MFX_DEINTERLACING_AUTO_SINGLE:
            uiOpMode = PTIR_AUTO_SINGLE;
            break;
        case MFX_DEINTERLACING_FULL_FR_OUT:
            uiOpMode = PTIR_DEINTERLACE_FULL;
            bFullFrameRate = true;
            break;
        case MFX_DEINTERLACING_HALF_FR_OUT:
            uiOpMode = PTIR_DEINTERLACE_HALF;
            break;
        case MFX_DEINTERLACING_24FPS_OUT:
            uiOpMode = PTIR_OUT24FPS_FIXED;
            break;
        case MFX_DEINTERLACING_FIXED_TELECINE_PATTERN:
            uiOpMode = PTIR_FIXED_TELECINE_PATTERN_REMOVAL;
            Env.control.uiPatternTypeInit = pDeinter->TelecinePattern;
            if(pDeinter->TelecineLocation < 5)
                Env.control.uiPatternTypeNumber = pDeinter->TelecineLocation;
            else
                Env.control.uiPatternTypeNumber = 0;
            break;
        case MFX_DEINTERLACING_30FPS_OUT:
            uiOpMode = PTIR_OUT30FPS_FIXED;
            break;
        case MFX_DEINTERLACING_DETECT_INTERLACE:
            uiOpMode = PTIR_ONLY_DETECT_INTERLACE;
            break;
        default: 
            return MFX_ERR_INVALID_VIDEO_PARAM;
            break;
        }

        if((bFullFrameRate || PTIR_FIXED_TELECINE_PATTERN_REMOVAL == uiOpMode || PTIR_DEINTERLACE_HALF == uiOpMode || PTIR_OUT24FPS_FIXED == uiOpMode ) && 
            MFX_PICSTRUCT_FIELD_BFF == par->vpp.In.PicStruct)
            _uiInterlaceParity = 1;
        else
            _uiInterlaceParity = 0;
    }

    PTIR_Init(&Env, _uiInWidth, _uiInHeight, _dFrameRate, _uiInterlaceParity, Env.control.uiPatternTypeNumber,Env.control.uiPatternTypeInit);

    work_par = *par;
    bInited = true;
    b_firstFrameProceed = false;

    return MFX_ERR_NONE;
}


mfxStatus PTIR_ProcessorCPU::Close()
{
    if(bInited)
    {
        try
        {
            free(pucIO);

            PTIR_Clean(&Env);
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

    if(true)
    {
        mfxSts = PTIR_ProcessFrame( surface_in, *surface_out, beof);
        if(MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts)
            (*surface_out) = 0;
        return mfxSts;
    }

    return mfxSts;
}

mfxStatus PTIR_ProcessorCPU::PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, bool beof, mfxFrameSurface1 *)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    if(!b_firstFrameProceed)
    {
        //(divide TimeStamp by 90,000 (90 KHz) to obtain the time in seconds)
        if(!surf_in)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        pts = (double) (surf_in->Data.TimeStamp / 90);
        frame_duration = 1000 / _dFrameRate;

        mfxSts = MFX_PTIR_PutFrame(surf_in, &Env, pts);
        if(mfxSts)
            return mfxSts;
        PTIR_Frame_Prep_and_Analysis(&Env);
        Env.frmBuffer[0]->frmProperties.processed = false;

        uiStart = 1;
        Env.control.uiFrame = 0;

        b_firstFrameProceed = true;
        return MFX_ERR_MORE_DATA;
    }// if(!b_firstFrameProceed)

    if(surf_in)
    {
        if (Env.control.uiCur >= 1)
            Env.control.uiNext = Env.control.uiCur - 1;
        else
        {
            assert(0);
        }
        uiProgress = 0;
        uiTimer = 0;

    //if (Env.control.uiFrame < uiStart + uiCount)
    //{
        pts += frame_duration;
        mfxSts = MFX_PTIR_PutFrame(surf_in, &Env, pts);
        if(mfxSts)
            return mfxSts;
        Env.control.uiFrame++;
        if(!PTIR_MultipleMode(&Env, uiOpMode))
        {
            while((Env.frmIn = PTIR_GetFrame(&Env)))
            {
                if(!surf_out)
                    surf_out = frmSupply->GetWorkSurfaceMfx();
                mfxSts = OutputFrameToMfx(Env.frmIn, Env.frmIO[LASTFRAME], surf_out, &uiLastFrameNumber);
                //OutputFrameToDisk(hOut, Env.frmIn, Env.frmIO[LASTFRAME], &uiLastFrameNumber, &uiBytesRead);
                //if(uiOpMode != 7)
                //    fprintf(fTCodeOut,"%4.3f\n",Env.frmIn->frmProperties.timestamp);

                Frame_Release(Env.frmIn);
                free(Env.frmIn);
                surf_out = 0;
                Env.control.uiFrameOut++;
            }
            return MFX_ERR_NONE;
        }
        else
        {
//            QueryPerformanceCounter(&liTime[uiTimer++]);
//            QueryPerformanceCounter(&liTime[uiTimer++]);
        }
//#if defined(_WIN32) || defined(_WIN64)
//        // Update Time Counters
//        for (i = 0; i < uiTimer - 1; ++i)
//            dTime[i] += (double)(liTime[i+1].QuadPart - liTime[i].QuadPart) / liFreq.QuadPart;
//#endif
//    }


        //----------------------------------------
        // Output
        //----------------------------------------
        //if(uiOpMode != 7)
        //    PrintOutputStats(hStd, sbInfo, Env.control.uiFrame, uiStart, uiCount, &uiProgress, Env.control.uiFrameOut, uiTimer, Env.fqIn, cOperations, dTime);
        //else
        //    PrintOutputStats(hStd, sbInfo, Env.control.uiFrame, uiStart, uiCount, &uiProgress, Env.control.uiFrame, uiTimer, Env.fqIn, cOperations, dTime);

        return MFX_ERR_MORE_DATA;
    } //if(surf_in)

    if(surf_out)
    {
        while(Env.control.uiCur)
        {
            Env.control.uiEndOfFrames = 1;
            PTIR_MultipleMode(&Env, uiOpMode);
            while((Env.frmIn = FrameQueue_Get(&Env.fqIn)))
            {
                if(!surf_out)
                    surf_out = frmSupply->GetWorkSurfaceMfx();
                mfxSts = OutputFrameToMfx(Env.frmIn, Env.frmIO[LASTFRAME], surf_out, &uiLastFrameNumber);
                //if(uiOpMode != 7)
                //    fprintf(fTCodeOut,"%4.3lf\n",Env.frmIn->frmProperties.timestamp);
                Frame_Release(Env.frmIn);
                free(Env.frmIn);
                surf_out = 0;
                Env.control.uiFrameOut++;
            }
            if(Env.control.uiCur > 0 )
                Env.control.uiCur--;
        }

        b_firstFrameProceed = false;
        return MFX_ERR_NONE;

        //if(uiOpMode != 7)
        //    PrintOutputStats(hStd, sbInfo, Env.control.uiFrame, uiStart, uiCount, &uiProgress, Env.control.uiFrameOut, uiTimer, Env.fqIn, cOperations, dTime);
        //else
        //    PrintOutputStats(hStd, sbInfo, Env.control.uiFrame, uiStart, uiCount, &uiProgress, Env.control.uiFrame, uiTimer, Env.fqIn, cOperations, dTime);
        //
        //if(uiOpMode == 6 || uiOpMode == 7) PrintOutputStats_PvsI(Env.control.mainPattern.ulCountInterlacedDetections, uiCount, cSummary);


    } //if(surf_out)

    return MFX_ERR_UNKNOWN;
}

mfxStatus PTIR_ProcessorCPU::MFX_PTIR_PutFrame(mfxFrameSurface1 *surf_in, PTIRSystemBuffer *SysBuffer, double dTimestamp)
{
    if(!surf_in || !SysBuffer || !m_pmfxCore || !SysBuffer->frmBuffer[SysBuffer->control.uiCur])
        return MFX_ERR_NULL_PTR;

    unsigned int status = 0;
    mfxStatus mfxSts   = MFX_ERR_NONE;
    mfxStatus mfxCCSts = MFX_ERR_NONE;
    bool isUnlockReq = false;
    mfxFrameSurface1* realSurf = surf_in;

    SysBuffer->frmBuffer[SysBuffer->control.uiCur]->frmProperties.timestamp = dTimestamp;

    if(work_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxSts = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, surf_in, &realSurf);
        if(mfxSts)
            return mfxSts;
    }

    if(realSurf->Data.MemId)
    {
        m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, &(realSurf->Data));
        isUnlockReq = true;
    }
    mfxCCSts = MfxNv12toPtir420(realSurf, Env.frmBuffer[BUFMINSIZE]->ucMem);
    if(mfxCCSts)
        return MFX_ERR_DEVICE_FAILED;
    assert(!mfxCCSts);
    Env.frmBuffer[BUFMINSIZE]->frmProperties.fr = _dFrameRate;
    SysBuffer->control.uiFrameCount += 1;

    if(isUnlockReq)
    {
        m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, realSurf->Data.MemId, &(realSurf->Data));
        isUnlockReq = false;
    }
    mfxSts = m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &surf_in->Data);
    if(mfxSts)
        return mfxSts;

    //SysBuffer->control.uiNext = SysBuffer->control.uiCur - 1;

    return MFX_ERR_NONE;
}

mfxStatus PTIR_ProcessorCPU::OutputFrameToMfx(Frame* frmIn, Frame* frmOut, mfxFrameSurface1 *surf_out, unsigned int * uiLastFrameNumber)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    *uiLastFrameNumber = frmIn->frmProperties.tindex;

    //TrimBorders(&frmIn->plaY, &frmOut->plaY);
    //TrimBorders(&frmIn->plaU, &frmOut->plaU);
    //TrimBorders(&frmIn->plaV, &frmOut->plaV);

    mfxSts = frmSupply->AddCPUPtirOutSurf(frmIn->ucMem, surf_out);
    surf_out->Data.TimeStamp = (mfxU64) (frmIn->frmProperties.timestamp * 90);
    if(mfxSts) return mfxSts;
    //ferror = WriteFile(hOut, frmOut->ucMem, frmOut->uiSize, uiBytesRead, NULL);

    return MFX_ERR_NONE;
}