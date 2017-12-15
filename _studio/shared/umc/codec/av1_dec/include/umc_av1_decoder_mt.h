//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include "umc_av1_decoder.h"

#ifndef __UMC_AV1_DECODER_MT_H
#define __UMC_AV1_DECODER_MT_H

struct aom_codec_ctx;

namespace UMC
{ class VideoProcessing; }

namespace UMC_AV1_DECODER
{
    class AV1DecoderMT
        : public AV1Decoder
    {
        std::unique_ptr<aom_codec_ctx>         context;
        aom_codec_ctx*                         codec;

        std::unique_ptr<UMC::VideoProcessing>  convertion;

    public:

        AV1DecoderMT();
        ~AV1DecoderMT();

        UMC::Status SetParams(UMC::BaseCodecParams*) override;
        UMC::Status RunDecoding() override;

    private:

        void AllocateFrameData(UMC::VideoDataInfo const&, UMC::FrameMemID, AV1DecoderFrame*) override;
    };
}

#endif // __UMC_AV1_DECODER_MT_H
#endif // UMC_ENABLE_AV1_VIDEO_DECODER