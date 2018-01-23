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
#include "umc_av1_bitstream.h"
#include "umc_av1_utils.h"
#include "umc_av1_frame.h"

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
        Ipp32u profile = bs->GetBit();
        profile |= bs->GetBit() << 1;
        if (profile > 2)
            profile += bs->GetBit();

        return profile;
    }

    inline
    void av1_bitdepth_colorspace_sampling(AV1Bitstream* bs, FrameHeader* fh)
    {
        GetBitDepthAndColorSpace(bs, fh);
    }

    inline
    void av1_get_render_size(AV1Bitstream* bs, FrameHeader* fh)
    {
        GetDisplaySize(bs, fh);
    }

    inline
    void av1_setup_loop_filter(AV1Bitstream* bs, FrameHeader* fh)
    {
        fh->lf.filterLevel = bs->GetBits(6);
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
    }

    inline
    void av1_read_cdef(AV1Bitstream* bs, FrameHeader* fh)
    {
        memset(fh->cdefStrength, 0, sizeof(fh->cdefStrength));
        memset(fh->cdefUVStrength, 0, sizeof(fh->cdefUVStrength));
#if UMC_AV1_DECODER_REV == 0
        fh->cdefDeringDamping = bs->GetBit() + 5;
        fh->cdefClpfDamping = bs->GetBits(2) + 3;
        const mfxU32 cdefBits = bs->GetBits(2);
        fh->nbCdefStrengths = 1 << cdefBits;
        for (Ipp8u i = 0; i < fh->nbCdefStrengths; i++) {
            fh->cdefStrength[i] = bs->GetBits(7);
            fh->cdefUVStrength[i] = bs->GetBits(7);
        }
#else
        fh->cdefPriDamping = fh->cdefSecDamping = bs->GetBits(2) + 3;
        const mfxU32 cdefBits = bs->GetBits(2);
        mfxU32 nbCdefStrengths = 1 << cdefBits;
        for (Ipp8u i = 0; i < nbCdefStrengths; i++) {
            fh->cdefStrength[i] = bs->GetBits(7);
            fh->cdefUVStrength[i] = fh->subsamplingX == fh->subsamplingY ?
                bs->GetBits(7) : 0;
        }
#endif
    }

    inline
    void av1_read_tx_mode(AV1Bitstream* bs, FrameHeader* fh, bool allLosless)
    {
        if (allLosless)
            fh->txMode = ONLY_4X4;
        else
            fh->txMode = bs->GetBit() ? TX_MODE_SELECT : (TX_MODE)bs->GetBits(2);
    }

    inline
    void av1_read_frame_reference_mode(AV1Bitstream* bs, FrameHeader* fh)
    {
        fh->referenceMode = SINGLE_REFERENCE;
        bool compoundRefAllowed = false;
#if UMC_AV1_DECODER_REV == 0
        for (Ipp8u i = 1; i < INTER_REFS; ++i)
        {
            if (fh->refFrameSignBias[i - 1] != fh->refFrameSignBias[i])
                compoundRefAllowed = true;
        }
#else
        compoundRefAllowed = !IsFrameIntraOnly(fh);
#endif
        if (compoundRefAllowed)
        {
            fh->referenceMode = bs->GetBit() ? REFERENCE_MODE_SELECT :
                bs->GetBit() ? COMPOUND_REFERENCE : SINGLE_REFERENCE;
        }
    }

#if UMC_AV1_DECODER_REV >= 251
    inline
    void av1_read_compound_tools(AV1Bitstream* bs, FrameHeader* fh)
    {
        if (!IsFrameIntraOnly(fh) && fh->referenceMode != COMPOUND_REFERENCE)
        {
            fh->allowInterIntraCompound = bs->GetBit();
        }
        if (!IsFrameIntraOnly(fh) && fh->referenceMode != SINGLE_REFERENCE)
        {
            fh->allowMaskedCompound = bs->GetBit();
        }
    }
#endif

    inline
    int read_inv_signed_literal(AV1Bitstream* bs, int bits) {
        const unsigned sign = bs->GetBit();
        const unsigned literal = bs->GetBits(bits);
        if (sign == 0)
            return literal; // if positive: literal
        else
            return literal - (1 << bits); // if negative: complement to literal with respect to 2^bits
    }

    inline
    void av1_setup_quantization(AV1Bitstream* bs, FrameHeader* fh)
    {
        fh->baseQIndex = bs->GetBits(QINDEX_BITS);

        if (bs->GetBit())
            fh->y_dc_delta_q = read_inv_signed_literal(bs, 6);
        else
            fh->y_dc_delta_q = 0;

        if (bs->GetBit())
            fh->uv_dc_delta_q = read_inv_signed_literal(bs, 6);
        else
            fh->uv_dc_delta_q = 0;

        if (bs->GetBit())
            fh->uv_ac_delta_q = read_inv_signed_literal(bs, 6);
        else
            fh->uv_ac_delta_q = 0;

        fh->lossless = (0 == fh->baseQIndex &&
            0 == fh->y_dc_delta_q &&
            0 == fh->uv_dc_delta_q &&
            0 == fh->uv_ac_delta_q);

#if UMC_AV1_DECODER_REV >= 251
        fh->useQMatrix = bs->GetBit();
        if (fh->useQMatrix) {
            fh->minQMLevel = bs->GetBits(QM_LEVEL_BITS);
            fh->maxQMLevel = bs->GetBits(QM_LEVEL_BITS);
        }
#endif
    }

    inline
    bool av1_setup_segmentation(AV1Bitstream* bs, FrameHeader* fh)
    {
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

                Ipp32s data = 0;
                mfxU32 nBits = 0;
                for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                {
                    for (Ipp8u j = 0; j < SEG_LVL_MAX; ++j)
                    {
                        data = 0;
                        if (bs->GetBit()) // feature_enabled
                        {
                            if (j == SEG_LVL_ALT_Q)
                                segmentQuantizerActive = true;
                            EnableSegFeature(fh->segmentation, i, (SEG_LVL_FEATURES)j);

                            nBits = GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                            data = bs->GetBits(nBits);
                            data = data > SEG_FEATURE_DATA_MAX[j] ? SEG_FEATURE_DATA_MAX[j] : data;

                            if (IsSegFeatureSigned((SEG_LVL_FEATURES)j))
                                data = bs->GetBit() ? -data : data;
                        }

                        SetSegData(fh->segmentation, i, (SEG_LVL_FEATURES)j, data);
                    }
                }
            }
        }

        return segmentQuantizerActive;
    }

#if UMC_AV1_DECODER_REV >= 251
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
#endif

#if UMC_AV1_DECODER_REV >= 251
    const Ipp8u MIN_TILE_WIDTH_MAX_SB = 4;
    const Ipp8u MAX_TILE_WIDTH_MAX_SB = 64;

    void AV1GetTileNBits(const Ipp32s miCols, Ipp32s & minLog2TileCols, Ipp32s & maxLog2TileCols)
    {
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
#endif

    inline
    void av1_read_tile_info(AV1Bitstream* bs, FrameHeader* fh)
    {
        const Ipp32u alignedWidth = ALIGN_POWER_OF_TWO(fh->width, MI_SIZE_LOG2);
        int minLog2TileColumns, maxLog2TileColumns, maxOnes;
        const Ipp32u miCols = alignedWidth >> MI_SIZE_LOG2;
#if UMC_AV1_DECODER_REV == 0
        GetTileNBits(miCols, minLog2TileColumns, maxLog2TileColumns);
#else
        AV1GetTileNBits(miCols, minLog2TileColumns, maxLog2TileColumns);
#endif
        maxOnes = maxLog2TileColumns - minLog2TileColumns;
        fh->log2TileColumns = minLog2TileColumns;
        while (maxOnes-- && bs->GetBit())
            fh->log2TileColumns++;

        fh->log2TileRows = bs->GetBit();
        if (fh->log2TileRows)
            fh->log2TileRows += bs->GetBit();
#if UMC_AV1_DECODER_REV >= 251
        fh->tileCols = get_num_tiles(miCols, fh->log2TileColumns);
        const Ipp32u alignedHeight = ALIGN_POWER_OF_TWO(fh->height, MI_SIZE_LOG2);
        const Ipp32u miRows = alignedHeight >> MI_SIZE_LOG2;
        fh->tileRows = get_num_tiles(miRows, fh->log2TileRows);
#endif

#if UMC_AV1_DECODER_REV <= 251
        fh->loopFilterAcrossTilesEnabled = bs->GetBit();
        fh->tileSizeBytes = bs->GetBits(2) + 1;

        // read_tile_group_range()
        const mfxU32 numBits = fh->log2TileRows + fh->log2TileColumns;
        bs->GetBits(numBits); // tg_start
        bs->GetBits(numBits); // tg_size
#endif
    }

#if UMC_AV1_DECODER_REV >= 251
    inline Ipp16u inv_recenter_non_neg(Ipp16u r, Ipp16u v) {
        if (v > (r << 1))
            return v;
        else if ((v & 1) == 0)
            return (v >> 1) + r;
        else
            return r - ((v + 1) >> 1);
    }

    inline Ipp16u inv_recenter_finite_non_neg(Ipp16u n, Ipp16u r, Ipp16u v) {
        if ((r << 1) <= n) {
            return inv_recenter_non_neg(r, v);
        }
        else {
            return n - 1 - inv_recenter_non_neg(n - 1 - r, v);
        }
    }

    const Ipp8u WARPEDMODEL_PREC_BITS = 16;
    const Ipp8u WARPEDMODEL_ROW3HOMO_PREC_BITS = 16;

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
    const Ipp8s GM_ROW3HOMO_PREC_DIFF = WARPEDMODEL_ROW3HOMO_PREC_BITS - GM_ROW3HOMO_PREC_BITS;
    const Ipp8u GM_ROW3HOMO_DECODE_FACTOR = 1 << GM_ROW3HOMO_PREC_DIFF;

    const Ipp16u GM_TRANS_MAX = 1 << GM_ABS_TRANS_BITS;
    const Ipp16u GM_ALPHA_MAX = 1 << GM_ABS_ALPHA_BITS;
    const Ipp16u GM_ROW3HOMO_MAX = 1 << GM_ABS_ROW3HOMO_BITS;

    const Ipp16s GM_TRANS_MIN = -GM_TRANS_MAX;
    const Ipp16s GM_ALPHA_MIN = -GM_ALPHA_MAX;
    const Ipp16s GM_ROW3HOMO_MIN = -GM_ROW3HOMO_MAX;

    const WarpedMotionParams default_warp_params = {
        IDENTITY,
        { 0, 0, (1 << WARPEDMODEL_PREC_BITS), 0, 0, (1 << WARPEDMODEL_PREC_BITS), 0,
        0 },
        0, 0, 0, 0
    };

    inline Ipp8u GetMSB(Ipp32u n)
    {
        if (n == 0)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        Ipp8u pos = 0;
        while (n > 1)
        {
            pos++;
            n >>= 1;
        }

        return pos;
    }

    inline Ipp16u read_primitive_quniform(AV1Bitstream* bs, Ipp16u n)
    {
        if (n <= 1) return 0;
        Ipp8u l = GetMSB(n - 1) + 1;
        const int m = (1 << l) - n;
        const int v = bs->GetBits(l - 1);
        const int result = v < m ? v : (v << 1) - m + bs->GetBit();
        return static_cast<Ipp16u>(result);
    }

    inline Ipp16u read_primitive_subexpfin(AV1Bitstream* bs, Ipp16u n, Ipp16u k)
    {
        int i = 0;
        int mk = 0;
        Ipp16u v;
        while (1) {
            int b = (i ? k + i - 1 : k);
            int a = (1 << b);
            if (n <= mk + 3 * a) {
                v = static_cast<Ipp16u>(read_primitive_quniform(bs, static_cast<Ipp16u>(n - mk)) + mk);
                break;
            }
            else {
                if (bs->GetBit()) {
                    i = i + 1;
                    mk += a;
                }
                else {
                    v = static_cast<Ipp16u>(bs->GetBits(b) + mk);
                    break;
                }
            }
        }
        return v;
    }

    inline Ipp16u read_primitive_refsubexpfin(AV1Bitstream* bs, Ipp16u n, Ipp16u k, Ipp16u ref)
    {
        return inv_recenter_finite_non_neg(n, ref,
            read_primitive_subexpfin(bs, n, k));
    }

    inline Ipp16s read_signed_primitive_refsubexpfin(AV1Bitstream* bs, Ipp16u n, Ipp16u k, Ipp16s ref) {
        ref += n - 1;
        const Ipp16u scaled_n = (n << 1) - 1;
        return read_primitive_refsubexpfin(bs, scaled_n, k, ref) - n + 1;
    }

    inline
    void av1_read_global_motion_params(WarpedMotionParams* params,
                                       WarpedMotionParams const* ref_params,
                                       AV1Bitstream* bs, FrameHeader* fh)
    {
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
    }
#endif

#if UMC_AV1_DECODER_REV >= 251
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
#endif

    void AV1Bitstream::GetSequenceHeader(SequenceHeader* sh)
    {
#if UMC_AV1_DECODER_REV >= 251
        sh->frame_id_numbers_present_flag = GetBit();
        if (sh->frame_id_numbers_present_flag) {
            sh->frame_id_length_minus7 = GetBits(4);
            sh->delta_frame_id_length_minus2 = GetBits(4);
        }
#else
        /* Placeholder for actually reading from the bitstream */
        sh->frame_id_numbers_present_flag = FRAME_ID_NUMBERS_PRESENT_FLAG;
        sh->frame_id_length_minus7 = FRAME_ID_LENGTH_MINUS7;
        sh->delta_frame_id_length_minus2 = DELTA_FRAME_ID_LENGTH_MINUS2;
#endif
    }

    void AV1Bitstream::GetFrameSizeWithRefs(FrameHeader* fh)
    {
        bool bFound = false;
        for (Ipp8u i = 0; i < INTER_REFS; ++i)
        {
            if (GetBit())
            {
                bFound = true;
                fh->width = fh->sizesOfRefFrame[fh->activeRefIdx[i]].width;
                fh->height = fh->sizesOfRefFrame[fh->activeRefIdx[i]].height;
                break;
            }
        }

        if (!bFound)
        {
            fh->width = GetBits(16) + 1;
            fh->height = GetBits(16) + 1;

            av1_get_render_size(this, fh);
        }
    }

    void AV1Bitstream::GetFrameHeaderPart1(FrameHeader* fh, SequenceHeader* sh)
    {
        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

#if UMC_AV1_DECODER_REV >= 251
        Ipp32u sizes[8];
        if (ReadSuperFrameIndex(sizes))
            throw av1_exception(UMC::UMC_ERR_FAILED); // no support for super frame indexes
#endif

        if (FRAME_MARKER != GetBits(2))
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

        fh->profile = av1_profile(this);
        if (fh->profile >= 4)
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

        fh->show_existing_frame = GetBit();
        if (fh->show_existing_frame)
        {
            fh->frame_to_show = GetBits(3);
            if (sh->frame_id_numbers_present_flag && fh->use_reference_buffer)
            {
                int const frame_id_len = sh->frame_id_length_minus7 + 7;
                fh->display_frame_id = GetBits(frame_id_len);
            }

            fh->width = fh->sizesOfRefFrame[fh->frame_to_show].width;
            fh->height = fh->sizesOfRefFrame[fh->frame_to_show].height;
            fh->showFrame = 1;

            return;
        }

        fh->frameType = static_cast<VP9_FRAME_TYPE>(GetBit());
        fh->showFrame = GetBit();
#if UMC_AV1_DECODER_REV > 0
#if UMC_AV1_DECODER_REV <= 251
        if (fh->frameType != KEY_FRAME)
        {
            fh->intraOnly = fh->showFrame ? 0 : GetBit();
        }
#else
        fh->intraOnly = fh->showFrame ? 0 : GetBit();
#endif
#endif
        fh->errorResilientMode = GetBit();

#if UMC_AV1_DECODER_REV >= 251
        if (IsFrameIntraOnly(fh))
        {
            GetSequenceHeader(sh);
        }
#endif


#if UMC_AV1_DECODER_REV <= 251
        if (sh->frame_id_numbers_present_flag)
        {
            int const frame_id_len = sh->frame_id_length_minus7 + 7;
            fh->display_frame_id = GetBits(frame_id_len);
        }
#else
        if (fh->frameType == KEY_FRAME || fh->intraOnly)
            fh->use_reference_buffer = GetBit();

        if (sh->frame_id_numbers_present_flag && fh->use_reference_buffer)
        {
            int const frame_id_len = sh->frame_id_length_minus7 + 7;
            fh->display_frame_id = GetBits(frame_id_len);
        }
#endif

        if (KEY_FRAME == fh->frameType)
        {
#if UMC_AV1_DECODER_REV == 0
            if (!av1_sync_code(this))
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
#endif

            av1_bitdepth_colorspace_sampling(this, fh);

            fh->refreshFrameFlags = (1 << NUM_REF_FRAMES) - 1;

            for (Ipp8u i = 0; i < INTER_REFS; ++i)
            {
                fh->activeRefIdx[i] = 0;
            }

            GetFrameSize(this, fh);

            fh->allowScreenContentTools = GetBit();
        }
        else
        {
#if UMC_AV1_DECODER_REV == 0
            fh->intraOnly = fh->showFrame ? 0 : GetBit();
#else
            if (fh->intraOnly)
                fh->allowScreenContentTools = GetBit();

            if (IsFrameResilent(fh))
            {
                fh->usePrevMVs = 0;
            }
#endif
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

            if (fh->intraOnly)
            {
#if UMC_AV1_DECODER_REV >= 251
                fh->allowScreenContentTools = GetBit();

                if (!av1_sync_code(this))
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
#endif

                av1_bitdepth_colorspace_sampling(this, fh);

                fh->refreshFrameFlags = (Ipp8u)GetBits(NUM_REF_FRAMES);
                GetFrameSize(this, fh);
            }
            else
            {
                fh->refreshFrameFlags = (Ipp8u)GetBits(NUM_REF_FRAMES);

                for (Ipp8u i = 0; i < INTER_REFS; ++i)
                {
                    Ipp32s const  ref = GetBits(REF_FRAMES_LOG2);
                    fh->activeRefIdx[i] = ref;
                    fh->refFrameSignBias[i] = GetBit();

                    if (sh->frame_id_numbers_present_flag)
                    {
                        int delta_frame_id_len = sh->delta_frame_id_length_minus2 + 2;
                        GetBits(delta_frame_id_len);
                        // TODO: add check of ref frame_id here
                    }
                }

                GetFrameSizeWithRefs(fh);

                fh->allowHighPrecisionMv = GetBit();

                fh->interpFilter = GetBit() ? SWITCHABLE : (INTERP_FILTER)GetBits(LOG2_SWITCHABLE_FILTERS);

#if UMC_AV1_DECODER_REV >= 251
                if (!IsFrameResilent(fh))
                {
                    fh->usePrevMVs = GetBit();
                }
#endif
            }
        }
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
                    segQIndex = clamp(fh->segmentation.absDelta == SEGMENT_ABSDATA ? data : fh->baseQIndex + data, 0, MAXQ);
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

        return qIndexAllZero && fh->y_dc_delta_q == 0 &&
            fh->uv_ac_delta_q == 0 && fh->uv_dc_delta_q == 0;
    }

    inline void CopyBitDepthAndSampling(FrameHeader* fh, FrameHeader const* prev_fh)
    {
        fh->bit_depth = prev_fh->bit_depth;
        fh->subsamplingX = prev_fh->subsamplingX;
        fh->subsamplingY = prev_fh->subsamplingY;
    }

    void AV1Bitstream::GetFrameHeaderFull(FrameHeader* fh, SequenceHeader const* sh, FrameHeader const* prev_fh)
    {
        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        if (!fh->width || !fh->height)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        if (fh->frameType != KEY_FRAME)
            CopyBitDepthAndSampling(fh, prev_fh);

        if (!fh->errorResilientMode)
        {
            fh->refreshFrameContext = GetBit()
                ? REFRESH_FRAME_CONTEXT_FORWARD
                : REFRESH_FRAME_CONTEXT_BACKWARD;
#if UMC_AV1_DECODER_REV > 251
            fh->frameParallelDecodingMode = GetBit();
#endif
        }
        else
        {
            fh->refreshFrameContext = REFRESH_FRAME_CONTEXT_FORWARD;
        }

        // This flag will be overridden by the call to vp9_setup_past_independence
        // below, forcing the use of context 0 for those frame types.
        fh->frameContextIdx = GetBits(FRAME_CONTEXTS_LOG2);

        if (IsFrameResilent(fh))
        {
            SetupPastIndependence(*fh);
        }

        av1_setup_loop_filter(this, fh);

#if UMC_AV1_DECODER_REV == 0
        av1_read_cdef(this, fh);
#endif

        av1_setup_quantization(this, fh);

        bool segmentQuantizerActive = av1_setup_segmentation(this, fh);

        fh->deltaQRes = 1;
        fh->deltaLFRes = 1;
        if (segmentQuantizerActive == false && fh->baseQIndex > 0)
            fh->deltaQPresentFlag = GetBit();

        if (fh->deltaQPresentFlag)
        {
            fh->deltaQRes = 1 << GetBits(2);
            fh->deltaLFPresentFlag = GetBit();

            if (fh->deltaLFPresentFlag)
                fh->deltaLFRes = 1 << GetBits(2);
        }

        bool allLosless = IsAllLosless(fh);

#if UMC_AV1_DECODER_REV >= 251
        if (allLosless == false)
            av1_read_cdef(this, fh);
#endif

        av1_read_tx_mode(this, fh, allLosless);

        av1_read_frame_reference_mode(this, fh);

#if UMC_AV1_DECODER_REV >= 251
        av1_read_compound_tools(this, fh);
#endif

        fh->reducedTxSetUsed = GetBit();

#if UMC_AV1_DECODER_REV >= 251
        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            fh->global_motion[i] = default_warp_params;
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
                    &default_warp_params : &prev_fh->global_motion[frame];
                WarpedMotionParams* params = &fh->global_motion[frame];
                av1_read_global_motion_params(params, ref_params, this, fh);
            }
        }
#else
        prev_fh;
#endif

        av1_read_tile_info(this, fh);

#if UMC_AV1_DECODER_REV == 0
        fh->firstPartitionSize = GetBits(16);
        if (!fh->firstPartitionSize)
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
#endif

        fh->frameHeaderLength = Ipp32u(BitsDecoded() / 8 + (BitsDecoded() % 8 > 0));
        fh->frameDataSize = m_maxBsSize; // TODO: check if m_maxBsSize can represent more than one frame and fix the code respectively if it can

        // vp9_loop_filter_frame_init()
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
    }

} // namespace UMC_AV1_DECODER

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
