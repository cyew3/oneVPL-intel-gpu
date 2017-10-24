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

#include "vm_debug.h"

#include "umc_structures.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_utils.h"

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

    void AV1Bitstream::GetSequenceHeader(SequenceHeader* sh)
    {
        /* Placeholder for actually reading from the bitstream */
        sh->frame_id_numbers_present_flag = FRAME_ID_NUMBERS_PRESENT_FLAG;
        sh->frame_id_length_minus7 = FRAME_ID_LENGTH_MINUS7;
        sh->delta_frame_id_length_minus2 = DELTA_FRAME_ID_LENGTH_MINUS2;
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
        }
    }

    void AV1Bitstream::GetFrameHeaderPart1(FrameHeader* fh, SequenceHeader const* sh)
    {
        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

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
#if !defined(UMC_AV1_DECODER_REV0)
        fh->intraOnly = fh->showFrame ? 0 : GetBit();
#endif
        fh->errorResilientMode = GetBit();


#if defined(UMC_AV1_DECODER_REV0)
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
#if defined(UMC_AV1_DECODER_REV0)
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
#if defined(UMC_AV1_DECODER_REV0)
            fh->intraOnly = fh->showFrame ? 0 : GetBit();
#else
            if (fh->intraOnly)
                fh->allowScreenContentTools = GetBit();
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
#if !defined(UMC_AV1_DECODER_REV0)
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
            }
        }
    }

    void AV1Bitstream::GetFrameHeaderFull(FrameHeader* fh, SequenceHeader const* sh)
    {
        if (!fh || !sh)
            throw av1_exception(UMC::UMC_ERR_NULL_PTR);

        if (!fh->width || !fh->height)
            throw av1_exception(UMC::UMC_ERR_FAILED);

        if (!fh->errorResilientMode)
        {
            fh->refreshFrameContext = GetBit()
                ? REFRESH_FRAME_CONTEXT_FORWARD
                : REFRESH_FRAME_CONTEXT_BACKWARD;
#if !defined(UMC_AV1_DECODER_REV0)
            fh->frameParallelDecodingMode = GetBit();
#endif
        }
        else
        {
            fh->refreshFrameContext = 0;
            fh->frameParallelDecodingMode = 1;
        }

        // This flag will be overridden by the call to vp9_setup_past_independence
        // below, forcing the use of context 0 for those frame types.
        fh->frameContextIdx = GetBits(FRAME_CONTEXTS_LOG2);

        if (IsFrameResilent(fh))
        {
            SetupPastIndependence(*fh);
        }

        //setup loop filter
        {
            fh->lf.filterLevel = GetBits(6);
            fh->lf.sharpnessLevel = GetBits(3);

            fh->lf.modeRefDeltaUpdate = 0;

            fh->lf.modeRefDeltaEnabled = (Ipp8u)GetBit();
            if (fh->lf.modeRefDeltaEnabled)
            {
                fh->lf.modeRefDeltaUpdate = (Ipp8u)GetBit();

                if (fh->lf.modeRefDeltaUpdate)
                {
                    Ipp8u bits = 6;
                    for (Ipp32u i = 0; i < TOTAL_REFS; i++)
                    {
                        if (GetBit())
                        {
                            const Ipp8u nbits = sizeof(unsigned) * 8 - bits - 1;
                            const Ipp32u value = (unsigned)GetBits(bits + 1) << nbits;
                            fh->lf.refDeltas[i] = (Ipp8s)(((Ipp32s)value) >> nbits);
                        }
                    }

                    for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
                    {
                        if (GetBit())
                        {
                            const Ipp8u nbits = sizeof(unsigned) * 8 - bits - 1;
                            const Ipp32u value = (unsigned)GetBits(bits + 1) << nbits;
                            fh->lf.modeDeltas[i] = (Ipp8s)(((Ipp32s)value) >> nbits);
                        }
                    }
                }
            }
        }

#if defined(UMC_AV1_DECODER_REV0)
        {
            memset(fh->cdefStrength, 0, sizeof(fh->cdefStrength));
            memset(fh->cdefUVStrength, 0, sizeof(fh->cdefUVStrength));
            // read CDEF
            fh->cdefDeringDamping = GetBit() + 5;
            fh->cdefClpfDamping = GetBits(2) + 3;
            const mfxU32 cdefBits = GetBits(2);
            fh->nbCdefStrengths = 1 << cdefBits;
            for (Ipp8u i = 0; i < fh->nbCdefStrengths; i++) {
                fh->cdefStrength[i] = GetBits(7);
                fh->cdefUVStrength[i] = GetBits(7);
            }
        }
#endif

        //setup_quantization()
        {
            fh->baseQIndex = GetBits(QINDEX_BITS);

            if (GetBit())
            {
                fh->y_dc_delta_q = GetBits(4);
                fh->y_dc_delta_q = GetBit() ? -fh->y_dc_delta_q : fh->y_dc_delta_q;
            }
            else
                fh->y_dc_delta_q = 0;

            if (GetBit())
            {
                fh->uv_dc_delta_q = GetBits(4);
                fh->uv_dc_delta_q = GetBit() ? -fh->uv_dc_delta_q : fh->uv_dc_delta_q;
            }
            else
                fh->uv_dc_delta_q = 0;

            if (GetBit())
            {
                fh->uv_ac_delta_q = GetBits(4);
                fh->uv_ac_delta_q = GetBit() ? -fh->uv_ac_delta_q : fh->uv_ac_delta_q;
            }
            else
                fh->uv_ac_delta_q = 0;

            fh->lossless = (0 == fh->baseQIndex &&
                0 == fh->y_dc_delta_q &&
                0 == fh->uv_dc_delta_q &&
                0 == fh->uv_ac_delta_q);
        }

        bool segmentQuantizerActive = false;
        // setup_segmentation()
        {
            fh->segmentation.updateMap = 0;
            fh->segmentation.updateData = 0;

            fh->segmentation.enabled = (Ipp8u)GetBit();
            if (fh->segmentation.enabled)
            {
                // Segmentation map update
                if (IsFrameResilent(fh))
                    fh->segmentation.updateMap = 1;
                else
                    fh->segmentation.updateMap = (Ipp8u)GetBit();
                if (fh->segmentation.updateMap)
                {
                    if (IsFrameResilent(fh))
                        fh->segmentation.temporalUpdate = 0;
                    else
                        fh->segmentation.temporalUpdate = (Ipp8u)GetBit();
                }

                fh->segmentation.updateData = (Ipp8u)GetBit();
                if (fh->segmentation.updateData)
                {
                    fh->segmentation.absDelta = (Ipp8u)GetBit();

                    ClearAllSegFeatures(fh->segmentation);

                    Ipp32s data = 0;
                    mfxU32 nBits = 0;
                    for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                    {
                        for (Ipp8u j = 0; j < SEG_LVL_MAX; ++j)
                        {
                            data = 0;
                            if (GetBit()) // feature_enabled
                            {
                                if (j == SEG_LVL_ALT_Q)
                                    segmentQuantizerActive = true;
                                EnableSegFeature(fh->segmentation, i, (SEG_LVL_FEATURES)j);

                                nBits = GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                                data = GetBits(nBits);
                                data = data > SEG_FEATURE_DATA_MAX[j] ? SEG_FEATURE_DATA_MAX[j] : data;

                                if (IsSegFeatureSigned((SEG_LVL_FEATURES)j))
                                    data = GetBit() ? -data : data;
                            }

                            SetSegData(fh->segmentation, i, (SEG_LVL_FEATURES)j, data);
                        }
                    }
                }
            }
        }

        fh->deltaQRes = 1;
        if (segmentQuantizerActive == false && fh->baseQIndex > 0)
            fh->deltaQPresentFlag = GetBit();

        if (fh->deltaQPresentFlag)
        {
            fh->deltaQRes = 1 << GetBits(2);
            fh->deltaLFPresentFlag = GetBit();

            if (fh->deltaLFPresentFlag)
                fh->deltaLFRes = 1 << GetBits(2);
        }

#if defined(UMC_AV1_DECODER_REV0)
        // TODO: add check for lossless mode (all segment qIndex are 0 and all AC/DC Luma/Chroma deltas are 0) here
        fh->txMode = GetBit() ? TX_MODE_SELECT : (TX_MODE)GetBits(2); // read_tx_mode(). TODO: add code branch for lossless case

        fh->referenceMode = SINGLE_REFERENCE;
        bool compoundRefAllowed = false;
        for (Ipp8u i = 1; i < INTER_REFS; ++i)
        {
            if (fh->refFrameSignBias[i - 1] != fh->refFrameSignBias[i])
                compoundRefAllowed = true;
        }
        if (compoundRefAllowed)
        {
            fh->referenceMode = GetBit() ? REFERENCE_MODE_SELECT :
                GetBit() ? COMPOUND_REFERENCE : SINGLE_REFERENCE;
        }

        fh->reducedTxSetUsed = GetBit();
#else
        {
            // read CDEF
            fh->cdefDeringDamping = GetBit();
            fh->cdefClpfDamping = GetBits(2);
            const mfxU32 cdefBits = GetBits(2);
            fh->nbCdefStrengths = 1 << cdefBits;
            for (Ipp8u i = 0; i < fh->nbCdefStrengths; i++) {
                fh->cdefStrength[i] = GetBits(7);
                fh->cdefUVStrength[i] = GetBits(7);
            }
        }

        // below is hardcoded logic for my test streams. TODO: implement proper logic.
        if (fh->frameType != KEY_FRAME)
        {
            GetBits(3); // read_tx_mode
            GetBits(2); // read_reference_frame_mode
            GetBit(); // read_compound_tools
            GetBit(); // reduced_tx_set_used
        }
        else
        {
            GetBits(1); // read_tx_mode
            GetBit(); // reduced_tx_set_used
        }
#endif

        // setup_tile_info()
        {
            const Ipp32s alignedWidth = ALIGN_POWER_OF_TWO(fh->width, MI_SIZE_LOG2);
            int minLog2TileColumns, maxLog2TileColumns, maxOnes;
            mfxU32 miCols = alignedWidth >> MI_SIZE_LOG2;
            GetTileNBits(miCols, minLog2TileColumns, maxLog2TileColumns);

            maxOnes = maxLog2TileColumns - minLog2TileColumns;
            fh->log2TileColumns = minLog2TileColumns;
            while (maxOnes-- && GetBit())
                fh->log2TileColumns++;

            fh->log2TileRows = GetBit();
            if (fh->log2TileRows)
                fh->log2TileRows += GetBit();
#if defined(UMC_AV1_DECODER_REV0)
            fh->loopFilterAcrossTilesEnabled = GetBit();
            fh->tileSizeBytes = GetBits(2) + 1;
#endif
        }

        fh->firstPartitionSize = GetBits(16);
        if (!fh->firstPartitionSize)
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

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
