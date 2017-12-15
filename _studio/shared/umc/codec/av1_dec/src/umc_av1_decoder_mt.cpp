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

#include "umc_av1_dec_defs.h"
#include "umc_av1_decoder_mt.h"
#include "umc_av1_frame.h"
#include "umc_av1_bitstream.h"
#include "umc_video_processing.h"

#pragma warning(push)
#pragma warning(disable : 4505)
#include "aom/aom_decoder.h"
//#include "aom_util/aom_thread.h"
#include "aom/aomdx.h"
#pragma warning(pop)

namespace UMC_AV1_DECODER
{
    /*namespace aux
    {
        static int reset(AVxWorker *const)
        { return 1;}
        static void end(AVxWorker *const)
        {}
    }*/

    AV1DecoderMT::AV1DecoderMT()
        : context(new aom_codec_ctx{})
        , codec(context.get())
        , convertion(new UMC::VideoProcessing{})
    {
        auto dx = aom_codec_av1_dx();
        if (!dx)
            throw av1_exception(UMC::UMC_ERR_INIT);

        auto const flags =
            AOM_CODEC_USE_FRAME_THREADING;

        auto sts = aom_codec_dec_init(codec, dx, NULL, flags);
        if (sts != AOM_CODEC_OK)
            throw av1_exception(sts);
    }

    AV1DecoderMT::~AV1DecoderMT()
    {
        aom_codec_destroy(codec);
    }

    UMC::Status AV1DecoderMT::SetParams(UMC::BaseCodecParams* info)
    {
        if (!info)
            return UMC::UMC_ERR_NULL_PTR;

        if (!codec || !allocator)
            return UMC::UMC_ERR_INIT;

        AV1DecoderParams* dp =
            DynamicCast<AV1DecoderParams, UMC::BaseCodecParams>(info);
        if (!dp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        Ipp32u const dpb_size =
            //AOM AV1 decoder tracks references itself, we don't need to take them into account
            params.async_depth;
        SetDPBSize(dpb_size);

        UMC::VideoProcessingParams pp{};
        UMC::Status sts = convertion->Init(&pp);
        return sts;
    }

    UMC::Status AV1DecoderMT::RunDecoding()
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

        UMC::MediaData* data = frame->GetSource();
        VM_ASSERT(data);

        auto src = reinterpret_cast<uint8_t const*>(data->GetDataPointer());
        auto size = unsigned(data->GetDataSize());
        auto ret = aom_codec_decode(codec, src, size, frame, 0);
        if (ret != AOM_CODEC_OK)
            return UMC::UMC_ERR_FAILED;

        aom_codec_iter_t it = NULL;
        auto image = aom_codec_get_frame(codec, &it);
        if (!image)
            return UMC::UMC_WRN_INFO_NOT_READY;;

        frame =
            reinterpret_cast<AV1DecoderFrame*>(image->user_priv);
        if (!frame)
            return UMC::UMC_ERR_FAILED;

        UMC::FrameData* fd = frame->GetFrameData();
        if (!fd)
            return UMC::UMC_ERR_FAILED;

        UMC::VideoDataInfo  const* vi = fd->GetInfo();
        if (!vi)
            return UMC::UMC_ERR_FAILED;

        UMC::VideoData in; in.Init(image->d_w, image->d_h, UMC::YUV420, image->bit_depth);
        for (int i = 0; i < in.GetNumPlanes(); ++i)
        {
            in.SetPlanePointer(image->planes[AOM_PLANE_Y + i], i);
            in.SetPlanePitch(image->stride[AOM_PLANE_Y + i], i);
        }

        UMC::VideoData out; out.Init(vi->GetWidth(), vi->GetHeight(), UMC::NV12);
        for (int i = 0; i < out.GetNumPlanes(); ++i)
        {
            auto mi = fd->GetPlaneMemoryInfo(i);
            if (!mi)
                return UMC::UMC_ERR_FAILED;

            out.SetPlanePointer(mi->m_planePtr, i);
            out.SetPlanePitch(mi->m_pitch, i);
        }

        UMC::Status sts = convertion->GetFrame(&in, &out);
        return sts;
    }

    void AV1DecoderMT::AllocateFrameData(UMC::VideoDataInfo const&, UMC::FrameMemID id, AV1DecoderFrame* frame)
    {
        VM_ASSERT(id != UMC::FRAME_MID_INVALID);
        VM_ASSERT(frame);

        UMC::FrameData const* fd = allocator->Lock(id);
        if (!fd)
            throw av1_exception(UMC::UMC_ERR_ALLOC);

        frame->Allocate(fd);

        UMC::FrameData* fd2 = frame->GetFrameData();
        VM_ASSERT(fd2);
        fd2->m_locked = true;
    }
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
