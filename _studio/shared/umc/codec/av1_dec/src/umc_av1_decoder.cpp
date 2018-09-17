//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_frame_data.h"

#include "umc_av1_decoder.h"
#include "umc_av1_utils.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_frame.h"

#include <algorithm>

namespace UMC_AV1_DECODER
{
    AV1Decoder::AV1Decoder()
        : allocator(nullptr)
#if UMC_AV1_DECODER_REV >= 5000
        , sequence_header(nullptr)
#else
        , sequence_header(new SequenceHeader{})
#endif
        , counter(0)
        , prev_frame(nullptr)
#if UMC_AV1_DECODER_REV >= 5000
        , curr_frame(nullptr)
#endif
    {
    }

    AV1Decoder::~AV1Decoder()
    {
        std::for_each(std::begin(dpb), std::end(dpb),
            std::default_delete<AV1DecoderFrame>()
        );
    }

#if UMC_AV1_DECODER_REV >= 5000
    inline bool CheckOBUType(AV1_OBU_TYPE type)
    {
        switch (type)
        {
        case OBU_TEMPORAL_DELIMITER:
        case OBU_SEQUENCE_HEADER:
        case OBU_FRAME_HEADER:
        case OBU_FRAME:
        case OBU_TILE_GROUP:
        case OBU_METADATA:
        case OBU_PADDING:
            return true;
        default:
            return false;
        }
    }
#endif // UMC_AV1_DECODER_REV >= 5000

    UMC::Status AV1Decoder::DecodeHeader(UMC::MediaData* in, UMC::VideoDecoderParams* par)
    {
        if (!in)
            return UMC::UMC_ERR_NULL_PTR;

#if UMC_AV1_DECODER_REV >= 5000
        bool gotSeqHeader = false;

        SequenceHeader sh = {};
        FrameHeader fh = {};

        while (in->GetDataSize() >= MINIMAL_DATA_SIZE)
        {
            try
            {
                const auto src = reinterpret_cast<Ipp8u*>(in->GetDataPointer());
                AV1Bitstream bs(src, Ipp32u(in->GetDataSize()));

                OBUInfo info;
                bs.ReadOBUHeader(&info);
                VM_ASSERT(CheckOBUType(info.header.obu_type)); // TODO: [clean up] Need to remove assert once decoder code is stabilized
                size_t OBUTotalSize = static_cast<Ipp32s>(info.size + info.sizeFieldLength);

                switch (info.header.obu_type)
                {
                case OBU_SEQUENCE_HEADER:
                    bs.GetSequenceHeader(&sh);
                    gotSeqHeader = true; // activate seq header
                    break;
                case OBU_FRAME_HEADER:
                case OBU_FRAME:
                    if (!gotSeqHeader)
                        break; // bypass frame header if there is no active seq header
                    // let's read frame header to check compatibility with sequence header
                    bs.GetFrameHeaderPart1(&fh, &sh);
                    in->MoveDataPointer(static_cast<Ipp32s>(OBUTotalSize));

                    if (FillVideoParam(sh, par) == UMC::UMC_OK)
                        return UMC::UMC_OK;

                    break;
                }

                in->MoveDataPointer(static_cast<Ipp32s>(OBUTotalSize));
            }
            catch (av1_exception const& e)
            {
                return e.GetStatus();
            }
        }
#else
        auto src = reinterpret_cast<Ipp8u*>(in->GetDataPointer());
        AV1Bitstream bs(src, Ipp32u(in->GetDataSize()));
        while (in->GetDataSize() >= MINIMAL_DATA_SIZE)
        {
            try
            {
                SequenceHeader sh = {};
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
#endif

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
            if ((fh.refresh_frame_flags >> i) & 1)
            {
                if (!prevFrameDPB.empty() && prevFrameDPB[i])
                    prevFrameDPB[i]->DecrementReference();

                updatedFrameDPB[i] = const_cast<AV1DecoderFrame*>(prevFrame);
                prevFrame->IncrementReference();
            }
        }

        return updatedFrameDPB;
    }

#if UMC_AV1_DECODER_REV >= 5000
    static void GetTileLocation(
        AV1Bitstream* bs,
        FrameHeader const* fh,
        TileGroupInfo const* tgInfo,
        Ipp32u idxInTG,
        size_t OBUSize,
        size_t OBUOffset,
        TileLocation* loc)
    {
        // calculate tile row and column
        const Ipp32u idxInFrame = tgInfo->startTileIdx + idxInTG;
        loc->row = idxInFrame / fh->TileCols;
        loc->col = idxInFrame - loc->row * fh->TileCols;

        size_t tileOffsetInTG = bs->BytesDecoded();

        if (idxInTG == tgInfo->numTiles - 1)
            loc->size = OBUSize - tileOffsetInTG;  // tile is last in tile group - no explicit size signaling
        else
        {
            tileOffsetInTG += fh->TileSizeBytes;

            // read tile size
            size_t reportedSize;
            size_t actualSize;
            bs->ReadTile(fh, reportedSize, actualSize);
            if (actualSize != reportedSize)
            {
                // before parsing tiles we check that tile_group_obu() is complete (bitstream has enough bytes to hold whole OBU)
                // but here we encountered incomplete tile inside this tile_group_obu() which means tile size corruption
                // later check for complete tile_group_obu() will be removed, and thus incomplete tile will be possible
                VM_ASSERT("Tile size corruption: Incomplete tile encountered inside complete tile_group_obu()!");
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            loc->size = reportedSize;
        }

        loc->offset = OBUOffset + tileOffsetInTG;
    }

    inline bool CheckTileGroup(Ipp32u prevNumTiles, FrameHeader const* fh, TileGroupInfo const* tgInfo)
    {
        if (prevNumTiles + tgInfo->numTiles > NumTiles(*fh))
            return false;

        if (tgInfo->numTiles == 0)
            return false;

        return true;
    }

    UMC::Status AV1Decoder::StartFrame(FrameHeader const& fh, DPBType const& frameDPB)
    {
        curr_frame = GetFrameBuffer(fh);

        if (!curr_frame)
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

        curr_frame->SetSeqHeader(*sequence_header.get());

        if (fh.refresh_frame_flags)
            curr_frame->SetRefValid(true);

        curr_frame->frame_dpb = frameDPB;
        curr_frame->UpdateReferenceList();

        curr_frame->StartDecoding();

        return UMC::UMC_OK;
    }

    UMC::Status AV1Decoder::GetFrame(UMC::MediaData* in, UMC::MediaData*)
    {
        FrameHeader fh = {};

        DPBType updated_refs;
        FrameHeader const* prev_fh = 0;
        if (prev_frame && !curr_frame)
        {
            updated_refs = DPBUpdate(prev_frame);
            prev_fh = &(prev_frame->GetFrameHeader());
        }

        bool gotFrameHeader = false;
        bool gotTileGroup = false;
        bool gotFullFrame = false;

        UMC::MediaData tmp = *in; // use local copy of [in] for OBU header parsing to not move data pointer in original [in] prematurely
        Ipp32u OBUOffset = 0;
        TileLayout layout;
        Ipp32u idxInLayout = 0;

        while (tmp.GetDataSize() >= MINIMAL_DATA_SIZE && gotFullFrame == false)
        {
            const auto src = reinterpret_cast<Ipp8u*>(tmp.GetDataPointer());
            AV1Bitstream bs(src, Ipp32u(tmp.GetDataSize()));

            OBUInfo info;
            bs.ReadOBUHeader(&info);
            VM_ASSERT(CheckOBUType(info.header.obu_type)); // TODO: [clean up] Need to remove assert once decoder code is stabilized
            const size_t OBUTotalSize = static_cast<Ipp32s>(info.size + info.sizeFieldLength);

            if (tmp.GetDataSize() < OBUTotalSize)
                return UMC::UMC_ERR_NOT_ENOUGH_DATA; // not enough data in the buffer to hold full OBU unit
                                                     // we return error because there is no support for chopped tile_group_obu() submission so far
                                                     // TODO: [Global] Add support for submission of incomplete tile group OBUs

            if (curr_frame && info.header.obu_type != OBU_TILE_GROUP)
            {
                VM_ASSERT("Series of tile_group_obu() was interrupted unexpectedly!");
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                // TODO: [robust] add support for cases when series of tile_group_obu() interrupted by other OBU type before end of frame was reached
            }

            switch (info.header.obu_type)
            {
            case OBU_SEQUENCE_HEADER:
                if (!sequence_header.get())
                    sequence_header.reset(new SequenceHeader);
                bs.GetSequenceHeader(sequence_header.get());
                break;
            case OBU_FRAME_HEADER:
            case OBU_FRAME:
                if (!sequence_header.get())
                    break; // bypass frame header if there is no active seq header
                bs.GetFrameHeaderPart1(&fh, sequence_header.get());
                bs.GetFrameHeaderFull(&fh, sequence_header.get(), prev_fh, updated_refs);
                gotFrameHeader = true;
                if (info.header.obu_type == OBU_FRAME)
                {
                    // TODO: [Rev0.5] Add support for case when tile_group_obu() is transmitted inside frame_obu()
                    VM_ASSERT("No support for frame_obu()!");
                    throw av1_exception(UMC::UMC_ERR_NOT_IMPLEMENTED);
                }
                break;
            case OBU_TILE_GROUP:
                FrameHeader const* pFH = nullptr;
                if (curr_frame)
                    pFH = &(curr_frame->GetFrameHeader());
                else if (gotFrameHeader)
                    pFH = &fh;

                if (pFH) // bypass tile group if there is no respective frame header
                {
                    TileGroupInfo tgInfo = {};
                    bs.ReadTileGroupHeader(pFH, &tgInfo);

                    if (!CheckTileGroup(static_cast<Ipp32u>(layout.size()), pFH, &tgInfo))
                        throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

                    layout.resize(layout.size() + tgInfo.numTiles,
                        { tgInfo.startTileIdx, tgInfo.endTileIdx });

                    Ipp32u idxInTG = 0;
                    for (; idxInLayout < layout.size(); idxInLayout++, idxInTG++)
                    {
                        TileLocation& loc = layout[idxInLayout];
                        GetTileLocation(&bs, pFH, &tgInfo, idxInTG, OBUTotalSize, OBUOffset, &loc);
                    }

                    gotTileGroup = true;

                    const Ipp32u numTilesAccumulated = static_cast<Ipp32u>(layout.size()) +
                        (curr_frame ? CalcTilesInTileSets(curr_frame->GetTileSets()) : 0);
                    if (numTilesAccumulated == NumTiles(*pFH))
                        gotFullFrame = true;

                    break;
                }
            }

            OBUOffset += static_cast<Ipp32u>(OBUTotalSize);
            tmp.MoveDataPointer(static_cast<Ipp32s>(OBUTotalSize));
        }

        if (!gotTileGroup)
            return UMC::UMC_ERR_NOT_ENOUGH_DATA; // no tile_group_obu() or incomplete tile_group_obu (no support for chopped tile_group_obu() submission so far)
                                                 // TODO: [Global] Add support for submission of incomplete tile group OBUs

        bool firstSubmission = false;

        if (curr_frame == 0)
        {
            UMC::Status umcRes = StartFrame(fh, updated_refs);
            if (umcRes != UMC::UMC_OK)
                return umcRes;
            firstSubmission = true;
        }

        curr_frame->AddTileSet(in, layout);
        in->MoveDataPointer(OBUOffset);

        UMC::Status umcRes = SubmitTiles(curr_frame, firstSubmission);

        if (gotFullFrame)
        {
            prev_frame = curr_frame;
            curr_frame = nullptr;
        }

        return umcRes;
    }
#else // UMC_AV1_DECODER_REV >= 5000
UMC::Status AV1Decoder::GetFrame(UMC::MediaData* in, UMC::MediaData*)
{
    DPBType updated_refs;
    FrameHeader const* prev_fh = 0;
    if (prev_frame)
    {
        updated_refs = DPBUpdate(prev_frame);
        prev_fh = &(prev_frame->GetFrameHeader());
    }

    Ipp32u const size = Ipp32u(in->GetDataSize());
    Ipp8u* src = reinterpret_cast<Ipp8u*>(in->GetDataPointer());
    if (size < MINIMAL_DATA_SIZE)
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;

    AV1Bitstream bs(src, size);

    FrameHeader fh = {};

    bs.GetFrameHeaderPart1(&fh, sequence_header.get());
    bs.GetFrameHeaderFull(&fh, sequence_header.get(), prev_fh, updated_refs);

    AV1DecoderFrame* frame = GetFrameBuffer(fh);
    if (!frame)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    frame->SetSeqHeader(*sequence_header.get());

    frame->AddSource(in);
    in->MoveDataPointer(size);

    frame->frame_dpb = updated_refs;
    frame->UpdateReferenceList();
    prev_frame = frame;

    UMC::Status umcRes = CompleteFrame(frame);

    frame->StartDecoding();

    return umcRes;
}
#endif // UMC_AV1_DECODER_REV >= 5000

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
                Ipp32u const id1 = h1.show_frame && !f1->Displayed() ? h1.display_frame_id : (std::numeric_limits<Ipp32u>::max)();
                Ipp32u const id2 = h2.show_frame && !f2->Displayed() ? h2.display_frame_id : (std::numeric_limits<Ipp32u>::max)();

                return  id1 < id2;
            }
        );

        if (i == std::end(dpb))
            return nullptr;

        AV1DecoderFrame* frame = *i;
        return
            frame->GetFrameHeader().show_frame ? frame : nullptr;
    }

#if UMC_AV1_DECODER_REV >= 5000
    UMC::Status AV1Decoder::FillVideoParam(SequenceHeader const& sh, UMC::VideoDecoderParams* par)
    {
        VM_ASSERT(par);

        par->info.stream_type = UMC::AV1_VIDEO;
        par->info.profile = sh.seq_profile;

        par->info.clip_info = { Ipp32s(sh.max_frame_width), Ipp32s(sh.max_frame_height) };
        par->info.disp_clip_info = par->info.clip_info;

        if (!sh.subsampling_x && !sh.subsampling_y)
            par->info.color_format = UMC::YUV444;
        else if (sh.subsampling_x && !sh.subsampling_y)
            par->info.color_format = UMC::YUY2;
        else if (sh.subsampling_x && sh.subsampling_y)
            par->info.color_format = UMC::NV12;

        if (sh.BitDepth == 8 && par->info.color_format == UMC::YUV444)
            par->info.color_format = UMC::AYUV;
        if (sh.BitDepth == 10)
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
        else if (sh.BitDepth == 12)
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
#else
    UMC::Status AV1Decoder::FillVideoParam(FrameHeader const& fh, UMC::VideoDecoderParams* par)
    {
        VM_ASSERT(par);

        par->info.stream_type = UMC::AV1_VIDEO;
        par->info.profile     = fh.profile;

        par->info.clip_info = { Ipp32s(fh.FrameWidth), Ipp32s(fh.FrameHeight) };
        par->info.disp_clip_info = par->info.clip_info;

        if (!fh.subsampling_x && !fh.subsampling_y)
            par->info.color_format = UMC::YUV444;
        else if (fh.subsampling_x && !fh.subsampling_y)
            par->info.color_format = UMC::YUY2;
        else if (fh.subsampling_x && fh.subsampling_y)
            par->info.color_format = UMC::NV12;

        if (fh.BitDepth == 10)
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
        else if (fh.BitDepth == 12)
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
#endif

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
            if (frame->Outputted() && frame->Displayed() && !frame->Decoded())
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
