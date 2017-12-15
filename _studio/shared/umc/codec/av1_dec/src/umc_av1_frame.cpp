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

#include "umc_av1_frame.h"
#include "umc_av1_utils.h"

#include "umc_frame_data.h"
#include "umc_media_data.h"

namespace UMC_AV1_DECODER
{
    AV1DecoderFrame::AV1DecoderFrame()
        : locked(0)
        , data(new UMC::FrameData{})
        , source(new UMC::MediaData{})
    {
        Reset();
    }

    AV1DecoderFrame::~AV1DecoderFrame()
    {
        VM_ASSERT(Empty());
    }

    void AV1DecoderFrame::Reset()
    {
        error = 0;
        displayed = false;
        data->Close();

        y_dc_delta_q = uv_dc_delta_q = uv_ac_delta_q = 0;

        memset(&header, 0, sizeof(header));
        header.display_frame_id = (std::numeric_limits<Ipp32u>::max)();
        header.currFrame = -1;
        header.frameCountInBS = 0;
        header.currFrameInBS = 0;
        memset(&header.ref_frame_map, -1, sizeof(header.ref_frame_map));

        UID = -1;
    }

    void AV1DecoderFrame::Reset(FrameHeader const* fh)
    {
        if (!fh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        Ipp32s id = UID;
        Reset();
        UID = id;

        header = *fh;

        if (y_dc_delta_q  != header.y_dc_delta_q  ||
            uv_dc_delta_q != header.uv_dc_delta_q ||
            uv_ac_delta_q != header.uv_ac_delta_q)
        {
            InitDequantizer(&header);

            y_dc_delta_q  = header.y_dc_delta_q;
            uv_dc_delta_q = header.uv_dc_delta_q;
            uv_ac_delta_q = header.uv_ac_delta_q;
        }
    }

    void AV1DecoderFrame::Allocate(UMC::FrameData const* fd)
    {
        if (!fd)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        VM_ASSERT(data);
        *data = *fd;
    }

    void AV1DecoderFrame::AddSource(UMC::MediaData* in)
    {
        if (!in)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        size_t const size = in->GetDataSize();
        source->Alloc(size);
        if (source->GetBufferSize() < size)
            throw av1_exception(UMC::UMC_ERR_ALLOC);

        memcpy_s(source->GetDataPointer(), size, in->GetDataPointer(), size);
        source->SetDataSize(size);
    }

    bool AV1DecoderFrame::Empty() const
    { return !data->m_locked; }

    UMC::FrameMemID AV1DecoderFrame::GetMemID() const
    {
        VM_ASSERT(data);
        return data->GetFrameMID();
    }
}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
