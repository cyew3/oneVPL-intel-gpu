/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "vpp_ext_buffers_storage.h"
#include "pipeline_transcode.h"
#include "transcode_utils.h"
#include "sample_utils.h"

#define VALUE_CHECK(val, argIdx, argName) \
{ \
    if (val) \
    { \
        PrintHelp(NULL, MSDK_STRING("Input argument number %d \"%s\" require more parameters"), argIdx, argName); \
        return MFX_ERR_UNSUPPORTED;\
    } \
}


using namespace TranscodingSample;

CVPPExtBuffersStorage::CVPPExtBuffersStorage(void)
{
}


CVPPExtBuffersStorage::~CVPPExtBuffersStorage(void)
{
}

mfxStatus CVPPExtBuffersStorage::Init(TranscodingSample::sInputParams* params)
{
    if(params->DenoiseLevel!=-1)
    {
        denoiseFilter.Header.BufferId=MFX_EXTBUFF_VPP_DENOISE;
        denoiseFilter.Header.BufferSz=sizeof(denoiseFilter);
        denoiseFilter.DenoiseFactor=(mfxU16)params->DenoiseLevel;

        ExtBuffers.push_back((mfxExtBuffer*)&denoiseFilter);
    }

    if(params->DetailLevel!=-1)
    {
        detailFilter.Header.BufferId=MFX_EXTBUFF_VPP_DETAIL;
        detailFilter.Header.BufferSz=sizeof(detailFilter);
        detailFilter.DetailFactor=(mfxU16)params->DetailLevel;

        ExtBuffers.push_back((mfxExtBuffer*)&detailFilter);
    }

    if(params->FRCAlgorithm)
    {
        memset(&frcFilter,0,sizeof(frcFilter));
        frcFilter.Header.BufferId=MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION;
        frcFilter.Header.BufferSz=sizeof(frcFilter);
        frcFilter.Algorithm=params->FRCAlgorithm;

        ExtBuffers.push_back((mfxExtBuffer*)&frcFilter);
    }

    if(params->bEnableDeinterlacing && params->DeinterlacingMode)
    {
        memset(&deinterlaceFilter,0,sizeof(deinterlaceFilter));
        deinterlaceFilter.Header.BufferId=MFX_EXTBUFF_VPP_DEINTERLACING;
        deinterlaceFilter.Header.BufferSz=sizeof(deinterlaceFilter);
        deinterlaceFilter.Mode=params->DeinterlacingMode;

        ExtBuffers.push_back((mfxExtBuffer*)&deinterlaceFilter);
    }

    return MFX_ERR_NONE;
}

void CVPPExtBuffersStorage::Clear()
{
    ExtBuffers.clear();
}

mfxStatus CVPPExtBuffersStorage::ParseCmdLine(msdk_char *argv[],mfxU32 argc,mfxU32& index,TranscodingSample::sInputParams* params,mfxU32& skipped)
{
    if (0 == msdk_strcmp(argv[index], MSDK_STRING("-denoise")))
    {
        VALUE_CHECK(index+1 == argc, index, argv[index]);
        index++;
        if (MFX_ERR_NONE != msdk_opt_read(argv[index], params->DenoiseLevel) || !(params->DenoiseLevel>=0 && params->DenoiseLevel<=100))
        {
            PrintHelp(NULL, MSDK_STRING("-denoise \"%s\" is invalid"), argv[index]);
            return MFX_ERR_UNSUPPORTED;
        }
        skipped+=2;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-detail")))
    {
        VALUE_CHECK(index+1 == argc, index, argv[index]);
        index++;
        if (MFX_ERR_NONE != msdk_opt_read(argv[index], params->DetailLevel) || !(params->DetailLevel>=0 && params->DetailLevel<=100))
        {
            PrintHelp(NULL, MSDK_STRING("-detail \"%s\" is invalid"), argv[index]);
            return MFX_ERR_UNSUPPORTED;
        }
        skipped+=2;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-FRC::PT")))
    {
        params->FRCAlgorithm=MFX_FRCALGM_PRESERVE_TIMESTAMP;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-FRC::DT")))
    {
        params->FRCAlgorithm=MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-FRC::INTERP")))
    {
        params->FRCAlgorithm=MFX_FRCALGM_FRAME_INTERPOLATION;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-deinterlace")))
    {
        params->bEnableDeinterlacing = true;
        params->DeinterlacingMode=0;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-deinterlace::ADI")))
    {
        params->bEnableDeinterlacing = true;
        params->DeinterlacingMode=MFX_DEINTERLACING_ADVANCED;
        return MFX_ERR_NONE;
    }
    else if (0 == msdk_strcmp(argv[index], MSDK_STRING("-deinterlace::ADI_NO_REF")))
    {
        params->bEnableDeinterlacing = true;
        params->DeinterlacingMode=MFX_DEINTERLACING_ADVANCED_NOREF;
        return MFX_ERR_NONE;
    }

    return MFX_ERR_MORE_DATA;
}