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

#include "vm_debug.h"

#include "umc_structures.h"
#include "umc_av1_bitstream_utils.h"
#include "umc_av1_utils.h"
#include "umc_av1_frame.h"

//#define AV1D_LOGGING
#if defined(AV1D_LOGGING) && (defined(WIN32) || defined(WIN64))
#define AV1D_LOG(string, ...) do { printf("(AV1D log) %s ",__FUNCTION__); printf(string, ##__VA_ARGS__); printf("\n"); fflush(0); } \
                              while (0)
#else
#define AV1D_LOG(string, ...)
#endif

namespace UMC_AV1_DECODER
{
    inline
    bool av1_sync_code(AV1Bitstream* bs)
    {
        return
            bs->GetBits(8) == SYNC_CODE_0 &&
            bs->GetBits(8) == SYNC_CODE_1 &&
            bs->GetBits(8) == SYNC_CODE_2
            ;
    }

    inline
    Ipp32u av1_profile(AV1Bitstream* bs)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        Ipp32u profile = bs->GetBit();
        profile |= bs->GetBit() << 1;
        if (profile > 2)
            profile += bs->GetBit();

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
        return profile;
    }

#if UMC_AV1_DECODER_REV >= 5000
    inline
    void av1_timing_info(AV1Bitstream* bs, SequenceHeader* sh)
    {
        sh->timing_info_present = bs->GetBit();  // timing info present flag

        if (sh->timing_info_present)
        {
            // not implemented so far
            throw av1_exception(UMC::UMC_ERR_NOT_IMPLEMENTED);
        }
    }

    static void av1_bitdepth_colorspace_sampling(AV1Bitstream* bs, SequenceHeader* sh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        sh->bit_depth = bs->GetBit() ? 10 : 8;
        if (sh->profile >= 2 && sh->bit_depth != 8)
            sh->bit_depth = bs->GetBit() ? 12 : 10;

        sh->use_highbitdepth = sh->bit_depth > 8;

        sh->monochrome = sh->profile != 1 ? bs->GetBit() : 0;

        Ipp32u color_description_present_flag = bs->GetBit();
        if (color_description_present_flag)
        {
            sh->color_primaries = bs->GetBits(8);
            sh->transfer_characteristics = bs->GetBits(8);
            sh->matrix_coefficients = bs->GetBits(8);
        }
        else
        {
            sh->color_primaries = AOM_CICP_CP_UNSPECIFIED;
            sh->transfer_characteristics = AOM_CICP_TC_UNSPECIFIED;
            sh->matrix_coefficients = AOM_CICP_MC_UNSPECIFIED;
        }

        if (sh->monochrome)
        {
            sh->color_range = AOM_CR_FULL_RANGE;
            sh->subsampling_y = sh->subsampling_x = 1;
            sh->chroma_sample_position = AOM_CSP_UNKNOWN;
            sh->separate_uv_delta_q = 0;
            return;
        }

        if (sh->color_primaries == AOM_CICP_CP_BT_709 &&
            sh->transfer_characteristics == AOM_CICP_TC_SRGB &&
            sh->matrix_coefficients == AOM_CICP_MC_IDENTITY)
        {
            sh->subsampling_y = sh->subsampling_x = 0;
            if (!(sh->profile == 1 || (sh->profile == 2 && sh->bit_depth == 12)))
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        else
        {
            // [16,235] (including xvycc) vs [0,255] range
            sh->color_range = bs->GetBit();
            if (sh->profile == 0) // 420 only
                sh->subsampling_x = sh->subsampling_y = 1;
            else if (sh->profile == 1) // 444 only
                sh->subsampling_x = sh->subsampling_y = 0;
            else if (sh->profile == 2)
            {
                if (sh->bit_depth == 12)
                {
                    sh->subsampling_x = bs->GetBit();
                    if (sh->subsampling_x == 0)
                        sh->subsampling_y = 0;  // 444
                    else
                        sh->subsampling_y = bs->GetBit();  // 422 or 420
                }
                else
                {
                    // 422
                    sh->subsampling_x = 1;
                    sh->subsampling_y = 0;
                }
            }
            if (sh->subsampling_x == 1 && sh->subsampling_y == 1)
                sh->chroma_sample_position = bs->GetBits(2);
        }

        sh->separate_uv_delta_q = bs->GetBit();

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }
#else // UMC_AV1_DECODER_REV >= 5000
    static void av1_bitdepth_colorspace_sampling(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        if (fh->profile >= 2)
            fh->bitDepth = bs->GetBit() ? 12 : 10;
        else
            fh->bitDepth = 8;

        if (fh->frameType != KEY_FRAME && fh->intraOnly && fh->profile == 0)
        {
            fh->subsamplingY = fh->subsamplingX = 1;
            return;
        }

        Ipp32u const colorspace
            = bs->GetBits(3);

        if (colorspace != UMC_VP9_DECODER::SRGB)
        {
            bs->GetBit(); // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
            if (1 == fh->profile || 3 == fh->profile)
            {
                fh->subsamplingX = bs->GetBit();
                fh->subsamplingY = bs->GetBit();
                bs->GetBit(); // reserved bit
            }
            else
                fh->subsamplingY = fh->subsamplingX = 1;
    }
        else
        {
            if (1 == fh->profile || 3 == fh->profile)
            {
                fh->subsamplingX = 0;
                fh->subsamplingY = 0;
                bs->GetBit(); // reserved bit
            }
            else
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }
#endif // UMC_AV1_DECODER_REV >= 5000

#if UMC_AV1_DECODER_REV >= 5000
    static void av1_setup_superres(AV1Bitstream* bs, FrameHeader* fh)
    {
        fh->superresUpscaledWidth = fh->width;
        fh->superresUpscaledHeight = fh->height;
        Ipp32u& denom = fh->superresScaleDenominator;
        if (bs->GetBit())
        {
            denom = bs->GetBits(SUPERRES_SCALE_BITS);
            denom += SUPERRES_SCALE_DENOMINATOR_MIN;
            if (denom != SCALE_NUMERATOR)
                fh->width = (fh->width * SCALE_NUMERATOR + denom / 2) / (denom);
        }
        else
            fh->superresScaleDenominator = SCALE_NUMERATOR;
    }
#endif

    inline
    void av1_get_render_size(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
#if UMC_AV1_DECODER_REV >= 5000
        fh->displayWidth = fh->superresUpscaledWidth;
        fh->displayHeight = fh->superresUpscaledHeight;
#else // UMC_AV1_DECODER_REV >= 5000
        fh->displayWidth = fh->width;
        fh->displayHeight = fh->height;
#endif // UMC_AV1_DECODER_REV >= 5000

        if (bs->GetBit())
        {
            fh->displayWidth = bs->GetBits(16) + 1;
            fh->displayHeight = bs->GetBits(16) + 1;
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_get_frame_size(AV1Bitstream* bs, FrameHeader* fh, SequenceHeader const * sh = 0)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
#if UMC_AV1_DECODER_REV >= 5000
        if (fh->frameSizeOverrideFlag)
        {
            fh->width = bs->GetBits(16) + 1;
            fh->height = bs->GetBits(16) + 1;
            if (fh->width > sh->max_frame_width || fh->height > sh->max_frame_height)
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        else
        {
            fh->width = sh->max_frame_width;
            fh->height = sh->max_frame_height;
        }

        av1_setup_superres(bs, fh);
#else // UMC_AV1_DECODER_REV >= 5000
        sh;
        fh->width = bs->GetBits(16) + 1;
        fh->height = bs->GetBits(16) + 1;
#endif // UMC_AV1_DECODER_REV >= 5000
        av1_get_render_size(bs, fh);
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline void av1_get_frame_sizes_with_refs(AV1Bitstream* bs, FrameHeader* fh, DPBType const& frameDpb)
    {
        AV1D_LOG("[+]: %d", (mfxU32)BitsDecoded());
        bool bFound = false;
        for (Ipp8u i = 0; i < INTER_REFS; ++i)
        {
            if (bs->GetBit())
            {
                bFound = true;
                FrameHeader const& refHrd = frameDpb[fh->refFrameIdx[i]]->GetFrameHeader();
                fh->width = refHrd.width;
                fh->height = refHrd.height;
                // TODO: [Rev0.5] add getting of displayWidth/displayHeight
#if UMC_AV1_DECODER_REV >= 5000
                av1_setup_superres(bs, fh);
#endif
                break;
            }
        }

        if (!bFound)
        {
            fh->width = bs->GetBits(16) + 1;
            fh->height = bs->GetBits(16) + 1;
#if UMC_AV1_DECODER_REV >= 5000
            av1_setup_superres(bs, fh);
#endif
            av1_get_render_size(bs, fh);
        }
        AV1D_LOG("[-]: %d", (mfxU32)BitsDecoded());
    }


    inline
    void av1_get_sb_size(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        fh->sbSize = bs->GetBit() ? BLOCK_128X128 : BLOCK_64X64;
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_setup_loop_filter(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
#if UMC_AV1_DECODER_REV >= 5000
        if (fh->allowIntraBC && NO_FILTER_FOR_IBC)
            return;
#endif
        fh->lf.filterLevel[0] = bs->GetBits(6);
        fh->lf.filterLevel[1] = bs->GetBits(6);
#if UMC_AV1_DECODER_REV >= 5000
        if (fh->numPlanes > 1)
#endif
            if (fh->lf.filterLevel[0] || fh->lf.filterLevel[1])
            {
                fh->lf.filterLevelU = bs->GetBits(6);
                fh->lf.filterLevelV = bs->GetBits(6);
            }
        fh->lf.sharpnessLevel = bs->GetBits(3);

        fh->lf.modeRefDeltaUpdate = 0;

        fh->lf.modeRefDeltaEnabled = (Ipp8u)bs->GetBit();
        if (fh->lf.modeRefDeltaEnabled)
        {
            fh->lf.modeRefDeltaUpdate = (Ipp8u)bs->GetBit();

            if (fh->lf.modeRefDeltaUpdate)
            {
                Ipp8u bits = 6;
                for (Ipp32u i = 0; i < TOTAL_REFS; i++)
                {
                    if (bs->GetBit())
                    {
                        const Ipp8u nbits = sizeof(unsigned) * 8 - bits - 1;
                        const Ipp32u value = (unsigned)bs->GetBits(bits + 1) << nbits;
                        fh->lf.refDeltas[i] = (Ipp8s)(((Ipp32s)value) >> nbits);
                    }
                }

                for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
                {
                    if (bs->GetBit())
                    {
                        const Ipp8u nbits = sizeof(unsigned) * 8 - bits - 1;
                        const Ipp32u value = (unsigned)bs->GetBits(bits + 1) << nbits;
                        fh->lf.modeDeltas[i] = (Ipp8s)(((Ipp32s)value) >> nbits);
                    }
                }
            }
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_read_cdef(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        memset(fh->cdefStrength, 0, sizeof(fh->cdefStrength));
        memset(fh->cdefUVStrength, 0, sizeof(fh->cdefUVStrength));

#if UMC_AV1_DECODER_REV >= 5000
        if (fh->allowIntraBC && NO_FILTER_FOR_IBC)
            return;
#endif
        fh->cdefPriDamping = fh->cdefSecDamping = bs->GetBits(2) + 3;
        fh->cdefBits = bs->GetBits(2);
        mfxU32 nbCdefStrengths = 1 << fh->cdefBits;
        for (Ipp8u i = 0; i < nbCdefStrengths; i++) {
            fh->cdefStrength[i] = bs->GetBits(CDEF_STRENGTH_BITS);
            fh->cdefUVStrength[i] =
#if UMC_AV1_DECODER_REV >= 5000
                fh->numPlanes > 1 ?
#else
                fh->subsamplingX == fh->subsamplingY ?
#endif
                bs->GetBits(CDEF_STRENGTH_BITS) : 0;
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

#if UMC_AV1_DECODER_REV >= 5000
    inline
    Ipp8u av1_num_planes(SequenceHeader const * sh)
    {
        return sh->monochrome ? 1 : MAX_MB_PLANE;
    }
#endif

    inline
#if UMC_AV1_DECODER_REV >= 5000
    void av1_decode_restoration_mode(AV1Bitstream* bs, SequenceHeader const* sh, FrameHeader* fh)
#else
    void av1_decode_restoration_mode(AV1Bitstream* bs, FrameHeader* fh)
#endif
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        bool allNone = true;;
        bool chromaNone = true;
#if UMC_AV1_DECODER_REV >= 5000
        if (fh->allowIntraBC && NO_FILTER_FOR_IBC)
            return;
        Ipp32u numPlanes = fh->numPlanes;
#else
        Ipp32u numPlanes = MAX_MB_PLANE;
#endif
        for (Ipp8u p = 0; p < numPlanes; ++p)
        {
            RestorationInfo* rsi = &fh->rstInfo[p];
            if (bs->GetBit()) {
                rsi->frameRestorationType =
                    bs->GetBit() ? RESTORE_SGRPROJ : RESTORE_WIENER;
            }
            else
            {
                rsi->frameRestorationType =
                    bs->GetBit() ? RESTORE_SWITCHABLE : RESTORE_NONE;
            }
            if (rsi->frameRestorationType != RESTORE_NONE)
            {
                allNone = false;
                if (p != 0)
                    chromaNone = false;
            }
        }

#if UMC_AV1_DECODER_REV >= 5000
        const Ipp32s size = sh->sb_size == BLOCK_128X128 ? 128 : 64;
#else
        const Ipp32s size = RESTORATION_TILESIZE_MAX >> 2;
#endif

        if (!allNone)
        {
            for (Ipp8u p = 0; p < numPlanes; ++p)
                fh->rstInfo[p].restorationUnitSize = size;

            RestorationInfo* rsi = &fh->rstInfo[0];
#if UMC_AV1_DECODER_REV >= 5000
            if (size == 64)
#endif
                rsi->restorationUnitSize <<= bs->GetBit();
#if UMC_AV1_DECODER_REV >= 5000
            if (rsi->restorationUnitSize > 64) {
#else
            if (rsi->restorationUnitSize != size) {
#endif
                rsi->restorationUnitSize <<= bs->GetBit();
            }
        }
        else
        {
            for (Ipp8u p = 0; p < numPlanes; ++p)
#if UMC_AV1_DECODER_REV >= 5000
                fh->rstInfo[p].restorationUnitSize = RESTORATION_UNITSIZE_MAX;
#else
                fh->rstInfo[p].restorationUnitSize = RESTORATION_TILESIZE_MAX;
#endif
        }

        if (numPlanes > 1) {
#if UMC_AV1_DECODER_REV >= 5000
            int s = IPP_MIN(sh->subsampling_x, sh->subsampling_y);
#else
            int s = IPP_MIN(fh->subsamplingX, fh->subsamplingY);
#endif
            if (s && !chromaNone) {
                fh->rstInfo[1].restorationUnitSize =
                    fh->rstInfo[0].restorationUnitSize >> (bs->GetBit() * s);
            }
            else {
                fh->rstInfo[1].restorationUnitSize =
                    fh->rstInfo[0].restorationUnitSize;
            }
            fh->rstInfo[2].restorationUnitSize =
                fh->rstInfo[1].restorationUnitSize;
        }

        fh->lrUnitShift = static_cast<Ipp32u>(log(fh->rstInfo[0].restorationUnitSize / size) / log(2));
        fh->lrUVShift = static_cast<Ipp32u>(log(fh->rstInfo[0].restorationUnitSize / fh->rstInfo[1].restorationUnitSize) / log(2));

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_read_tx_mode(AV1Bitstream* bs, FrameHeader* fh, bool allLosless)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        if (allLosless)
            fh->txMode = ONLY_4X4;
        else
#if UMC_AV1_DECODER_REV >= 5000
            fh->txMode = bs->GetBit() ? TX_MODE_SELECT : TX_MODE_LARGEST;
#else
            fh->txMode = bs->GetBit() ? TX_MODE_SELECT : (TX_MODE)bs->GetBits(2);
#endif
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    bool av1_is_compound_reference_allowed(FrameHeader const* fh, DPBType const& frameDpb)
    {
#if UMC_AV1_DECODER_REV >= 5000
        if (IsFrameIntraOnly(fh))
            return false;

        // Check whether two different reference frames exist.
        FrameHeader const& refHdr0 = frameDpb[fh->refFrameIdx[0]]->GetFrameHeader();
        const Ipp32u refOrderHint0 = refHdr0.orderHint;

        for (Ipp32u ref = 1; ref < INTER_REFS; ref++)
        {
            FrameHeader const& refHdr1 = frameDpb[fh->refFrameIdx[ref]]->GetFrameHeader();
            if (refOrderHint0 != refHdr1.orderHint)
                return true;
        }

        return false;
#else
        frameDpb;
        return !IsFrameIntraOnly(fh);
#endif
    }

    inline
    void av1_read_frame_reference_mode(AV1Bitstream* bs, FrameHeader* fh, DPBType const& frameDpb)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        fh->referenceMode = SINGLE_REFERENCE;

        if (av1_is_compound_reference_allowed(fh, frameDpb))
        {
            fh->referenceMode = bs->GetBit() ? REFERENCE_MODE_SELECT :
#if UMC_AV1_DECODER_REV >= 5000
                SINGLE_REFERENCE;
#else
                bs->GetBit() ? COMPOUND_REFERENCE : SINGLE_REFERENCE;
#endif
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_read_compound_tools(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
#if UMC_AV1_DECODER_REV >= 5000
        if (!IsFrameIntraOnly(fh))
#else
        if (!IsFrameIntraOnly(fh) && fh->referenceMode != COMPOUND_REFERENCE)
#endif
        {
            fh->allowInterIntraCompound = bs->GetBit();
        }
        if (!IsFrameIntraOnly(fh) && fh->referenceMode != SINGLE_REFERENCE)
        {
            fh->allowMaskedCompound = bs->GetBit();
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

#if UMC_AV1_DECODER_REV >= 5000
    inline Ipp32s av1_get_relative_dist(SequenceHeader const* sh, const Ipp32u a, const Ipp32u b) {
        if (!sh->enable_order_hint)
            return 0;

        const Ipp32u bits = sh->order_hint_bits_minus1 + 1;

        Ipp32s diff = a - b;
        Ipp32s m = 1 << (bits - 1);
        diff = (diff & (m - 1)) - (diff & m);
        return diff;
    }

    static bool av1_is_skip_mode_allowed(FrameHeader const* fh, SequenceHeader const* sh, DPBType const& frameDpb)
    {
        if (!sh->enable_order_hint || fh->errorResilientMode ||
            IsFrameIntraOnly(fh) || fh->referenceMode == SINGLE_REFERENCE)
            return false;

        const Ipp32s MAX_VALUE = (std::numeric_limits<Ipp32s>::max)();
        const Ipp32s INVALID_IDX = -1;

        Ipp32s refFrameOffset[2] = { -1, MAX_VALUE };
        int refIdx[2] = { INVALID_IDX, INVALID_IDX };

        // Identify the nearest forward and backward references.
        for (Ipp32s i = 0; i < INTER_REFS; ++i)
        {
            FrameHeader const& refHdr = frameDpb[fh->refFrameIdx[i]]->GetFrameHeader();
            const Ipp32u refOrderHint = refHdr.orderHint;

            if (av1_get_relative_dist(sh, refOrderHint, fh->orderHint) < 0)
            {
                // Forward reference
                if (refFrameOffset[0] == -1 ||
                    av1_get_relative_dist(sh, refOrderHint, refFrameOffset[0]) > 0)
                {
                    refFrameOffset[0] = refOrderHint;
                    refIdx[0] = i;
                }
            }
            else if (av1_get_relative_dist(sh, refOrderHint, fh->orderHint) > 0)
            {
                // Backward reference
                if (refFrameOffset[1] == MAX_VALUE ||
                    av1_get_relative_dist(sh, refOrderHint, refFrameOffset[1]) < 0)
                {
                    refFrameOffset[1] = refOrderHint;
                    refIdx[1] = i;
                }
            }
        }

        if (refIdx[0] != INVALID_IDX && refIdx[1] != INVALID_IDX)
        {
            return true;
        }
        else if (refIdx[0] != INVALID_IDX && refIdx[1] == INVALID_IDX)
        {
            // == Forward prediction only ==
            // Identify the second nearest forward reference.
            refFrameOffset[1] = -1;
            for (int i = 0; i < INTER_REFS; ++i)
            {
                FrameHeader const& refHdr = frameDpb[fh->refFrameIdx[i]]->GetFrameHeader();
                const Ipp32u refOrderHint = refHdr.orderHint;

                if ((refFrameOffset[0] >= 0 &&
                    av1_get_relative_dist(sh, refOrderHint, refFrameOffset[0]) < 0) &&
                    (refFrameOffset[1] < 0 ||
                        av1_get_relative_dist(sh, refOrderHint, refFrameOffset[1]) > 0))
                {
                    // Second closest forward reference
                    refFrameOffset[1] = refOrderHint;
                    refIdx[1] = i;
                }
            }

            if (refFrameOffset[1] >= 0)
                return true;
        }

        return false;
    }
#endif // UMC_AV1_DECODER_REV >= 5000

    inline
    Ipp32s av1_read_q_delta(AV1Bitstream* bs)
    {
        return (bs->GetBit()) ?
            read_inv_signed_literal(bs, 6) : 0;
    }

    inline
#if UMC_AV1_DECODER_REV >= 5000
    void av1_setup_quantization(AV1Bitstream* bs, SequenceHeader const * sh, FrameHeader* fh)
#else //  UMC_AV1_DECODER_REV >= 5000
    void av1_setup_quantization(AV1Bitstream* bs, FrameHeader* fh)
#endif //  UMC_AV1_DECODER_REV >= 5000
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        fh->baseQIndex = bs->GetBits(UMC_VP9_DECODER::QINDEX_BITS);

        fh->yDcDeltaQ = av1_read_q_delta(bs);

#if UMC_AV1_DECODER_REV >= 5000
        if (fh->numPlanes > 1)
        {
            Ipp32s diffUVDelta = 0;
            if (sh->separate_uv_delta_q)
                diffUVDelta = bs->GetBit();

            fh->uDcDeltaQ = av1_read_q_delta(bs);
            fh->uAcDeltaQ = av1_read_q_delta(bs);

            if (diffUVDelta)
            {
                fh->vDcDeltaQ = av1_read_q_delta(bs);
                fh->vAcDeltaQ = av1_read_q_delta(bs);
            }
            else
            {
                fh->vDcDeltaQ = fh->uDcDeltaQ;
                fh->vAcDeltaQ = fh->uAcDeltaQ;
            }
        }
#else // UMC_AV1_DECODER_REV >= 5000
        fh->uDcDeltaQ = av1_read_q_delta(bs);
        fh->uAcDeltaQ = av1_read_q_delta(bs);
        fh->vDcDeltaQ = fh->uDcDeltaQ;
        fh->vAcDeltaQ = fh->uAcDeltaQ;
#endif // UMC_AV1_DECODER_REV >= 5000

        fh->lossless = (0 == fh->baseQIndex &&
            0 == fh->yDcDeltaQ &&
            0 == fh->uDcDeltaQ &&
            0 == fh->uAcDeltaQ &&
            0 == fh->vDcDeltaQ &&
            0 == fh->vAcDeltaQ);

        fh->useQMatrix = bs->GetBit();
        if (fh->useQMatrix) {
#if UMC_AV1_DECODER_REV >= 5000
            fh->qmY = bs->GetBits(QM_LEVEL_BITS);
            fh->qmU = bs->GetBits(QM_LEVEL_BITS);

            if (!sh->separate_uv_delta_q)
                fh->qmV = fh->qmV;
            else
                fh->qmV = bs->GetBits(QM_LEVEL_BITS);
#else // UMC_AV1_DECODER_REV >= 5000
            fh->minQMLevel = bs->GetBits(QM_LEVEL_BITS);
            fh->maxQMLevel = bs->GetBits(QM_LEVEL_BITS);
#endif // UMC_AV1_DECODER_REV >= 5000
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    bool av1_setup_segmentation(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        bool segmentQuantizerActive = false;
        fh->segmentation.updateMap = 0;
        fh->segmentation.updateData = 0;

        fh->segmentation.enabled = (Ipp8u)bs->GetBit();
        if (fh->segmentation.enabled)
        {
            // Segmentation map update
            if (IsFrameResilent(fh))
                fh->segmentation.updateMap = 1;
            else
                fh->segmentation.updateMap = (Ipp8u)bs->GetBit();
            if (fh->segmentation.updateMap)
            {
                if (IsFrameResilent(fh))
                    fh->segmentation.temporalUpdate = 0;
                else
                    fh->segmentation.temporalUpdate = (Ipp8u)bs->GetBit();
            }

            fh->segmentation.updateData = (Ipp8u)bs->GetBit();
            if (fh->segmentation.updateData)
            {
                fh->segmentation.absDelta = (Ipp8u)bs->GetBit();

                ClearAllSegFeatures(fh->segmentation);

                for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                {
                    for (Ipp8u j = 0; j < SEG_LVL_MAX; ++j)
                    {
                        Ipp32s data = 0;
                        if (bs->GetBit()) // feature_enabled
                        {
                            if (j == SEG_LVL_ALT_Q)
                                segmentQuantizerActive = true;
                            EnableSegFeature(fh->segmentation, i, (SEG_LVL_FEATURES)j);

                            const Ipp32u nBits = UMC_VP9_DECODER::GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                            const Ipp32u isSigned = IsSegFeatureSigned((SEG_LVL_FEATURES)j);
#if UMC_AV1_DECODER_REV >= 5000
                            const Ipp32s dataMax = SEG_FEATURE_DATA_MAX[j];
                            const Ipp32s dataMin = -dataMax;

                            if (isSigned)
                                data = read_inv_signed_literal(bs, nBits);
                            else
                                data = bs->GetBits(nBits);

                            data = UMC_VP9_DECODER::clamp(data, dataMin, dataMax);

#else // UMC_AV1_DECODER_REV >= 5000
                            data = bs->GetBits(nBits);
                            data = data > SEG_FEATURE_DATA_MAX[j] ? SEG_FEATURE_DATA_MAX[j] : data;

                            if (isSigned)
                                data = bs->GetBit() ? -data : data;
#endif // UMC_AV1_DECODER_REV >= 5000
                        }

                        SetSegData(fh->segmentation, i, (SEG_LVL_FEATURES)j, data);
                    }
                }
            }
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
        return segmentQuantizerActive;
    }

    inline
    Ipp32u get_num_tiles(Ipp32u mi_frame_size, Ipp32u log2_tile_num) {
        // Round the frame up to a whole number of max superblocks
        mi_frame_size = ALIGN_POWER_OF_TWO(mi_frame_size, MAX_MIB_SIZE_LOG2);

        // Divide by the signalled number of tiles, rounding up to the multiple of
        // the max superblock size. To do this, shift right (and round up) to get the
        // tile size in max super-blocks and then shift left again to convert it to
        // mi units.
        const int shift = log2_tile_num + MAX_MIB_SIZE_LOG2;
        const int max_sb_tile_size =
            ALIGN_POWER_OF_TWO(mi_frame_size, shift) >> shift;
        const int mi_tile_size = max_sb_tile_size << MAX_MIB_SIZE_LOG2;

        // The actual number of tiles is the ceiling of the frame size in mi units
        // divided by mi_size. This is at most 1 << log2_tile_num but might be
        // strictly less if max_sb_tile_size got rounded up significantly.
        return (mi_frame_size + mi_tile_size - 1) / mi_tile_size;
    }

#if UMC_AV1_DECODER_REV < 5000
    inline
    void av1_get_tile_nbits(const Ipp32s miCols, Ipp32s & minLog2TileCols, Ipp32s & maxLog2TileCols)
    {
        const Ipp8u MIN_TILE_WIDTH_MAX_SB = 2;
        const Ipp8u MAX_TILE_WIDTH_MAX_SB = 32;

        const Ipp32s maxSbCols = ALIGN_POWER_OF_TWO(miCols, MAX_MIB_SIZE_LOG2) >> MAX_MIB_SIZE_LOG2;
        Ipp32s minLog2 = 0, maxLog2 = 1;

        while ((maxSbCols >> maxLog2) >= MIN_TILE_WIDTH_MAX_SB)
            ++maxLog2;
        --maxLog2;
        if (maxLog2 < 0)
            maxLog2 = 0;

        while ((MAX_TILE_WIDTH_MAX_SB << minLog2) < maxSbCols)
            ++minLog2;

        VM_ASSERT(minLog2 <= maxLog2);

        minLog2TileCols = minLog2;
        maxLog2TileCols = maxLog2;
    }
#endif // UMC_AV1_DECODER_REV < 5000

#if UMC_AV1_DECODER_REV >= 5000
    static void read_tile_info_max_tile(AV1Bitstream* bs, FrameHeader* fh)
    {
        fh->uniformTileSpacingFlag = bs->GetBit();

        // TODO: [Rev0.5] implement proper read of tile columns
        if (fh->uniformTileSpacingFlag)
            bs->GetBit();

        // TODO: [Rev0.5] implement proper read of tile rows
        if (fh->uniformTileSpacingFlag)
            bs->GetBit();
    }
#endif // UMC_AV1_DECODER_REV >= 5000

    inline
    void av1_read_tile_info(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
#if UMC_AV1_DECODER_REV >= 5000
        const bool largeScaleTile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        if (largeScaleTile)
        {
            // TODO: [Rev0.5] add support of large scale tile
            return;
        }
        read_tile_info_max_tile(bs, fh);
#else // UMC_AV1_DECODER_REV >= 5000
        const Ipp32u alignedWidth = ALIGN_POWER_OF_TWO(fh->width, MI_SIZE_LOG2);
        int minLog2TileColumns, maxLog2TileColumns, maxOnes;
        const Ipp32u miCols = alignedWidth >> MI_SIZE_LOG2;

        av1_get_tile_nbits(miCols, minLog2TileColumns, maxLog2TileColumns);

        maxOnes = maxLog2TileColumns - minLog2TileColumns;
        fh->log2TileColumns = minLog2TileColumns;
        while (maxOnes-- && bs->GetBit())
            fh->log2TileColumns++;

        fh->log2TileRows = bs->GetBit();
        if (fh->log2TileRows)
            fh->log2TileRows += bs->GetBit();

        fh->tileCols = get_num_tiles(miCols, fh->log2TileColumns);
        const Ipp32u alignedHeight = ALIGN_POWER_OF_TWO(fh->height, MI_SIZE_LOG2);
        const Ipp32u miRows = alignedHeight >> MI_SIZE_LOG2;
        fh->tileRows = get_num_tiles(miRows, fh->log2TileRows);
#endif // UMC_AV1_DECODER_REV >= 5000

#if UMC_AV1_DECODER_REV >= 5000
        if (fh->tileCols > 1)
            fh->loopFilterAcrossTilesVEnabled = bs->GetBit();
        else
            fh->loopFilterAcrossTilesVEnabled = 1;

        if (fh->tileRows > 1)
            fh->loopFilterAcrossTilesHEnabled = bs->GetBit();
        else
            fh->loopFilterAcrossTilesHEnabled = 1;

        if (fh->tileCols * fh->tileRows > 1)
            fh->tileSizeBytes = bs->GetBits(2) + 1;
#else
        if (fh->tileCols * fh->tileRows > 1)
            fh->loopFilterAcrossTilesEnabled = bs->GetBit();
        fh->tileSizeBytes = bs->GetBits(2) + 1;

        fh->tileGroupBitOffset = (mfxU32)bs->BitsDecoded();

        // read_tile_group_range()
        const mfxU32 numBits = fh->log2TileRows + fh->log2TileColumns;
        bs->GetBits(numBits); // tg_start
        bs->GetBits(numBits); // tg_size
#endif


        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    const WarpedMotionParams default_warp_params = {
        IDENTITY,
        { 0, 0, (1 << WARPEDMODEL_PREC_BITS), 0, 0, (1 << WARPEDMODEL_PREC_BITS), 0,
        0 },
        0, 0, 0, 0
    };

    inline
    void av1_read_global_motion_params(WarpedMotionParams* params,
                                       WarpedMotionParams const* ref_params,
                                       AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        const Ipp8u SUBEXPFIN_K = 3;
        const Ipp8u GM_TRANS_PREC_BITS = 6;
        const Ipp8u GM_ABS_TRANS_BITS = 12;
        const Ipp8u GM_ABS_TRANS_ONLY_BITS = (GM_ABS_TRANS_BITS - GM_TRANS_PREC_BITS + 3);
        const Ipp8u GM_TRANS_PREC_DIFF = WARPEDMODEL_PREC_BITS - GM_TRANS_PREC_BITS;
        const Ipp8u GM_TRANS_ONLY_PREC_DIFF = WARPEDMODEL_PREC_BITS - 3;
        const Ipp16u GM_TRANS_DECODE_FACTOR = 1 << GM_TRANS_PREC_DIFF;
        const Ipp16u GM_TRANS_ONLY_DECODE_FACTOR = 1 << GM_TRANS_ONLY_PREC_DIFF;

        const Ipp8u GM_ALPHA_PREC_BITS = 15;
        const Ipp8u GM_ABS_ALPHA_BITS = 12;
        const Ipp8s GM_ALPHA_PREC_DIFF = WARPEDMODEL_PREC_BITS - GM_ALPHA_PREC_BITS;
        const Ipp8u GM_ALPHA_DECODE_FACTOR = 1 << GM_ALPHA_PREC_DIFF;

        const Ipp8u GM_ROW3HOMO_PREC_BITS = 16;
        const Ipp8u GM_ABS_ROW3HOMO_BITS = 11;
        const Ipp8u WARPEDMODEL_ROW3HOMO_PREC_BITS = 16;
        const Ipp8s GM_ROW3HOMO_PREC_DIFF = WARPEDMODEL_ROW3HOMO_PREC_BITS - GM_ROW3HOMO_PREC_BITS;
        const Ipp8u GM_ROW3HOMO_DECODE_FACTOR = 1 << GM_ROW3HOMO_PREC_DIFF;

        const Ipp16u GM_TRANS_MAX = 1 << GM_ABS_TRANS_BITS;
        const Ipp16u GM_ALPHA_MAX = 1 << GM_ABS_ALPHA_BITS;
        const Ipp16u GM_ROW3HOMO_MAX = 1 << GM_ABS_ROW3HOMO_BITS;

        const Ipp16s GM_TRANS_MIN = -GM_TRANS_MAX;
        const Ipp16s GM_ALPHA_MIN = -GM_ALPHA_MAX;
        const Ipp16s GM_ROW3HOMO_MIN = -GM_ROW3HOMO_MAX;

        TRANSFORMATION_TYPE type;
        type = static_cast<TRANSFORMATION_TYPE>(bs->GetBit());
        if (type != IDENTITY) {
            if (bs->GetBit())
                type = ROTZOOM;
            else
                type = bs->GetBit() ? TRANSLATION : AFFINE;
        }
        fh->globalMotionType = type; // TOOD: realize how to fill globalMotionType properly having multiple references

        int trans_bits;
        int trans_dec_factor;
        int trans_prec_diff;
        *params = default_warp_params;
        params->wmtype = type;
        switch (type) {
        case HOMOGRAPHY:
        case HORTRAPEZOID:
        case VERTRAPEZOID:
            if (type != HORTRAPEZOID)
                params->wmmat[6] =
                read_signed_primitive_refsubexpfin(bs, GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
                    static_cast<Ipp16s>(ref_params->wmmat[6] >> GM_ROW3HOMO_PREC_DIFF)) *
                GM_ROW3HOMO_DECODE_FACTOR;
            if (type != VERTRAPEZOID)
                params->wmmat[7] =
                read_signed_primitive_refsubexpfin(bs, GM_ROW3HOMO_MAX + 1, SUBEXPFIN_K,
                    static_cast<Ipp16s>(ref_params->wmmat[7] >> GM_ROW3HOMO_PREC_DIFF)) *
                GM_ROW3HOMO_DECODE_FACTOR;
        case AFFINE:
        case ROTZOOM:
            params->wmmat[2] = read_signed_primitive_refsubexpfin(bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS)) *
                GM_ALPHA_DECODE_FACTOR +
                (1 << WARPEDMODEL_PREC_BITS);
            if (type != VERTRAPEZOID)
                params->wmmat[3] = read_signed_primitive_refsubexpfin(bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                    static_cast<Ipp16s>(ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF)) *
                GM_ALPHA_DECODE_FACTOR;
            if (type >= AFFINE) {
                if (type != HORTRAPEZOID)
                    params->wmmat[4] = read_signed_primitive_refsubexpfin(bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                        static_cast<Ipp16s>(ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF)) *
                    GM_ALPHA_DECODE_FACTOR;
                params->wmmat[5] = read_signed_primitive_refsubexpfin(bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                    static_cast<Ipp16s>(ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                    (1 << GM_ALPHA_PREC_BITS)) *
                    GM_ALPHA_DECODE_FACTOR +
                    (1 << WARPEDMODEL_PREC_BITS);
            }
            else {
                params->wmmat[4] = -params->wmmat[3];
                params->wmmat[5] = params->wmmat[2];
            }
            // fallthrough intended
        case TRANSLATION:
        {
            Ipp8u disallowHP = fh->allowHighPrecisionMv ? 0 : 1;
            trans_bits = (type == TRANSLATION) ? GM_ABS_TRANS_ONLY_BITS - disallowHP
                : GM_ABS_TRANS_BITS;
            trans_dec_factor = (type == TRANSLATION)
                ? GM_TRANS_ONLY_DECODE_FACTOR * (1 << disallowHP)
                : GM_TRANS_DECODE_FACTOR;
            trans_prec_diff = (type == TRANSLATION)
                ? GM_TRANS_ONLY_PREC_DIFF + disallowHP
                : GM_TRANS_PREC_DIFF;
            params->wmmat[0] = read_signed_primitive_refsubexpfin(bs, (1 << trans_bits) + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[0] >> trans_prec_diff)) *
                trans_dec_factor;
            params->wmmat[1] = read_signed_primitive_refsubexpfin(bs, (1 << trans_bits) + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[1] >> trans_prec_diff)) *
                trans_dec_factor;
        }
        case IDENTITY: break;
        default: throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

#if UMC_AV1_DECODER_REV >= 5000
    void av1_read_film_grain_params(FilmGrain* params, AV1Bitstream* bs, SequenceHeader const * sh, FrameHeader const* fh)
    {
        params->apply_grain = bs->GetBit();
        if (!params->apply_grain) {
            memset(params, 0, sizeof(*params));
            return;
        }

        params->random_seed = bs->GetBits(16);
        if (fh->frameType == INTER_FRAME)
            params->update_parameters = bs->GetBit();
        else
            params->update_parameters = 1;

        if (!params->update_parameters)
        {
            // inherit parameters from a previous reference frame
            // TODO: [Rev0.5] implement proper film grain param inheritance
            return;
        }

        // Scaling functions parameters
        params->num_y_points = bs->GetBits(4);  // max 14
        if (params->num_y_points > 14)
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

        for (int i = 0; i < params->num_y_points; i++)
        {
            params->scaling_points_y[i][0] = bs->GetBits(8);
            if (i && params->scaling_points_y[i - 1][0] >= params->scaling_points_y[i][0])
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
            params->scaling_points_y[i][1] = bs->GetBits(8);
        }

        if (!sh->monochrome)
            params->chroma_scaling_from_luma = bs->GetBit();

        if (sh->monochrome || params->chroma_scaling_from_luma)
        {
            params->num_cb_points = 0;
            params->num_cr_points = 0;
        }
        else
        {
            params->num_cb_points = bs->GetBits(4);  // max 10
            if (params->num_cb_points > 10)
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

            for (int i = 0; i < params->num_cb_points; i++)
            {
                params->scaling_points_cb[i][0] = bs->GetBits(8);
                if (i &&
                    params->scaling_points_cb[i - 1][0] >= params->scaling_points_cb[i][0])
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                params->scaling_points_cb[i][1] = bs->GetBits(8);
            }

            params->num_cr_points = bs->GetBits(4);  // max 10
            if (params->num_cr_points > 10)
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

            for (int i = 0; i < params->num_cr_points; i++)
            {
                params->scaling_points_cr[i][0] = bs->GetBits(8);
                if (i &&
                    params->scaling_points_cr[i - 1][0] >= params->scaling_points_cr[i][0])
                        throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                params->scaling_points_cr[i][1] = bs->GetBits(8);
            }
        }

        params->scaling_shift = bs->GetBits(2) + 8;  // 8 + value

        // AR coefficients
        // Only sent if the corresponsing scaling function has
        // more than 0 points

        params->ar_coeff_lag = bs->GetBits(2);

        int num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
        int num_pos_chroma = num_pos_luma;
        if (params->num_y_points > 0) ++num_pos_chroma;

        if (params->num_y_points)
            for (int i = 0; i < num_pos_luma; i++)
                params->ar_coeffs_y[i] = bs->GetBits(8) - 128;

        if (params->num_cb_points || params->chroma_scaling_from_luma)
            for (int i = 0; i < num_pos_chroma; i++)
                params->ar_coeffs_cb[i] = bs->GetBits(8) - 128;

        if (params->num_cr_points || params->chroma_scaling_from_luma)
            for (int i = 0; i < num_pos_chroma; i++)
                params->ar_coeffs_cr[i] = bs->GetBits(8) - 128;

        params->ar_coeff_shift = bs->GetBits(2) + 6;  // 6 + value

        params->grain_scale_shift = bs->GetBits(2);

        if (params->num_cb_points) {
            params->cb_mult = bs->GetBits(8);
            params->cb_luma_mult = bs->GetBits(8);
            params->cb_offset = bs->GetBits(9);
        }

        if (params->num_cr_points) {
            params->cr_mult = bs->GetBits(8);
            params->cr_luma_mult = bs->GetBits(8);
            params->cr_offset = bs->GetBits(9);
        }

        params->overlap_flag = bs->GetBit();

        params->clip_to_restricted_range = bs->GetBit();
    }
#endif

    Ipp8u AV1Bitstream::ReadSuperFrameIndex(Ipp32u sizes[8])
    {
        if (m_bitOffset)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        Ipp8u marker = *m_pbs;
        size_t frame_sz_sum = 0;

        if ((marker & 0xe0) == 0xc0) {
            const Ipp8u frames = (marker & 0x7) + 1;
            const Ipp8u mag = ((marker >> 3) & 0x3) + 1;
            const size_t index_sz = 2 + mag * (frames - 1);

            size_t space_left = m_maxBsSize - (m_pbs - m_pbsBase);
            if (space_left < index_sz)
                throw av1_exception(UMC::UMC_ERR_FAILED);

            {
                const Ipp8u marker2 = *(m_pbs + index_sz - 1);
                if (marker != marker2 || frames == 0 || mag == 0)
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            {
                // Found a valid superframe index.
                Ipp32s i;
                GetBits(8); // first marker
                for (i = 0; i < frames - 1; ++i) {
                    Ipp32u this_sz = GetBits(mag * 8) + 1;
                    sizes[i] = this_sz;
                    frame_sz_sum += this_sz;
                }
                // TODO: implement here getting of size of last frame in superframe
                GetBits(8); // second marker

                return frames;
            }
        }

        return 0;
    }

#if UMC_AV1_DECODER_REV >= 5000
    inline
    void av1_read_obu_size(AV1Bitstream* bs, size_t* const size, size_t* const length)
    {
        size_t start = bs->BytesDecoded();
        *size = read_leb128(bs);
        *length = bs->BytesDecoded() - start;
    }

    OBUHeader av1_read_obu_header(AV1Bitstream* bs)
    {
        bs->GetBit(); // first bit is obu_forbidden_bit (0) - hello and thanks to Dima G. :)

        OBUHeader header;

        header.type = (AV1_OBU_TYPE)bs->GetBits(4);
        bs->GetBits(2);  // reserved

        if (bs->GetBits(1)) // if has extension
        {
            header.temporalLayerId = bs->GetBits(3);
            header.enhancementLayerId = bs->GetBits(2);
            bs->GetBits(3);  // reserved
        }

        return header;
    }

    void AV1Bitstream::ReadOBUHeader(OBUInfo* info)
    {
        av1_read_obu_size(this, &info->size, &info->sizeFieldLength);
        info->header = av1_read_obu_header(this);
    }
#endif

    void AV1Bitstream::GetSequenceHeader(SequenceHeader* sh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)BitsDecoded());

#if UMC_AV1_DECODER_REV >= 5000
        sh->profile = av1_profile(this);
        GetBits(4); // level

        Ipp32u elayers_cnt = GetBits(2);
        for (Ipp32u i = 1; i <= elayers_cnt; i++)
            GetBits(4); // level for each enhancement layer

        sh->num_bits_width = GetBits(4) + 1;
        sh->num_bits_height = GetBits(4) + 1;

        sh->max_frame_width = GetBits(sh->num_bits_width) + 1;
        sh->max_frame_height = GetBits(sh->num_bits_height) + 1;
#endif // UMC_AV1_DECODER_REV >= 5000

        sh->frame_id_numbers_present_flag = GetBit();
        if (sh->frame_id_numbers_present_flag) {
            sh->delta_frame_id_length = GetBits(4) + 2;
            sh->frame_id_length = GetBits(3) + sh->delta_frame_id_length + 1;
        }

#if UMC_AV1_DECODER_REV >= 5000

        sh->sb_size = GetBit() ? BLOCK_128X128 : BLOCK_64X64;
        sh->enable_dual_filter = GetBit();
        sh->enable_order_hint = GetBit();

        sh->enable_jnt_comp =
            sh->enable_order_hint ? GetBit() : 0;

        if (GetBit())
            sh->force_screen_content_tools = 2;
        else
            sh->force_screen_content_tools = GetBit();

        if (sh->force_screen_content_tools > 0)
        {
            if (GetBit())
                sh->force_integer_mv = 2;
            else
                sh->force_integer_mv = GetBit();
        }
        else
            sh->force_integer_mv = 2;

        sh->order_hint_bits_minus1 =
            sh->enable_order_hint ? GetBits(3) : -1;

        av1_bitdepth_colorspace_sampling(this, sh);

        av1_timing_info(this, sh);

        sh->film_grain_param_present = GetBit();
#endif // UMC_AV1_DECODER_REV >= 5000

        AV1D_LOG("[-]: %d", (mfxU32)BitsDecoded());
    }

    inline
    bool FrameMightUsePrevFrameMVs(FrameHeader const* fh)
    {
#if UMC_AV1_DECODER_REV >= 5000
        const bool largeScaleTile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        return !IsFrameResilent(fh) && !largeScaleTile;
#else
        return !IsFrameResilent(fh);
#endif
    }

    inline
    bool FrameCanUsePrevFrameMVs(FrameHeader const* fh, FrameHeader const* prev_fh = 0)
    {
        return (FrameMightUsePrevFrameMVs(fh) &&
            prev_fh &&
#if UMC_AV1_DECODER_REV < 5000
             prev_fh->showFrame &&
#endif
            !prev_fh->intraOnly &&
            fh->width == prev_fh->width &&
            fh->height == prev_fh->height);
    }

#if UMC_AV1_DECODER_REV >= 5000
    void AV1Bitstream::GetFrameHeaderPart1(FrameHeader* fh, SequenceHeader const* sh)
#else
    void AV1Bitstream::GetFrameHeaderPart1(FrameHeader* fh, SequenceHeader* sh)
#endif
    {
        AV1D_LOG("[+]: %d", (mfxU32)BitsDecoded());

        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

#if UMC_AV1_DECODER_REV < 5000
        Ipp32u sizes[8];
        if (ReadSuperFrameIndex(sizes))
            throw av1_exception(UMC::UMC_ERR_FAILED); // no support for super frame indexes

        if (FRAME_MARKER != GetBits(2))
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

        fh->profile = av1_profile(this);
        if (fh->profile >= 4)
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
#endif // UMC_AV1_DECODER_REV < 5000

        fh->showExistingFrame = GetBit();
        if (fh->showExistingFrame)
        {
            fh->frameToShow = GetBits(3);
            if (sh->frame_id_numbers_present_flag)
                fh->displayFrameId = GetBits(sh->frame_id_length);

            fh->showFrame = 1;

            return;
        }

#if UMC_AV1_DECODER_REV >= 5000
        fh->frameType = static_cast<FRAME_TYPE>(GetBits(2));
        fh->showFrame = GetBit();
        fh->intraOnly = fh->frameType == INTRA_ONLY_FRAME;
        fh->errorResilientMode = GetBit();
        fh->enableIntraEdgeFilter = GetBit();
        fh->allowFilterIntra = GetBit();
        fh->disableCdfUpdate = GetBit();
        if (sh->force_screen_content_tools == 2)
            fh->allowScreenContentTools = GetBit();
        else
            fh->allowScreenContentTools = sh->force_screen_content_tools;

        if (fh->allowScreenContentTools)
        {
            if (sh->force_integer_mv == 2)
                fh->curFrameForceIntegerMV = GetBit();
            else
                fh->curFrameForceIntegerMV = sh->force_integer_mv;
        }
        else
            fh->curFrameForceIntegerMV = 0;
#else // UMC_AV1_DECODER_REV >= 5000
        fh->frameType = static_cast<FRAME_TYPE>(GetBit());
        fh->showFrame = GetBit();

        if (fh->frameType != KEY_FRAME)
        {
            fh->intraOnly = fh->showFrame ? 0 : GetBit();
        }

        fh->errorResilientMode = GetBit();

        if (IsFrameIntraOnly(fh))
        {
            GetSequenceHeader(sh);
        }
#endif // UMC_AV1_DECODER_REV >= 5000


        if (sh->frame_id_numbers_present_flag)
            fh->currentFrameId = GetBits(sh->frame_id_length);

#if UMC_AV1_DECODER_REV >= 5000
        fh->frameSizeOverrideFlag = GetBits(1);
        fh->orderHint = GetBits(sh->order_hint_bits_minus1 + 1);
#endif

        if (KEY_FRAME == fh->frameType)
        {
#if UMC_AV1_DECODER_REV >= 5000
            if (!fh->showFrame)
                fh->refreshFrameFlags = static_cast<Ipp8u>(GetBits(NUM_REF_FRAMES));
            else
                fh->refreshFrameFlags = (1 << NUM_REF_FRAMES) - 1;
#else
            av1_bitdepth_colorspace_sampling(this, fh);
            fh->refreshFrameFlags = (1 << NUM_REF_FRAMES) - 1;
#endif

            for (Ipp8u i = 0; i < INTER_REFS; ++i)
            {
                fh->refFrameIdx[i] = -1;
            }

#if UMC_AV1_DECODER_REV >= 5000
            av1_get_frame_size(this, fh, sh);
#else // UMC_AV1_DECODER_REV >= 5000
            av1_get_frame_size(this, fh);
            av1_get_sb_size(this, fh);
#endif // UMC_AV1_DECODER_REV >= 5000

#if UMC_AV1_DECODER_REV >= 5000
            if (fh->allowScreenContentTools &&
                (fh->width == fh->superresUpscaledWidth || !NO_FILTER_FOR_IBC))
                fh->allowIntraBC = GetBit();
#else
            fh->allowScreenContentTools = GetBit();
#endif
        }
        else
        {
#if UMC_AV1_DECODER_REV < 5000
            if (fh->intraOnly)
                fh->allowScreenContentTools = GetBit();
#endif

            if (IsFrameResilent(fh))
            {
                fh->usePrevMVs = 0;
            }

#if UMC_AV1_DECODER_REV < 5000
            if (fh->errorResilientMode)
                fh->resetFrameContext = RESET_FRAME_CONTEXT_ALL;
            else
            {
                if (fh->intraOnly)
                {
                    fh->resetFrameContext = GetBit() ?
                        RESET_FRAME_CONTEXT_ALL
                        : RESET_FRAME_CONTEXT_CURRENT;
                }
                else
                {
                    fh->resetFrameContext = GetBit() ?
                        RESET_FRAME_CONTEXT_CURRENT
                        : RESET_FRAME_CONTEXT_NONE;
                    if (fh->resetFrameContext == RESET_FRAME_CONTEXT_CURRENT)
                    {
                        fh->resetFrameContext = GetBit() ?
                            RESET_FRAME_CONTEXT_ALL
                            : RESET_FRAME_CONTEXT_CURRENT;
                    }
                }
            }
#endif // UMC_AV1_DECODER_REV < 5000

            if (fh->intraOnly)
            {
#if UMC_AV1_DECODER_REV < 5000
                av1_bitdepth_colorspace_sampling(this, fh);
#endif

                fh->refreshFrameFlags = (Ipp8u)GetBits(NUM_REF_FRAMES);
#if UMC_AV1_DECODER_REV >= 5000
                av1_get_frame_size(this, fh, sh);
#else // UMC_AV1_DECODER_REV >= 5000
                av1_get_frame_size(this, fh);
                av1_get_sb_size(this, fh);
#endif // UMC_AV1_DECODER_REV >= 5000

#if UMC_AV1_DECODER_REV >= 5000
                if (fh->allowScreenContentTools &&
                    (fh->width == fh->superresUpscaledWidth || !NO_FILTER_FOR_IBC))
                    fh->allowIntraBC = GetBit();
#endif
            }
        }

        AV1D_LOG("[-]: %d", (mfxU32)BitsDecoded());
    }

    static bool IsAllLosless(FrameHeader const * fh)
    {
        bool qIndexAllZero = true;

        if (fh->segmentation.enabled)
        {
            for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
            {
                Ipp32s segQIndex = fh->baseQIndex;
                if (IsSegFeatureActive(fh->segmentation, i, SEG_LVL_ALT_Q))
                {
                    const Ipp32s data = GetSegData(fh->segmentation, i, SEG_LVL_ALT_Q);
                    segQIndex = UMC_VP9_DECODER::clamp(fh->segmentation.absDelta == UMC_VP9_DECODER::SEGMENT_ABSDATA ? data : fh->baseQIndex + data, 0, UMC_VP9_DECODER::MAXQ);
                }
                if (segQIndex)
                {
                    qIndexAllZero = false;
                    break;
                }
            }
        }
        else
        {
            qIndexAllZero = (fh->baseQIndex == 0);
        }

        return qIndexAllZero && fh->lossless;
    }

#if UMC_AV1_DECODER_REV < 5000
    inline void InheritFromKeyFrame(FrameHeader* fh, FrameHeader const* prev_fh)
    {
        fh->sbSize = prev_fh->sbSize;
        fh->bitDepth = prev_fh->bitDepth;
        fh->subsamplingX = prev_fh->subsamplingX;
        fh->subsamplingY = prev_fh->subsamplingY;
    }
#endif // UMC_AV1_DECODER_REV < 5000

    inline void InheritFromPrevFrame(FrameHeader* fh, FrameHeader const* prev_fh)
    {
#if UMC_AV1_DECODER_REV < 5000
        InheritFromKeyFrame(fh, prev_fh);
#endif // UMC_AV1_DECODER_REV < 5000

        for (Ipp32u i = 0; i < TOTAL_REFS; i++)
            fh->lf.refDeltas[i] = prev_fh->lf.refDeltas[i];

        for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
            fh->lf.modeDeltas[i] = prev_fh->lf.modeDeltas[i];
    }

    inline void GetRefFramesHeaders(std::vector<FrameHeader const*>* headers, DPBType const* dpb)
    {
        if (!dpb)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        if (dpb->empty())
            return;

        for (Ipp8u i = 0; i < NUM_REF_FRAMES; ++i)
        {
            AV1DecoderFrame const* frame = (*dpb)[i];
            VM_ASSERT(frame);
            FrameHeader const& ref_fh = frame->GetFrameHeader();
            headers->push_back(&ref_fh);
        }
    }

#if UMC_AV1_DECODER_REV >= 5000
    struct RefFrameInfo {
        Ipp32u mapIdx;
        Ipp32u shiftedOrderHint;
    };

    inline Ipp32s av1_compare_ref_frame_info(void const* left, void const* right)
    {
        RefFrameInfo const* a = (RefFrameInfo*)left;
        RefFrameInfo const* b = (RefFrameInfo*)right;

        if (a->shiftedOrderHint < b->shiftedOrderHint)
            return -1;

        if (a->shiftedOrderHint > a->shiftedOrderHint)
            return 1;

        return (a->mapIdx < b->mapIdx)
            ? -1
            : ((a->mapIdx > b->mapIdx) ? 1 : 0);
    }

    static void av1_set_frame_refs(SequenceHeader const* sh, FrameHeader* fh, DPBType const& frameDpb, Ipp32u last_frame_idx, Ipp32u gold_frame_idx)
    {
        const Ipp32u curFrameHint = 1 << sh->order_hint_bits_minus1;

        RefFrameInfo refFrameInfo[NUM_REF_FRAMES]; // RefFrameInfo structure contains
                                                   // (1) shiftedOrderHint
                                                   // (2) index in DPB (allows to correct sorting of frames having equal shiftedOrderHint)
        Ipp32u refFlagList[INTER_REFS] = { 0, 0, 0, 0, 0, 0, 0 };

        for (int i = 0; i < NUM_REF_FRAMES; ++i)
        {
            const Ipp32u mapIdx = i;

            refFrameInfo[i].mapIdx = mapIdx;

            FrameHeader const& refHdr = frameDpb[i]->GetFrameHeader();
            refFrameInfo[i].shiftedOrderHint =
                curFrameHint + av1_get_relative_dist(sh, refHdr.orderHint, fh->orderHint);
        }

        const Ipp32u lastOrderHint = refFrameInfo[last_frame_idx].shiftedOrderHint;
        const Ipp32u goldOrderHint = refFrameInfo[gold_frame_idx].shiftedOrderHint;

        // Confirm both LAST_FRAME and GOLDEN_FRAME are valid forward reference frames
        if (lastOrderHint >= curFrameHint)
        {
            VM_ASSERT("lastOrderHint is not less than curFrameHint!");
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        if (goldOrderHint >= curFrameHint)
        {
            VM_ASSERT("goldOrderHint is not less than curFrameHint!");
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        // Sort ref frames based on their shiftedOrderHint values.
        qsort(refFrameInfo, NUM_REF_FRAMES, sizeof(RefFrameInfo),
            av1_compare_ref_frame_info);

        // Identify forward and backward reference frames.
        // Forward  reference: ref orderHint < fh->orderHint
        // Backward reference: ref orderHint >= fh->orderHint
        Ipp32u fwdStartIdx = 0;
        Ipp32u fwdEndIdx = NUM_REF_FRAMES - 1;

        for (Ipp32u i = 0; i < NUM_REF_FRAMES; i++)
        {
            if (refFrameInfo[i].shiftedOrderHint >= curFrameHint)
            {
                fwdEndIdx = i - 1;
                break;
            }
        }

        int bwdStartIdx = fwdEndIdx + 1;
        int bwdEndIdx = NUM_REF_FRAMES - 1;

        // === Backward Reference Frames ===

        // == ALTREF_FRAME ==
        if (bwdStartIdx <= bwdEndIdx)
        {
            fh->refFrameIdx[ALTREF_FRAME - LAST_FRAME] = refFrameInfo[bwdEndIdx].mapIdx;
            refFlagList[ALTREF_FRAME - LAST_FRAME] = 1;
            bwdEndIdx--;
        }

        // == BWDREF_FRAME ==
        if (bwdStartIdx <= bwdEndIdx)
        {
            fh->refFrameIdx[BWDREF_FRAME - LAST_FRAME] = refFrameInfo[bwdStartIdx].mapIdx;
            refFlagList[BWDREF_FRAME - LAST_FRAME] = 1;
            bwdStartIdx++;
        }

        // == ALTREF2_FRAME ==
        if (bwdStartIdx <= bwdEndIdx)
        {
            fh->refFrameIdx[ALTREF2_FRAME - LAST_FRAME] = refFrameInfo[bwdStartIdx].mapIdx;
            refFlagList[ALTREF2_FRAME - LAST_FRAME] = 1;
        }

        // === Forward Reference Frames ===

        for (Ipp32u i = fwdStartIdx; i <= fwdEndIdx; ++i)
        {
            // == LAST_FRAME ==
            if (refFrameInfo[i].mapIdx == last_frame_idx)
            {
                fh->refFrameIdx[LAST_FRAME - LAST_FRAME] = refFrameInfo[i].mapIdx;
                refFlagList[LAST_FRAME - LAST_FRAME] = 1;
            }

            // == GOLDEN_FRAME ==
            if (refFrameInfo[i].mapIdx == gold_frame_idx)
            {
                fh->refFrameIdx[GOLDEN_FRAME - LAST_FRAME] = refFrameInfo[i].mapIdx;
                refFlagList[GOLDEN_FRAME - LAST_FRAME] = 1;
            }
        }

        VM_ASSERT(refFlagList[LAST_FRAME - LAST_FRAME] == 1 &&
            refFlagList[GOLDEN_FRAME - LAST_FRAME] == 1);

        // == LAST2_FRAME ==
        // == LAST3_FRAME ==
        // == BWDREF_FRAME ==
        // == ALTREF2_FRAME ==
        // == ALTREF_FRAME ==

        // Set up the reference frames in the anti-chronological order.
        static const MV_REFERENCE_FRAME ref_frame_list[INTER_REFS - 2] =
        { LAST2_FRAME, LAST3_FRAME, BWDREF_FRAME, ALTREF2_FRAME, ALTREF_FRAME };

        Ipp32u refIdx;
        for (refIdx = 0; refIdx < (INTER_REFS - 2); refIdx++)
        {
            const MV_REFERENCE_FRAME refFrame = ref_frame_list[refIdx];

            if (refFlagList[refFrame - LAST_FRAME] == 1) continue;

            while (fwdStartIdx <= fwdEndIdx &&
                (refFrameInfo[fwdEndIdx].mapIdx == last_frame_idx ||
                    refFrameInfo[fwdEndIdx].mapIdx == gold_frame_idx))
            {
                fwdEndIdx--;
            }
            if (fwdStartIdx > fwdEndIdx) break;

            fh->refFrameIdx[refFrame - LAST_FRAME] = refFrameInfo[fwdEndIdx].mapIdx;
            refFlagList[refFrame - LAST_FRAME] = 1;

            fwdEndIdx--;
        }

        // Assign all the remaining frame(s), if any, to the earliest reference frame.
        for (; refIdx < (INTER_REFS - 2); refIdx++)
        {
            const MV_REFERENCE_FRAME refFrame = ref_frame_list[refIdx];
            fh->refFrameIdx[refFrame - LAST_FRAME] = refFrameInfo[fwdStartIdx].mapIdx;
            refFlagList[refFrame - LAST_FRAME] = 1;
        }

        for (int i = 0; i < INTER_REFS; i++)
            VM_ASSERT(refFlagList[i] == 1);
    }
#endif // UMC_AV1_DECODER_REV >= 5000

    static void av1_mark_ref_frames(SequenceHeader const* sh, FrameHeader* fh, DPBType const& frameDpb)
    {
        const Ipp32u idLen = sh->frame_id_length;
        const Ipp32u diffLen = sh->delta_frame_id_length;
        for (Ipp32s i = 0; i < NUM_REF_FRAMES; i++)
        {
            FrameHeader const& refHdr = frameDpb[i]->GetFrameHeader();
            if ((static_cast<Ipp32s>(fh->currentFrameId) - (1 << diffLen)) > 0)
            {
                if (refHdr.currentFrameId > fh->currentFrameId ||
                    refHdr.currentFrameId < fh->currentFrameId - (1 << diffLen))
                    frameDpb[i]->SetRefValid(false);
            }
            else
            {
                if (refHdr.currentFrameId > fh->currentFrameId &&
                    refHdr.currentFrameId < ((1 << idLen) + fh->currentFrameId - (1 << diffLen)))
                    frameDpb[i]->SetRefValid(false);
            }
        }
    }

    void AV1Bitstream::GetFrameHeaderFull(FrameHeader* fh, SequenceHeader const* sh, FrameHeader const* prev_fh, DPBType const& frameDpb)
    {
        using UMC_VP9_DECODER::REF_FRAMES_LOG2;

        AV1D_LOG("[+]: %d", (mfxU32)BitsDecoded());
        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        if (fh->frameType != KEY_FRAME)
        {
            VM_ASSERT(prev_fh);
            VM_ASSERT(frameDpb.size() == NUM_REF_FRAMES);
            // TODO: [Global] Add handling of case when decoding starts from non key frame and thus frameDpb is empty
        }

        bool missingRefFrame = false; // TODO: [Global] Use this flag to trigger error resilience actions

        if (fh->showExistingFrame)
        {
            FrameHeader const& refHdr = frameDpb[fh->frameToShow]->GetFrameHeader();

            // get frame resolution
            fh->width  = refHdr.width;
            fh->height = refHdr.height;

            // check that there is no confilct between display_frame_id and respective frame in DPB
            if (sh->frame_id_numbers_present_flag &&
                (fh->displayFrameId != refHdr.currentFrameId ||
                false == frameDpb[fh->frameToShow]->RefValid()))
                VM_ASSERT("Frame_to_show is absent in DPB or invalid!");
                missingRefFrame = true;

            return;
        }
        else
        {
            // check current_frame_id and DPB frames for validity
            if (sh->frame_id_numbers_present_flag && KEY_FRAME != fh->frameType)
            {
                // check current_frame_id as described in AV1 spec 6.8.2
                const Ipp32u idLen = sh->frame_id_length;
                const Ipp32u prevFrameId = prev_fh->currentFrameId;
                const Ipp32s diffFrameId = fh->currentFrameId > prevFrameId ?
                    fh->currentFrameId - prevFrameId :
                    (1 << idLen) + fh->currentFrameId - prevFrameId;

                if (fh->currentFrameId == prevFrameId ||
                    diffFrameId >= (1 << (idLen - 1)))
                {
                    VM_ASSERT("current_frame_id is incompliant to AV1 spec!");
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                }

                //  check and mark ref frames as not valid as described in "Reference frame marking process" AV1 spec 5.9.4
                av1_mark_ref_frames(sh, fh, frameDpb);
            }
        }

        if (fh->frameType != KEY_FRAME && fh->intraOnly == 0)
        {
            fh->refreshFrameFlags = (Ipp8u)GetBits(NUM_REF_FRAMES);

#if UMC_AV1_DECODER_REV >= 5000
            if (!fh->errorResilientMode && sh->enable_order_hint)
                fh->frameRefsShortSignalling = GetBit();

            if (fh->frameRefsShortSignalling)
            {
                const Ipp32u last_frame_idx = GetBits(REF_FRAMES_LOG2);
                const Ipp32u gold_frame_idx = GetBits(REF_FRAMES_LOG2);

                // set last and gold reference frames
                fh->refFrameIdx[LAST_FRAME - LAST_FRAME]   = last_frame_idx;
                fh->refFrameIdx[GOLDEN_FRAME - LAST_FRAME] = gold_frame_idx;

                // compute other active references as specified in "Set frame refs process" AV1 spec 7.8
                av1_set_frame_refs(sh, fh, frameDpb, last_frame_idx, gold_frame_idx);
            }
#endif

            for (Ipp8u i = 0; i < INTER_REFS; ++i)
            {
#if UMC_AV1_DECODER_REV >= 5000
                if (!fh->frameRefsShortSignalling)
                    fh->refFrameIdx[i] = GetBits(REF_FRAMES_LOG2);
#else
                fh->refFrameIdx[i] = GetBits(REF_FRAMES_LOG2);
                fh->refFrameSignBias[i] = GetBit();
#endif // UMC_AV1_DECODER_REV >= 5000

                if (sh->frame_id_numbers_present_flag)
                {
                    const Ipp32s deltaFrameId = GetBits(sh->delta_frame_id_length) + 1;

                    // compute expected reference frame id from delta_frame_id and check that it's equal to one saved in DPB
                    const Ipp32u idLen = sh->frame_id_length;
                    const Ipp32u expectedFrameId = ((fh->currentFrameId - deltaFrameId + (1 << idLen))
                            % (1 << idLen));

                    AV1DecoderFrame const* refFrm = frameDpb[fh->refFrameIdx[i]];
                    FrameHeader const& refHdr = refFrm->GetFrameHeader();

                    if (expectedFrameId != refHdr.currentFrameId ||
                        false == refFrm->RefValid())
                    {
                        VM_ASSERT("Active reference frame is absent in DPB or invalid!");
                        missingRefFrame = true;
                    }
                }
            }

#if UMC_AV1_DECODER_REV >= 5000
            if (fh->errorResilientMode && fh->frameSizeOverrideFlag)
                av1_get_frame_sizes_with_refs(this, fh, frameDpb);
            else
                av1_get_frame_size(this, fh, sh);

            if (fh->curFrameForceIntegerMV)
                fh->allowHighPrecisionMv = 0;
            else
                fh->allowHighPrecisionMv = GetBit();
#else // UMC_AV1_DECODER_REV >= 5000
            av1_get_frame_sizes_with_refs(this, fh, frameDpb);

            fh->allowHighPrecisionMv = GetBit();
#endif // UMC_AV1_DECODER_REV >= 5000

            fh->interpFilter = GetBit() ? SWITCHABLE : (INTERP_FILTER)GetBits(LOG2_SWITCHABLE_FILTERS);
#if UMC_AV1_DECODER_REV >= 5000
            fh->switchableMotionMode = GetBit();
#endif

#if UMC_AV1_DECODER_REV >= 5000
            if (FrameMightUsePrevFrameMVs(fh) && sh->enable_order_hint)
#else
            if (FrameMightUsePrevFrameMVs(fh))
#endif
            {
                const Ipp32u useRefFrameMVs = GetBit();
                fh->usePrevMVs = useRefFrameMVs && FrameCanUsePrevFrameMVs(fh, prev_fh);
            }
        }

        if (!fh->width || !fh->height)
            throw av1_exception(UMC::UMC_ERR_FAILED);

#if UMC_AV1_DECODER_REV >= 5000
        const bool largeScaleTile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        if (!(fh->errorResilientMode || largeScaleTile))
        {
            fh->refreshFrameContext = GetBit()
                ? REFRESH_FRAME_CONTEXT_DISABLED
                : REFRESH_FRAME_CONTEXT_BACKWARD;
        }
        else
        {
            fh->refreshFrameContext = REFRESH_FRAME_CONTEXT_DISABLED;
        }
#else // UMC_AV1_DECODER_REV >= 5000
        if (!fh->errorResilientMode)
        {
            fh->refreshFrameContext = GetBit()
                ? REFRESH_FRAME_CONTEXT_FORWARD
                : REFRESH_FRAME_CONTEXT_BACKWARD;
        }
        else
        {
            fh->refreshFrameContext = REFRESH_FRAME_CONTEXT_FORWARD;
        }
#endif // UMC_AV1_DECODER_REV >= 5000

#if UMC_AV1_DECODER_REV >= 5000
        if (!IsFrameResilent(fh))
            fh->primaryRefFrame = GetBits(PRIMARY_REF_BITS);
#else
        // This flag will be overridden by the call to vp9_setup_past_independence
        // below, forcing the use of context 0 for those frame types.
        fh->frameContextIdx = GetBits(FRAME_CONTEXTS_LOG2);
#endif // UMC_AV1_DECODER_REV >= 5000

        if (IsFrameResilent(fh))
        {
            SetupPastIndependence(*fh);

#if UMC_AV1_DECODER_REV < 5000
            if (fh->frameType != KEY_FRAME)
            {
                InheritFromKeyFrame(fh, prev_fh);
            }
#endif
        }
        else
        {
            InheritFromPrevFrame(fh, prev_fh);
        }

#if UMC_AV1_DECODER_REV < 5000
        av1_setup_loop_filter(this, fh);
#endif

#if UMC_AV1_DECODER_REV >= 5000
        fh->numPlanes = av1_num_planes(sh);
        av1_setup_quantization(this, sh, fh);
#else
        av1_setup_quantization(this, fh);
#endif

        av1_setup_segmentation(this, fh);

        fh->deltaQRes = 1;
        fh->deltaLFRes = 1;
        if (fh->baseQIndex > 0)
            fh->deltaQPresentFlag = GetBit();

        if (fh->deltaQPresentFlag)
        {
            fh->deltaQRes = 1 << GetBits(2);
#if UMC_AV1_DECODER_REV >= 5000
            if (!fh->allowIntraBC || !NO_FILTER_FOR_IBC)
#endif
                fh->deltaLFPresentFlag = GetBit();

            if (fh->deltaLFPresentFlag)
            {
                fh->deltaLFRes = 1 << GetBits(2);
                fh->deltaLFMulti = GetBit();
            }
        }

        bool allLosless = IsAllLosless(fh);

        if (allLosless == false)
        {
#if UMC_AV1_DECODER_REV >= 5000
            av1_setup_loop_filter(this, fh);
#endif

            av1_read_cdef(this, fh);

#if UMC_AV1_DECODER_REV >= 5000
            av1_decode_restoration_mode(this, sh, fh);
#endif
        }

#if UMC_AV1_DECODER_REV < 5000
        av1_decode_restoration_mode(this, fh);
#endif

        av1_read_tx_mode(this, fh, allLosless);

        av1_read_frame_reference_mode(this, fh, frameDpb);

#if UMC_AV1_DECODER_REV >= 5000
        fh->skipModeFlag = av1_is_skip_mode_allowed(fh, sh, frameDpb) ? GetBit() : 0;
#endif // UMC_AV1_DECODER_REV >= 5000

        av1_read_compound_tools(this, fh);

        fh->reducedTxSetUsed = GetBit();

        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            fh->globalMotion[i] = default_warp_params;
        }

        if (!IsFrameIntraOnly(fh))
        {
            // read_global_motion()
            mfxI32 frame;
            for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame)
            {
                if (!prev_fh)
                    throw av1_exception(UMC::UMC_ERR_NULL_PTR);

                WarpedMotionParams const *ref_params = fh->errorResilientMode ?
                    &default_warp_params : &prev_fh->globalMotion[frame];
                WarpedMotionParams* params = &fh->globalMotion[frame];
                av1_read_global_motion_params(params, ref_params, this, fh);
            }
        }

#if UMC_AV1_DECODER_REV >= 5000
        fh->showableFrame = fh->showFrame ? 0 : GetBit();
        if (fh->showFrame || fh->showableFrame)
        {
            // read_film_grain()
            if (sh->film_grain_param_present)
                av1_read_film_grain_params(&fh->filmGrainParams, this, sh, fh);
            else
                memset(&fh->filmGrainParams, 0, sizeof(fh->filmGrainParams));

            fh->filmGrainParams.bit_depth = sh->bit_depth;
        }
#endif // UMC_AV1_DECODER_REV >= 5000

        av1_read_tile_info(this, fh);

        fh->frameHeaderLength = Ipp32u(BitsDecoded() / 8 + (BitsDecoded() % 8 > 0));
        fh->frameDataSize = m_maxBsSize; // TODO: [Global] check if m_maxBsSize can represent more than one frame and fix the code respectively if it can

        // code below isn't used so far. Need to refactor it based on new filter_level syntax/semantics in AV1 uncompressed header.
        // TODO: [Global] modify and uncomment code below once driver will have support of multiple filter levels.
        /*
        if (fh->lf.filterLevel)
        {
            const Ipp32s scale = 1 << (fh->lf.filterLevel >> 5);

            LoopFilterInfo & lf_info = fh->lf_info;

            for (Ipp8u segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
            {
                Ipp32s segmentFilterLevel = fh->lf.filterLevel;
                if (IsSegFeatureActive(fh->segmentation, segmentId, SEG_LVL_ALT_LF))
                {
                    const Ipp32s data = GetSegData(fh->segmentation, segmentId, SEG_LVL_ALT_LF);
                    segmentFilterLevel = clamp(fh->segmentation.absDelta == SEGMENT_ABSDATA ? data : fh->lf.filterLevel + data,
                        0,
                        MAX_LOOP_FILTER);
                }

                if (!fh->lf.modeRefDeltaEnabled)
                {
                    memset(lf_info.level[segmentId], segmentFilterLevel, sizeof(lf_info.level[segmentId]) );
                }
                else
                {
                    const Ipp32s intra_lvl = segmentFilterLevel + fh->lf.refDeltas[INTRA_FRAME] * scale;
                    lf_info.level[segmentId][INTRA_FRAME][0] = (Ipp8u)clamp(intra_lvl, 0, MAX_LOOP_FILTER);

                    for (Ipp8u ref = LAST_FRAME; ref < MAX_REF_FRAMES; ++ref)
                    {
                        for (Ipp8u mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                        {
                            const Ipp32s inter_lvl = segmentFilterLevel + fh->lf.refDeltas[ref] * scale
                                + fh->lf.modeDeltas[mode] * scale;
                            lf_info.level[segmentId][ref][mode] = (Ipp8u)clamp(inter_lvl, 0, MAX_LOOP_FILTER);
                        }
                    }
                }
            }
        }
        */

        AV1D_LOG("[-]: %d", (mfxU32)BitsDecoded());
    }

} // namespace UMC_AV1_DECODER

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
