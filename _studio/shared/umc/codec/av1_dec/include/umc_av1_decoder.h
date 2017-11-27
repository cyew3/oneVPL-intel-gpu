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

#ifndef __UMC_AV1_DECODER_H
#define __UMC_AV1_DECODER_H

#include "umc_video_decoder.h"
#include "umc_av1_dec_defs.h"

#include <mutex>
#include <memory>
#include <vector>
#include <deque>

namespace UMC
{ class FrameAllocator; }

namespace UMC_AV1_DECODER
{
    struct SequenceHeader;
    struct FrameHeader;
    class AV1DecoderFrame;

    class AV1DecoderParams
        : public UMC::VideoDecoderParams
    {
        DYNAMIC_CAST_DECL(AV1DecoderParams, UMC::VideoDecoderParams)

    public:

        AV1DecoderParams()
            : allocator(nullptr)
            , async_depth(0)
        {}

    public:

        UMC::FrameAllocator* allocator;
        Ipp32u               async_depth;
    };

    class AV1Decoder
        : public UMC::VideoDecoder
    {

    public:

        AV1Decoder();
        ~AV1Decoder();

    public:

        static UMC::Status DecodeHeader(UMC::MediaData*, UMC::VideoDecoderParams*);

        /* UMC::BaseCodec interface */
        UMC::Status Init(UMC::BaseCodecParams*) override;
        UMC::Status GetFrame(UMC::MediaData* in, UMC::MediaData* out) override;

        UMC::Status Reset()
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        UMC::Status GetInfo(UMC::BaseCodecParams*) override;

    public:

        /* UMC::VideoDecoder interface */
        UMC::Status ResetSkipCount()
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        UMC::Status SkipVideoFrame(Ipp32s)
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        Ipp32u GetNumOfSkippedFrames()
        { return 0; }

    public:

        AV1DecoderFrame* FindFrameByMemID(UMC::FrameMemID);
        AV1DecoderFrame* FindFrameByDispID(Ipp32u);
        AV1DecoderFrame* GetFrameToDisplay();

        virtual UMC::Status RunDecoding() = 0;

    protected:

        static UMC::Status FillVideoParam(FrameHeader const&, UMC::VideoDecoderParams*);

        virtual void SetDPBSize(Ipp32u);
        virtual AV1DecoderFrame* GetFreeFrame();
        virtual AV1DecoderFrame* GetFrameBuffer(FrameHeader const&);

        virtual void AllocateFrameData(UMC::VideoDataInfo const&, UMC::FrameMemID, AV1DecoderFrame*) = 0;
        virtual void CompleteDecodedFrames();

    private:

        template <typename F>
        AV1DecoderFrame* FindFrame(F pred);

    protected:

        std::mutex                      guard;

        UMC::FrameAllocator*            allocator;

        std::unique_ptr<SequenceHeader> sequence_header;
        DPBType                         dpb;     // store of decoded frames
        std::deque<AV1DecoderFrame*>    queue;

        Ipp32u                          counter;
        AV1DecoderParams                params;

        AV1DecoderFrame*                prev_frame;
    };
}

#endif // __UMC_AV1_DECODER_H
#endif // UMC_ENABLE_AV1_VIDEO_DECODER