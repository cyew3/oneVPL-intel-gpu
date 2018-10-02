//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_av1_dec_defs.h"

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_BITSTREAM_H_
#define __UMC_AV1_BITSTREAM_H_

#include "umc_vp9_bitstream.h"

namespace UMC_AV1_DECODER
{
    class AV1DecoderFrame;

    class AV1Bitstream
        : public UMC_VP9_DECODER::VP9Bitstream
    {
    public:

        void ReadOBUInfo(OBUInfo*);
        void ReadTileGroupHeader(FrameHeader const*, TileGroupInfo*);
        void ReadTile(FrameHeader const*, size_t&, size_t&);
        void ReadByteAlignment();
        Ipp64u GetLE(Ipp32u);
        void ReadSequenceHeader(SequenceHeader*);
        void ReadUncompressedHeader(FrameHeader*, SequenceHeader const*, FrameHeader const*, DPBType const&);

        using UMC_VP9_DECODER::VP9Bitstream::VP9Bitstream;

        Ipp8u GetBit()
        {
            return static_cast<Ipp8u>(UMC_VP9_DECODER::VP9Bitstream::GetBit());
        }

    protected:
    };
}

#endif // __UMC_AV1_BITSTREAM_H_

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
