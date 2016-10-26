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
#include "../pacm/utilities.h"
#include "mfxvideo++int.h"

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

#define GPUPATH

DeinterlaceFilter* pdeinterlaceFilter;

PTIR_ProcessorCM::PTIR_ProcessorCM(mfxCoreInterface* mfxCore, frameSupplier* _frmSupply, eMFXHWType _HWType)
{
    deinterlaceFilter  = 0;
    m_pCmDevice        = 0;
    m_pCmProgram       = 0;
    m_pCmKernel1       = 0;
    m_pCmKernel2       = 0;
    mb_UsePtirSurfs    = false;
    CmToMfxSurfmap.clear();

    m_pmfxCore = mfxCore;
    frmSupply = _frmSupply;
    HWType = _HWType;
    isHW = true;
    bInited = false;
    b_firstFrameProceed = false;
    bFullFrameRate = false;
}

PTIR_ProcessorCM::~PTIR_ProcessorCM()
{
    //frmSupply->FreeFrames();
    frmSupply->SetDevice(0);
    frmSupply->SetMap(0);
    Close();
    if(deinterlaceFilter)
    {
        delete deinterlaceFilter;
        deinterlaceFilter = 0;
        pdeinterlaceFilter = 0;
    }
}

mfxStatus PTIR_ProcessorCM::Init(mfxVideoParam *par, mfxExtVPPDeinterlacing* pDeinter)
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

    //PTIR's frames init
    try //try is useless here since frames allocated by malloc, but probably in future it could be changed to new
    {
        mfxHandleType mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
        mfxHDL mfxDeviceHdl = 0;
        mfxCoreParam mfxCorePar;
        mfxStatus mfxSts = MFX_ERR_NONE;
        mfxSts = m_pmfxCore->GetCoreParam(m_pmfxCore->pthis, &mfxCorePar);
        if(MFX_ERR_NONE > mfxSts) 
            return mfxSts;
        if(MFX_IMPL_VIA_D3D9 == (mfxCorePar.Impl & 0xF00))
            mfxDeviceType = MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9;
        else if(MFX_IMPL_VIA_D3D11 == (mfxCorePar.Impl & 0xF00))
            mfxDeviceType = MFX_HANDLE_D3D11_DEVICE;
        else if(MFX_IMPL_VIA_VAAPI == (mfxCorePar.Impl & 0xF00))
            mfxDeviceType = MFX_HANDLE_VA_DISPLAY;
        else
            return MFX_ERR_DEVICE_FAILED;
        mfxSts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, mfxDeviceType, &mfxDeviceHdl);
        if(MFX_ERR_NONE > mfxSts) 
            return mfxSts;
        if(!HWType) return MFX_ERR_DEVICE_FAILED;
    
        //const char * pIsaFileNames[] = { "Deinterlace_genx.isa" };
        deinterlaceFilter = new DeinterlaceFilter(HWType, _uiInWidth, _uiInHeight, mfxDeviceType, mfxDeviceHdl);
        pdeinterlaceFilter = deinterlaceFilter;
    }
    catch(std::bad_alloc&)
    {
        bInited = false;
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...)
    {
        bInited = false;
        return MFX_ERR_UNKNOWN;
    }

    PTIRCM_Init(&Env, _uiInWidth, _uiInHeight, _dFrameRate, _uiInterlaceParity, Env.control.uiPatternTypeNumber,Env.control.uiPatternTypeInit);
    frmSupply->SetDevice(&deinterlaceFilter->DeviceEx());
    frmSupply->SetMap(&CmToMfxSurfmap);
    frmSupply->SetFrmBuffer(Env.frmBuffer);
    Env.frmSupply = (void*) frmSupply;
    work_par = *par;
    bInited = true;
    b_firstFrameProceed = false;

    return MFX_ERR_NONE;
}


mfxStatus PTIR_ProcessorCM::Close()
{
    if(bInited)
    {
        try
        {
            PTIRCM_Clean(&Env);

            bInited = false;

            if(deinterlaceFilter)
            {
                delete deinterlaceFilter;
                deinterlaceFilter = 0;
                pdeinterlaceFilter = 0;
            }
            return MFX_ERR_NONE;
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;;
        }
        catch(...) 
        { 
            return MFX_ERR_UNKNOWN;
        }
    }
    else
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    return MFX_ERR_UNKNOWN;
}

mfxStatus PTIR_ProcessorCM::Process(mfxFrameSurface1 *surface_in, mfxFrameSurface1 **surface_out, mfxCoreInterface *mfx_core, mfxFrameSurface1 **surface_outt, bool beof, mfxFrameSurface1 *exp_surf)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    mfxSts = PTIR_ProcessFrame(surface_in, *surface_out, 0, beof, exp_surf);
    *surface_out = 0;

    return mfxSts;
}

mfxStatus PTIR_ProcessorCM::PTIR_ProcessFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out,  mfxFrameSurface1 **surf_outt, bool beof, mfxFrameSurface1 *exp_surf)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    if(!b_firstFrameProceed)
    {
        //(divide TimeStamp by 90,000 (90 KHz) to obtain the time in seconds)
        if(!surf_in)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        pts = (double) (surf_in->Data.TimeStamp / 90);
        frame_duration = 1000 / _dFrameRate;

        mfxSts = MFX_PTIRCM_PutFrame(surf_in, surf_out, Env.frmBuffer[Env.control.uiCur], &Env, pts);
        if(mfxSts)
            return mfxSts;

        //if(static_cast<CmSurface2DEx*>(Env.frmBuffer[0]->inSurf)->pCmSurface2D && !mb_UsePtirSurfs)
        //    frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(Env.frmBuffer[0]->inSurf)->pCmSurface2D);
        //if(static_cast<CmSurface2DEx*>(Env.frmBuffer[0]->outSurf)->pCmSurface2D && !mb_UsePtirSurfs)
        //    frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(Env.frmBuffer[0]->outSurf)->pCmSurface2D);
        //if(static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D && !mb_UsePtirSurfs)
        //    frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D);
        if(static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D)
            frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D);

        if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D)
            static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE+1]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();

        PTIRCM_Frame_Prep_and_Analysis(&Env);
        Env.frmBuffer[0]->frmProperties.processed = false;

        uiStart = 1;
        Env.control.uiFrame = 0;

        b_firstFrameProceed = true;
        return MFX_ERR_MORE_DATA;
    }// if(!b_firstFrameProceed)

    if(surf_in)
    {
        //if(beof)
        //    Env.control.uiEndOfFrames = 1;

        if(Env.control.uiCur >= 1)
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
        mfxSts = MFX_PTIRCM_PutFrame(surf_in, surf_out, Env.frmBuffer[Env.control.uiCur], &Env, pts);
        if(mfxSts)
            return mfxSts;
        Env.control.uiFrame++;

        if(static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D == 0)
           static_cast<CmSurface2DEx*>(Env.frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();

        //if(beof)
        //{
        //    int result = -1;
        //    CmDeviceEx& device = pdeinterlaceFilter->DeviceEx();
        //    for(mfxU32 i = 0; i < (BUFMINSIZE - 1); ++i)
        //    {
        //        if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D)
        //        {
        //            static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
        //            assert(0 != static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D);
        //        }
        //        if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->inSurf)->pCmSurface2D)
        //        {
        //            result = device->CreateSurface2D(_uiInWidth, _uiInHeight, CM_SURFACE_FORMAT_NV12, static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->inSurf)->pCmSurface2D );
        //            assert(result == 0);
        //            b_firstFrameProceed = false;
        //        }
        //    }
        //}

        if(!PTIRCM_MultipleMode(&Env, uiOpMode))
        {
            while((Env.frmIn = PTIR_GetFrame(&Env)))
            {
                //if(!surf_out)
                //    surf_out = frmSupply->GetWorkSurfaceMfx();
                mfxSts = OutputFrameToMfx(Env.frmIn, surf_out, exp_surf, &uiLastFrameNumber);
                //OutputFrameToDisk(hOut, Env.frmIn, Env.frmIO[LASTFRAME], &uiLastFrameNumber, &uiBytesRead);
                //if(uiOpMode != 7)
                //    fprintf(fTCodeOut,"%4.3f\n",Env.frmIn->frmProperties.timestamp);

                Frame_ReleaseCM(Env.frmIn);
                free(Env.frmIn);
                surf_out = 0;
                Env.control.uiFrameOut++;
            }

            //if(beof)
            //    Env.control.uiCur--;
            if(!beof)
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

    if(surf_out || beof)
    {
        while(Env.control.uiCur)
        {
            Env.control.uiEndOfFrames = 1;

            //place fake CM surfaces
            if(beof)
            {
                int result = -1;
                CmDeviceEx& device = pdeinterlaceFilter->DeviceEx();
                for(mfxU32 i = 0; i < BUFMINSIZE +1; ++i)
                {
                    if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D)
                    {
                        static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
                        //result = device->CreateSurface2D(_uiInWidth, _uiInHeight, CM_SURFACE_FORMAT_NV12, static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D );
                        assert(0 != static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D);
                    }
                    if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->inSurf)->pCmSurface2D)
                    {
                        result = device->CreateSurface2D(_uiInWidth, _uiInHeight, CM_SURFACE_FORMAT_NV12, static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->inSurf)->pCmSurface2D );
                        assert(result == 0);
                        b_firstFrameProceed = false;
                    }
                }
                for(mfxU32 i = BUFMINSIZE + 1; i < LASTFRAME; ++i)
                {
                    if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D)
                    {
                        result = device->CreateSurface2D(_uiInWidth, _uiInHeight, CM_SURFACE_FORMAT_NV12, static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D );
                        assert(0 != static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->outSurf)->pCmSurface2D);
                    }
                    if(!static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->inSurf)->pCmSurface2D)
                    {
                        result = device->CreateSurface2D(_uiInWidth, _uiInHeight, CM_SURFACE_FORMAT_NV12, static_cast<CmSurface2DEx*>(Env.frmBuffer[i]->inSurf)->pCmSurface2D );
                        assert(result == 0);
                        b_firstFrameProceed = false;
                    }
                }
            }

            PTIRCM_MultipleMode(&Env, uiOpMode);
            while((Env.frmIn = PTIR_GetFrame(&Env)))
            {
                //if(!surf_out)
                //    surf_out = frmSupply->GetWorkSurfaceMfx();
                mfxSts = OutputFrameToMfx(Env.frmIn, surf_out, exp_surf, &uiLastFrameNumber);
                //if(uiOpMode != 7)
                //    fprintf(fTCodeOut,"%4.3lf\n",Env.frmIn->frmProperties.timestamp);
                Frame_ReleaseCM(Env.frmIn);
                free(Env.frmIn);
                //surf_out = 0;
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

    return MFX_ERR_MORE_DATA;
}

mfxStatus PTIR_ProcessorCM::MFX_PTIRCM_PutFrame(mfxFrameSurface1 *surf_in, mfxFrameSurface1 *surf_out, Frame* frame, PTIRSystemBuffer *SysBuffer, double dTimestamp)
{
    if(!surf_in || !frame || !m_pmfxCore || !SysBuffer)
        return MFX_ERR_NULL_PTR;
    mfxStatus mfxSts = MFX_ERR_NONE;

    frame->frmProperties.timestamp = dTimestamp;
    frame->frmProperties.fr = _dFrameRate;
    frame->frmProperties.processed = false;
    SysBuffer->control.uiFrameCount += 1;


    //Create or reuse CM surface
    CmSurface2D *pInCmSurface2D = 0;
    CmSurface2D *pOutCmSurface2D = 0;
#if defined(_DEBUG)
    //debug: find duplicates:
    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator ittt;
    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator ittt2;
    for (ittt = CmToMfxSurfmap.begin(); ittt != CmToMfxSurfmap.end(); ++ittt)
    {
        for (ittt2 = CmToMfxSurfmap.begin(); ittt2 != CmToMfxSurfmap.end(); ++ittt2)
        {
            if(ittt2 != ittt)
            {
                if(ittt2->second == ittt->second)
                {
                    assert(ittt2->second != ittt->second);
                }
            }
        }
    }
#endif

    CmDeviceEx& pCMdevice = this->deinterlaceFilter->DeviceEx();
    int result = -1;

    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
    for (it = CmToMfxSurfmap.begin(); it != CmToMfxSurfmap.end(); ++it)
    {
        if(surf_in == it->second)
        {
            pInCmSurface2D = it->first;
            if(work_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                frmSupply->CMCopySysToGpu(surf_in, pInCmSurface2D);
            break;
        }
    }
    if(!pInCmSurface2D)
    {
        mfxSts = frmSupply->CMCreateSurfaceIn(surf_in,pInCmSurface2D,true);
        if(MFX_ERR_NONE != mfxSts)
            return mfxSts;
    }

    for (it = CmToMfxSurfmap.begin(); it != CmToMfxSurfmap.end(); ++it)
    {
        if(surf_out == it->second)
        {
            pOutCmSurface2D = it->first;
            break;
        }
    }
    if(!pOutCmSurface2D)
    {
        mfxSts = frmSupply->CMCreateSurfaceOut(surf_out,pOutCmSurface2D,false);
        if(MFX_ERR_NONE != mfxSts)
            return mfxSts;
    }

    if(static_cast<CmSurface2DEx*>(frame->inSurf)->pCmSurface2D)
    {
        mfxSts = frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(frame->inSurf)->pCmSurface2D);
    }
    if(static_cast<CmSurface2DEx*>(frame->outSurf)->pCmSurface2D)
    {
        mfxSts = frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(frame->outSurf)->pCmSurface2D);
    }

    static_cast<CmSurface2DEx*>(frame->inSurf)->pCmSurface2D = pInCmSurface2D;
    static_cast<CmSurface2DEx*>(frame->outSurf)->pCmSurface2D = pOutCmSurface2D;

    if(static_cast<CmSurface2DEx*>(SysBuffer->frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D == 0)
    {
        static_cast<CmSurface2DEx*>(SysBuffer->frmBuffer[BUFMINSIZE]->outSurf)->pCmSurface2D = frmSupply->GetWorkSurfaceCM();
    }

    return MFX_ERR_NONE;
}


mfxStatus PTIR_ProcessorCM::OutputFrameToMfx(Frame* frmOut, mfxFrameSurface1* surf_out, mfxFrameSurface1*& exp_surf, unsigned int * uiLastFrameNumber)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1* output = 0;
    mfxFrameSurface1* input  = 0;

    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
    it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmOut->outSurf)->pCmSurface2D);
    if(it != CmToMfxSurfmap.end())
        output = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmOut->outSurf)->pCmSurface2D];
    it = CmToMfxSurfmap.find(static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D);
    if(it != CmToMfxSurfmap.end())
        input = CmToMfxSurfmap[static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D];

    if(input)
    {
        if(frmOut->frmProperties.detection)
        {
            if(work_par.vpp.In.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
                input->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
            else
                input->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else
        {
            input->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
    }
    //assert(0 != output);
    //assert(0 != input);
    if(output && frmOut->outState == Frame::OUT_UNCHANGED)
    {
        assert(0 != static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D);
        frmSupply->CMCopyGPUToGpu(static_cast<CmSurface2DEx*>(frmOut->outSurf)->pCmSurface2D, static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D);
    }

    if(output)
    {
        frmSupply->AddOutputSurf(output, exp_surf);
        exp_surf = 0;
    }
    else
    {
        output = frmSupply->GetWorkSurfaceMfx();
        if(output)
        {
            bool ready = false;
            CmSurface2D* out_cm = 0;
            mfxSts = frmSupply->CMCreateSurfaceOut(output, out_cm, false);
            if(mfxSts)
                return mfxSts;
            if(!out_cm)
                return MFX_ERR_DEVICE_FAILED;
            if(frmOut->outState == Frame::OUT_UNCHANGED && static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D)
            {
                mfxSts = frmSupply->CMCopyGPUToGpu(out_cm, static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D);
                ready = true;
            }
            else if(frmOut->outState == Frame::OUT_PROCESSED && static_cast<CmSurface2DEx*>(frmOut->outSurf)->pCmSurface2D)
            {
                mfxSts = frmSupply->CMCopyGPUToGpu(out_cm, static_cast<CmSurface2DEx*>(frmOut->outSurf)->pCmSurface2D);
                ready = true;
            }
            else
            {
                assert(false);
            }
            if(mfxSts) 
                return mfxSts;
            frmSupply->AddOutputSurf(output, exp_surf);
            exp_surf = 0;
        }
    }
    if(output)
        output->Data.TimeStamp = (mfxU64) (frmOut->frmProperties.timestamp * 90);

    mfxSts = frmSupply->FreeSurface(static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D);
    static_cast<CmSurface2DEx*>(frmOut->outSurf)->pCmSurface2D = 0;
    static_cast<CmSurface2DEx*>(frmOut->inSurf)->pCmSurface2D = 0;

    return MFX_ERR_NONE;
}