//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//

#include <vector>

#include "test_vpp_utils.h"
#include "test_statistics.h"
#include "test_vpp_pts.h"
#include "test_vpp_roi.h"
#include "vm_interlocked.h"

#define MFX_CHECK(sts) {if (sts != MFX_ERR_NONE) return sts;}

using namespace std;



void PutPerformanceToFile(sInputParams& Params, mfxF64 FPS)
{
    vm_file *fPRF;
    fPRF = vm_file_fopen(Params.strPerfFile, VM_STRING("ab"));
    if (!fPRF)
        return;

    vm_char *iopattern = const_cast<vm_char*>(IOpattern2Str(Params.IOPattern));
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


//sMemoryAllocator    allocator;


#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, vm_char *argv[])
#endif
{
    mfxStatus                    sts = MFX_ERR_NONE;
    mfxU32                       nFrames;
    Timer                        statTimer; 
    std::vector<sInputParams *>  Params;
    std::vector<sAppResources *> Resources;
    
    //sFrameProcessor             *frameProcessor;
    sMemoryAllocator            *allocator;


    CRawVideoReader             yuvReader;
    
    auto_ptr<PTSMaker>          ptsMaker;

    allocator   = new sMemoryAllocator;

    //parse input string
    sts = vppParseInputString(argv, (mfxU8)argc, Params);
    if (MFX_ERR_NONE != sts)
    {
        vppPrintHelp(argv[0], VM_STRING("Parameters parsing error"));
        return 1;
    }
    CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    for ( size_t i = 0 ; i < Params.size(); i++)
    {
        sAppResources *element = new sAppResources;

        if ( i == 0 )
        {
            // First elment will be reading from file. Others will use output from previous stages as input.
            element->m_pSrcFileReader    = &yuvReader;
        }
        
        if ( Params[i]->isOutput )
        {
            GeneralWriter *yuvWriter = new GeneralWriter;
            element->m_pDstFileWriter    = yuvWriter;
        }

        element->pProcessor        = &(element->frameProcessor);
        element->pAllocator        = allocator;
        element->pVppParams        = &element->m_mfxParamsVideo;
        element->m_pParams           = Params[i];
        element->pSurfStore        = &element->surfStore;
        element->indexInPipe       = i * 2;
        if ( Params[i]->use_extapi )
        {
            element->work_surf = VPP_WORK;
        }
        // to prevent incorrect read/write of image in case of CropW/H != width/height 
        // application save real image size
        vppSafeRealInfo( &(Params[i]->frameInfo[VPP_IN]),  &element->realFrameInfo[VPP_IN]);
        vppSafeRealInfo( &(Params[i]->frameInfo[VPP_OUT]), &element->realFrameInfo[VPP_OUT]);

        if( VPP_FILTER_DISABLED != Params[i]->svcParam.mode )
        {
            SaveRealInfoForSvcOut(Params[i]->svcParam.descr, Resources[i]->realSvcOutFrameInfo, Params[i]->frameInfo[VPP_OUT].FourCC);
        }

        if (!CheckInputParams(argv, Params[i]))
        {
            return 1;
        }

        if (Params[i]->ptsCheck && ! ptsMaker.get() )
        {
            ptsMaker.reset(new PTSMaker);
        }

        // to check time stamp settings 
        if (Params[i]->ptsFR)
        {
            Params[i]->frameInfo[VPP_IN].dFrameRate = Params[i]->ptsFR;
        }

        Resources.push_back(element);


        //prepare file writer (YUV file)
        if ( Params[i]->isOutput )
        {
        vm_char* istream = Params[i]->isOutput ? Params[i]->strDstFile : NULL;
        sts = Resources[i]->m_pDstFileWriter->Init(
        istream, 
        ptsMaker.get(),
        (VPP_FILTER_DISABLED != Params[i]->svcParam.mode) ? Params[i]->svcParam.descr : NULL,
        Params[i]->isOutYV12, 
        Params[i]->need_crc);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to init YUV writer\n")); WipeResources(Resources.front());});
        }
        //vppDefaultInitParams( Params[i] );
    }
    //parse input string

    //prepare file reader (YUV/RGB file)  
    sts = yuvReader.Init(Params.front()->strSrcFile, ptsMaker.get());
    CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    

#ifdef LIBVA_SUPPORT
    Resources.front()->pAllocator->libvaKeeper.reset(CreateLibVA());
#endif

    //prepare mfxParams
    for (unsigned int i = 0; i < Params.size(); i++)
    {
        sts = InitParamsVPP(&Resources[i]->m_mfxParamsVideo, Params[i]);
    }
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to prepare mfxParams\n")); WipeResources(Resources.front());});

    // prepare pts Checker
    if (ptsMaker.get())
    {
        sts = ptsMaker.get()->Init(&Resources.front()->m_mfxParamsVideo, Params.front()->asyncNum - 1, Params.front()->ptsAdvanced);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to prepare pts Checker\n")); WipeResources(Resources.front());});
    }

    Resources.front()->pAllocator->bUsedAsExternalAllocator = !Params.front()->bDefAlloc;

    for (unsigned int i = 0; i < Params.size(); i++)
    {
        // prepare ROI generator
        if( ROI_VAR_TO_FIX == Params[i]->roiCheckParam.mode || ROI_VAR_TO_VAR == Params[i]->roiCheckParam.mode )
        {
            Resources[i]->inROIGenerator.Init(Resources[i]->realFrameInfo[VPP_IN].Width, Resources[i]->realFrameInfo[VPP_IN].Height, Params[i]->roiCheckParam.srcSeed);
            Params[i]->roiCheckParam.srcSeed = Resources[i]->inROIGenerator.GetSeed();
            Resources[i]->bROITest[VPP_IN] = true;

        }
        if( ROI_FIX_TO_VAR == Params[i]->roiCheckParam.mode || ROI_VAR_TO_VAR == Params[i]->roiCheckParam.mode )
        {      
            Resources[i]->outROIGenerator.Init(Resources[i]->realFrameInfo[VPP_OUT].Width, Resources[i]->realFrameInfo[VPP_OUT].Height, Params[i]->roiCheckParam.dstSeed);
            Params[i]->roiCheckParam.dstSeed = Resources[i]->outROIGenerator.GetSeed();
            Resources[i]->bROITest[VPP_OUT] = true;
        }

        sts = ConfigVideoEnhancementFilters(Params[i], Resources[i]);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to ConfigVideoEnhancementFilters\n")); WipeResources(Resources[i]);});

        // allocate needed number of aux vpp data    
        //-----------------------------------------------------
        mfxExtVppReport *extVPPAuxData = new mfxExtVppReport[Params[i]->asyncNum * Params[i]->multiViewParam.viewCount];
        Resources[i]->pExtVPPAuxData = reinterpret_cast<mfxExtVppAuxData*>(extVPPAuxData);

        if( extVPPAuxData )
        {
            mfxU32  BufferId = MFX_EXTBUFF_VPP_SCENE_CHANGE;
            mfxU32  BufferSz = sizeof(mfxExtVppAuxData);

            if(Params[i]->varianceParam.mode != VPP_FILTER_DISABLED)
            {
                BufferId = MFX_EXTBUFF_VPP_VARIANCE_REPORT;
                BufferSz = sizeof(mfxExtVppReport);
            }
            else if( Params[i]->idetectParam.mode != VPP_FILTER_DISABLED )
            {
                BufferId = MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION;
                BufferSz = sizeof(mfxExtVppReport);//aya: fixme
            }

            for(int auxIdx = 0; auxIdx < Params[i]->asyncNum * Params[i]->multiViewParam.viewCount; auxIdx++)
            {
                extVPPAuxData[auxIdx].Header.BufferId = BufferId;
                extVPPAuxData[auxIdx].Header.BufferSz = BufferSz;
            }
        }


        //-----------------------------------------------------
        sts = InitResources(Resources[i], &Resources[i]->m_mfxParamsVideo, Params[i]);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to ConfigVideoEnhancementFilters\n")); WipeResources(Resources[i]);});


        sts = InitAllocator(Resources[i], &Resources[i]->m_mfxParamsVideo, Params[i], 0 == i ? true : false, Resources[i]->indexInPipe);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to ConfigVideoEnhancementFilters\n")); WipeResources(Resources[i]);});

        sts = InitVPP(Resources[i], &Resources[i]->m_mfxParamsVideo, Params[i]);
        if(MFX_WRN_FILTER_SKIPPED == sts)
        {
            vm_string_printf(VM_STRING("\nVPP_WRN: some filter(s) skipped\n"));
            IGNORE_MFX_STS(sts, MFX_WRN_FILTER_SKIPPED);
        }
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to InitResources\n")); WipeResources(Resources[i]);});
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            Params[i]->bPartialAccel = true;
        }
        else
        {
            Params[i]->bPartialAccel = false;
        }
        PrintInfo(Params[i], &Resources[i]->m_mfxParamsVideo, &Resources[i]->pProcessor->mfxSession);
        PrintDllInfo();


        if ( i > 0 )
        {
            // Link previous session with current one
            Resources[i-1]->next = Resources[i];
        }



    }

    for (unsigned int i = 0; i < Resources.size(); i++)
    {
        mfxU16 addInputSurf  = 0;
        mfxU16 addOutputSurf = 0;
        if ( Resources[i]->next )
        {
            sts = QuerySurfaces(Resources[i]->next->pProcessor, Resources[i]->next->pAllocator, &Resources[i]->next->m_mfxParamsVideo, Params[i+1], addInputSurf, addOutputSurf, Resources[i]->next->indexInPipe);
            CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to ConfigVideoEnhancementFilters\n")); WipeResources(Resources[i]);});
        }

        sts = AllocateSurfaces(Resources[i]->pProcessor, Resources[i]->pAllocator, &Resources[i]->m_mfxParamsVideo, Params[i], addOutputSurf, addInputSurf, Resources[i]->indexInPipe);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Failed to ConfigVideoEnhancementFilters\n")); WipeResources(Resources[i]);});

    }

    for (unsigned int j = 0; j < Resources.front()->pAllocator->pSurfaces.size(); j++)
    {
        fprintf(stderr, "Surface dimentions: %dx%d\n", Resources.front()->pAllocator->pSurfaces[j]->Info.Width, Resources.front()->pAllocator->pSurfaces[j]->Info.Height);
    }

    vm_string_printf(VM_STRING("VPP started\n"));
    statTimer.Start(); 
    Resources.front()->Process();
    nFrames = Resources.front()->GetFramesProcessed();
    statTimer.Stop();

    IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

    // report any errors that occurred 
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, 1, { vm_string_printf(VM_STRING("Unexpected error during processing\n")); WipeResources(Resources.front());});

    vm_string_printf(VM_STRING("\nVPP finished\n"));
    vm_string_printf(VM_STRING("\n"));

    vm_string_printf(VM_STRING("Total frames %d \n"), nFrames);
    vm_string_printf(VM_STRING("Total time %.2f sec \n"), statTimer.OverallTiming());
    vm_string_printf(VM_STRING("Frames per second %.3f fps \n"), nFrames / statTimer.OverallTiming());

    PutPerformanceToFile(*Params.front(), nFrames / statTimer.OverallTiming());
#if 0
    if ( Params.back()->need_crc ) {
        mfxU32 crc = yuvWriter.GetCRC(Resources.back()->pSurf[VPP_OUT]);
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
#endif
    return 0; /* OK */
} // int _tmain(int argc, vm_char *argv[])
/* EOF */
