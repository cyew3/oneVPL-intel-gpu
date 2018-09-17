//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include "umc_av1_dec_defs.h"
#include "umc_av1_frame.h"
#include "umc_av1_utils.h"
#include <limits>
#include <algorithm>

#include "umc_frame_data.h"
#include "umc_media_data.h"

namespace UMC_AV1_DECODER
{
    TileSet::TileSet(UMC::MediaData* in, TileLayout const& layout_)
        : layout(layout_)
    {
        if (!in)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        TileLocation const& lastTile = layout.back();
        size_t const size = lastTile.offset + lastTile.size;
        source.Alloc(size);
        if (source.GetBufferSize() < size)
            throw av1_exception(UMC::UMC_ERR_ALLOC);

        memcpy_s(source.GetDataPointer(), size, in->GetDataPointer(), size);
        source.SetDataSize(size);
    }

    size_t TileSet::Submit(Ipp8u* bsBuffer, size_t spaceInBuffer, size_t offsetInBuffer, TileLayout& layoutWithOffset)
    {
        if (submitted)
            return 0;

        Ipp8u* data = static_cast<Ipp8u*>(source.GetDataPointer());
        const size_t length = std::min<size_t>(source.GetDataSize(), spaceInBuffer);

        mfx_memcpy(bsBuffer + offsetInBuffer, length, data, length);
        source.Close();

        for (auto& loc : layout)
        {
            layoutWithOffset.emplace_back(loc);
            layoutWithOffset.back().offset += offsetInBuffer;
        }

        submitted = true;

        return length;
    }

    AV1DecoderFrame::AV1DecoderFrame()
        : locked(0)
        , data(new UMC::FrameData{})
#if UMC_AV1_DECODER_REV < 5000
        , source(new UMC::MediaData{})
#endif
        , seq_header(new SequenceHeader{})
        , header(new FrameHeader{})
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
        outputted = false;
        decoded   = false;
        decoding_started = false;
        decoding_completed = false;
        data->Close();
#if UMC_AV1_DECODER_REV >= 5000
        tile_sets.resize(0);
#endif

        memset(seq_header.get(), 0, sizeof(SequenceHeader));
        memset(header.get(), 0, sizeof(FrameHeader));
        header->display_frame_id = (std::numeric_limits<Ipp32u>::max)();
        // TODO: [Global] Restore work with below fields if required
        //header->currFrame = -1;
        //header->frameCountInBS = 0;
        //header->currFrameInBS = 0;
        //memset(&header->ref_frame_map, -1, sizeof(header->ref_frame_map));

        ResetRefCounter();
        FreeReferenceFrames();
        frame_dpb.clear();

        SetRefValid(false);

        UID = -1;
    }

    void AV1DecoderFrame::Reset(FrameHeader const* fh)
    {
        if (!fh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        Ipp32s id = UID;
        Reset();
        UID = id;

        *header = *fh;
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

    void AV1DecoderFrame::AddTileSet(UMC::MediaData* in, TileLayout const& layout)
    {
        tile_sets.emplace_back(in, layout);
    }

    void AV1DecoderFrame::SetSeqHeader(SequenceHeader const& sh)
    {
        *seq_header = sh;
    }

    bool AV1DecoderFrame::Empty() const
    { return !data->m_locked; }

    bool AV1DecoderFrame::Decoded() const
    {
        return decoded;
    }

    UMC::FrameMemID AV1DecoderFrame::GetMemID() const
    {
        VM_ASSERT(data);
        return data->GetFrameMID();
    }

    void AV1DecoderFrame::AddReferenceFrame(AV1DecoderFrame * frm)
    {
        if (!frm || frm == this)
            return;

        if (std::find(references.begin(), references.end(), frm) != references.end())
            return;

        frm->IncrementReference();
        references.push_back(frm);
    }

    void AV1DecoderFrame::FreeReferenceFrames()
    {
        for (DPBType::iterator i = references.begin(); i != references.end(); i++)
            (*i)->DecrementReference();

        references.clear();
    }

    void AV1DecoderFrame::UpdateReferenceList()
    {
        if (header->frame_type == KEY_FRAME)
            return;

        for (Ipp8u i = 0; i < INTER_REFS; ++i)
        {
            Ipp32s refIdx = header->ref_frame_idx[i];
            AddReferenceFrame(frame_dpb[refIdx]);
        }
    }

    void AV1DecoderFrame::OnDecodingCompleted()
    {
        DecrementReference();
        FreeReferenceFrames();
        decoded = true;
    }

    Ipp32u AV1DecoderFrame::GetUpscaledWidth() const
    {
#if UMC_AV1_DECODER_REV >= 5000
        return header->UpscaledWidth;
#else
        return header->FrameWidth;
#endif
    }
    Ipp32u AV1DecoderFrame::GetHeight() const
    {
        return header->FrameHeight;
    }

}

#endif //UMC_ENABLE_AV1_VIDEO_DECODER
