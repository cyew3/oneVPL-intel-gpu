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

#ifndef __UMC_AV1_FRAME_H__
#define __UMC_AV1_FRAME_H__

#include "umc_frame_allocator.h"
#include "umc_av1_dec_defs.h"
#include "mfx_common_decode_int.h"

#include <memory>

namespace UMC_AV1_DECODER
{
    struct SequenceHeader;
    struct FrameHeader;

    struct TileLocation
    {
        Ipp32u startIdx; // index of 1st tile in current tile group
        Ipp32u endIdx; // index of last tile in current tile group
        size_t offset; // offset in the buffer
        size_t size; // size of the tile
        Ipp32u row; // row in tile grid
        Ipp32u col; // column in tile grid
    };

    typedef std::vector<TileLocation> TileLayout;

    class TileSet
    {
    public:

        TileSet() {};
        TileSet(UMC::MediaData*, TileLayout const&);
        ~TileSet() {};

        size_t Submit(Ipp8u*, size_t, size_t, TileLayout&);
        Ipp32u GetTileCount() const
        { return static_cast<Ipp32u>(layout.size()); }
        size_t GetSize() const
        { return source.GetDataSize(); }

    private:
        UMC::MediaData source;
        TileLayout layout;
        bool submitted = false;
    };

#if UMC_AV1_DECODER_REV >= 5000
    inline Ipp32u CalcTilesInTileSets(std::vector<TileSet> const& tileSets)
    {
        Ipp32u numTiles = 0;
        for (auto& tileSet : tileSets)
            numTiles += tileSet.GetTileCount();

        return numTiles;
    }

    inline size_t CalcSizeOfTileSets(std::vector<TileSet> const& tileSets)
    {
        size_t size = 0;
        for (auto& tileSet : tileSets)
            size += tileSet.GetSize();

        return size;
    }
#endif // UMC_AV1_DECODER_REV >= 5000

    class AV1DecoderFrame : public RefCounter
    {

    public:

        AV1DecoderFrame();
        ~AV1DecoderFrame();

        void Reset();
        void Reset(FrameHeader const*);
        void Allocate(UMC::FrameData const*);

        void AddSource(UMC::MediaData*);

        UMC::MediaData* GetSource()
        { return source.get(); }

        void AddTileSet(UMC::MediaData* in, TileLayout const& layout);

        std::vector<TileSet>& GetTileSets()
        { return tile_sets; }

        Ipp32s GetError() const
        { return error; }

        void SetError(Ipp32s e)
        { error = e; }

        void AddError(Ipp32s e)
        { error |= e; }

        void SetSeqHeader(SequenceHeader const&);

        SequenceHeader const& GetSeqHeader() const
        { return *seq_header; }

        FrameHeader& GetFrameHeader()
        { return *header; }
        FrameHeader const& GetFrameHeader() const
        { return *header; }

        bool Empty() const;
        bool Decoded() const;

        bool Displayed() const
        { return displayed; }
        void Displayed(bool d)
        { displayed = d; }

        bool Outputted() const
        { return outputted; }
        void Outputted(bool o)
        { outputted = o; }

        bool DecodingStarted() const
        { return decoding_started; }
        void StartDecoding()
        { decoding_started = true; }

        bool DecodingCompleted() const
        { return decoding_completed; }
        void CompleteDecoding()
        { decoding_completed = true; }

        UMC::FrameData* GetFrameData()
        { return data.get(); }
        UMC::FrameData const* GetFrameData() const
        { return data.get(); }

        UMC::FrameMemID GetMemID() const;

        void AddReferenceFrame(AV1DecoderFrame* frm);
        void FreeReferenceFrames();
        void UpdateReferenceList();
        void OnDecodingCompleted();

        void SetRefValid(bool valid)
        { ref_valid = valid; }
        bool RefValid() const
        { return ref_valid; };

        Ipp32u GetUpscaledWidth() const;
        Ipp32u GetHeight() const;

    public:

        Ipp32s           UID;
        DPBType          frame_dpb;

    protected:
        virtual void Free()
        {
            Reset();
        }

    private:

        Ipp16u                            locked;
        bool                              outputted; // set in [application thread] when frame is mapped to respective output mfxFrameSurface
        bool                              displayed; // set in [scheduler thread] when frame decoding is finished and
                                                     // respective mfxFrameSurface prepared for output to application
        bool                              decoded;   // set in [application thread] to signal that frame is completed and respective reference counter decremented
                                                     // after it frame still may remain in [AV1Decoder::dpb], but only as reference

        bool                              decoding_started;   // set in [application thread] right after frame submission to the driver
        bool                              decoding_completed; // set in [scheduler thread] after getting driver status report for the frame


        std::unique_ptr<UMC::FrameData>   data;
        std::unique_ptr<UMC::MediaData>   source;

        std::vector<TileSet>              tile_sets;

        Ipp32s                            error;

        std::unique_ptr<SequenceHeader>   seq_header;
        std::unique_ptr<FrameHeader>      header;

        DPBType                           references;

        bool                              ref_valid;
    };

} // end namespace UMC_VP9_DECODER

#endif // __UMC_AV1_FRAME_H__
#endif // UMC_ENABLE_AV1_VIDEO_DECODER
