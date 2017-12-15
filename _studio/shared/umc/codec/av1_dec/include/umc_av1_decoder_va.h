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

#ifndef __UMC_AV1_DECODER_VA_H
#define __UMC_AV1_DECODER_VA_H

namespace UMC_AV1_DECODER
{
    class Packer;
    class AV1DecoderVA
        : public AV1Decoder
    {
        UMC::VideoAccelerator*     va;
        std::unique_ptr<Packer>    packer;

    public:

        AV1DecoderVA();

        UMC::Status SetParams(UMC::BaseCodecParams*) override;
        UMC::Status RunDecoding() override;

    private:

        void AllocateFrameData(UMC::VideoDataInfo const&, UMC::FrameMemID, AV1DecoderFrame*) override;
    };
}

#endif // __UMC_AV1_DECODER_VA_H
#endif // UMC_ENABLE_AV1_VIDEO_DECODER