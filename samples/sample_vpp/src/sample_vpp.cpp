//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//

#include <vector>

#include "sample_vpp_utils.h"
#include "sample_vpp_pts.h"
#include "sample_vpp_roi.h"
#include "vm/atomic_defs.h"

#define TIME_STATS 1
#include "time_statistics.h"

#define MFX_CHECK(sts) {if (sts != MFX_ERR_NONE) return sts;}

using namespace std;

void IncreaseReference(mfxFrameData *ptr)
{
    msdk_atomic_inc16((volatile mfxU16*)&ptr->Locked);
}

void DecreaseReference(mfxFrameData *ptr)
{
    msdk_atomic_dec16((volatile mfxU16*)&ptr->Locked);
}

void PutPerformanceToFile(sInputParams& Params, mfxF64 FPS)
{
    FILE *fPRF;
    MSDK_FOPEN(fPRF,Params.strPerfFile, MSDK_STRING("ab"));
    if (!fPRF)
        return;

    msdk_char *iopattern = const_cast<msdk_char*>(IOpattern2Str(Params.IOPattern));
    msdk_char   iopattern_ascii[32] = {0};
    msdk_char  *pIOP = iopattern_ascii;
    while (*iopattern)
        *pIOP++ = (msdk_char)*iopattern++;
    msdk_char *srcFileName = Params.strSrcFile;
    msdk_char   srcFileName_ascii[1024] = {0};
    msdk_char  *pFileName = srcFileName_ascii;
    while (*srcFileName)
        *pFileName++ = (msdk_char)*srcFileName++;


    msdk_tstring filters;

    if (Params.frameInfoIn[0].PicStruct != Params.frameInfoOut[0].PicStruct)
    {
        filters+=MSDK_STRING("DI ");
    }
    if (VPP_FILTER_DISABLED != Params.denoiseParam[0].mode)
    {
        filters+=MSDK_STRING("DN ");
    }
    if (filters.empty())
    {
        filters=MSDK_STRING("NoFilters ");
    }


    msdk_fprintf(fPRF, MSDK_STRING("%s, %dx%d, %dx%d, %s, %s, %f\r\n"), srcFileName_ascii,
        Params.frameInfoIn[0].nWidth,
        Params.frameInfoIn[0].nHeight,
        Params.frameInfoOut[0].nWidth,
        Params.frameInfoOut[0].nHeight,
        iopattern_ascii,
        filters.c_str(),
        FPS);
    fclose(fPRF);

} // void PutPerformanceToFile(sInputVppParams& Params, mfxF64 FPS)

static
void vppDefaultInitParams( sInputParams* pParams, sFiltersParam* pDefaultFiltersParam )
{
    pParams->frameInfoIn.clear();  pParams->frameInfoIn.push_back ( *pDefaultFiltersParam->pOwnFrameInfo );
    pParams->frameInfoOut.clear(); pParams->frameInfoOut.push_back( *pDefaultFiltersParam->pOwnFrameInfo );

    pParams->IOPattern    = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    pParams->ImpLib       = MFX_IMPL_AUTO;
    pParams->sptr         = NO_PTR;
    pParams->bDefAlloc   = false;
    pParams->asyncNum     = 1;
    pParams->bPerf       = false;
    pParams->isOutput     = false;
    pParams->ptsCheck     = false;
    pParams->ptsAdvanced  = false;
    pParams->ptsFR        = 0;
    pParams->vaType       = ALLOC_IMPL_VIA_SYS;
    pParams->rotate.clear(); pParams->rotate.push_back(0);
    pParams->bScaling     = false;
    pParams->scalingMode  = MFX_SCALING_MODE_DEFAULT;
    pParams->numFrames    = 0;

    // Optional video processing features
    pParams->mirroringParam.clear();        pParams->mirroringParam.push_back(      *pDefaultFiltersParam->pMirroringParam      );
    pParams->videoSignalInfoParam.clear();  pParams->videoSignalInfoParam.push_back(*pDefaultFiltersParam->pVideoSignalInfo     );
    pParams->deinterlaceParam.clear();      pParams->deinterlaceParam.push_back(    *pDefaultFiltersParam->pDIParam             );
    pParams->denoiseParam.clear();          pParams->denoiseParam.push_back(        *pDefaultFiltersParam->pDenoiseParam        );
    pParams->detailParam.clear();           pParams->detailParam.push_back(         *pDefaultFiltersParam->pDetailParam         );
    pParams->procampParam.clear();          pParams->procampParam.push_back(        *pDefaultFiltersParam->pProcAmpParam        );
      // analytics
    pParams->frcParam.clear();              pParams->frcParam.push_back(            *pDefaultFiltersParam->pFRCParam            );
    // MSDK 3.0
    pParams->multiViewParam.clear();        pParams->multiViewParam.push_back(      *pDefaultFiltersParam->pMultiViewParam      );
    // MSDK API 1.5
    pParams->gamutParam.clear();            pParams->gamutParam.push_back(          *pDefaultFiltersParam->pGamutParam          );
    pParams->tccParam.clear();              pParams->tccParam.push_back(            *pDefaultFiltersParam->pClrSaturationParam  );
    pParams->aceParam.clear();              pParams->aceParam.push_back(            *pDefaultFiltersParam->pContrastParam       );
    pParams->steParam.clear();              pParams->steParam.push_back(            *pDefaultFiltersParam->pSkinParam           );
    pParams->istabParam.clear();            pParams->istabParam.push_back(          *pDefaultFiltersParam->pImgStabParam        );

    // ROI check
    pParams->roiCheckParam.mode = ROI_FIX_TO_FIX; // ROI check is disabled
    pParams->roiCheckParam.srcSeed = 0;
    pParams->roiCheckParam.dstSeed = 0;
    pParams->isOutYV12 = false;

    // plug-in GUID
    pParams->need_plugin = false;

    // Use RunFrameVPPAsyncEx
    pParams->use_extapi  = false;

    // Do not call MFXVideoVPP_Reset
    pParams->resetFrmNums.clear();

    pParams->bInitEx      = false;
    pParams->GPUCopyValue = MFX_GPUCOPY_DEFAULT;

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
    mfxU32 &nFrames,
    mfxU32 paramID)
{
    mfxStatus sts;
    mfxFrameSurface1*   pProcessedSurface;

    for(;!Resources.pSurfStore->m_SyncPoints.empty();Resources.pSurfStore->m_SyncPoints.pop_front())
    {
        sts = Resources.pProcessor->mfxSession.SyncOperation(
            Resources.pSurfStore->m_SyncPoints.front().first,
            MSDK_VPP_WAIT_INTERVAL);
        if(sts)
            msdk_printf(MSDK_STRING("SyncOperation wait interval exceeded\n"));
        MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        pProcessedSurface = Resources.pSurfStore->m_SyncPoints.front().second.pSurface;

        DecreaseReference(&pProcessedSurface->Data);

        GeneralWriter* writer = (1 == Resources.dstFileWritersN) ? &Resources.pDstFileWriters[0] : &Resources.pDstFileWriters[paramID];
        sts = writer->PutNextFrame(
            Resources.pAllocator,
            &(frameInfo[VPP_OUT]),
            pProcessedSurface);
        if(sts)
            msdk_printf(MSDK_STRING("Failed to write frame to disk\n"));
        MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        nFrames++;

        //VPP progress
        if (!Resources.pParams->bPerf)
        {
            msdk_printf(MSDK_STRING("Frame number: %d\r"), nFrames);
        }
        else
        {
            if (!(nFrames % 100))
                msdk_printf(MSDK_STRING("."));
        }

    }
    return MFX_ERR_NONE;

} // mfxStatus OutputProcessFrame(


#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, msdk_char *argv[])
#endif
{
    mfxStatus           sts = MFX_ERR_NONE;
    mfxU32              nFrames = 0;

    CRawVideoReader     yuvReader;

    CTimeStatistics     statTimer;

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

    SurfaceVPPStore     surfStore;

    auto_ptr<PTSMaker> ptsMaker;

    /* generators for ROI testing */
    ROIGenerator       inROIGenerator;
    ROIGenerator       outROIGenerator;
    bool               bROITest[2] = {false, false};
    unsigned int       work_surf   = VPP_OUT;

    /* default parameters */
    sOwnFrameInfo             defaultOwnFrameInfo         = { 0, 352, 288, 0, 0, 352, 288, MFX_FOURCC_NV12, MFX_PICSTRUCT_PROGRESSIVE, 30.0 };
    sDIParam                  defaultDIParam              = { 0, 0, 0, VPP_FILTER_DISABLED };
    sProcAmpParam             defaultProcAmpParam         = { 0.0, 1.0, 1.0, 0.0, VPP_FILTER_DISABLED };
    sDetailParam              defaultDetailParam          = { 1,  VPP_FILTER_DISABLED };
    sDenoiseParam             defaultDenoiseParam         = { 1,  VPP_FILTER_DISABLED };
    sVideoAnalysisParam       defaultVAParam              = { VPP_FILTER_DISABLED };
    sIDetectParam             defaultIDetectParam         = { VPP_FILTER_DISABLED };
    sFrameRateConversionParam defaultFRCParam             = { MFX_FRCALGM_PRESERVE_TIMESTAMP, VPP_FILTER_DISABLED };
    //MSDK 3.0
    sMultiViewParam           defaultMultiViewParam       = { 1, VPP_FILTER_DISABLED };
    //MSDK API 1.5
    sGamutMappingParam        defaultGamutParam           = { false,  VPP_FILTER_DISABLED };
    sTccParam                 defaultClrSaturationParam   = { 160, 160, 160, 160, VPP_FILTER_DISABLED };
    sAceParam                 defaultContrastParam        = { VPP_FILTER_DISABLED };
    sSteParam                 defaultSkinParam            = { 4, VPP_FILTER_DISABLED };
    sIStabParam               defaultImgStabParam         = { MFX_IMAGESTAB_MODE_BOXING, VPP_FILTER_DISABLED };
    sSVCParam                 defaultSVCParam             = { 0, 0, 0, 0, 0, 0, 0, 0, VPP_FILTER_DISABLED };
    sVideoSignalInfoParam     defaultVideoSignalInfoParam;
    sMirroringParam           defaultMirroringParam;

    sFiltersParam             defaultFiltersParam =
            { &defaultOwnFrameInfo,
              &defaultDIParam,
              &defaultProcAmpParam,
              &defaultDetailParam,
              &defaultDenoiseParam,
              &defaultVAParam,
              &defaultIDetectParam,
              &defaultFRCParam,
              &defaultMultiViewParam,
              &defaultGamutParam,
              &defaultClrSaturationParam,
              &defaultContrastParam,
              &defaultSkinParam,
              &defaultImgStabParam,
              &defaultSVCParam,
              &defaultVideoSignalInfoParam,
              &defaultMirroringParam};

    //reset pointers to the all internal resources
    MSDK_ZERO_MEMORY(Resources);
    //MSDK_ZERO_MEMORY(mfxParamsVideo);
    //MSDK_ZERO_MEMORY(Params);
    MSDK_ZERO_MEMORY(allocator);
    //MSDK_ZERO_MEMORY(frameProcessor);
    MSDK_ZERO_MEMORY(realFrameInfo[VPP_IN]  );
    MSDK_ZERO_MEMORY(realFrameInfo[VPP_OUT] );

    Resources.pSrcFileReader    = &yuvReader;
    Resources.pProcessor        = &frameProcessor;
    Resources.pAllocator        = &allocator;
    Resources.pVppParams        = &mfxParamsVideo;
    Resources.pParams           = &Params;
    Resources.pSurfStore        = &surfStore;

    vppDefaultInitParams( &Params, &defaultFiltersParam );

    //parse input string
    sts = vppParseInputString(argv, (mfxU8)argc, &Params, &defaultFiltersParam);
    if (MFX_ERR_NONE != sts)
    {
        vppPrintHelp(argv[0], MSDK_STRING("Parameters parsing error"));
        return 1;
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    if ( Params.use_extapi )
    {
        work_surf = VPP_WORK;
    }

    // to prevent incorrect read/write of image in case of CropW/H != width/height
    // application save real image size
    vppSafeRealInfo( &(Params.frameInfoIn[0]),  &realFrameInfo[VPP_IN]);
    vppSafeRealInfo( &(Params.frameInfoOut[0]), &realFrameInfo[VPP_OUT]);

    // to check time stamp settings
    if (Params.ptsFR)
    {
        Params.frameInfoIn[0].dFrameRate = Params.ptsFR;
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
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    //prepare file writers (YUV file)
    Resources.dstFileWritersN = (mfxU32)Params.strDstFiles.size();
    Resources.pDstFileWriters = new GeneralWriter[Resources.dstFileWritersN];
    const msdk_char* istream;
    for (mfxU32 i = 0; i < Resources.dstFileWritersN; i++)
    {
        istream = Params.isOutput ? Params.strDstFiles[i].c_str() : NULL;
        sts = Resources.pDstFileWriters[i].Init(
            istream,
            ptsMaker.get(),
            NULL,
            Params.isOutYV12);
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to init YUV writer\n")); WipeResources(&Resources); WipeParams(&Params);});
    }

#ifdef LIBVA_SUPPORT
    if(!(Params.ImpLib & MFX_IMPL_SOFTWARE))
        allocator.libvaKeeper.reset(CreateLibVA());
#endif

    //prepare mfxParams
    sts = InitParamsVPP(&mfxParamsVideo, &Params, 0);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to prepare mfxParams\n")); WipeResources(&Resources); WipeParams(&Params);});

    // prepare pts Checker
    if (ptsMaker.get())
    {
        sts = ptsMaker.get()->Init(&mfxParamsVideo, Params.asyncNum - 1, Params.ptsAdvanced);
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to prepare pts Checker\n")); WipeResources(&Resources); WipeParams(&Params);});
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

    sts = ConfigVideoEnhancementFilters(&Params, &Resources, 0);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to ConfigVideoEnhancementFilters\n")); WipeResources(&Resources); WipeParams(&Params);});

    Resources.pAllocator->bUsedAsExternalAllocator = !Params.bDefAlloc;

    sts = InitResources(&Resources, &mfxParamsVideo, &Params);
    if(MFX_WRN_FILTER_SKIPPED == sts)
    {
        msdk_printf(MSDK_STRING("\nVPP_WRN: some filter(s) skipped\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_FILTER_SKIPPED);

    }
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to InitResources\n")); WipeResources(&Resources); WipeParams(&Params);});

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
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to yuvReader.PreAllocateFrameChunk\n")); WipeResources(&Resources); WipeParams(&Params);});
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

    msdk_printf(MSDK_STRING("VPP started\n"));
    statTimer.StartTimeMeasurement();

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


    bool bDoNotUpdateIn = false;
    if(Params.use_extapi)
        bDoNotUpdateIn = true;

    // pre-multi-view preparation
    bool bMultiView   = (VPP_FILTER_ENABLED_CONFIGURED == Params.multiViewParam[0].mode) ? true : false;
    vector<bool> bMultipleOutStore(Params.multiViewParam[0].viewCount, false);
    mfxFrameSurface1* viewSurfaceStore[MULTI_VIEW_COUNT_MAX];

    ViewGenerator  viewGenerator( Params.multiViewParam[0].viewCount );

    if( bMultiView )
    {
        memset(viewSurfaceStore, 0, Params.multiViewParam[0].viewCount * sizeof( mfxFrameSurface1* ));

        if( bFrameNumLimit )
        {
            Params.numFrames *= Params.multiViewParam[0].viewCount;
        }
    }

    bool bNeedReset = false;
    mfxU16 paramID = 0;
    mfxU32 nextResetFrmNum = (Params.resetFrmNums.size() > 0) ? Params.resetFrmNums[0] : NOT_INIT_VALUE;

//---------------------------------------------------------
    do
    {
        if (bNeedReset)
        {
            paramID++;
            bNeedReset = false;
            nextResetFrmNum = (Params.resetFrmNums.size() > paramID) ? Params.resetFrmNums[paramID] : NOT_INIT_VALUE;

            //prepare mfxParams
            sts = InitParamsVPP(&mfxParamsVideo, &Params, paramID);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to InitParamsVPP for VPP_Reset\n")); WipeResources(&Resources); WipeParams(&Params);});

            sts = ConfigVideoEnhancementFilters(&Params, &Resources, paramID);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to ConfigVideoEnhancementFilters for VPP_Reset\n")); WipeResources(&Resources); WipeParams(&Params);});

            sts = Resources.pProcessor->pmfxVPP->Reset(Resources.pVppParams);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to Reset VPP component\n")); WipeResources(&Resources); WipeParams(&Params);});

            vppSafeRealInfo( &(Params.frameInfoIn[paramID]),  &realFrameInfo[VPP_IN]);
            vppSafeRealInfo( &(Params.frameInfoOut[paramID]), &realFrameInfo[VPP_OUT]);
            UpdateSurfacePool(mfxParamsVideo.vpp.Out, allocator.response[VPP_OUT].NumFrameActual, allocator.pSurfaces[VPP_OUT]);
            UpdateSurfacePool(mfxParamsVideo.vpp.In,  allocator.response[VPP_IN].NumFrameActual,  allocator.pSurfaces[VPP_IN]);
            msdk_printf(MSDK_STRING("VPP reseted at frame number %d\n"), numGetFrames);
        }

        while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bDoNotUpdateIn )
        {
            mfxU16 viewID = 0;
            mfxU16 viewIndx = 0;

            // update for multiple output of multi-view
            if( bMultiView )
            {
                viewID = viewGenerator.GetNextViewID();
                viewIndx = viewGenerator.GetViewIndx( viewID );

                if( bMultipleOutStore[viewIndx] )
                {
                    pSurf[VPP_IN] = viewSurfaceStore[viewIndx];
                    bDoNotUpdateIn = true;

                    bMultipleOutStore[viewIndx] = false;
                }
                else
                {
                    bDoNotUpdateIn = false;
                }
            }

            if( !bDoNotUpdateIn )
            {
                if (nextResetFrmNum == numGetFrames)
                {
                    bNeedReset = true;
                    sts = MFX_ERR_MORE_DATA;
                    break;
                }

                sts = yuvReader.GetNextInputFrame(
                    &allocator,
                    &(realFrameInfo[VPP_IN]),
                    &pSurf[VPP_IN]);
                MSDK_BREAK_ON_ERROR(sts);

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
            bDoNotUpdateIn = false;

            sts = GetFreeSurface(
                allocator.pSurfaces[VPP_OUT],
                allocator.response[VPP_OUT].NumFrameActual ,
                &pSurf[work_surf]);
            MSDK_BREAK_ON_ERROR(sts);

            if( bROITest[VPP_IN] )
            {
                inROIGenerator.SetROI(  &(pSurf[VPP_IN]->Info) );
            }
            if( bROITest[VPP_OUT] )
            {
                outROIGenerator.SetROI( &(pSurf[work_surf]->Info) );
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
                    MSDK_SLEEP(500);
                    sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx(
                        pSurf[VPP_IN],
                        pSurf[VPP_WORK],
                        //pExtData,
                        &pSurf[VPP_OUT],
                        &syncPoint);
                }

                if(MFX_ERR_MORE_DATA != sts)
                    bDoNotUpdateIn = true;
            }
            else
            {
                sts = frameProcessor.pmfxVPP->RunFrameVPPAsync(
                    pSurf[VPP_IN],
                    pSurf[VPP_OUT],
                    NULL,
                    &syncPoint);
            }

            if (MFX_ERR_MORE_DATA == sts)
            {
                continue;
            }

            //MFX_ERR_MORE_SURFACE (&& !use_extapi) means output is ready but need more surface
            //because VPP produce multiple out. example: Frame Rate Conversion 30->60
            //MFX_ERR_MORE_SURFACE && use_extapi means that component need more work surfaces
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
                    bDoNotUpdateIn = true;
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

            MSDK_BREAK_ON_ERROR(sts);
            surfStore.m_SyncPoints.push_back(SurfaceVPPStore::SyncPair(syncPoint,pSurf[VPP_OUT]));
            IncreaseReference(&pSurf[VPP_OUT]->Data);
            if (surfStore.m_SyncPoints.size() != (size_t)(Params.asyncNum * Params.multiViewParam[paramID].viewCount))
            {
                continue;
            }

            sts = OutputProcessFrame(Resources, realFrameInfo, nFrames, paramID);
            MSDK_BREAK_ON_ERROR(sts);

        } // main while loop
    //---------------------------------------------------------

        //process remain sync points
        if (MFX_ERR_MORE_DATA == sts)
        {
            sts = OutputProcessFrame(Resources, realFrameInfo, nFrames, paramID);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Failed to process remain sync points\n")); WipeResources(&Resources); WipeParams(&Params);});
        }

        // means that file has ended, need to go to buffering loop
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        // exit in case of other errors
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Exit in case of other errors\n")); WipeResources(&Resources); WipeParams(&Params);});


        // loop to get buffered frames from VPP
        while (MFX_ERR_NONE <= sts)
        {
            sts = GetFreeSurface(
                allocator.pSurfaces[VPP_OUT],
                allocator.response[VPP_OUT].NumFrameActual ,
                &pSurf[work_surf]);
            MSDK_BREAK_ON_ERROR(sts);

            bDoNotUpdateIn = false;

            if ( Params.use_extapi )
            {
                sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx(
                    NULL,
                    pSurf[VPP_WORK],
                    &pSurf[VPP_OUT],
                    &syncPoint );
                while(MFX_WRN_DEVICE_BUSY == sts)
                {
                    MSDK_SLEEP(500);
                    sts = frameProcessor.pmfxVPP->RunFrameVPPAsyncEx(
                        NULL,
                        pSurf[VPP_WORK],
                        &pSurf[VPP_OUT],
                        &syncPoint );
                }
            }
            else
            {
                sts = frameProcessor.pmfxVPP->RunFrameVPPAsync(
                    NULL,
                    pSurf[VPP_OUT],
                    NULL,
                    &syncPoint );
            }
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
            MSDK_BREAK_ON_ERROR(sts);

            sts = Resources.pProcessor->mfxSession.SyncOperation(
                syncPoint,
                MSDK_VPP_WAIT_INTERVAL);
            if(sts)
                msdk_printf(MSDK_STRING("SyncOperation wait interval exceeded\n"));
            MSDK_BREAK_ON_ERROR(sts);

            GeneralWriter* writer = (1 == Resources.dstFileWritersN) ? &Resources.pDstFileWriters[0] : &Resources.pDstFileWriters[paramID];
            sts = writer->PutNextFrame(
                Resources.pAllocator,
                &(realFrameInfo[VPP_OUT]),
                pSurf[VPP_OUT]);
            if(sts)
                msdk_printf(MSDK_STRING("Failed to write frame to disk\n"));
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

        nFrames++;

            //VPP progress
            if (!Params.bPerf)
                msdk_printf(MSDK_STRING("Frame number: %d\r"), nFrames);
            else
            {
                if (!(nFrames % 100))
                    msdk_printf(MSDK_STRING("."));
            }
        }
    } while (bNeedReset);

    statTimer.StopTimeMeasurement();

    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

    // report any errors that occurred
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { msdk_printf(MSDK_STRING("Unexpected error during processing\n")); WipeResources(&Resources); WipeParams(&Params);});

    msdk_printf(MSDK_STRING("\nVPP finished\n"));
    msdk_printf(MSDK_STRING("\n"));

    msdk_printf(MSDK_STRING("Total frames %d \n"), nFrames);
    msdk_printf(MSDK_STRING("Total time %.2f sec \n"), statTimer.GetTotalTime());
    msdk_printf(MSDK_STRING("Frames per second %.3f fps \n"), nFrames / statTimer.GetTotalTime());

    PutPerformanceToFile(Params, nFrames / statTimer.GetTotalTime());

    WipeResources(&Resources);
    WipeParams(&Params);

    return 0; /* OK */

} // int _tmain(int argc, msdk_char *argv[])
/* EOF */
