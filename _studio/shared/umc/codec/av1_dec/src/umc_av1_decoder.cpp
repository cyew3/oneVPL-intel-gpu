// Copyright (c) 2017-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
        , sequence_header(nullptr)
        , counter(0)
        , prev_frame(nullptr)
        , curr_frame(nullptr)
    {
    }

    AV1Decoder::~AV1Decoder()
    {
        std::for_each(std::begin(dpb), std::end(dpb),
            std::default_delete<AV1DecoderFrame>()
        );
    }

    inline bool CheckOBUType(AV1_OBU_TYPE type)
    {
        switch (type)
        {
        case OBU_TEMPORAL_DELIMITER:
        case OBU_SEQUENCE_HEADER:
        case OBU_FRAME_HEADER:
        case OBU_REDUNDANT_FRAME_HEADER:
        case OBU_FRAME:
        case OBU_TILE_GROUP:
        case OBU_METADATA:
        case OBU_PADDING:
            return true;
        default:
            return false;
        }
    }

    UMC::Status AV1Decoder::DecodeHeader(UMC::MediaData* in, UMC_AV1_DECODER::AV1DecoderParams& par)
    {
        if (!in)
            return UMC::UMC_ERR_NULL_PTR;

        SequenceHeader sh = {};
        FrameHeader fh = {};

        while (in->GetDataSize() >= MINIMAL_DATA_SIZE)
        {
            try
            {
                const auto src = reinterpret_cast<uint8_t*>(in->GetDataPointer());
                AV1Bitstream bs(src, uint32_t(in->GetDataSize()));

                OBUInfo obuInfo;
                bs.ReadOBUInfo(obuInfo);
                VM_ASSERT(CheckOBUType(obuInfo.header.obu_type)); // TODO: [clean up] Need to remove assert once decoder code is stabilized

                if (obuInfo.header.obu_type == OBU_SEQUENCE_HEADER)
                {
                    bs.ReadSequenceHeader(sh);

                    in->MoveDataPointer(static_cast<int32_t>(obuInfo.size));

                    if (FillVideoParam(sh, par) == UMC::UMC_OK)
                        return UMC::UMC_OK;
                }

                in->MoveDataPointer(static_cast<int32_t>(obuInfo.size));
            }
            catch (av1_exception const& e)
            {
                return e.GetStatus();
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
        VM_ASSERT(prevFrame);

        DPBType updatedFrameDPB;

        DPBType const& prevFrameDPB = prevFrame->frame_dpb;
        if (prevFrameDPB.empty())
            updatedFrameDPB.resize(NUM_REF_FRAMES);
        else
            updatedFrameDPB = prevFrameDPB;

        const FrameHeader& fh = prevFrame->GetFrameHeader();

        for (uint8_t i = 0; i < NUM_REF_FRAMES; i++)
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

    static void GetTileLocation(
        AV1Bitstream* bs,
        FrameHeader const& fh,
        TileGroupInfo const& tgInfo,
        uint32_t idxInTG,
        size_t OBUSize,
        size_t OBUOffset,
        TileLocation& loc)
    {
        // calculate tile row and column
        const uint32_t idxInFrame = tgInfo.startTileIdx + idxInTG;
        loc.row = idxInFrame / fh.tile_info.TileCols;
        loc.col = idxInFrame - loc.row * fh.tile_info.TileCols;

        size_t tileOffsetInTG = bs->BytesDecoded();

        if (idxInTG == tgInfo.numTiles - 1)
            loc.size = OBUSize - tileOffsetInTG;  // tile is last in tile group - no explicit size signaling
        else
        {
            tileOffsetInTG += fh.tile_info.TileSizeBytes;

            // read tile size
            size_t reportedSize = 0;
            size_t actualSize = 0;
            bs->ReadTile(fh, reportedSize, actualSize);
            if (actualSize != reportedSize)
            {
                // before parsing tiles we check that tile_group_obu() is complete (bitstream has enough bytes to hold whole OBU)
                // but here we encountered incomplete tile inside this tile_group_obu() which means tile size corruption
                // [maybe] later check for complete tile_group_obu() will be removed, and thus incomplete tile will be possible
                VM_ASSERT("Tile size corruption: Incomplete tile encountered inside complete tile_group_obu()!");
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            loc.size = reportedSize;
        }

        loc.offset = OBUOffset + tileOffsetInTG;
    }

    inline bool CheckTileGroup(uint32_t prevNumTiles, FrameHeader const& fh, TileGroupInfo const& tgInfo)
    {
        if (prevNumTiles + tgInfo.numTiles > NumTiles(fh.tile_info))
            return false;

        if (tgInfo.numTiles == 0)
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

    static void ReadTileGroup(TileLayout& layout, AV1Bitstream& bs, FrameHeader const& fh, size_t obuOffset, size_t obuSize)
    {
        TileGroupInfo tgInfo = {};
        bs.ReadTileGroupHeader(fh, tgInfo);

        if (!CheckTileGroup(static_cast<uint32_t>(layout.size()), fh, tgInfo))
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

        uint32_t idxInLayout = static_cast<uint32_t>(layout.size());

        layout.resize(layout.size() + tgInfo.numTiles,
            { tgInfo.startTileIdx, tgInfo.endTileIdx });

        for (int idxInTG = 0; idxInLayout < layout.size(); idxInLayout++, idxInTG++)
        {
            TileLocation& loc = layout[idxInLayout];
            GetTileLocation(&bs, fh, tgInfo, idxInTG, obuSize, obuOffset, loc);
        }
    }

    inline bool NextFrameDetected(AV1_OBU_TYPE obuType)
    {
        switch (obuType)
        {
        case OBU_REDUNDANT_FRAME_HEADER:
        case OBU_TILE_GROUP:
            return false;
        default:
            return true;
        }
    }

    inline bool GotFullFrame(AV1DecoderFrame const* curr_frame, FrameHeader const& fh, TileLayout const& layout)
    {
        const unsigned numMissingTiles = curr_frame ? GetNumMissingTiles(*curr_frame) : NumTiles(fh.tile_info);
        if (layout.size() == numMissingTiles) // current tile group_obu() contains all missing tiles
            return true;
        else
            return false;
    }

    inline bool HaveTilesToSubmit(AV1DecoderFrame const* curr_frame, TileLayout const& layout)
    {
        if (!layout.empty() ||
            curr_frame && BytesReadyForSubmission(curr_frame->GetTileSets()))
            return true;

        return false;
    }

    inline bool AllocComplete(AV1DecoderFrame const& frame)
    {
        return frame.GetFrameData(SURFACE_DISPLAY) &&
            frame.GetFrameData(SURFACE_RECON);
    }

    UMC::Status AV1Decoder::GetFrame(UMC::MediaData* in, UMC::MediaData*)
    {
        if (!in)
            return UMC::UMC_ERR_NULL_PTR;

        FrameHeader fh = {};

        DPBType updated_refs;
        FrameHeader const* prev_fh = 0;
        if (prev_frame && !curr_frame)
        {
            updated_refs = DPBUpdate(prev_frame);
            prev_fh = &(prev_frame->GetFrameHeader());
        }

        bool gotFullFrame = false;

        if (curr_frame && GetNumMissingTiles(*curr_frame) == 0)
        {
            /* this code is executed if and only if whole frame (all tiles) was already got from applicaiton during previous calls of DecodeFrameAsync
               but there were no sufficient surfaces to start decoding (e.g. to apply film_grain)
               in this case reading from bitstream must be skipped, and code should proceed to frame submission to the driver */

            VM_ASSERT(!AllocComplete(*curr_frame));
            gotFullFrame = true;
        }
        else
        {
            /*do bitstream parsing*/

            bool gotFrameHeader = false;
            uint32_t OBUOffset = 0;
            TileLayout layout;

            UMC::MediaData tmp = *in; // use local copy of [in] for OBU header parsing to not move data pointer in original [in] prematurely

            while (tmp.GetDataSize() >= MINIMAL_DATA_SIZE && gotFullFrame == false)
            {
                const auto src = reinterpret_cast<uint8_t*>(tmp.GetDataPointer());
                AV1Bitstream bs(src, uint32_t(tmp.GetDataSize()));

                OBUInfo obuInfo;
                bs.ReadOBUInfo(obuInfo);
                const AV1_OBU_TYPE obuType = obuInfo.header.obu_type;
                VM_ASSERT(CheckOBUType(obuType)); // TODO: [clean up] Need to remove assert once decoder code is stabilized

                if (tmp.GetDataSize() < obuInfo.size) // not enough data left in the buffer to hold full OBU unit
                    break;

                if (curr_frame && NextFrameDetected(obuType))
                {
                    VM_ASSERT(!"Current frame was interrupted unexpectedly!");
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                    // TODO: [robust] add support for cases when series of tile_group_obu() interrupted by other OBU type before end of frame was reached
                }

                switch (obuType)
                {
                case OBU_SEQUENCE_HEADER:
                    if (!sequence_header.get())
                        sequence_header.reset(new SequenceHeader);
                    *sequence_header = SequenceHeader{};
                    bs.ReadSequenceHeader(*sequence_header);
                    break;
                case OBU_FRAME_HEADER:
                case OBU_REDUNDANT_FRAME_HEADER:
                case OBU_FRAME:
                    if (!sequence_header.get())
                        break; // bypass frame header if there is no active seq header
                    bs.ReadUncompressedHeader(fh, *sequence_header, prev_fh, updated_refs, obuInfo.header);
                    gotFrameHeader = true;
                    if (obuType != OBU_FRAME)
                        break;
                    bs.ReadByteAlignment();
                case OBU_TILE_GROUP:
                    FrameHeader const* pFH = nullptr;
                    if (curr_frame)
                        pFH = &(curr_frame->GetFrameHeader());
                    else if (gotFrameHeader)
                        pFH = &fh;

                    if (pFH) // bypass tile group if there is no respective frame header
                    {
                        ReadTileGroup(layout, bs, *pFH, OBUOffset, obuInfo.size);
                        gotFullFrame = GotFullFrame(curr_frame, *pFH, layout);
                        break;
                    }
                }

                OBUOffset += static_cast<uint32_t>(obuInfo.size);
                tmp.MoveDataPointer(static_cast<int32_t>(obuInfo.size));
            }

            if (!HaveTilesToSubmit(curr_frame, layout))
                return UMC::UMC_ERR_NOT_ENOUGH_DATA;

            if (curr_frame == 0)
            {
                UMC::Status umcRes = StartFrame(fh, updated_refs);
                if (umcRes != UMC::UMC_OK)
                    return umcRes;
            }

            if (!layout.empty())
            {
                curr_frame->AddTileSet(in, layout);
                in->MoveDataPointer(OBUOffset);
            }
        }

        VM_ASSERT(curr_frame);

        UMC::Status umcRes = UMC::UMC_OK;

        bool firstSubmission = false;

        if (!AllocComplete(*curr_frame))
        {
            AddFrameData(*curr_frame);

            if (!AllocComplete(*curr_frame))
                return UMC::UMC_OK;

            firstSubmission = true;
        }

        umcRes = SubmitTiles(*curr_frame, firstSubmission);
        if (umcRes != UMC::UMC_OK)
            return umcRes;

        if (gotFullFrame)
        {
            prev_frame = curr_frame;
            curr_frame = nullptr;
        }
        else
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;

        return umcRes;
    }

    AV1DecoderFrame* AV1Decoder::FindFrameByMemID(UMC::FrameMemID id)
    {
        return FindFrame(
            [id](AV1DecoderFrame const* f)
            { return f->GetMemID() == id; }
        );
    }

    AV1DecoderFrame* AV1Decoder::FindFrameByDispID(uint32_t id)
    {
        return FindFrame(
            [id](AV1DecoderFrame const* f)
            { return f->GetFrameHeader().display_frame_id == id; }
        );
    }

    AV1DecoderFrame* AV1Decoder::GetFrameToDisplay()
    {
        std::unique_lock<std::mutex> l(guard);

        // When all frames in DPB are displayed, and app keeps sending mfxBitstream with zero DataLength
        // This function will be called and will always return dpb.front(), so app will get MFX_ERR_NONE in infinite loop
        // And will never get MFX_ERR_MORE_DATA (as it should be per design/API)
        // TODO: [Rev0.85] Fix this when will modify this function to add proper support of reordering.

        auto i = std::min_element(std::begin(dpb), std::end(dpb),
            [](AV1DecoderFrame const* f1, AV1DecoderFrame const* f2)
            {
                FrameHeader const& h1 = f1->GetFrameHeader(); FrameHeader const& h2 = f2->GetFrameHeader();
                uint32_t const id1 = h1.show_frame && !f1->Displayed() ? h1.display_frame_id : (std::numeric_limits<uint32_t>::max)();
                uint32_t const id2 = h2.show_frame && !f2->Displayed() ? h2.display_frame_id : (std::numeric_limits<uint32_t>::max)();

                return  id1 < id2;
            }
        );

        if (i == std::end(dpb))
            return nullptr;

        AV1DecoderFrame* frame = *i;
        return
            frame->GetFrameHeader().show_frame && AllocComplete(*frame) ? frame : nullptr;
    }

    UMC::Status AV1Decoder::FillVideoParam(SequenceHeader const& sh, UMC_AV1_DECODER::AV1DecoderParams& par)
    {
        par.info.stream_type = UMC::AV1_VIDEO;
        par.info.profile = sh.seq_profile;

        par.info.clip_info = { int32_t(sh.max_frame_width), int32_t(sh.max_frame_height) };
        par.info.disp_clip_info = par.info.clip_info;

        if (!sh.color_config.subsampling_x && !sh.color_config.subsampling_y)
            par.info.color_format = UMC::YUV444;
        else if (sh.color_config.subsampling_x && !sh.color_config.subsampling_y)
            par.info.color_format = UMC::YUY2;
        else if (sh.color_config.subsampling_x && sh.color_config.subsampling_y)
            par.info.color_format = UMC::NV12;

        if (sh.color_config.BitDepth == 8 && par.info.color_format == UMC::YUV444)
            par.info.color_format = UMC::AYUV;
        if (sh.color_config.BitDepth == 10)
        {
            switch (par.info.color_format)
            {
            case UMC::NV12:   par.info.color_format = UMC::P010; break;
            case UMC::YUY2:   par.info.color_format = UMC::Y210; break;
            case UMC::YUV444: par.info.color_format = UMC::Y410; break;

            default:
                VM_ASSERT(!"Unknown subsampling");
                return UMC::UMC_ERR_UNSUPPORTED;
            }
        }
        else if (sh.color_config.BitDepth == 12)
        {
            switch (par.info.color_format)
            {
            case UMC::NV12:   par.info.color_format = UMC::P016; break;
            case UMC::YUY2:   par.info.color_format = UMC::Y216; break;
            case UMC::YUV444: par.info.color_format = UMC::Y416; break;

            default:
                VM_ASSERT(!"Unknown subsampling");
                return UMC::UMC_ERR_UNSUPPORTED;
            }
        }

        par.info.interlace_type = UMC::PROGRESSIVE;
        par.info.aspect_ratio_width = par.info.aspect_ratio_height = 1;

        par.lFlags = 0;

        par.film_grain = sh.film_grain_param_present;

        return UMC::UMC_OK;
    }

    void AV1Decoder::SetDPBSize(uint32_t size)
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
        if (fh.show_existing_frame)
        {
            // TODO: [Rev0.85] Implement proper processing of reordering
            throw av1_exception(UMC::UMC_ERR_NOT_IMPLEMENTED);
        }

        CompleteDecodedFrames();
        AV1DecoderFrame* frame = GetFreeFrame();
        if (!frame)
            return nullptr;

        frame->Reset(&fh);

        // increase ref counter when we get empty frame from DPB
        frame->IncrementReference();

        return frame;
    }

    void AV1Decoder::AddFrameData(AV1DecoderFrame& frame)
    {
        FrameHeader const& fh = frame.GetFrameHeader();

        if (fh.show_existing_frame)
        {
            // TODO: [Rev0.85] Implement proper processing of reordering
            throw av1_exception(UMC::UMC_ERR_NOT_IMPLEMENTED);
        }

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
