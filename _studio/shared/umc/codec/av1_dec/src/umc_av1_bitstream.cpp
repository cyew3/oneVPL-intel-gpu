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

#include <algorithm>
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

#if UMC_AV1_DECODER_REV >= 8500
    inline void av1_decoder_model_info(AV1Bitstream* bs, DecoderModelInfo* info)
    {
        info->buffer_delay_length_minus_1 = bs->GetBits(5);
        info->num_units_in_decoding_tick = bs->GetBits(32);
        info->buffer_removal_time_length_minus_1 = bs->GetBits(5);
        info->frame_presentation_time_length_minus_1 = bs->GetBits(5);
    }

    inline void av1_operating_parameters_info(AV1Bitstream* bs, OperatingParametersInfo* info, Ipp32u length)
    {
        info->decoder_buffer_delay = bs->GetBits(length);
        info->encoder_buffer_delay = bs->GetBits(length);
        info->low_delay_mode_flag = bs->GetBit();
    }
#endif

    inline void av1_timing_info(AV1Bitstream* bs, TimingInfo* info)
    {
        info->num_units_in_display_tick = bs->GetBits(32);
        info->time_scale = bs->GetBits(32);
        info->equal_picture_interval = bs->GetBit();
        if (info->equal_picture_interval)
            info->num_ticks_per_picture_minus_1 = read_uvlc(bs);
    }

    static void av1_color_config(AV1Bitstream* bs, ColorConfig* config, Ipp32u profile)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        config->BitDepth = bs->GetBit() ? 10 : 8;
        if (profile == 2 && config->BitDepth != 8)
            config->BitDepth = bs->GetBit() ? 12 : 10;

        config->mono_chrome = profile != 1 ? bs->GetBit() : 0;

        Ipp32u color_description_present_flag = bs->GetBit();
        if (color_description_present_flag)
        {
            config->color_primaries = bs->GetBits(8);
            config->transfer_characteristics = bs->GetBits(8);
            config->matrix_coefficients = bs->GetBits(8);
        }
        else
        {
            config->color_primaries = AOM_CICP_CP_UNSPECIFIED;
            config->transfer_characteristics = AOM_CICP_TC_UNSPECIFIED;
            config->matrix_coefficients = AOM_CICP_MC_UNSPECIFIED;
        }

        if (config->mono_chrome)
        {
#if UMC_AV1_DECODER_REV >= 8500
            config->color_range = bs->GetBit();
#else
            config->color_range = AOM_CR_FULL_RANGE;
#endif
            config->subsampling_y = config->subsampling_x = 1;
            config->chroma_sample_position = AOM_CSP_UNKNOWN;
            config->separate_uv_delta_q = 0;
            return;
        }

        if (config->color_primaries == AOM_CICP_CP_BT_709 &&
            config->transfer_characteristics == AOM_CICP_TC_SRGB &&
            config->matrix_coefficients == AOM_CICP_MC_IDENTITY)
        {
#if UMC_AV1_DECODER_REV >= 8500
            config->color_range = AOM_CR_FULL_RANGE;
#endif
            config->subsampling_y = config->subsampling_x = 0;
            if (!(profile == 1 || (profile == 2 && config->BitDepth == 12)))
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        else
        {
            // [16,235] (including xvycc) vs [0,255] range
            config->color_range = bs->GetBit();
            if (profile == 0) // 420 only
                config->subsampling_x = config->subsampling_y = 1;
            else if (profile == 1) // 444 only
                config->subsampling_x = config->subsampling_y = 0;
            else if (profile == 2)
            {
                if (config->BitDepth == 12)
                {
                    config->subsampling_x = bs->GetBit();
                    if (config->subsampling_x == 0)
                        config->subsampling_y = 0;  // 444
                    else
                        config->subsampling_y = bs->GetBit();  // 422 or 420
                }
                else
                {
                    // 422
                    config->subsampling_x = 1;
                    config->subsampling_y = 0;
                }
            }
            if (config->subsampling_x == 1 && config->subsampling_y == 1)
                config->chroma_sample_position = bs->GetBits(2);
        }

        config->separate_uv_delta_q = bs->GetBit();

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline void av1_set_mi_and_sb_size(FrameHeader* fh, SequenceHeader const* sh)
    {
        // set frame width and heignt in MI
        const Ipp32u alignedWidth = ALIGN_POWER_OF_TWO(fh->FrameWidth, 3);
        const int alignedHeight = ALIGN_POWER_OF_TWO(fh->FrameHeight, 3);
        fh->MiCols = alignedWidth >> MI_SIZE_LOG2;
        fh->MiRows = alignedHeight >> MI_SIZE_LOG2;

        // set frame width and height in SB
        const Ipp32u mibSizeLog2 = sh->sbSize == BLOCK_64X64 ? 4 : 5;
        const Ipp32u widthMI = ALIGN_POWER_OF_TWO(fh->MiCols, mibSizeLog2);
        const Ipp32u heightMI = ALIGN_POWER_OF_TWO(fh->MiRows, mibSizeLog2);
        fh->sbCols = widthMI >> mibSizeLog2;
        fh->sbRows = heightMI >> mibSizeLog2;
    }

    static void av1_setup_superres(AV1Bitstream* bs, FrameHeader* fh)
    {
        fh->UpscaledWidth = fh->FrameWidth;
        Ipp32u& denom = fh->SuperresDenom;
        if (bs->GetBit())
        {
            denom = bs->GetBits(SUPERRES_SCALE_BITS);
            denom += SUPERRES_SCALE_DENOMINATOR_MIN;
            if (denom != SCALE_NUMERATOR)
                fh->FrameWidth = (fh->FrameWidth * SCALE_NUMERATOR + denom / 2) / (denom);
        }
        else
            fh->SuperresDenom = SCALE_NUMERATOR;
    }

    inline
    void av1_get_render_size(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        fh->RenderWidth = fh->UpscaledWidth;
        fh->RenderHeight = fh->FrameHeight;

        if (bs->GetBit())
        {
            fh->RenderWidth = bs->GetBits(16) + 1;
            fh->RenderHeight = bs->GetBits(16) + 1;
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_get_frame_size(AV1Bitstream* bs, FrameHeader* fh, SequenceHeader const * sh = 0)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        if (fh->frame_size_override_flag)
        {
            fh->FrameWidth = bs->GetBits(sh->frame_width_bits) + 1;
            fh->FrameHeight = bs->GetBits(sh->frame_height_bits) + 1;
            if (fh->FrameWidth > sh->max_frame_width || fh->FrameHeight > sh->max_frame_height)
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        else
        {
            fh->FrameWidth = sh->max_frame_width;
            fh->FrameHeight = sh->max_frame_height;
        }

        av1_setup_superres(bs, fh);
        av1_set_mi_and_sb_size(fh, sh);
        av1_get_render_size(bs, fh);

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline void av1_get_frame_sizes_with_refs(AV1Bitstream* bs, FrameHeader* fh, DPBType const& frameDpb, SequenceHeader const * sh = 0)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        bool bFound = false;
        for (Ipp8u i = 0; i < INTER_REFS; ++i)
        {
            if (bs->GetBit())
            {
                bFound = true;
                FrameHeader const& refHrd = frameDpb[fh->ref_frame_idx[i]]->GetFrameHeader();
                fh->FrameWidth = refHrd.FrameWidth;
                fh->FrameHeight = refHrd.FrameHeight;
                // TODO: [Rev0.5] add getting of RenderWidth/RenderHeight
                av1_setup_superres(bs, fh);
                break;
            }
        }

        if (!bFound)
        {
            fh->FrameWidth = bs->GetBits(sh->frame_width_bits) + 1;
            fh->FrameHeight = bs->GetBits(sh->frame_height_bits) + 1;
            av1_setup_superres(bs, fh);
            av1_get_render_size(bs, fh);
        }

        av1_set_mi_and_sb_size(fh, sh);

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_setup_loop_filter(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        if (fh->allow_intrabc && NO_FILTER_FOR_IBC)
            return;

        fh->loop_filter_params.loop_filter_level[0] = bs->GetBits(6);
        fh->loop_filter_params.loop_filter_level[1] = bs->GetBits(6);

        if (fh->NumPlanes > 1)
        {
            if (fh->loop_filter_params.loop_filter_level[0] || fh->loop_filter_params.loop_filter_level[1])
            {
                fh->loop_filter_params.loop_filter_level[2] = bs->GetBits(6);
                fh->loop_filter_params.loop_filter_level[3] = bs->GetBits(6);
            }
        }

        fh->loop_filter_params.loop_filter_sharpness = bs->GetBits(3);

        fh->loop_filter_params.loop_filter_delta_update = 0;

        fh->loop_filter_params.loop_filter_delta_enabled = (Ipp8u)bs->GetBit();
        if (fh->loop_filter_params.loop_filter_delta_enabled)
        {
            fh->loop_filter_params.loop_filter_delta_update = (Ipp8u)bs->GetBit();

            if (fh->loop_filter_params.loop_filter_delta_update)
            {
                Ipp8u bits = 6;
                for (Ipp32u i = 0; i < TOTAL_REFS; i++)
                {
                    if (bs->GetBit())
                    {
                        const Ipp8u nbits = sizeof(unsigned) * 8 - bits - 1;
                        const Ipp32u value = (unsigned)bs->GetBits(bits + 1) << nbits;
                        fh->loop_filter_params.loop_filter_ref_deltas[i] = (Ipp8s)(((Ipp32s)value) >> nbits);
                    }
                }

                for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
                {
                    if (bs->GetBit())
                    {
                        const Ipp8u nbits = sizeof(unsigned) * 8 - bits - 1;
                        const Ipp32u value = (unsigned)bs->GetBits(bits + 1) << nbits;
                        fh->loop_filter_params.loop_filter_mode_deltas[i] = (Ipp8s)(((Ipp32s)value) >> nbits);
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

        if (fh->allow_intrabc && NO_FILTER_FOR_IBC)
        {
            fh->cdef_damping = 3;
            memset(fh->cdef_y_strength, 0, sizeof(fh->cdef_y_strength));
            memset(fh->cdef_uv_strength, 0, sizeof(fh->cdef_uv_strength));
            return;
        }

        fh->cdef_damping = bs->GetBits(2) + 3;
        fh->cdef_bits = bs->GetBits(2);
        mfxU32 nbcdef_y_strengths = 1 << fh->cdef_bits;
        for (Ipp8u i = 0; i < nbcdef_y_strengths; i++)
        {
            fh->cdef_y_strength[i] = bs->GetBits(CDEF_STRENGTH_BITS);
            fh->cdef_uv_strength[i] = fh->NumPlanes > 1 ?
                bs->GetBits(CDEF_STRENGTH_BITS) : 0;
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    Ipp8u av1_num_planes(SequenceHeader const * sh)
    {
        return sh->color_config.mono_chrome ? 1 : MAX_MB_PLANE;
    }

    inline
    void av1_decode_restoration_mode(AV1Bitstream* bs, SequenceHeader const* sh, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        bool allNone = true;;
        bool chromaNone = true;

        if (fh->allow_intrabc && NO_FILTER_FOR_IBC)
            return;

        Ipp32u NumPlanes = fh->NumPlanes;

        for (Ipp8u p = 0; p < NumPlanes; ++p)
        {
            if (bs->GetBit()) {
                fh->lr_type[p] =
                    bs->GetBit() ? RESTORE_SGRPROJ : RESTORE_WIENER;
            }
            else
            {
                fh->lr_type[p] =
                    bs->GetBit() ? RESTORE_SWITCHABLE : RESTORE_NONE;
            }
            if (fh->lr_type[p] != RESTORE_NONE)
            {
                allNone = false;
                if (p != 0)
                    chromaNone = false;
            }
        }

        const Ipp32s size = sh->sbSize == BLOCK_128X128 ? 128 : 64;

        Ipp32s restorationUnitSize[MAX_MB_PLANE];

        if (!allNone)
        {
            for (Ipp8u p = 0; p < NumPlanes; ++p)
                restorationUnitSize[p] = size;

            if (size == 64)
                restorationUnitSize[0] <<= bs->GetBit();

            if (restorationUnitSize[0] > 64)
                restorationUnitSize[0] <<= bs->GetBit();
        }
        else
        {
            for (Ipp8u p = 0; p < NumPlanes; ++p)
                restorationUnitSize[p] = RESTORATION_UNITSIZE_MAX;
        }

        if (NumPlanes > 1)
        {
            int s = std::min(sh->color_config.subsampling_x, sh->color_config.subsampling_y);

            if (s && !chromaNone) {
                restorationUnitSize[1] =
                    restorationUnitSize[0] >> (bs->GetBit() * s);
            }
            else {
                restorationUnitSize[1] =
                    restorationUnitSize[0];
            }
            restorationUnitSize[2] =
                restorationUnitSize[1];
        }

        fh->lr_unit_shift = static_cast<Ipp32u>(log(restorationUnitSize[0] / size) / log(2));
        fh->lr_uv_shift = static_cast<Ipp32u>(log(restorationUnitSize[0] / restorationUnitSize[1]) / log(2));

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_read_tx_mode(AV1Bitstream* bs, FrameHeader* fh, bool allLosless)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        if (allLosless)
            fh->TxMode = ONLY_4X4;
        else
            fh->TxMode = bs->GetBit() ? TX_MODE_SELECT : TX_MODE_LARGEST;

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    bool av1_is_compound_reference_allowed(FrameHeader const* fh, DPBType const& frameDpb)
    {
        if (FrameIsIntra(fh))
            return false;

        // Check whether two different reference frames exist.
        FrameHeader const& refHdr0 = frameDpb[fh->ref_frame_idx[0]]->GetFrameHeader();
        const Ipp32u refOrderHint0 = refHdr0.order_hint;

        for (Ipp32u ref = 1; ref < INTER_REFS; ref++)
        {
            FrameHeader const& refHdr1 = frameDpb[fh->ref_frame_idx[ref]]->GetFrameHeader();
            if (refOrderHint0 != refHdr1.order_hint)
                return true;
        }

        return false;
    }

    inline
    void av1_read_frame_reference_mode(AV1Bitstream* bs, FrameHeader* fh, DPBType const& frameDpb)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        fh->reference_mode = SINGLE_REFERENCE;

        if (av1_is_compound_reference_allowed(fh, frameDpb))
        {
            fh->reference_mode = bs->GetBit() ? REFERENCE_MODE_SELECT :
                SINGLE_REFERENCE;
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    void av1_read_compound_tools(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        if (!FrameIsIntra(fh))
            fh->enable_interintra_compound = bs->GetBit();

        if (!FrameIsIntra(fh) && fh->reference_mode != SINGLE_REFERENCE)
            fh->enable_masked_compound = bs->GetBit();

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline Ipp32s av1_get_relative_dist(SequenceHeader const* sh, const Ipp32u a, const Ipp32u b)
    {
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
        if (!sh->enable_order_hint || fh->error_resilient_mode ||
            FrameIsIntra(fh) || fh->reference_mode == SINGLE_REFERENCE)
            return false;

        const Ipp32s MAX_VALUE = (std::numeric_limits<Ipp32s>::max)();
        const Ipp32s INVALID_IDX = -1;

        Ipp32s refFrameOffset[2] = { -1, MAX_VALUE };
        int refIdx[2] = { INVALID_IDX, INVALID_IDX };

        // Identify the nearest forward and backward references.
        for (Ipp32s i = 0; i < INTER_REFS; ++i)
        {
            FrameHeader const& refHdr = frameDpb[fh->ref_frame_idx[i]]->GetFrameHeader();
            const Ipp32u refOrderHint = refHdr.order_hint;

            if (av1_get_relative_dist(sh, refOrderHint, fh->order_hint) < 0)
            {
                // Forward reference
                if (refFrameOffset[0] == -1 ||
                    av1_get_relative_dist(sh, refOrderHint, refFrameOffset[0]) > 0)
                {
                    refFrameOffset[0] = refOrderHint;
                    refIdx[0] = i;
                }
            }
            else if (av1_get_relative_dist(sh, refOrderHint, fh->order_hint) > 0)
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
                FrameHeader const& refHdr = frameDpb[fh->ref_frame_idx[i]]->GetFrameHeader();
                const Ipp32u refOrderHint = refHdr.order_hint;

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

    inline
    Ipp32s av1_read_q_delta(AV1Bitstream* bs)
    {
        return (bs->GetBit()) ?
            read_inv_signed_literal(bs, 6) : 0;
    }

    inline
    void av1_setup_quantization(AV1Bitstream* bs, SequenceHeader const * sh, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        fh->base_q_idx = bs->GetBits(UMC_VP9_DECODER::QINDEX_BITS);

        fh->DeltaQYDc = av1_read_q_delta(bs);

        if (fh->NumPlanes > 1)
        {
            Ipp32s diffUVDelta = 0;
            if (sh->color_config.separate_uv_delta_q)
                diffUVDelta = bs->GetBit();

            fh->DeltaQUDc = av1_read_q_delta(bs);
            fh->DeltaQUAc = av1_read_q_delta(bs);

            if (diffUVDelta)
            {
                fh->DeltaQVDc = av1_read_q_delta(bs);
                fh->DeltaQVac = av1_read_q_delta(bs);
            }
            else
            {
                fh->DeltaQVDc = fh->DeltaQUDc;
                fh->DeltaQVac = fh->DeltaQUAc;
            }
        }

        fh->lossless = (0 == fh->base_q_idx &&
            0 == fh->DeltaQYDc &&
            0 == fh->DeltaQUDc &&
            0 == fh->DeltaQUAc &&
            0 == fh->DeltaQVDc &&
            0 == fh->DeltaQVac);

        fh->using_qmatrix = bs->GetBit();
        if (fh->using_qmatrix) {
            fh->qm_y = bs->GetBits(QM_LEVEL_BITS);
            fh->qm_u = bs->GetBits(QM_LEVEL_BITS);

            if (!sh->color_config.separate_uv_delta_q)
                fh->qm_v = fh->qm_u;
            else
                fh->qm_v = bs->GetBits(QM_LEVEL_BITS);
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    inline
    bool av1_setup_segmentation(AV1Bitstream* bs, FrameHeader* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        bool segmentQuantizerActive = false;
        fh->segmentation_params.segmentation_update_map = 0;
        fh->segmentation_params.segmentation_update_data = 0;

        fh->segmentation_params.segmentation_enabled = (Ipp8u)bs->GetBit();
        if (fh->segmentation_params.segmentation_enabled)
        {
            // Segmentation map update
            if (FrameIsResilient(fh))
                fh->segmentation_params.segmentation_update_map = 1;
            else
                fh->segmentation_params.segmentation_update_map = (Ipp8u)bs->GetBit();
            if (fh->segmentation_params.segmentation_update_map)
            {
                if (FrameIsResilient(fh))
                    fh->segmentation_params.segmentation_temporal_update = 0;
                else
                    fh->segmentation_params.segmentation_temporal_update = (Ipp8u)bs->GetBit();
            }

            fh->segmentation_params.segmentation_update_data = (Ipp8u)bs->GetBit();
            if (fh->segmentation_params.segmentation_update_data)
            {
                ClearAllSegFeatures(fh->segmentation_params);

                for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                {
                    for (Ipp8u j = 0; j < SEG_LVL_MAX; ++j)
                    {
                        Ipp32s data = 0;
                        if (bs->GetBit()) // feature_enabled
                        {
                            if (j == SEG_LVL_ALT_Q)
                                segmentQuantizerActive = true;
                            EnableSegFeature(fh->segmentation_params, i, (SEG_LVL_FEATURES)j);

                            const Ipp32u nBits = UMC_VP9_DECODER::GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                            const Ipp32u isSigned = IsSegFeatureSigned((SEG_LVL_FEATURES)j);
                            const Ipp32s dataMax = SEG_FEATURE_DATA_MAX[j];
                            const Ipp32s dataMin = -dataMax;

                            if (isSigned)
                                data = read_inv_signed_literal(bs, nBits);
                            else
                                data = bs->GetBits(nBits);

                            data = UMC_VP9_DECODER::clamp(data, dataMin, dataMax);
                        }

                        SetSegData(fh->segmentation_params, i, (SEG_LVL_FEATURES)j, data);
                    }
                }
            }
        }

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
        return segmentQuantizerActive;
    }

    struct TileLimits
    {
        Ipp32u maxTileWidthSB;
        Ipp32u maxTileHeightSB;
        Ipp32u maxTileAreaInSB;

        Ipp32u maxlog2_tile_cols;
        Ipp32u maxlog2_tile_rows;

        Ipp32u minlog2_tile_cols;
        Ipp32u minlog2_tile_rows;

        Ipp32u minLog2Tiles;
    };

    inline Ipp32u av1_tile_log2(Ipp32u blockSize, Ipp32u target)
    {
        Ipp32u k;
        for (k = 0; (blockSize << k) < target; k++) {}
        return k;
    }

    inline void av1_get_tile_limits(FrameHeader const* fh, Ipp32u sbSize, TileLimits* limits)
    {
        const Ipp32u mibSizeLog2 = sbSize == BLOCK_64X64 ? 4 : 5;
        const Ipp32u sbSizeLog2 = mibSizeLog2 + MI_SIZE_LOG2;

        limits->maxTileWidthSB = MAX_TILE_WIDTH >> sbSizeLog2;
        limits->maxTileAreaInSB = MAX_TILE_AREA >> (2 * sbSizeLog2);

        limits->minlog2_tile_cols = av1_tile_log2(limits->maxTileWidthSB,fh->sbCols);
        limits->maxlog2_tile_cols = av1_tile_log2(1, std::min(fh->sbCols, MAX_TILE_COLS));
        limits->maxlog2_tile_rows = av1_tile_log2(1, std::min(fh->sbRows, MAX_TILE_ROWS));
        limits->minLog2Tiles = av1_tile_log2(limits->maxTileAreaInSB, fh->sbCols * fh->sbRows);
        limits->minLog2Tiles = std::max(limits->minLog2Tiles, limits->minlog2_tile_cols);
    }

    static void av1_calculate_tile_cols(FrameHeader* fh, TileLimits* limits)
    {
        Ipp32u i;

        if (fh->uniform_tile_spacing_flag)
        {
            Ipp32u startSB;
            Ipp32u sizeSB = ALIGN_POWER_OF_TWO(fh->sbCols, fh->TileColsLog2);
            sizeSB >>= fh->TileColsLog2;
            VM_ASSERT(sizeSB > 0);
            for (i = 0, startSB = 0; startSB < fh->sbCols; i++)
            {
                fh->SbColStarts[i] = startSB;
                startSB += sizeSB;
            }
            fh->TileCols = i;
            fh->SbColStarts[i] = fh->sbCols;
            limits->minlog2_tile_rows = std::max(static_cast<Ipp32s>(limits->minLog2Tiles - fh->TileColsLog2), 0);
            limits->maxTileHeightSB = fh->sbRows >> limits->minlog2_tile_rows;
        }
        else
        {
            limits->maxTileAreaInSB = (fh->sbRows * fh->sbCols);
            Ipp32u widestTileSB = 1;
            fh->TileColsLog2 = av1_tile_log2(1, fh->TileCols);
            for (i = 0; i < fh->TileCols; i++)
            {
                Ipp32u sizeSB = fh->SbColStarts[i + 1] - fh->SbColStarts[i];
                widestTileSB = std::max(widestTileSB, sizeSB);
            }
            if (limits->minLog2Tiles)
                limits->maxTileAreaInSB >>= (limits->minLog2Tiles + 1);
            limits->maxTileHeightSB = std::max(limits->maxTileAreaInSB / widestTileSB, 1u);
        }
    }

    static void av1_calculate_tile_rows(FrameHeader* fh)
    {
        Ipp32u startSB;
        Ipp32u sizeSB;
        Ipp32u i;

        if (fh->uniform_tile_spacing_flag)
        {
            sizeSB = ALIGN_POWER_OF_TWO(fh->sbRows, fh->TileRowsLog2);
            sizeSB >>= fh->TileRowsLog2;
            VM_ASSERT(sizeSB > 0);
            for (i = 0, startSB = 0; startSB < fh->sbRows; i++)
            {
                fh->SbRowStarts[i] = startSB;
                startSB += sizeSB;
            }
            fh->TileRows = i;
            fh->SbRowStarts[i] = fh->sbRows;
        }
        else
            fh->TileRowsLog2 = av1_tile_log2(1, fh->TileRows);
    }

    static void av1_read_tile_info_max_tile(AV1Bitstream* bs, FrameHeader* fh, Ipp32u sbSize)
    {
        TileLimits limits = {};
        av1_get_tile_limits(fh, sbSize, &limits);

        fh->uniform_tile_spacing_flag = bs->GetBit();

        // Read tile columns
        if (fh->uniform_tile_spacing_flag)
        {
            fh->TileColsLog2 = limits.minlog2_tile_cols;
            while (fh->TileColsLog2 < limits.maxlog2_tile_cols)
            {
                if (!bs->GetBit())
                    break;
                fh->TileColsLog2++;
            }
        }
        else
        {
            Ipp32u i;
            Ipp32u startSB;
            Ipp32u sbCols = fh->sbCols;
            for (i = 0, startSB = 0; sbCols > 0 && i < MAX_TILE_COLS; i++)
            {
                const Ipp32u sizeInSB =
                    1 + read_uniform(bs, std::min(sbCols, limits.maxTileWidthSB));
                fh->SbColStarts[i] = startSB;
                startSB += sizeInSB;
                sbCols -= sizeInSB;
            }
            fh->TileCols = i;
            fh->SbColStarts[i] = startSB + sbCols;
        }
        av1_calculate_tile_cols(fh, &limits);

        // Read tile rows
        if (fh->uniform_tile_spacing_flag)
        {
            fh->TileRowsLog2 = limits.minlog2_tile_rows;
            while (fh->TileRowsLog2 < limits.maxlog2_tile_rows)
            {
                if (!bs->GetBit())
                    break;
                fh->TileRowsLog2++;
            }
        }
        else
        {
            Ipp32u i;
            Ipp32u startSB;
            Ipp32u sbRows = fh->sbRows;
            for (i = 0, startSB = 0; sbRows > 0 && i < MAX_TILE_ROWS; i++)
            {
                const Ipp32u sizeSB =
                    1 + read_uniform(bs, std::min(sbRows, limits.maxTileHeightSB));
                fh->SbRowStarts[i] = startSB;
                startSB += sizeSB;
                sbRows -= sizeSB;
            }
            fh->TileRows = i;
            fh->SbRowStarts[i] = startSB + sbRows;
        }
        av1_calculate_tile_rows(fh);
    }

    inline
    void av1_read_tile_info(AV1Bitstream* bs, FrameHeader* fh, Ipp32u sbSize)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
        const bool large_scale_tile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        if (large_scale_tile)
        {
            // TODO: [Rev0.85] add support of large scale tile
            return;
        }
        av1_read_tile_info_max_tile(bs, fh, sbSize);

        if (fh->TileCols > 1)
            fh->loop_filter_across_tiles_v_enabled = bs->GetBit();
        else
            fh->loop_filter_across_tiles_v_enabled = 1;

        if (fh->TileRows > 1)
            fh->loop_filter_across_tiles_h_enabled = bs->GetBit();
        else
            fh->loop_filter_across_tiles_h_enabled = 1;

        if (NumTiles(*fh) > 1)
            fh->TileSizeBytes = bs->GetBits(2) + 1;

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

        *params = default_warp_params;
        params->wmtype = type;

        if (type >= ROTZOOM)
        {
            params->wmmat[2] = read_signed_primitive_refsubexpfin(
                bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS)) *
                GM_ALPHA_DECODE_FACTOR +
                (1 << WARPEDMODEL_PREC_BITS);
            params->wmmat[3] = read_signed_primitive_refsubexpfin(
                bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF)) *
                GM_ALPHA_DECODE_FACTOR;
        }

        if (type >= AFFINE)
        {
            params->wmmat[4] = read_signed_primitive_refsubexpfin(
                bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF)) *
                GM_ALPHA_DECODE_FACTOR;
            params->wmmat[5] = read_signed_primitive_refsubexpfin(
                bs, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS)) *
                GM_ALPHA_DECODE_FACTOR +
                (1 << WARPEDMODEL_PREC_BITS);
        }
        else
        {
            params->wmmat[4] = -params->wmmat[3];
            params->wmmat[5] = params->wmmat[2];
        }

        if (type >= TRANSLATION)
        {
            const Ipp8u disallowHP = fh->allow_high_precision_mv ? 0 : 1;
            const Ipp32s trans_bits = (type == TRANSLATION)
                ? GM_ABS_TRANS_ONLY_BITS - disallowHP
                : GM_ABS_TRANS_BITS;
            const Ipp32s trans_dec_factor =
                (type == TRANSLATION) ? GM_TRANS_ONLY_DECODE_FACTOR * (1 << disallowHP)
                : GM_TRANS_DECODE_FACTOR;
            const Ipp32s trans_prec_diff = (type == TRANSLATION)
                ? GM_TRANS_ONLY_PREC_DIFF + disallowHP
                : GM_TRANS_PREC_DIFF;
            params->wmmat[0] = read_signed_primitive_refsubexpfin(
                bs, (1 << trans_bits) + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[0] >> trans_prec_diff)) *
                trans_dec_factor;
            params->wmmat[1] = read_signed_primitive_refsubexpfin(
                bs, (1 << trans_bits) + 1, SUBEXPFIN_K,
                static_cast<Ipp16s>(ref_params->wmmat[1] >> trans_prec_diff)) *
                trans_dec_factor;
        }

        // TODO: [Rev0.5] Add reading of shear params
        /*if (params->wmtype <= AFFINE)
        {
            if (!get_shear_params(params))
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }*/

        AV1D_LOG("[-]: %d", (mfxU32)bs->BitsDecoded());
    }

    void av1_read_film_grain_params(FilmGrain* params, AV1Bitstream* bs, SequenceHeader const * sh, FrameHeader const* fh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());

        params->apply_grain = bs->GetBit();
        if (!params->apply_grain) {
            memset(params, 0, sizeof(*params));
            return;
        }

        params->random_seed = bs->GetBits(16);
        if (fh->frame_type == INTER_FRAME)
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

        if (!sh->color_config.mono_chrome)
            params->chroma_scaling_from_luma = bs->GetBit();

        if (sh->color_config.mono_chrome || params->chroma_scaling_from_luma)
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

        AV1D_LOG("[+]: %d", (mfxU32)bs->BitsDecoded());
    }

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

        header.obu_type = (AV1_OBU_TYPE)bs->GetBits(4);
#if UMC_AV1_DECODER_REV >= 8500
        const int obu_extension_flag = bs->GetBit();
        header.obu_has_size_field = bs->GetBit();
        bs->GetBit();  // reserved
#else
        bs->GetBits(2);  // reserved
        const int obu_extension_flag = bs->GetBit();
#endif

        if (obu_extension_flag) // if has extension
        {
            header.temporal_id = bs->GetBits(3);
            header.spatial_id = bs->GetBits(2);
            bs->GetBits(3);  // reserved
        }

        return header;
    }

    void AV1Bitstream::ReadOBUInfo(OBUInfo* info)
    {
        size_t sizeFieldLength = 0;
        size_t obu_size = 0;

#if UMC_AV1_DECODER_REV >= 8500
        const size_t start = BytesDecoded();
        info->header = av1_read_obu_header(this);
        size_t headerSize = BytesDecoded() - start;

        if (info->header.obu_has_size_field)
            av1_read_obu_size(this, &obu_size, &sizeFieldLength);
        else if (info->header.obu_type != OBU_TEMPORAL_DELIMITER)
            throw av1_exception(UMC::UMC_ERR_NOT_IMPLEMENTED); // no support for OBUs w/o size field so far

        info->size = headerSize + sizeFieldLength + obu_size;
#else
        av1_read_obu_size(this, &obu_size, &sizeFieldLength);
        info->size = obu_size + sizeFieldLength;
        info->header = av1_read_obu_header(this);
#endif
    }

    void AV1Bitstream::ReadByteAlignment()
    {
        if (m_bitOffset)
        {
            const Ipp32u bitsToRead = 8 - m_bitOffset;
            const Ipp32u bits = GetBits(bitsToRead);
            if (bits)
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    void AV1Bitstream::ReadTileGroupHeader(FrameHeader const* fh, TileGroupInfo* info)
    {
        Ipp8u tile_start_and_end_present_flag = 0;
        if (!fh->large_scale_tile && NumTiles(*fh) > 1)
            tile_start_and_end_present_flag = GetBit();

        if (tile_start_and_end_present_flag)
        {
            const Ipp32u log2Tiles = fh->TileColsLog2 + fh->TileRowsLog2;
            info->startTileIdx = GetBits(log2Tiles);
            info->endTileIdx = GetBits(log2Tiles);
            info->numTiles = info->endTileIdx - info->startTileIdx + 1;
        }
        else
        {
            info->numTiles = NumTiles(*fh);
            info->startTileIdx = 0;
            info->endTileIdx = info->numTiles - 1;
        }

        ReadByteAlignment();
    }

    Ipp64u AV1Bitstream::GetLE(Ipp32u n)
    {
        VM_ASSERT(m_bitOffset == 0);
        VM_ASSERT(n <= 8);

        Ipp64u t = 0;
        for (Ipp32u i = 0; i < n; i++)
            t += (*m_pbs++) << (i * 8);

        return t;
    }

    void AV1Bitstream::ReadTile(FrameHeader const* fh, size_t& reportedSize, size_t& actualSize)
    {
        actualSize = reportedSize = static_cast<size_t>(GetLE(fh->TileSizeBytes));

        if (BytesLeft() < reportedSize)
            actualSize = BytesLeft();

        m_pbs += actualSize;
    }

    void AV1Bitstream::ReadSequenceHeader(SequenceHeader* sh)
    {
        AV1D_LOG("[+]: %d", (mfxU32)BitsDecoded());

#if UMC_AV1_DECODER_REV >= 8500
        sh->seq_profile = GetBits(3);
        sh->still_picture = GetBit();
        sh->reduced_still_picture_header = GetBit();

        if (sh->reduced_still_picture_header)
            GetBits(5); // seq_level_idx[0]
        else
        {
            sh->timing_info_present_flag = GetBit();
            if (sh->timing_info_present_flag)
            {
                av1_timing_info(this, &sh->timing_info);
                sh->decoder_model_info_present_flag = GetBit();
                if (sh->decoder_model_info_present_flag)
                    av1_decoder_model_info(this, &sh->decoder_model_info);
            }

            const int initial_display_delay_present_flag = GetBit();
            sh->operating_points_cnt_minus_1 = GetBits(5);
            for (unsigned int i = 0; i <= sh->operating_points_cnt_minus_1; i++)
            {
                sh->operating_point_idc[i] = GetBits(12);
                sh->seq_level_idx[i] = GetBits(5);
                if (sh->seq_level_idx[i] > 7)
                    sh->seq_tier[i] = GetBit();

                if (sh->decoder_model_info_present_flag)
                {
                    sh->decoder_model_present_for_this_op[i] = GetBit();
                    if (sh->decoder_model_present_for_this_op[i])
                        av1_operating_parameters_info(this,
                            &sh->operating_parameters_info[i],
                            sh->decoder_model_info.buffer_delay_length_minus_1 + 1);
                }

                const int BufferPoolMaxSize = 10; // av1 spec E.2
                sh->initial_display_delay_minus_1[i] = BufferPoolMaxSize - 1;
                if (initial_display_delay_present_flag)
                {
                    if (GetBit()) // initial_display_delay_present_for_this_op[i]
                        sh->initial_display_delay_minus_1[i] = GetBits(4);
                }
            }

        }
#else // UMC_AV1_DECODER_REV >= 8500
        sh->seq_profile = GetBits(2);
        GetBits(4); // level

        Ipp32u elayers_cnt = GetBits(2);
        for (Ipp32u i = 1; i <= elayers_cnt; i++)
            GetBits(4); // level for each enhancement layer
#endif // UMC_AV1_DECODER_REV >= 8500

        sh->frame_width_bits = GetBits(4) + 1;
        sh->frame_height_bits = GetBits(4) + 1;

        sh->max_frame_width = GetBits(sh->frame_width_bits) + 1;
        sh->max_frame_height = GetBits(sh->frame_height_bits) + 1;

        sh->frame_id_numbers_present_flag = GetBit();
        if (sh->frame_id_numbers_present_flag) {
            sh->delta_frame_id_length = GetBits(4) + 2;
            sh->idLen = GetBits(3) + sh->delta_frame_id_length + 1;
        }

        sh->sbSize = GetBit() ? BLOCK_128X128 : BLOCK_64X64;

#if UMC_AV1_DECODER_REV >= 8500
        sh->enable_filter_intra = GetBit();
        sh->enable_intra_edge_filter = GetBit();

        if (sh->reduced_still_picture_header)
        {
            sh->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
            sh->seq_force_integer_mv = SELECT_INTEGER_MV;
        }
        else
        {
            sh->enable_interintra_compound = GetBit();
            sh->enable_masked_compound = GetBit();
            sh->enable_warped_motion = GetBit();
#endif // UMC_AV1_DECODER_REV >= 8500
            sh->enable_dual_filter = GetBit();
            sh->enable_order_hint = GetBit();
            if (sh->enable_order_hint)
            {
                sh->enable_jnt_comp = GetBit();
#if UMC_AV1_DECODER_REV >= 8500
                sh->enable_ref_frame_mvs = GetBit();
#endif
            }

            if (GetBit()) // seq_choose_screen_content_tools
                sh->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
            else
                sh->seq_force_screen_content_tools = GetBit();

            if (sh->seq_force_screen_content_tools > 0)
            {
                if (GetBit()) // seq_choose_integer_mv
                    sh->seq_force_integer_mv = 2;
                else
                    sh->seq_force_integer_mv = GetBit();
            }
            else
                sh->seq_force_integer_mv = 2;

            sh->order_hint_bits_minus1 =
                sh->enable_order_hint ? GetBits(3) : -1;
#if UMC_AV1_DECODER_REV >= 8500
        }

        sh->enable_superres = GetBit();
        sh->enable_cdef = GetBit();
        sh->enable_restoration = GetBit();
#endif

        av1_color_config(this, &sh->color_config, sh->seq_profile);

#if UMC_AV1_DECODER_REV == 5000
        sh->timing_info_present_flag = GetBit();
        if (sh->timing_info_present_flag)
            av1_timing_info(this, &sh->timing_info);
#endif

        sh->film_grain_param_present = GetBit();

        AV1D_LOG("[-]: %d", (mfxU32)BitsDecoded());
    }

    inline
    bool FrameMightUsePrevFrameMVs(FrameHeader const* fh)
    {
        const bool large_scale_tile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        return !FrameIsResilient(fh) && !large_scale_tile;
    }

    inline
    bool FrameCanUsePrevFrameMVs(FrameHeader const* fh, FrameHeader const* prev_fh = 0)
    {
        return (FrameMightUsePrevFrameMVs(fh) &&
            prev_fh &&
            !FrameIsIntraOnly(prev_fh) &&
            fh->FrameWidth == prev_fh->FrameWidth &&
            fh->FrameHeight == prev_fh->FrameHeight);
    }

    static bool IsAllLosless(FrameHeader const * fh)
    {
        bool qIndexAllZero = true;

        if (fh->segmentation_params.segmentation_enabled)
        {
            for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
            {
                Ipp32s segQIndex = fh->base_q_idx;
                if (IsSegFeatureActive(fh->segmentation_params, i, SEG_LVL_ALT_Q))
                {
                    const Ipp32s data = GetSegData(fh->segmentation_params, i, SEG_LVL_ALT_Q);
                    segQIndex = UMC_VP9_DECODER::clamp(fh->base_q_idx + data, 0, UMC_VP9_DECODER::MAXQ);
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
            qIndexAllZero = (fh->base_q_idx == 0);
        }

        return qIndexAllZero && fh->lossless;
    }

    inline void InheritFromPrevFrame(FrameHeader* fh, FrameHeader const* prev_fh)
    {
        for (Ipp32u i = 0; i < TOTAL_REFS; i++)
            fh->loop_filter_params.loop_filter_ref_deltas[i] = prev_fh->loop_filter_params.loop_filter_ref_deltas[i];

        for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
            fh->loop_filter_params.loop_filter_mode_deltas[i] = prev_fh->loop_filter_params.loop_filter_mode_deltas[i];

        fh->cdef_damping = prev_fh->cdef_damping;
        for (Ipp32u i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            fh->cdef_y_strength[i] = prev_fh->cdef_y_strength[i];
            fh->cdef_uv_strength[i] = prev_fh->cdef_uv_strength[i];
        }

        memcpy_s(&fh->segmentation_params, sizeof(AV1Segmentation), &prev_fh->segmentation_params, sizeof(AV1Segmentation));
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
                curFrameHint + av1_get_relative_dist(sh, refHdr.order_hint, fh->order_hint);
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
        // Forward  reference: ref order_hint < fh->order_hint
        // Backward reference: ref order_hint >= fh->order_hint
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
            fh->ref_frame_idx[ALTREF_FRAME - LAST_FRAME] = refFrameInfo[bwdEndIdx].mapIdx;
            refFlagList[ALTREF_FRAME - LAST_FRAME] = 1;
            bwdEndIdx--;
        }

        // == BWDREF_FRAME ==
        if (bwdStartIdx <= bwdEndIdx)
        {
            fh->ref_frame_idx[BWDREF_FRAME - LAST_FRAME] = refFrameInfo[bwdStartIdx].mapIdx;
            refFlagList[BWDREF_FRAME - LAST_FRAME] = 1;
            bwdStartIdx++;
        }

        // == ALTREF2_FRAME ==
        if (bwdStartIdx <= bwdEndIdx)
        {
            fh->ref_frame_idx[ALTREF2_FRAME - LAST_FRAME] = refFrameInfo[bwdStartIdx].mapIdx;
            refFlagList[ALTREF2_FRAME - LAST_FRAME] = 1;
        }

        // === Forward Reference Frames ===

        for (Ipp32u i = fwdStartIdx; i <= fwdEndIdx; ++i)
        {
            // == LAST_FRAME ==
            if (refFrameInfo[i].mapIdx == last_frame_idx)
            {
                fh->ref_frame_idx[LAST_FRAME - LAST_FRAME] = refFrameInfo[i].mapIdx;
                refFlagList[LAST_FRAME - LAST_FRAME] = 1;
            }

            // == GOLDEN_FRAME ==
            if (refFrameInfo[i].mapIdx == gold_frame_idx)
            {
                fh->ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = refFrameInfo[i].mapIdx;
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

            fh->ref_frame_idx[refFrame - LAST_FRAME] = refFrameInfo[fwdEndIdx].mapIdx;
            refFlagList[refFrame - LAST_FRAME] = 1;

            fwdEndIdx--;
        }

        // Assign all the remaining frame(s), if any, to the earliest reference frame.
        for (; refIdx < (INTER_REFS - 2); refIdx++)
        {
            const MV_REFERENCE_FRAME refFrame = ref_frame_list[refIdx];
            fh->ref_frame_idx[refFrame - LAST_FRAME] = refFrameInfo[fwdStartIdx].mapIdx;
            refFlagList[refFrame - LAST_FRAME] = 1;
        }

        for (int i = 0; i < INTER_REFS; i++)
            VM_ASSERT(refFlagList[i] == 1);
    }

    static void av1_mark_ref_frames(SequenceHeader const* sh, FrameHeader* fh, DPBType const& frameDpb)
    {
        const Ipp32u idLen = sh->idLen;
        const Ipp32u diffLen = sh->delta_frame_id_length;
        for (Ipp32s i = 0; i < NUM_REF_FRAMES; i++)
        {
            FrameHeader const& refHdr = frameDpb[i]->GetFrameHeader();
            if ((static_cast<Ipp32s>(fh->current_frame_id) - (1 << diffLen)) > 0)
            {
                if (refHdr.current_frame_id > fh->current_frame_id ||
                    refHdr.current_frame_id < fh->current_frame_id - (1 << diffLen))
                    frameDpb[i]->SetRefValid(false);
            }
            else
            {
                if (refHdr.current_frame_id > fh->current_frame_id &&
                    refHdr.current_frame_id < ((1 << idLen) + fh->current_frame_id - (1 << diffLen)))
                    frameDpb[i]->SetRefValid(false);
            }
        }
    }

    void AV1Bitstream::ReadUncompressedHeader(FrameHeader* fh, SequenceHeader const* sh, FrameHeader const* prev_fh, DPBType const& frameDpb)
    {
        using UMC_VP9_DECODER::REF_FRAMES_LOG2;

        AV1D_LOG("[+]: %d", (mfxU32)BitsDecoded());

        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        bool missingRefFrame = false; // TODO: [Global] Use this flag to trigger error resilience actions

        fh->show_existing_frame = GetBit();
        if (fh->show_existing_frame)
        {
            fh->frame_to_show_map_idx = GetBits(3);

            FrameHeader const& refHdr = frameDpb[fh->frame_to_show_map_idx]->GetFrameHeader();

            if (sh->frame_id_numbers_present_flag)
            {
                fh->display_frame_id = GetBits(sh->idLen);

                // check that there is no confilct between display_frame_id and respective frame in DPB
                if ((fh->display_frame_id != refHdr.current_frame_id ||
                    false == frameDpb[fh->frame_to_show_map_idx]->RefValid()))
                {
                    VM_ASSERT("Frame_to_show is absent in DPB or invalid!");
                    missingRefFrame = true;
                }

                fh->current_frame_id = fh->display_frame_id;
            }

            // get frame resolution
            fh->FrameWidth = refHdr.FrameWidth;
            fh->FrameHeight = refHdr.FrameHeight;

            fh->show_frame = 1;

            return;
        }

        fh->frame_type = static_cast<FRAME_TYPE>(GetBits(2));

        if (fh->frame_type != KEY_FRAME)
        {
            VM_ASSERT(prev_fh);
            VM_ASSERT(frameDpb.size() == NUM_REF_FRAMES);
            // TODO: [Global] Add handling of case when decoding starts from non key frame and thus frameDpb is empty
        }

        fh->show_frame = GetBit();
        fh->error_resilient_mode = GetBit();
        fh->enable_intra_edge_filter = GetBit();
        fh->enable_filter_intra = GetBit();

        if (fh->frame_type == KEY_FRAME && !frameDpb.empty())
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
                frameDpb[i]->SetRefValid(false);
        }

        fh->disable_cdf_update = GetBit();
        if (sh->seq_force_screen_content_tools == 2)
            fh->allow_screen_content_tools = GetBit();
        else
            fh->allow_screen_content_tools = sh->seq_force_screen_content_tools;

        if (fh->allow_screen_content_tools)
        {
            if (sh->seq_force_integer_mv == 2)
                fh->seq_force_integer_mv = GetBit();
            else
                fh->seq_force_integer_mv = sh->seq_force_integer_mv;
        }
        else
            fh->seq_force_integer_mv = 0;

        if (sh->frame_id_numbers_present_flag)
        {
            fh->current_frame_id = GetBits(sh->idLen);

            if (fh->frame_type != KEY_FRAME)
            {
                // check current_frame_id as described in AV1 spec 6.8.2
                const Ipp32u idLen = sh->idLen;
                const Ipp32u prevFrameId = prev_fh->current_frame_id;
                const Ipp32s diffFrameId = fh->current_frame_id > prevFrameId ?
                    fh->current_frame_id - prevFrameId :
                    (1 << idLen) + fh->current_frame_id - prevFrameId;

                if (fh->current_frame_id == prevFrameId ||
                    diffFrameId >= (1 << (idLen - 1)))
                {
                    VM_ASSERT("current_frame_id is incompliant to AV1 spec!");
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                }

                //  check and mark ref frames as not valid as described in "Reference frame marking process" AV1 spec 5.9.4
                av1_mark_ref_frames(sh, fh, frameDpb);
            }
        }

        fh->frame_size_override_flag = GetBits(1);
        fh->order_hint = GetBits(sh->order_hint_bits_minus1 + 1);

        if (KEY_FRAME == fh->frame_type)
        {
            if (!fh->show_frame)
                fh->refresh_frame_flags = static_cast<Ipp8u>(GetBits(NUM_REF_FRAMES));
            else
                fh->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;

            for (Ipp8u i = 0; i < INTER_REFS; ++i)
            {
                fh->ref_frame_idx[i] = -1;
            }

            av1_get_frame_size(this, fh, sh);

            if (fh->allow_screen_content_tools &&
                (fh->FrameWidth == fh->UpscaledWidth || !NO_FILTER_FOR_IBC))
                fh->allow_intrabc = GetBit();
        }
        else
        {
            if (FrameIsResilient(fh))
            {
                fh->use_ref_frame_mvs = 0;
            }

            if (FrameIsIntraOnly(fh))
            {
                fh->refresh_frame_flags = (Ipp8u)GetBits(NUM_REF_FRAMES);
                av1_get_frame_size(this, fh, sh);

                if (fh->allow_screen_content_tools &&
                    (fh->FrameWidth == fh->UpscaledWidth || !NO_FILTER_FOR_IBC))
                    fh->allow_intrabc = GetBit();
            }
            else
            {
                fh->refresh_frame_flags = (Ipp8u)GetBits(NUM_REF_FRAMES);

                Ipp32u frame_refs_short_signalling = 0;

                if (!fh->error_resilient_mode && sh->enable_order_hint)
                    frame_refs_short_signalling = GetBit();

                if (frame_refs_short_signalling)
                {
                    const Ipp32u last_frame_idx = GetBits(REF_FRAMES_LOG2);
                    const Ipp32u gold_frame_idx = GetBits(REF_FRAMES_LOG2);

                    // set last and gold reference frames
                    fh->ref_frame_idx[LAST_FRAME - LAST_FRAME] = last_frame_idx;
                    fh->ref_frame_idx[GOLDEN_FRAME - LAST_FRAME] = gold_frame_idx;

                    // compute other active references as specified in "Set frame refs process" AV1 spec 7.8
                    av1_set_frame_refs(sh, fh, frameDpb, last_frame_idx, gold_frame_idx);
                }

                for (Ipp8u i = 0; i < INTER_REFS; ++i)
                {
                    if (!frame_refs_short_signalling)
                        fh->ref_frame_idx[i] = GetBits(REF_FRAMES_LOG2);

                    if (sh->frame_id_numbers_present_flag)
                    {
                        const Ipp32s deltaFrameId = GetBits(sh->delta_frame_id_length) + 1;

                        // compute expected reference frame id from delta_frame_id and check that it's equal to one saved in DPB
                        const Ipp32u idLen = sh->idLen;
                        const Ipp32u expectedFrameId = ((fh->current_frame_id - deltaFrameId + (1 << idLen))
                            % (1 << idLen));

                        AV1DecoderFrame const* refFrm = frameDpb[fh->ref_frame_idx[i]];
                        FrameHeader const& refHdr = refFrm->GetFrameHeader();

                        if (expectedFrameId != refHdr.current_frame_id ||
                            false == refFrm->RefValid())
                        {
                            VM_ASSERT("Active reference frame is absent in DPB or invalid!");
                            missingRefFrame = true;
                        }
                    }
                }

                if (fh->error_resilient_mode == 0 && fh->frame_size_override_flag)
                    av1_get_frame_sizes_with_refs(this, fh, frameDpb, sh);
                else
                    av1_get_frame_size(this, fh, sh);

                if (fh->seq_force_integer_mv)
                    fh->allow_high_precision_mv = 0;
                else
                    fh->allow_high_precision_mv = GetBit();

                fh->interpolation_filter = GetBit() ? SWITCHABLE : (INTERP_FILTER)GetBits(LOG2_SWITCHABLE_FILTERS);
                fh->is_motion_mode_switchable = GetBit();
                FrameHeader const& last_fh = frameDpb[fh->ref_frame_idx[LAST_FRAME - LAST_FRAME]]->GetFrameHeader();

                if (FrameMightUsePrevFrameMVs(fh) && sh->enable_order_hint)
                {
                    const Ipp32u useRefFrameMVs = GetBit();
                    fh->use_ref_frame_mvs = useRefFrameMVs && FrameCanUsePrevFrameMVs(fh, &last_fh);
                }
            }
        }

        if (!fh->FrameWidth || !fh->FrameHeight)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        fh->large_scale_tile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        if (!(fh->error_resilient_mode || fh->large_scale_tile))
        {
            fh->refresh_frame_context = GetBit()
                ? REFRESH_FRAME_CONTEXT_DISABLED
                : REFRESH_FRAME_CONTEXT_BACKWARD;
        }
        else
            fh->refresh_frame_context = REFRESH_FRAME_CONTEXT_DISABLED;

        if (FrameIsResilient(fh))
            fh->primary_ref_frame = PRIMARY_REF_NONE;
        else
            fh->primary_ref_frame = GetBits(PRIMARY_REF_BITS);

        if (FrameIsResilient(fh))
            SetupPastIndependence(*fh);
        else
            InheritFromPrevFrame(fh, prev_fh);

        fh->NumPlanes = av1_num_planes(sh);
        av1_setup_quantization(this, sh, fh);

        av1_setup_segmentation(this, fh);

        fh->delta_q_res = 1;
        fh->delta_lf_res = 1;
        if (fh->base_q_idx > 0)
            fh->delta_q_present = GetBit();

        if (fh->delta_q_present)
        {
            fh->delta_q_res = 1 << GetBits(2);
            if (!fh->allow_intrabc || !NO_FILTER_FOR_IBC)
                fh->delta_lf_present = GetBit();

            if (fh->delta_lf_present)
            {
                fh->delta_lf_res = 1 << GetBits(2);
                fh->delta_lf_multi = GetBit();
            }
        }

        bool allLosless = IsAllLosless(fh);

        if (allLosless == false)
        {
            av1_setup_loop_filter(this, fh);

            av1_read_cdef(this, fh);

            av1_decode_restoration_mode(this, sh, fh);
        }

        av1_read_tx_mode(this, fh, allLosless);

        av1_read_frame_reference_mode(this, fh, frameDpb);

        fh->skipModeAllowed = av1_is_skip_mode_allowed(fh, sh, frameDpb) ? GetBit() : 0;

        av1_read_compound_tools(this, fh);

        fh->reduced_tx_set = GetBit();

        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            fh->global_motion_params[i] = default_warp_params;
        }

        if (!FrameIsIntra(fh))
        {
            // read_global_motion()
            mfxI32 frame;
            for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame)
            {
                FrameHeader const& last_fh = frameDpb[fh->ref_frame_idx[LAST_FRAME - LAST_FRAME]]->GetFrameHeader();

                WarpedMotionParams const *ref_params = fh->error_resilient_mode ?
                    &default_warp_params : &last_fh.global_motion_params[frame];
                WarpedMotionParams* params = &fh->global_motion_params[frame];
                av1_read_global_motion_params(params, ref_params, this, fh);
            }
        }

        fh->showable_frame = fh->show_frame ? 0 : GetBit();
        if (fh->show_frame || fh->showable_frame)
        {
            // read_film_grain()
            if (sh->film_grain_param_present)
                av1_read_film_grain_params(&fh->film_grain_params, this, sh, fh);
            else
                memset(&fh->film_grain_params, 0, sizeof(fh->film_grain_params));

            fh->film_grain_params.BitDepth = sh->color_config.BitDepth;
        }

        av1_read_tile_info(this, fh, sh->sbSize);

        AV1D_LOG("[-]: %d", (mfxU32)BitsDecoded());
    }

} // namespace UMC_AV1_DECODER

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
