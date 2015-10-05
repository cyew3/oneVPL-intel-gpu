//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//  This sample was distributed or derived from the Intel's Media Samples package.
//  The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
//  or https://software.intel.com/en-us/media-client-solutions-support.
//        Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __VPP_EXT_BUFFERS_STORAGE_H__
#define __VPP_EXT_BUFFERS_STORAGE_H__

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxstructures.h"
#include "sample_defs.h"
#include <vector>

namespace TranscodingSample
{
    struct sInputParams;

    class CVPPExtBuffersStorage
    {
    public:
        CVPPExtBuffersStorage(void);
        ~CVPPExtBuffersStorage(void);
        mfxStatus Init(TranscodingSample::sInputParams* params);
        void Clear();

        std::vector<mfxExtBuffer*> ExtBuffers;

        static mfxStatus ParseCmdLine(msdk_char *argv[],mfxU32 argc,mfxU32& index,TranscodingSample::sInputParams* params,mfxU32& skipped);

    protected:
        mfxExtVPPDenoise denoiseFilter;
        mfxExtVPPDetail detailFilter;
        mfxExtVPPFrameRateConversion frcFilter;
        mfxExtVPPDeinterlacing deinterlaceFilter;
        mfxExtVPPFieldProcessing vppFieldProcessingFilter;
    };
}

#endif