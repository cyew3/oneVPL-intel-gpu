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
#include "umc_va_base.h"

#include "umc_av1_decoder_va.h"
#include "umc_av1_frame.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_va_packer.h"

#include "umc_frame_data.h"

namespace UMC_AV1_DECODER
{
    AV1DecoderVA::AV1DecoderVA()
        : va(nullptr)
    {}

    UMC::Status AV1DecoderVA::SetParams(UMC::BaseCodecParams* info)
    {
        if (!info)
            return UMC::UMC_ERR_NULL_PTR;

        AV1DecoderParams* dp =
            DynamicCast<AV1DecoderParams, UMC::BaseCodecParams>(info);
        if (!dp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        if (!dp->pVideoAccelerator)
            return UMC::UMC_ERR_NULL_PTR;

        va = dp->pVideoAccelerator;
        packer.reset(Packer::CreatePacker(va));

        Ipp32u const dpb_size =
            params.async_depth + TOTAL_REFS;
        SetDPBSize(dpb_size);

        return UMC::UMC_OK;
    }

    UMC::Status AV1DecoderVA::RunDecoding()
    {
        AV1DecoderFrame* frame;
        {
            std::unique_lock<std::mutex> l(guard);
            if (queue.empty())
                return UMC::UMC_OK;

            frame = queue.front();
            queue.pop_front();
        }

        VM_ASSERT(frame);
        if (!frame)
            return UMC::UMC_ERR_FAILED;;

        VM_ASSERT(va);
        UMC::Status sts = va->BeginFrame(frame->GetMemID());
        if (sts != UMC::UMC_OK)
            return sts;

        VM_ASSERT(packer);
        packer->BeginFrame();

        UMC::MediaData* in = frame->GetSource();
        VM_ASSERT(in);

        Ipp8u* src = reinterpret_cast<Ipp8u*>(in->GetDataPointer());
        AV1Bitstream bs(src, Ipp32u(in->GetDataSize()));
        packer->PackAU(&bs, frame);

        packer->EndFrame();

        sts = va->Execute();
        sts = va->EndFrame();
        return sts;
    }

    void AV1DecoderVA::AllocateFrameData(UMC::VideoDataInfo const& info, UMC::FrameMemID id, AV1DecoderFrame* frame)
    {
        VM_ASSERT(id != UMC::FRAME_MID_INVALID);
        VM_ASSERT(frame);

        UMC::FrameData fd;
        fd.Init(&info, id, allocator);

        frame->Allocate(&fd);

        UMC::FrameData* fd2 = frame->GetFrameData();
        VM_ASSERT(fd2);
        fd2->m_locked = true;
    }
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
