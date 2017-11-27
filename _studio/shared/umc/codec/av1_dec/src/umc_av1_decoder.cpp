//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_frame_data.h"

#include "umc_av1_decoder.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_frame.h"

#include <algorithm>

namespace UMC_AV1_DECODER
{
    AV1Decoder::AV1Decoder()
        : allocator(nullptr)
        , sequence_header(new SequenceHeader{})
        , counter(0)
        , prev_frame(nullptr)
    {
#if UMC_AV1_DECODER_REV == 0
        //TEMP stub
        AV1Bitstream{}.GetSequenceHeader(sequence_header.get());
#endif
    }

    AV1Decoder::~AV1Decoder()
    {
        std::for_each(std::begin(dpb), std::end(dpb),
            std::default_delete<AV1DecoderFrame>()
        );
    }

    UMC::Status AV1Decoder::DecodeHeader(UMC::MediaData* in, UMC::VideoDecoderParams* par)
    {
        if (!in)
            return UMC::UMC_ERR_NULL_PTR;

        auto src = reinterpret_cast<Ipp8u*>(in->GetDataPointer());
        AV1Bitstream bs(src, Ipp32u(in->GetDataSize()));

        while (in->GetDataSize() >= MINIMAL_DATA_SIZE)
        {
            try
            {
                SequenceHeader sh = {};
#if UMC_AV1_DECODER_REV == 0
                bs.GetSequenceHeader(&sh);
#endif

                FrameHeader fh = {};
                bs.GetFrameHeaderPart1(&fh, &sh);
                in->MoveDataPointer(fh.frameHeaderLength);

                if (FillVideoParam(fh, par) == UMC::UMC_OK)
                    return UMC::UMC_OK;
            }
            catch (av1_exception const& e)
            {
                auto sts = e.GetStatus();
                if (sts != UMC::UMC_ERR_INVALID_STREAM)
                    return sts;
            }
        }

        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    UMC::Status AV1Decoder::Init(UMC::BaseCodecParams* vp)
    {
        if (!vp)
            return UMC::UMC_ERR_NULL_PTR;

        AV1DecoderParams* dp =
            DynamicCast<AV1DecoderParams, UMC::BaseCodecParams>(vp);
        if (!dp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        if (!dp->allocator)
            return UMC::UMC_ERR_INVALID_PARAMS;
        allocator = dp->allocator;

        params = *dp;
        return SetParams(vp);
    }

    UMC::Status AV1Decoder::GetInfo(UMC::BaseCodecParams* info)
    {
        UMC::VideoDecoderParams* vp =
            DynamicCast<UMC::VideoDecoderParams, UMC::BaseCodecParams>(info);
        if (!vp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        *vp = params;
        return UMC::UMC_OK;
    }

    DPBType DPBUpdate(AV1DecoderFrame const * prevFrame)
    {
        DPBType updatedFrameDPB;

        DPBType const& prevFrameDPB = prevFrame->frame_dpb;
        if (prevFrameDPB.empty())
            updatedFrameDPB.resize(NUM_REF_FRAMES);
        else
            updatedFrameDPB = prevFrameDPB;

        const FrameHeader& fh = prevFrame->GetFrameHeader();

        for (Ipp8u i = 0; i < NUM_REF_FRAMES; i++)
        {
            if ((fh.refreshFrameFlags >> i) & 1)
            {
                if (!prevFrameDPB.empty() && prevFrameDPB[i])
                    prevFrameDPB[i]->DecrementReference();

                updatedFrameDPB[i] = const_cast<AV1DecoderFrame*>(prevFrame);
                prevFrame->IncrementReference();
            }
        }

        return updatedFrameDPB;
    }

    static void FillRefFrameSizes(FrameHeader* fh, DPBType const* dpb)
    {
        if (!dpb)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        if (dpb->empty())
            return;

        for (Ipp8u i = 0; i < NUM_REF_FRAMES; ++i)
        {
            AV1DecoderFrame const* frame = (*dpb)[i];
            if (frame)
            {
                FrameHeader const& ref_fh = frame->GetFrameHeader();
                fh->sizesOfRefFrame[i].width = ref_fh.width;
                fh->sizesOfRefFrame[i].height = ref_fh.height;
            }
        }
    }

    UMC::Status AV1Decoder::GetFrame(UMC::MediaData* in, UMC::MediaData*)
    {
        Ipp8u* src = reinterpret_cast<Ipp8u*>(in->GetDataPointer());
        Ipp32u const size = Ipp32u(in->GetDataSize());
        if (size < MINIMAL_DATA_SIZE)
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;

        DPBType updated_refs;
        if (prev_frame)
            updated_refs = DPBUpdate(prev_frame);

        AV1Bitstream bs(src, size);

        FrameHeader fh = {};
        FillRefFrameSizes(&fh, &updated_refs);
        bs.GetFrameHeaderPart1(&fh, sequence_header.get());
        bs.GetFrameHeaderFull(&fh, sequence_header.get(), &(prev_frame->GetFrameHeader()));

        AV1DecoderFrame* frame = GetFrameBuffer(fh);
        if (!frame)
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

        frame->SetSeqHeader(*sequence_header.get());

        frame->AddSource(in);
        in->MoveDataPointer(size);

        frame->frame_dpb = updated_refs;
        frame->UpdateReferenceList();
        prev_frame = frame;

        std::unique_lock<std::mutex> l(guard);
        queue.push_back(frame);

        return UMC::UMC_OK;
    }

    AV1DecoderFrame* AV1Decoder::FindFrameByMemID(UMC::FrameMemID id)
    {
        return FindFrame(
            [id](AV1DecoderFrame const* f)
            { return f->GetMemID() == id; }
        );
    }

    AV1DecoderFrame* AV1Decoder::FindFrameByDispID(Ipp32u id)
    {
        return FindFrame(
            [id](AV1DecoderFrame const* f)
            { return f->GetFrameHeader().display_frame_id == id; }
        );
    }

    AV1DecoderFrame* AV1Decoder::GetFrameToDisplay()
    {
        std::unique_lock<std::mutex> l(guard);

        auto i = std::min_element(std::begin(dpb), std::end(dpb),
            [](AV1DecoderFrame const* f1, AV1DecoderFrame const* f2)
            {
                FrameHeader const& h1 = f1->GetFrameHeader(); FrameHeader const& h2 = f2->GetFrameHeader();
                Ipp32u const id1 = h1.showFrame && !f1->Displayed() ? h1.display_frame_id : (std::numeric_limits<Ipp32u>::max)();
                Ipp32u const id2 = h2.showFrame && !f2->Displayed() ? h2.display_frame_id : (std::numeric_limits<Ipp32u>::max)();

                return  id1 < id2;
            }
        );

        if (i == std::end(dpb))
            return nullptr;

        AV1DecoderFrame* frame = *i;
        return
            frame->GetFrameHeader().showFrame ? frame : nullptr;
    }

    UMC::Status AV1Decoder::FillVideoParam(FrameHeader const& fh, UMC::VideoDecoderParams* par)
    {
        VM_ASSERT(par);

        par->info.stream_type = UMC::AV1_VIDEO;
        par->info.profile     = fh.profile;

        par->info.clip_info = { Ipp32s(fh.width), Ipp32s(fh.height) };
        par->info.disp_clip_info = par->info.clip_info;

        if (!fh.subsamplingX && !fh.subsamplingY)
            par->info.color_format = UMC::YUV444;
        else if (fh.subsamplingX && !fh.subsamplingY)
            par->info.color_format = UMC::YUY2;
        else if (fh.subsamplingX && fh.subsamplingY)
            par->info.color_format = UMC::NV12;

        if (fh.bit_depth == 10)
        {
            switch (par->info.color_format)
            {
                case UMC::NV12:   par->info.color_format = UMC::P010; break;
                case UMC::YUY2:   par->info.color_format = UMC::Y210; break;
                case UMC::YUV444: par->info.color_format = UMC::Y410; break;

                default:
                    VM_ASSERT(!"Unknown subsampling");
                    return UMC::UMC_ERR_UNSUPPORTED;
            }
        }
        else if (fh.bit_depth == 12)
        {
            switch (par->info.color_format)
            {
                case UMC::NV12:   par->info.color_format = UMC::P016; break;
                case UMC::YUY2:   par->info.color_format = UMC::Y216; break;
                case UMC::YUV444: par->info.color_format = UMC::Y416; break;

                default:
                    VM_ASSERT(!"Unknown subsampling");
                    return UMC::UMC_ERR_UNSUPPORTED;
            }
        }

        par->info.interlace_type = UMC::PROGRESSIVE;
        par->info.aspect_ratio_width = par->info.aspect_ratio_height = 1;

        par->lFlags = 0;
        return UMC::UMC_OK;
    }

    void AV1Decoder::SetDPBSize(Ipp32u size)
    {
        VM_ASSERT(size > 0);
        VM_ASSERT(size < 128);

        dpb.resize(size);
        std::generate(std::begin(dpb), std::end(dpb),
            [] { return new AV1DecoderFrame{}; }
        );
    }

    void AV1Decoder::CompleteDecodedFrames()
    {
        std::unique_lock<std::mutex> l(guard);

        std::for_each(dpb.begin(), dpb.end(),
            [](AV1DecoderFrame* frame)
            {
            if (frame->DecodingCompleted() && !frame->Decoded())
                frame->OnDecodingCompleted();
            }
        );
    }

    AV1DecoderFrame* AV1Decoder::GetFreeFrame()
    {
        std::unique_lock<std::mutex> l(guard);

        auto i = std::find_if(std::begin(dpb), std::end(dpb),
            [](AV1DecoderFrame const* frame)
            { return frame->Empty(); }
        );

        AV1DecoderFrame* frame =
            i != std::end(dpb) ? *i : nullptr;

        if (frame)
            frame->UID = counter++;

        return frame;
    }

    AV1DecoderFrame* AV1Decoder::GetFrameBuffer(FrameHeader const& fh)
    {
        CompleteDecodedFrames();
        AV1DecoderFrame* frame = GetFreeFrame();
        if (!frame)
            return nullptr;

        frame->Reset(&fh);

        // increase ref counter when we get empty frame from DPB
        frame->IncrementReference();

        if (fh.show_existing_frame)
        {
            AV1DecoderFrame* existing_frame =  FindFrameByDispID(fh.display_frame_id);
            if (!existing_frame)
            {
                VM_ASSERT(!"Existing frame is not found");
                return nullptr;
            }

            UMC::FrameData* fd = frame->GetFrameData();
            VM_ASSERT(fd);
            fd->m_locked = true;
        }
        else
        {
            if (!allocator)
                throw av1_exception(UMC::UMC_ERR_NOT_INITIALIZED);

            UMC::VideoDataInfo info{};
            UMC::Status sts = info.Init(params.info.clip_info.width, params.info.clip_info.height, params.info.color_format, 0);
            if (sts != UMC::UMC_OK)
                throw av1_exception(sts);

            UMC::FrameMemID id;
            sts = allocator->Alloc(&id, &info, 0);
            if (sts != UMC::UMC_OK)
                throw av1_exception(UMC::UMC_ERR_ALLOC);

            AllocateFrameData(info, id, frame);
        }

        return frame;
    }

    template <typename F>
    AV1DecoderFrame* AV1Decoder::FindFrame(F pred)
    {
        std::unique_lock<std::mutex> l(guard);

        auto i = std::find_if(std::begin(dpb), std::end(dpb),pred);
        return
            i != std::end(dpb) ? (*i) : nullptr;
    }
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
