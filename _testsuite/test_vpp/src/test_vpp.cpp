//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2012 Intel Corporation. All Rights Reserved.
//

#include <vector>

#include "test_vpp_utils.h"
#include "test_statistics.h"
#include "test_vpp_pts.h"
#include "test_vpp_roi.h"
#include "vm_interlocked.h"

#define MFX_CHECK(sts) {if (sts != MFX_ERR_NONE) return sts;}

using namespace std;

void IncreaseReference(mfxFrameData *ptr)
{
    vm_interlocked_inc16((volatile Ipp16u*)&ptr->Locked);
}

void DecreaseReference(mfxFrameData *ptr)
{
    vm_interlocked_dec16((volatile Ipp16u*)&ptr->Locked);
}

void PutPerformanceToFile(sInputParams& Params, mfxF64 FPS)
{
    vm_file *fPRF;
    fPRF = vm_file_fopen(Params.strPerfFile, VM_STRING("ab"));
    if (!fPRF)
        return;

    vm_char *iopattern = IOpattern2Str(Params.IOPattern);
    vm_char   iopattern_ascii[32] = {0};
    vm_char  *pIOP = iopattern_ascii;
    while (*iopattern)
        *pIOP++ = (vm_char)*iopattern++;
    vm_char *srcFileName = Params.strSrcFile;
    vm_char   srcFileName_ascii[1024] = {0};
    vm_char  *pFileName = srcFileName_ascii;
    while (*srcFileName)
        *pFileName++ = (vm_char)*srcFileName++;

    vm_char filters[11];
    vm_char *_tptr = filters;

    if (Params.frameInfo[0].PicStruct != Params.frameInfo[1].PicStruct)
    {
        vm_string_sprintf(_tptr, VM_STRING("DI "));
        _tptr += 3;
    }
    if (VPP_FILTER_DISABLED != Params.denoiseParam.mode)
    {
        vm_string_sprintf(_tptr, VM_STRING("DN "));
        _tptr += 3;

    }
    if (VPP_FILTER_DISABLED != Params.vaParam.mode)
    {
        vm_string_sprintf(_tptr, VM_STRING("SA "));
        _tptr += 3;

    }
    if (_tptr == filters)
        vm_string_sprintf(_tptr, VM_STRING("NoFilters "));


    vm_file_fprintf(fPRF, VM_STRING("%s, %dx%d, %dx%d, %s, %s, %f\r\n"), srcFileName_ascii,
        Params.frameInfo[0].nWidth, 
        Params.frameInfo[0].nHeight, 
        Params.frameInfo[1].nWidth, 
        Params.frameInfo[1].nHeight, 
        iopattern_ascii,
        filters,
        FPS);
    vm_file_fclose(fPRF);

} // void PutPerformanceToFile(sInputVppParams& Params, mfxF64 FPS)

const sOwnFrameInfo  defaultOwnFrameInfo = {0, 352, 288, 0, 0, 352, 288, MFX_FOURCC_NV12, MFX_PICSTRUCT_PROGRESSIVE, 30.0};
const sProcAmpParam  defaultProcAmpParam = { 0.0, 1.0, 1.0, 0.0, VPP_FILTER_DISABLED};
const sDetailParam   defaultDetailParam  = { 1,  VPP_FILTER_DISABLED};
const sDenoiseParam  defaultDenoiseParam = { 1,  VPP_FILTER_DISABLED};
const sVideoAnalysisParam defaultVAParam = { VPP_FILTER_DISABLED};
const sVarianceReportParam defaultVarianceParam = { VPP_FILTER_DISABLED};
const sIDetectParam defaultIDetectParam = { VPP_FILTER_DISABLED};
const sFrameRateConversionParam defaultFRCParam = { MFX_FRCALGM_PRESERVE_TIMESTAMP, VPP_FILTER_DISABLED};
//MSDK 3.0
const sMultiViewParam       defaultMultiViewParam     = {1, VPP_FILTER_DISABLED}; 
//MSDK API 1.5
const sGamutMappingParam    defaultGamutParam         = { false,  VPP_FILTER_DISABLED};
const sTccParam defaultClrSaturationParam = { 160, 160, 160, 160, VPP_FILTER_DISABLED };
const sAceParam     defaultContrastParam      = { VPP_FILTER_DISABLED };
const sSteParam         defaultSkinParam          = { 4, VPP_FILTER_DISABLED };
const sIStabParam  defaultImgStabParam   = { MFX_IMAGESTAB_MODE_BOXING, VPP_FILTER_DISABLED };

static
void vppDefaultInitParams( sInputParams* pParams )
{
    pParams->frameInfo[VPP_IN] = defaultOwnFrameInfo;
    pParams->frameInfo[VPP_OUT] = defaultOwnFrameInfo;

    pParams->IOPattern    = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    pParams->ImpLib       = MFX_IMPL_AUTO;
    pParams->sptr         = NO_PTR;
    pParams->bDefAlloc   = false;
    pParams->asyncNum     = 1;
    pParams->bPerf       = false;
    pParams->isOutput     = false;
    pParams->ptsCheck     = false;
    pParams->ptsFR        = 0;
    pParams->vaType       = ALLOC_IMPL_VIA_SYS;

    // Optional video processing features
    pParams->denoiseParam = defaultDenoiseParam;
    pParams->detailParam  = defaultDetailParam;
    pParams->procampParam = defaultProcAmpParam;
      // analytics
    pParams->vaParam      = defaultVAParam;
    pParams->varianceParam= defaultVarianceParam; 
    pParams->idetectParam = defaultIDetectParam; 
    pParams->frcParam     = defaultFRCParam;
    // MSDK 3.0
    pParams->multiViewParam     = defaultMultiViewParam;
    // MSDK API 1.5
    pParams->gamutParam         = defaultGamutParam;
    pParams->tccParam = defaultClrSaturationParam;
    pParams->aceParam      = defaultContrastParam;
    pParams->steParam          = defaultSkinParam;
    pParams->istabParam       = defaultImgStabParam;

    //SVC
    memset( (void*)&(pParams->svcParam), 0, sizeof(sSVCParam));
    pParams->svcParam.mode = VPP_FILTER_DISABLED;

    // ROI check
    pParams->roiCheckParam.mode = ROI_FIX_TO_FIX; // ROI check is disabled
    pParams->roiCheckParam.srcSeed = 0;
    pParams->roiCheckParam.dstSeed = 0;
    pParams->isOutYV12 = false;
    pParams->need_crc  = false;

    // plug-in GUID
    pParams->need_plugin = false;

    // Use RunFrameVPPAsyncEx
    pParams->use_extapi  = false;

    return;

} // void vppDefaultInitParams( sInputParams* pParams )


static
void vppSafeRealInfo( sOwnFrameInfo* in, mfxFrameInfo* out)
{
    out->Width  = in->nWidth;
    out->Height = in->nHeight;
    out->CropX  = 0;
    out->CropY  = 0;
    out->CropW  = out->Width;
    out->CropH  = out->Height;
    out->FourCC = in->FourCC;

    return;

} // void vppSafeRealInfo( sOwnFrameInfo* in, mfxFrameInfo* out)


void SaveRealInfoForSvcOut(sSVCLayerDescr in[8], mfxFrameInfo out[8], mfxU32 fourcc)
{
    for(mfxU32 did = 0; did < 8; did++)
    {
        out[did].Width  = in[did].width;
        out[did].Height = in[did].height;

        //temp solution for crop
        out[did].CropX = 0;
        out[did].CropY = 0;
        out[did].CropW = out[did].Width;
        out[did].CropH = out[did].Height;

        // additional
        out[did].FourCC = fourcc;
    }

} // void SaveRealInfoForSvcOut(sSVCLayerDescr in[8], mfxFrameInfo out[8])


mfxStatus OutputProcessFrame(
    sAppResources Resources, 
    mfxFrameInfo* frameInfo, 
    mfxU32 &nFrames)
{ 
    mfxStatus sts;
    mfxFrameSurface1*   pProcessedSurface;
    mfxU32 counter = 0;
    bool bSvcMode = false;

    for(int did = 0; did < 8; did++)
    {
        if( Resources.svcConfig.DependencyLayer[did].Active )
        {
            bSvcMode = true;
            break;
        }
    }

    for(;!Resources.pSurfStore->m_SyncPoints.empty();Resources.pSurfStore->m_SyncPoints.pop_front())
    {
        sts = Resources.pProcessor->mfxSession.SyncOperation(
            Resources.pSurfStore->m_SyncPoints.front().first, 
            VPP_WAIT_INTERVAL);
        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        pProcessedSurface = Resources.pSurfStore->m_SyncPoints.front().second.pSurface;

        DecreaseReference(&pProcessedSurface->Data);

        sts = Resources.pDstFileWriter->PutNextFrame(
            Resources.pAllocator, 
            //(bSvcMode) ? &(pProcessedSurface->Info) : &(frameInfo[VPP_OUT]),
            (bSvcMode) ? &(Resources.realSvcOutFrameInfo[pProcessedSurface->Info.FrameId.DependencyId]) : &(frameInfo[VPP_OUT]),
            pProcessedSurface);
        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        nFrames++;

        //VPP progress
        if( VPP_FILTER_DISABLED == Resources.pParams->vaParam.mode && 
            VPP_FILTER_DISABLED == Resources.pParams->varianceParam.mode &&
            VPP_FILTER_DISABLED == Resources.pParams->idetectParam.mode)
        {
            if (!Resources.pParams->bPerf)
                vm_string_printf(VM_STRING("Frame number: %d\r"), nFrames);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));
            }
        } 
        else 
        {
            if (!Resources.pParams->bPerf)
            {
                if(VPP_FILTER_DISABLED != Resources.pParams->vaParam.mode)
                {
                    vm_string_printf(VM_STRING("Frame number: %hd, spatial: %hd, temporal: %hd, scd: %hd \n"), nFrames,
                        Resources.pExtVPPAuxData[counter].SpatialComplexity,
                        Resources.pExtVPPAuxData[counter].TemporalComplexity,
                        Resources.pExtVPPAuxData[counter].SceneChangeRate);
                }
                else if(VPP_FILTER_DISABLED != Resources.pParams->varianceParam.mode)
                {
                    mfxExtVppReport* pReport = reinterpret_cast<mfxExtVppReport*>(&Resources.pExtVPPAuxData[counter]);
                    vm_string_printf(VM_STRING("Frame number: %hd, variance:"), nFrames);
                    for(int vIdx = 0; vIdx < 11; vIdx++)
                    {
                        mfxU32 variance = pReport->Variances[vIdx];
                        vm_string_printf(VM_STRING("%i "), variance);
                    }

                    vm_string_printf(VM_STRING("\n"));
                }
                else if(VPP_FILTER_DISABLED != Resources.pParams->idetectParam.mode)
                {
                    vm_string_printf(
                        VM_STRING("Frame number: %hd PicStruct = %s\n"), 
                        nFrames, 
                        PicStruct2Str(Resources.pExtVPPAuxData[counter].PicStruct));
                }
                counter++;
            }
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));

            }
        }
    }
    return MFX_ERR_NONE;

} // mfxStatus OutputProcessFrame(


#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, vm_char *argv[])
#endif
{
    mfxStatus           sts = MFX_ERR_NONE;
    mfxU32              nFrames = 0;

    CRawVideoReader     yuvReader;
    GeneralWriter       yuvWriter;

    Timer               statTimer;

    sFrameProcessor     frameProcessor;
    sMemoryAllocator    allocator;

    sInputParams        Params;
    mfxVideoParam       mfxParamsVideo;

    // to prevent incorrect read/write of image in case of CropW/H != width/height 
    mfxFrameInfo        realFrameInfo[2];

    mfxFrameSurface1*   pSurf[3];// 0 - in, 1 - out, 2 - work

    sAppResources       Resources;

    mfxSyncPoint        syncPoint;

    bool                bFrameNumLimit = false;
    mfxU32              numGetFrames = 0;

    mfxExtVppReport    *extVPPAuxData = NULL;

    SurfaceVPPStore     surfStore;

    auto_ptr<PTSMaker> ptsMaker;

    /* generators for ROI testing */
    ROIGenerator       inROIGenerator;     
    ROIGenerator       outROIGenerator;
    bool               bROITest[2] = {false, false};
    unsigned int       work_surf   = VPP_OUT;


    //reset pointers to the all internal resources
    ZERO_MEMORY(Resources);
    //ZERO_MEMORY(mfxParamsVideo);
    ZERO_MEMORY(Params);
    ZERO_MEMORY(allocator);
    //ZERO_MEMORY(frameProcessor);
    ZERO_MEMORY(realFrameInfo[VPP_IN]  );
    ZERO_MEMORY(realFrameInfo[VPP_OUT] );

    Resources.pSrcFileReader    = &yuvReader;
    Resources.pDstFileWriter    = &yuvWriter;
    Resources.pProcessor        = &frameProcessor;
    Resources.pAllocator        = &allocator;
    Resources.pVppParams        = &mfxParamsVideo;
    Resources.pParams           = &Params;
    Resources.pSurfStore        = &surfStore;

    vppDefaultInitParams( &Params );

    //parse input string
    sts = vppParseInputString(argv, (mfxU8)argc, &Params);
    if (MFX_ERR_NONE != sts)
    {
        vppPrintHelp(argv[0], VM_STRING("Parameters parsing error"));
        return 1;
    }
    CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    if ( Params.use_extapi )
    {
        work_surf = VPP_WORK;
    }

    // to prevent incorrect read/write of image in case of CropW/H != width/height 
    // application save real image size
    vppSafeRealInfo( &(Params.frameInfo[VPP_IN]),  &realFrameInfo[VPP_IN]);
    vppSafeRealInfo( &(Params.frameInfo[VPP_OUT]), &realFrameInfo[VPP_OUT]);
    if( VPP_FILTER_DISABLED != Params.svcParam.mode )
    {
        SaveRealInfoForSvcOut(Params.svcParam.descr, Resources.realSvcOutFrameInfo, Params.frameInfo[VPP_OUT].FourCC);
        
    }

    // to check time stamp settings 
    if (Params.ptsFR)
    {
        Params.frameInfo[VPP_IN].dFrameRate = Params.ptsFR;
    }

    if (!CheckInputParams(argv, &Params))
    {
        return 1;
    }

    if (Params.ptsCheck)
    {
        ptsMaker.reset(new PTSMaker);
    }

    //prepare file reader (YUV/RGB file)  
    sts = yuvReader.Init(Params.strSrcFile, ptsMaker.get());
    CHECK_RESULT(sts, MFX_ERR_NONE, 1);     

    //prepare file writer (YUV file)  
    vm_char* istream = Params.isOutput ? Params.strDstFile : NULL;
    sts = yuvWriter.Init(
        istream, 
        ptsMaker.get(),
        (VPP_FILTER_DISABLED != Params.svcParam.mode) ? Params.svcParam.descr : NULL,
        Params.isOutYV12, 
        Params.need_crc);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));

#ifdef LIBVA_SUPPORT
    allocator.libvaKeeper.reset(CreateLibVA());
#endif

    //prepare mfxParams
    sts = InitParamsVPP(&mfxParamsVideo, &Params);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));

    // prepare pts Checker
    if (ptsMaker.get())
    {
        sts = ptsMaker.get()->Init(&mfxParamsVideo, Params.asyncNum - 1, Params.ptsAdvanced);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));
    }

    // prepare ROI generator
    if( ROI_VAR_TO_FIX == Params.roiCheckParam.mode || ROI_VAR_TO_VAR == Params.roiCheckParam.mode )
    {
        inROIGenerator.Init(realFrameInfo[VPP_IN].Width, realFrameInfo[VPP_IN].Height, Params.roiCheckParam.srcSeed);
        Params.roiCheckParam.srcSeed = inROIGenerator.GetSeed();
        bROITest[VPP_IN] = true;

    }
    if( ROI_FIX_TO_VAR == Params.roiCheckParam.mode || ROI_VAR_TO_VAR == Params.roiCheckParam.mode )
    {      
        outROIGenerator.Init(realFrameInfo[VPP_OUT].Width, realFrameInfo[VPP_OUT].Height, Params.roiCheckParam.dstSeed);
        Params.roiCheckParam.dstSeed = outROIGenerator.GetSeed();
        bROITest[VPP_OUT] = true;
    }

    sts = ConfigVideoEnhancementFilters(&Params, &Resources);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));

    Resources.pAllocator->bUsedAsExternalAllocator = !Params.bDefAlloc;

    // allocate needed number of aux vpp data    
    //-----------------------------------------------------
    extVPPAuxData = new mfxExtVppReport[Params.asyncNum * Params.multiViewParam.viewCount];
    Resources.pExtVPPAuxData = reinterpret_cast<mfxExtVppAuxData*>(extVPPAuxData);

    if( extVPPAuxData )
    {
        mfxU32  BufferId = MFX_EXTBUFF_VPP_SCENE_CHANGE;
        mfxU32  BufferSz = sizeof(mfxExtVppAuxData);

        if(Params.varianceParam.mode != VPP_FILTER_DISABLED)
        {
            BufferId = MFX_EXTBUFF_VPP_VARIANCE_REPORT;
            BufferSz = sizeof(mfxExtVppReport);
        }
        else if( Params.idetectParam.mode != VPP_FILTER_DISABLED )
        {
            BufferId = MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION;
            BufferSz = sizeof(mfxExtVppReport);//aya: fixme
        }

        for(int auxIdx = 0; auxIdx < Params.asyncNum * Params.multiViewParam.viewCount; auxIdx++)
        {
            extVPPAuxData[auxIdx].Header.BufferId = BufferId;
            extVPPAuxData[auxIdx].Header.BufferSz = BufferSz;
        }
    }
    //-----------------------------------------------------

    sts = InitResources(&Resources, &mfxParamsVideo, &Params);
    if(MFX_WRN_FILTER_SKIPPED == sts)
    {
        vm_string_printf(VM_STRING("\nVPP_WRN: some filter(s) skipped\n"));
        IGNORE_MFX_STS(sts, MFX_WRN_FILTER_SKIPPED);

    }
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));

    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        Params.bPartialAccel = true;
    }
    else
    {
        Params.bPartialAccel = false;
    }

    if (Params.bPerf)
    {
        sts = yuvReader.PreAllocateFrameChunk(&mfxParamsVideo, &Params, allocator.pMfxAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));
    }
    else if (Params.numFrames)
    {
        bFrameNumLimit = true;
    }

    // print parameters to console
    PrintInfo(&Params, &mfxParamsVideo, &Resources.pProcessor->mfxSession);
    PrintDllInfo();

    sts = MFX_ERR_NONE;
    nFrames = 0;

    vm_string_printf(VM_STRING("VPP started\n"));
    statTimer.Start(); 

    mfxU32 StartJumpFrame = 13;

    // need to correct jumping parameters due to async mode
    if (ptsMaker.get() && Params.ptsJump && (Params.asyncNum > 1))
    {
        if (Params.asyncNum > StartJumpFrame)
        {
            StartJumpFrame = Params.asyncNum;
        }
        else
        {
            StartJumpFrame = StartJumpFrame/Params.asyncNum*Params.asyncNum;
        }
    }


    bool bMultipleOut = false;

    bool bSvcMode          = (VPP_FILTER_ENABLED_CONFIGURED == Params.svcParam.mode) ? true : false;
    mfxU32 lastActiveLayer = 0;
    for(mfxI32 did = 7; did >= 0; did--)
    {
        if( Params.svcParam.descr[did].active )
        {
            lastActiveLayer = did;
            break;
        }
    }

    // pre-multi-view preparation
    bool bMultiView   = (VPP_FILTER_ENABLED_CONFIGURED == Params.multiViewParam.mode) ? true : false;
    vector<bool> bMultipleOutStore(Params.multiViewParam.viewCount, false);  
    mfxFrameSurface1* viewSurfaceStore[MULTI_VIEW_COUNT_MAX];

    ViewGenerator  viewGenerator( (bSvcMode) ? 8 : Params.multiViewParam.viewCount );

    if( bMultiView )
    {
        memset(viewSurfaceStore, 0, Params.multiViewParam.viewCount * sizeof( mfxFrameSurface1* )); 

        if( bFrameNumLimit )
        {
            Params.numFrames *= Params.multiViewParam.viewCount;
        }
    }
//---------------------------------------------------------
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bMultipleOut )
    {
        mfxU16 viewID = 0;
        mfxU16 viewIndx = 0;
        mfxU16 did = 0;

        if( bSvcMode )
        {
            do{
                did = viewID = viewGenerator.GetNextViewID();
            } while (0 == Params.svcParam.descr[did].active);
        }

        // update for multiple output of multi-view
        if( bMultiView )
        {
            viewID = viewGenerator.GetNextViewID();
            viewIndx = viewGenerator.GetViewIndx( viewID );

            if( bMultipleOutStore[viewIndx] )
            {              
                pSurf[VPP_IN] = viewSurfaceStore[viewIndx];
                bMultipleOut = true;           

                bMultipleOutStore[viewIndx] = false;
            }
            else
            {
                bMultipleOut = false;  
            }
        }

        if( !bMultipleOut )
        {
            sts = yuvReader.GetNextInputFrame(
                &allocator, 
                &(realFrameInfo[VPP_IN]), 
                &pSurf[VPP_IN]);
            BREAK_ON_ERROR(sts);

            if( bMultiView )
            {
                pSurf[VPP_IN]->Info.FrameId.ViewId = viewID;
            }

            if (numGetFrames++ == Params.numFrames && bFrameNumLimit)
            {
                sts = MFX_ERR_MORE_DATA;
                break;
            }
        }

        // VPP processing    
        bMultipleOut = false;

        if( !bSvcMode )
        {
            sts = GetFreeSurface(
                allocator.pSurfaces[VPP_OUT], 
                allocator.response[VPP_OUT].NumFrameActual , 
                &pSurf[work_surf]);
            BREAK_ON_ERROR(sts);
        }
        else
        {
            sts = GetFreeSurface(
                allocator.pSvcSurfaces[did], 
                allocator.svcResponse[did].NumFrameActual , 
                &pSurf[work_surf]);
            BREAK_ON_ERROR(sts);
        }

        if( bROITest[VPP_IN] )
        {
            inROIGenerator.SetROI(  &(pSurf[VPP_IN]->Info) );
        }
        if( bROITest[VPP_OUT] )
        {
            outROIGenerator.SetROI( &(pSurf[work_surf]->Info) );
        }

        mfxExtVppAuxData* pExtData = NULL;
        if( VPP_FILTER_DISABLED != Params.vaParam.mode || 
            VPP_FILTER_DISABLED != Params.varianceParam.mode || 
            VPP_FILTER_DISABLED != Params.idetectParam.mode)
        {
            pExtData = reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[surfStore.m_SyncPoints.size()]);
        }       

        if ( Params.use_extapi ) 
        {
            sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx( 
                pSurf[VPP_IN], 
                pSurf[VPP_WORK],
                //pExtData, 
                &pSurf[VPP_OUT],
                &syncPoint);

            while(MFX_WRN_DEVICE_BUSY == sts)
            {
                vm_time_sleep(500);
                sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx( 
                    pSurf[VPP_IN], 
                    pSurf[VPP_WORK],
                    //pExtData, 
                    &pSurf[VPP_OUT],
                    &syncPoint);
            }
        }
        else 
        {
            sts = frameProcessor.pmfxVPP->RunFrameVPPAsync( 
                pSurf[VPP_IN], 
                pSurf[VPP_OUT],
                pExtData, 
                &syncPoint);
        }
        
        if (MFX_ERR_MORE_DATA == sts)
        {
            if(bSvcMode)
            {
                if( lastActiveLayer != did )
                {
                    bMultipleOut = true;
                }
            }
            continue;
        }

        //MFX_ERR_MORE_SURFACE means output is ready but need more surface
        //because VPP produce multiple out. example: Frame Rate Conversion 30->60
        if (MFX_ERR_MORE_SURFACE == sts)
        {       
            sts = MFX_ERR_NONE;

            if( bMultiView )
            {                      
                if( viewSurfaceStore[viewIndx] )
                {
                    DecreaseReference( &(viewSurfaceStore[viewIndx]->Data) );
                }

                viewSurfaceStore[viewIndx] = pSurf[VPP_IN];
                IncreaseReference( &(viewSurfaceStore[viewIndx]->Data) );            

                bMultipleOutStore[viewIndx] = true;
            }
            else
            {
                bMultipleOut = true;
            }
            
            if ( Params.use_extapi )
            {
                // RunFrameAsyncEx is used
                continue;
            }
        }
        else if (MFX_ERR_NONE == sts && !((nFrames+1)% StartJumpFrame) && ptsMaker.get() && Params.ptsJump) // pts jump 
        {
            ptsMaker.get()->JumpPTS();
        }
        else if ( MFX_ERR_NONE == sts && bMultiView )
        {       
            if( viewSurfaceStore[viewIndx] )
            {
                DecreaseReference( &(viewSurfaceStore[viewIndx]->Data) );
                viewSurfaceStore[viewIndx] = NULL;
            }
        }
        else if(bSvcMode)
        {
            if( lastActiveLayer != did )
            {
                bMultipleOut = true;
            }
        }

        BREAK_ON_ERROR(sts);
        surfStore.m_SyncPoints.push_back(SurfaceVPPStore::SyncPair(syncPoint,pSurf[VPP_OUT]));
        IncreaseReference(&pSurf[VPP_OUT]->Data);
        if (surfStore.m_SyncPoints.size() != (size_t)(Params.asyncNum * Params.multiViewParam.viewCount))
        {
            continue;
        }

        sts = OutputProcessFrame(Resources, realFrameInfo, nFrames);
        BREAK_ON_ERROR(sts);

    } // main while loop
//---------------------------------------------------------



    //process remain sync points 
    if (MFX_ERR_MORE_DATA == sts)
    {
        sts = OutputProcessFrame(Resources, realFrameInfo, nFrames);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));  
    }

    // means that file has ended, need to go to buffering loop
    IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));


    // loop to get buffered frames from VPP
    while (MFX_ERR_NONE <= sts)
    {
        mfxU32 did;

        if( bSvcMode )
        {
            do{
                did = viewGenerator.GetNextViewID();
            } while (0 == Params.svcParam.descr[did].active);
        }

        /*sts = GetFreeSurface(
            Allocator.pSurfaces[VPP_OUT], 
            Allocator.response[VPP_OUT].NumFrameActual, 
            &pSurf[VPP_OUT]);
        BREAK_ON_ERROR(sts);*/
        if( !bSvcMode )
        {
            sts = GetFreeSurface(
                allocator.pSurfaces[VPP_OUT], 
                allocator.response[VPP_OUT].NumFrameActual , 
                &pSurf[work_surf]);
            BREAK_ON_ERROR(sts);
        }
        else
        {
            sts = GetFreeSurface(
                allocator.pSvcSurfaces[did], 
                allocator.svcResponse[did].NumFrameActual , 
                &pSurf[work_surf]);
            BREAK_ON_ERROR(sts);
        }

        bMultipleOut = false;

        if ( Params.use_extapi )
        {
            sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx( 
                NULL, 
                pSurf[VPP_WORK],
                //(VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                &pSurf[VPP_OUT],
                &syncPoint );
            while(MFX_WRN_DEVICE_BUSY == sts)
            {
                vm_time_sleep(500);
                sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx( 
                    NULL, 
                    pSurf[VPP_WORK],
                    //(VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                    &pSurf[VPP_OUT],
                    &syncPoint );
            }
        }
        else 
        {
            sts = frameProcessor.pmfxVPP->RunFrameVPPAsync( 
                NULL, 
                pSurf[VPP_OUT],
                (VPP_FILTER_DISABLED != Params.vaParam.mode || VPP_FILTER_DISABLED != Params.varianceParam.mode)? reinterpret_cast<mfxExtVppAuxData*>(&extVPPAuxData[0]) : NULL, 
                &syncPoint );
        }
        IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
        BREAK_ON_ERROR(sts);

        sts = Resources.pProcessor->mfxSession.SyncOperation(
            syncPoint, 
            VPP_WAIT_INTERVAL);
        BREAK_ON_ERROR(sts);

        sts = Resources.pDstFileWriter->PutNextFrame(
            Resources.pAllocator, 
            (bSvcMode) ?  &(Resources.realSvcOutFrameInfo[pSurf[VPP_OUT]->Info.FrameId.DependencyId]) : &(realFrameInfo[VPP_OUT]), 
            pSurf[VPP_OUT]);
        CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        if(bSvcMode)
        {
            if( lastActiveLayer != did )
            {
                bMultipleOut = true;
            }
        }

        nFrames++;

        //VPP progress
        if( VPP_FILTER_DISABLED == Params.vaParam.mode )
        {
            if (!Params.bPerf)
                vm_string_printf(VM_STRING("Frame number: %d\r"), nFrames);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));
            }
        } 
        else 
        {
            if (!Params.bPerf)
                vm_string_printf(VM_STRING("Frame number: %d, spatial: %hd, temporal: %d, scd: %d \n"), nFrames,
                extVPPAuxData[0].SpatialComplexity,
                extVPPAuxData[0].TemporalComplexity,
                extVPPAuxData[0].SceneChangeRate);
            else
            {
                if (!(nFrames % 100))
                    vm_string_printf(VM_STRING("."));

            }
        }
    }
    statTimer.Stop();

    IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

    // report any errors that occurred 
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, WipeResources(&Resources));

    vm_string_printf(VM_STRING("\nVPP finished\n"));
    vm_string_printf(VM_STRING("\n"));

    vm_string_printf(VM_STRING("Total frames %d \n"), nFrames);
    vm_string_printf(VM_STRING("Total time %.2f sec \n"), statTimer.OverallTiming());
    vm_string_printf(VM_STRING("Frames per second %.3f fps \n"), nFrames / statTimer.OverallTiming());

    PutPerformanceToFile(Params, nFrames / statTimer.OverallTiming());

    if ( Params.need_crc ) {
        mfxU32 crc = yuvWriter.GetCRC(pSurf[VPP_OUT]);
        vm_file *fcrc;
        fcrc = vm_file_fopen(Params.strCRCFile, VM_STRING("wb"));
        if (!fcrc)
            return 1;

        vm_file_fprintf(fcrc, VM_STRING("0x%X"), crc);
        vm_file_fclose(fcrc);
        printf("CRC: 0x%X\n", crc);
    }

    WipeResources(&Resources);

    SAFE_DELETE_ARRAY(extVPPAuxData);

    return 0; /* OK */

} // int _tmain(int argc, vm_char *argv[])
/* EOF */
