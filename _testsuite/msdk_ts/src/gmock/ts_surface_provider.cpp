/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_surface_provider.h"
#include "ts_decoder.h"
#include "ts_bitstream.h"

class tsIvfVP9FileDecoder : public tsSurfaceProvider
{
private:
    tsBitstreamReaderIVF reader;
    tsVideoDecoder    decoder;
public:
    tsIvfVP9FileDecoder(const char* fname, mfxU32 codec)
        : reader(fname, 100000)
        , decoder(MFX_CODEC_VP9)
    {
        decoder.m_bs_processor = &reader;
        decoder.Init();
    }

    virtual ~tsIvfVP9FileDecoder()
    {
        decoder.Close();
    }

    virtual mfxFrameSurface1* NextSurface()
    {
        mfxStatus sts = decoder.DecodeFrames(1, false);
        if (sts == MFX_ERR_NONE) {
            return decoder.m_pSurfOut;
        }
        return NULL;
    }
};

std::unique_ptr<tsSurfaceProvider> tsSurfaceProvider::CreateIvfVP9Decoder(const char *fileName) {
    return std::unique_ptr<tsSurfaceProvider>(new tsIvfVP9FileDecoder(fileName, MFX_CODEC_VP9));
}