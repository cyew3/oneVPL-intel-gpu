#pragma once

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
    };
}
