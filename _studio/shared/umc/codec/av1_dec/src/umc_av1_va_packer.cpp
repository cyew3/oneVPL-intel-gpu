//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_vp9_dec_defs.h"

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include <algorithm>
#include "umc_structures.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"
#include "umc_av1_utils.h"

#include "umc_av1_frame.h"
#include "umc_av1_va_packer.h"

#ifdef UMC_VA_DXVA
#include "umc_va_base.h"
#include "umc_va_dxva2_protected.h"
#endif

using namespace UMC;

namespace UMC_AV1_DECODER
{
    Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
    {
#ifdef UMC_VA_DXVA
        return new PackerIntel(va);
#elif defined(UMC_VA_LINUX)
        return new PackerVA(va);
#else
        return 0;
#endif
    }

    Packer::Packer(UMC::VideoAccelerator * va)
        : m_va(va)
    {}

    Packer::~Packer()
    {}

#ifdef UMC_VA_DXVA

    /****************************************************************************************************/
    // DXVA Windows packer implementation
    /****************************************************************************************************/
    PackerDXVA::PackerDXVA(VideoAccelerator * va)
        : Packer(va)
        , m_report_counter(1)
    {}

    Status PackerDXVA::GetStatusReport(void * pStatusReport, size_t size)
    {
        return
            m_va->ExecuteStatusReportBuffer(pStatusReport, (Ipp32u)size);
    }

    void PackerDXVA::BeginFrame()
    {
        m_report_counter++;
    }

    void PackerDXVA::EndFrame()
    {
    }

    PackerIntel::PackerIntel(VideoAccelerator * va)
        : PackerDXVA(va)
    {}

#if UMC_AV1_DECODER_REV >= 5000
    void PackerIntel::PackAU(std::vector<TileSet>& tileSets, AV1DecoderFrame const* info, bool firtsSubmission)
    {
        if (firtsSubmission)
        {
            // it's first submission for current frame - need to fill and submit picture parameters
            UMC::UMCVACompBuffer* compBufPic = NULL;
            DXVA_Intel_PicParams_AV1 *picParam = (DXVA_Intel_PicParams_AV1*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBufPic);
            if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_Intel_PicParams_AV1)))
                throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

            compBufPic->SetDataSize(sizeof(DXVA_Intel_PicParams_AV1));
            memset(picParam, 0, sizeof(DXVA_Intel_PicParams_AV1));

            PackPicParams(picParam, info);
        }

        UMC::UMCVACompBuffer* compBufBs = nullptr;
        Ipp8u* const bistreamData = (mfxU8 *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBufBs);
        if (!bistreamData || !compBufBs)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::vector<DXVA_Intel_Tile_AV1> tileControlParams;

        size_t offsetInBuffer = 0;
        for (auto& tileSet : tileSets)
        {
            const size_t spaceInBuffer = compBufBs->GetBufferSize() - offsetInBuffer;
            TileLayout layout;
            const size_t bytesSubmitted = tileSet.Submit(bistreamData, spaceInBuffer, offsetInBuffer, layout);

            if (bytesSubmitted)
            {
                offsetInBuffer += bytesSubmitted;

                for (auto& loc : layout)
                {
                    tileControlParams.emplace_back();
                    PackTileControlParams(&tileControlParams.back(), loc);
                }
            }
        }
        compBufBs->SetDataSize(static_cast<Ipp32u>(offsetInBuffer));

        UMCVACompBuffer* compBufTile = nullptr;
        const Ipp32s tileControlInfoSize = static_cast<Ipp32s>(sizeof(DXVA_Intel_Tile_AV1) * tileControlParams.size());
        DXVA_Intel_Tile_AV1 *tileControlParam = (DXVA_Intel_Tile_AV1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBufTile);
        if (!tileControlParam || !compBufTile || (compBufTile->GetBufferSize() < tileControlInfoSize))
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        memcpy_s(tileControlParam, compBufTile->GetBufferSize(), tileControlParams.data(), tileControlInfoSize);
        compBufTile->SetDataSize(tileControlInfoSize);
    }
#else // #if UMC_AV1_DECODER_REV >= 5000
    void PackerIntel::PackAU(UMC_VP9_DECODER::VP9Bitstream* bs, AV1DecoderFrame const* info)
    {
        UMC::UMCVACompBuffer* compBufPic = NULL;
        DXVA_Intel_PicParams_AV1 *picParam = (DXVA_Intel_PicParams_AV1*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBufPic);
        if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_Intel_PicParams_AV1)))
            throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

        compBufPic->SetDataSize(sizeof(DXVA_Intel_PicParams_AV1));
        memset(picParam, 0, sizeof(DXVA_Intel_PicParams_AV1));

        PackPicParams(picParam, info);

        Ipp8u* data;
        Ipp32u length;
        bs->GetOrg(&data, &length);

        UMCVACompBuffer* compBufBsCtrl = NULL;

        DXVA_Intel_BitStream_AV1_Short *bsControlParam = (DXVA_Intel_BitStream_AV1_Short*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBufBsCtrl);
        if (!bsControlParam || !compBufBsCtrl || (compBufBsCtrl->GetBufferSize() < sizeof(DXVA_Intel_BitStream_AV1_Short)))
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        compBufBsCtrl->SetDataSize(sizeof(DXVA_Intel_BitStream_AV1_Short));
        memset(bsControlParam, 0, sizeof(DXVA_Intel_BitStream_AV1_Short));

        PackBitstreamControlParams(bsControlParam, info);

        Ipp32u offset = (Ipp32u)bs->BytesDecoded();
        length -= offset;

        do
        {
            UMC::UMCVACompBuffer* compBufBs = NULL;
            mfxU8 *bistreamData = (mfxU8 *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBufBs);
            if (!bistreamData || !compBufBs)
                throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

            mfxU32 lenght2 = length;
            if (compBufBs->GetBufferSize() < (mfxI32)length)
                lenght2 = compBufBs->GetBufferSize();

            mfx_memcpy(bistreamData, lenght2, data + offset, lenght2);
            compBufBs->SetDataSize(lenght2);

            length -= lenght2;
            offset += lenght2;

            if (length)
            {
                Status sts = m_va->Execute();
                if (sts != UMC::UMC_OK)
                    throw UMC_VP9_DECODER::vp9_exception(sts);
            }
        } while (length);
    }
#endif // #if UMC_AV1_DECODER_REV >= 5000

    void PackerIntel::PackPicParams(DXVA_Intel_PicParams_AV1* picParam, AV1DecoderFrame const* frame)
    {
        SequenceHeader const& sh = frame->GetSeqHeader();

        FrameHeader const& info =
            frame->GetFrameHeader();

        picParam->frame_width_minus1 = (USHORT)info.FrameWidth - 1;
        picParam->frame_height_minus1 = (USHORT)info.FrameHeight - 1;

#if AV1D_DDI_VERSION >= 21
        // fill seq parameters
        picParam->profile = (UCHAR)sh.seq_profile;

        auto& ddiSeqParam = picParam->dwSeqInfoFlags.fields;

        ddiSeqParam.still_picture = 0;
        ddiSeqParam.sb_size_128x128 = (sh.sbSize == BLOCK_128X128) ? 1 : 0;
        ddiSeqParam.enable_filter_intra = info.enable_filter_intra;
        ddiSeqParam.enable_intra_edge_filter = info.enable_intra_edge_filter;

        ddiSeqParam.enable_interintra_compound = info.enable_interintra_compound;
        ddiSeqParam.enable_masked_compound = info.enable_masked_compound;

        ddiSeqParam.enable_dual_filter = sh.enable_dual_filter;
        ddiSeqParam.enable_order_hint = sh.enable_order_hint;
        ddiSeqParam.enable_jnt_comp = sh.enable_jnt_comp;
        ddiSeqParam.enable_cdef = 1;
        ddiSeqParam.enable_restoration = 1;
        ddiSeqParam.BitDepthIdx = (sh.BitDepth == 10) ? 1 :
            (sh.BitDepth == 12) ? 2 : 0;
        ddiSeqParam.mono_chrome = sh.mono_chrome;
        ddiSeqParam.color_range = sh.color_range;
        ddiSeqParam.subsampling_x = sh.subsampling_x;
        ddiSeqParam.subsampling_y = sh.subsampling_y;
        ddiSeqParam.chroma_sample_position = sh.chroma_sample_position;
        ddiSeqParam.film_grain_params_present = sh.film_grain_param_present;
#ifdef DDI_HACKS_FOR_REV_5
        ddiSeqParam.order_hint_bits_minus1 = sh.order_hint_bits_minus1;
#endif

        // fill pic params
        auto& ddiPicParam = picParam->dwPicInfoFlags.fields;

        ddiPicParam.frame_type = info.frame_type;
        ddiPicParam.show_frame = info.show_frame;
        ddiPicParam.showable_frame = info.showable_frame;
        ddiPicParam.error_resilient_mode = info.error_resilient_mode;
        ddiPicParam.disable_cdf_update = info.disable_cdf_update;
        ddiPicParam.allow_screen_content_tools = info.allow_screen_content_tools;
        ddiPicParam.force_integer_mv = info.seq_force_integer_mv;
        ddiPicParam.allow_intrabc = info.allow_intrabc;
        ddiPicParam.use_superres = (info.SuperresDenom == SCALE_NUMERATOR) ? 0 : 1;
        ddiPicParam.allow_high_precision_mv = info.allow_high_precision_mv;
        ddiPicParam.is_motion_mode_switchable = info.is_motion_mode_switchable;
        ddiPicParam.use_ref_frame_mvs = info.use_ref_frame_mvs;
        ddiPicParam.disable_frame_end_update_cdf = (info.refresh_frame_context == REFRESH_FRAME_CONTEXT_DISABLED) ? 1 : 0;;
        ddiPicParam.uniform_tile_spacing_flag = info.uniform_tile_spacing_flag;
        ddiPicParam.allow_warped_motion = 0;
        ddiPicParam.refresh_frame_context = info.refresh_frame_context;
        ddiPicParam.large_scale_tile = info.large_scale_tile;

        picParam->order_hint = (UCHAR)info.order_hint;
        picParam->superres_scale_denominator = (UCHAR)info.SuperresDenom;
#else // AV1D_DDI_VERSION >= 21
        picParam->profile = (UCHAR)info.profile;

        picParam->dwFormatAndPictureInfoFlags.fields.frame_id_numbers_present_flag = sh.frame_id_numbers_present_flag;
        picParam->dwFormatAndPictureInfoFlags.fields.use_reference_buffer = sh.frame_id_numbers_present_flag;

        picParam->dwFormatAndPictureInfoFlags.fields.frame_type = info.frame_type;
        picParam->dwFormatAndPictureInfoFlags.fields.show_frame = info.show_frame;
        picParam->dwFormatAndPictureInfoFlags.fields.error_resilient_mode = info.error_resilient_mode;
        picParam->dwFormatAndPictureInfoFlags.fields.intra_only = info.intra_only;
        picParam->dwFormatAndPictureInfoFlags.fields.extra_plane = 0;

        picParam->dwFormatAndPictureInfoFlags.fields.allow_high_precision_mv = info.allow_high_precision_mv;
        picParam->dwFormatAndPictureInfoFlags.fields.sb_size_128x128 = (info.sbSize == BLOCK_128X128) ? 1 : 0;

        picParam->dwFormatAndPictureInfoFlags.fields.reset_frame_context = info.reset_frame_context;
        picParam->dwFormatAndPictureInfoFlags.fields.refresh_frame_context = info.refresh_frame_context;
        picParam->dwFormatAndPictureInfoFlags.fields.frame_context_idx = info.frame_context_idx;

        picParam->dwFormatAndPictureInfoFlags.fields.subsampling_x = (UCHAR)info.subsampling_x;
        picParam->dwFormatAndPictureInfoFlags.fields.subsampling_y = (UCHAR)info.subsampling_y;
#endif // AV1D_DDI_VERSION >= 21

        picParam->frame_interp_filter = (UCHAR)info.interpolation_filter;

        picParam->stAV1Segments.enabled = info.segmentation_params.segmentation_enabled;
        picParam->stAV1Segments.temporal_update = info.segmentation_params.segmentation_temporal_update;
        picParam->stAV1Segments.update_map = info.segmentation_params.segmentation_update_map;
        picParam->stAV1Segments.Reserved4Bits = 0;

        for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
        {
            picParam->stAV1Segments.feature_mask[i] = (UCHAR)info.segmentation_params.FeatureMask[i];
            for (Ipp8u j = 0; j < SEG_LVL_MAX; j++)
                picParam->stAV1Segments.feature_data[i][j] = (SHORT)info.segmentation_params.FeatureData[i][j];
        }

        if (KEY_FRAME == info.frame_type)
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
            {
#if AV1D_DDI_VERSION >= 21
                picParam->ref_frame_map[i].wPicEntry = USHRT_MAX;
#else
                picParam->ref_frame_map[i].bPicEntry = UCHAR_MAX;
#endif
            }
        }
        else
        {
            for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
#if AV1D_DDI_VERSION >= 21
                picParam->ref_frame_map[ref].wPicEntry = (USHORT)frame->frame_dpb[ref]->GetMemID();
#else
                picParam->ref_frame_map[ref].bPicEntry = (UCHAR)frame->frame_dpb[ref]->GetMemID();
#endif

            for (mfxU8 ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
            {
                Ipp8u idxInDPB = (Ipp8u)info.ref_frame_idx[ref_idx];
#if AV1D_DDI_VERSION >= 21
                picParam->ref_frame_idx[ref_idx] = idxInDPB;
                picParam->ref_order_hint[ref_idx] = (UCHAR)frame->frame_dpb[idxInDPB]->GetFrameHeader().order_hint;
#else
                picParam->ref_frame_idx[ref_idx].bPicEntry = idxInDPB;
                picParam->ref_frame_sign_bias[ref_idx + 1] = (UCHAR)info.ref_frame_sign_bias[ref_idx];
#endif
            }
        }

#if AV1D_DDI_VERSION >= 21
        picParam->CurrPic.wPicEntry = (USHORT)frame->GetMemID();
        picParam->CurrDisplayPic.wPicEntry = (USHORT)frame->GetMemID();
        picParam->primary_ref_frame = (UCHAR)info.primary_ref_frame;
#else
        picParam->CurrPic.bPicEntry = (UCHAR)frame->GetMemID();
#endif

        picParam->filter_level[0] = (UCHAR)info.loop_filter_params.loop_filter_level[0];
        picParam->filter_level[1] = (UCHAR)info.loop_filter_params.loop_filter_level[1];
        picParam->filter_level_u = (UCHAR)info.loop_filter_params.loop_filter_level[2];
        picParam->filter_level_v = (UCHAR)info.loop_filter_params.loop_filter_level[3];

#if AV1D_DDI_VERSION >= 21
        auto& ddiLFInfoFlags = picParam->cLoopFilterInfoFlags.fields;
        ddiLFInfoFlags.sharpness_level = (UCHAR)info.loop_filter_params.loop_filter_sharpness;
        ddiLFInfoFlags.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        ddiLFInfoFlags.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;
#else
        picParam->sharpness_level = (UCHAR)info.loop_filter_params.loop_filter_sharpness;
        picParam->wControlInfoFlags.fields.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        picParam->wControlInfoFlags.fields.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;
        picParam->wControlInfoFlags.fields.use_prev_frame_mvs = info.use_ref_frame_mvs;

        picParam->UncompressedHeaderLengthInBytes = (UCHAR)info.frameHeaderLength;
        picParam->BSBytesInBuffer = info.frameDataSize;
#endif

        picParam->StatusReportFeedbackNumber = ++m_report_counter;

#ifdef DDI_HACKS_FOR_REV_252
        picParam->BitDepthMinus8 = (UCHAR)info.BitDepth - 8;
#endif
        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            picParam->ref_deltas[i] = info.loop_filter_params.loop_filter_ref_deltas[i];
        }
        for (Ipp8u i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam->mode_deltas[i] = info.loop_filter_params.loop_filter_mode_deltas[i];
        }

        picParam->base_qindex = (SHORT)info.base_q_idx;
        picParam->y_dc_delta_q = (CHAR)info.DeltaQYDc;
        picParam->u_dc_delta_q = (CHAR)info.DeltaQUDc;
        picParam->v_dc_delta_q = (CHAR)info.DeltaQVDc;
        picParam->u_ac_delta_q = (CHAR)info.DeltaQUAc;
        picParam->v_ac_delta_q = (CHAR)info.DeltaQVac;

        memset(&picParam->stAV1Segments.feature_data, 0, sizeof(picParam->stAV1Segments.feature_data)); // TODO: [Global] implement proper setting
        memset(&picParam->stAV1Segments.feature_mask, 0, sizeof(&picParam->stAV1Segments.feature_mask)); // TODO: [Global] implement proper setting

#if AV1D_DDI_VERSION >= 21
        picParam->cdef_damping_minus_3 = (UCHAR)(info.cdef_damping - 3);
#else
        picParam->cdef_pri_damping = (UCHAR)info.cdef_damping;
        picParam->cdef_sec_damping = (UCHAR)info.cdef_sec_damping;
#endif
        picParam->cdef_bits = (UCHAR)info.cdef_bits;

        for (Ipp8u i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            picParam->cdef_y_strengths[i] = (UCHAR)info.cdef_y_strength[i];
            picParam->cdef_uv_strengths[i] = (UCHAR)info.cdef_uv_strength[i];
        }

#if AV1D_DDI_VERSION >= 21
        auto& ddiQMFlags = picParam->wQMatrixFlags.fields;
        ddiQMFlags.using_qmatrix = info.using_qmatrix;
        ddiQMFlags.qm_y = info.qm_y;
        ddiQMFlags.qm_u = info.qm_u;;
        ddiQMFlags.qm_v = info.qm_v;;
#else
        picParam->dwModeControlFlags.fields.using_qmatrix = info.using_qmatrix;
        picParam->dwModeControlFlags.fields.min_qmlevel = info.min_qmlevel;
        picParam->dwModeControlFlags.fields.max_qmlevel = info.max_qmlevel;
        picParam->dwModeControlFlags.fields.allow_interintra_compound = info.enable_interintra_compound;
        picParam->dwModeControlFlags.fields.allow_masked_compound = info.enable_masked_compound;
        picParam->dwModeControlFlags.fields.loop_filter_across_tiles_enabled = info.loop_filter_across_tiles_enabled;
        picParam->dwModeControlFlags.fields.allow_screen_content_tools = info.allow_screen_content_tools;
#endif
        picParam->dwModeControlFlags.fields.delta_q_present_flag = info.delta_q_present;
        picParam->dwModeControlFlags.fields.log2_delta_q_res = CeilLog2(info.delta_q_res);
        picParam->dwModeControlFlags.fields.delta_lf_present_flag = info.delta_lf_present;
        picParam->dwModeControlFlags.fields.log2_delta_lf_res = CeilLog2(info.delta_lf_res);
        picParam->dwModeControlFlags.fields.delta_lf_multi = info.delta_lf_multi;
        picParam->dwModeControlFlags.fields.tx_mode = info.TxMode;
        picParam->dwModeControlFlags.fields.reference_mode = info.reference_mode;
        picParam->dwModeControlFlags.fields.reduced_tx_set_used = info.reduced_tx_set;
        picParam->dwModeControlFlags.fields.ReservedField = 0;

        picParam->LoopRestorationFlags.fields.yframe_restoration_type = info.lr_type[0];
        picParam->LoopRestorationFlags.fields.cbframe_restoration_type = info.lr_type[1];
        picParam->LoopRestorationFlags.fields.crframe_restoration_type = info.lr_type[2];
        picParam->LoopRestorationFlags.fields.lr_unit_shift = info.lr_unit_shift;
        picParam->LoopRestorationFlags.fields.lr_uv_shift = info.lr_uv_shift;

#if AV1D_DDI_VERSION >= 21
        for (Ipp8u i = 0; i < INTER_REFS; i++)
        {
            picParam->wm[i].wmtype = info.global_motion_params[i + 1].wmtype;
            for (Ipp8u j = 0; j < 8; j++)
            {
                picParam->wm[i].wmmat[j] = info.global_motion_params[i + 1].wmmat[j];
                // TODO: [Rev0.5] implement processing of alpha, beta, gamma, delta.
            }
        }
#else // AV1D_DDI_VERSION >= 21
        for (Ipp8u i = 0; i < INTER_REFS; i++)
        {
            picParam->gm_type[i] = (UCHAR)info.global_motion_params[i + 1].wmtype;
            for (Ipp8u j = 0; j < 6; j++) // why only 6?
            {
                picParam->gm_params[i][j] = info.global_motion_params[i + 1].wmmat[j];
            }
        }
#endif // AV1D_DDI_VERSION >= 21

        picParam->tg_size_bit_offset = info.tile_group_bit_offset;

#if AV1D_DDI_VERSION >= 21
        if (info.uniform_tile_spacing_flag)
        {
            picParam->log2_tile_rows = (UCHAR)info.TileRowsLog2;
            picParam->log2_tile_cols = (UCHAR)info.TileColsLog2;
        }
        else
        {
            picParam->tile_cols = (USHORT)info.TileCols;
            picParam->tile_rows = (USHORT)info.TileRows;

            for (Ipp32u i = 0; i < picParam->tile_cols; i++)
            {
                picParam->width_in_sbs_minus_1[i] =
                    (USHORT)(info.SbColStarts[i + 1] - info.SbColStarts[i]);
            }

            for (int i = 0; i < picParam->tile_rows; i++)
            {
                picParam->height_in_sbs_minus_1[i] =
                    (USHORT)(info.SbRowStarts[i + 1] - info.SbRowStarts[i]);
            }
        }
#else
        picParam->log2_tile_rows = (UCHAR)info.TileRowsLog2;
        picParam->log2_tile_cols = (UCHAR)info.TileColsLog2;

        picParam->tile_cols = (USHORT)info.TileCols;
        picParam->tile_rows = (USHORT)info.TileRows;

        picParam->tile_size_bytes = (UCHAR)info.TileSizeBytes;
#endif
    }

#if AV1D_DDI_VERSION >= 21
    void PackerIntel::PackTileControlParams(DXVA_Intel_Tile_AV1* tileControlParam, TileLocation const& loc)
    {
        // TODO: [Rev0.5] Add support for multiple tiles
        tileControlParam->BSTileDataLocation = (UINT)loc.offset;
        tileControlParam->BSTileBytesInBuffer = (UINT)loc.size;
        tileControlParam->wBadBSBufferChopping = 0;
        tileControlParam->tile_row = (USHORT)loc.row;
        tileControlParam->tile_column = (USHORT)loc.col;
        tileControlParam->StartTileIdx = (USHORT)loc.startIdx;
        tileControlParam->EndTileIdx = (USHORT)loc.endIdx;
        tileControlParam->anchor_frame_idx.Index15Bits = 0;
        tileControlParam->BSTilePayloadSizeInBytes = (UINT)loc.size;
    }
#else
    void PackerIntel::PackBitstreamControlParams(DXVA_Intel_BitStream_AV1_Short* bsControlParam, AV1DecoderFrame const* frame)
    {
        FrameHeader const& info =
            frame->GetFrameHeader();

        bsControlParam->BSOBUDataLocation = info.frameHeaderLength;
        bsControlParam->BitStreamBytesInBuffer = info.frameDataSize - info.frameHeaderLength;
        bsControlParam->wBadBSBufferChopping = 0;
    }
#endif

#endif // UMC_VA_DXVA

#ifdef UMC_VA_LINUX
/****************************************************************************************************/
// Linux VAAPI packer implementation
/****************************************************************************************************/

    PackerVA::PackerVA(UMC::VideoAccelerator * va)
        : Packer(va)
    {}

    UMC::Status PackerVA::GetStatusReport(void * pStatusReport, size_t size)
    {
        return UMC_OK;
    }

    void PackerVA::BeginFrame() {}
    void PackerVA::EndFrame() {}

#if UMC_AV1_DECODER_REV >= 5000
    void PackerVA::PackAU(std::vector<TileSet>& tileSets, AV1DecoderFrame const* info, bool firstSubmission)
    {
        if (firstSubmission)
        {
            // it's first submission for current frame - need to fill and submit picture parameters
            UMC::UMCVACompBuffer* compBufPic = nullptr;
            VADecPictureParameterBufferAV1 *picParam =
                (VADecPictureParameterBufferAV1*)m_va->GetCompBuffer(VAPictureParameterBufferType, &compBufPic, sizeof(VADecPictureParameterBufferAV1));

            if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < static_cast<Ipp32s>(sizeof(VADecPictureParameterBufferAV1))))
                throw av1_exception(MFX_ERR_MEMORY_ALLOC);

            compBufPic->SetDataSize(sizeof(VADecPictureParameterBufferAV1));
            memset(picParam, 0, sizeof(VADecPictureParameterBufferAV1));
            PackPicParams(picParam, info);
        }

        UMC::UMCVACompBuffer* compBufBs = nullptr;
        Ipp8u* const bitstreamData = (mfxU8 *)m_va->GetCompBuffer(VASliceDataBufferType, &compBufBs, CalcSizeOfTileSets(tileSets));
        if (!bitstreamData || !compBufBs)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::vector<VABitStreamParameterBufferAV1> tileControlParams;

        for (auto& tileSet : tileSets)
        {
            const size_t offsetInBuffer = compBufBs->GetDataSize();
            const size_t spaceInBuffer = compBufBs->GetBufferSize() - offsetInBuffer;
            TileLayout layout;
            const size_t bytesSubmitted = tileSet.Submit(bitstreamData, spaceInBuffer, offsetInBuffer, layout);

            if (bytesSubmitted)
            {
                compBufBs->SetDataSize(static_cast<Ipp32u>(offsetInBuffer + bytesSubmitted));

                for (auto& loc : layout)
                {
                    tileControlParams.emplace_back();
                    PackTileControlParams(&tileControlParams.back(), loc);
                }
            }
        }

        UMCVACompBuffer* compBufTile = nullptr;
        const Ipp32s tileControlInfoSize = static_cast<Ipp32s>(sizeof(VABitStreamParameterBufferAV1) * tileControlParams.size());
        VABitStreamParameterBufferAV1 *tileControlParam = (VABitStreamParameterBufferAV1*)m_va->GetCompBuffer(VASliceParameterBufferType, &compBufTile, tileControlInfoSize);
        if (!tileControlParam || !compBufTile || (compBufTile->GetBufferSize() < tileControlInfoSize))
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        memcpy_s(tileControlParam, compBufTile->GetBufferSize(), tileControlParams.data(), tileControlInfoSize);
        compBufTile->SetDataSize(tileControlInfoSize);
    }
#else
    void PackerVA::PackAU(UMC_VP9_DECODER::VP9Bitstream* bs, AV1DecoderFrame const* info)
    {
        if (!bs || !info)
            throw av1_exception(MFX_ERR_NULL_PTR);

        UMC::UMCVACompBuffer* pCompBuf = NULL;
        VADecPictureParameterBufferAV1 *picParam =
            (VADecPictureParameterBufferAV1*)m_va->GetCompBuffer(VAPictureParameterBufferType, &pCompBuf, sizeof(VADecPictureParameterBufferAV1));

        if (!picParam)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        memset(picParam, 0, sizeof(VADecPictureParameterBufferAV1));
        PackPicParams(picParam, info);

        pCompBuf = NULL;
        VABitStreamParameterBufferAV1 *bsControlParam =
            (VABitStreamParameterBufferAV1*)m_va->GetCompBuffer(VASliceParameterBufferType, &pCompBuf, sizeof(VABitStreamParameterBufferAV1));
        if (!bsControlParam)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        memset(bsControlParam, 0, sizeof(VABitStreamParameterBufferAV1));
        PackBitstreamControlParams(bsControlParam, info);

        Ipp8u* data;
        Ipp32u length;
        bs->GetOrg(&data, &length);
        Ipp32u const offset = bs->BytesDecoded();
        length -= offset;

        pCompBuf = NULL;
        mfxU8 *bistreamData = (mfxU8*)m_va->GetCompBuffer(VASliceDataBufferType, &pCompBuf, length);
        if (!bistreamData)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        mfx_memcpy(bistreamData, length, data + offset, length);
        pCompBuf->SetDataSize(length);
    }
#endif // UMC_AV1_DECODER_REV >= 5000

    void PackerVA::PackPicParams(VADecPictureParameterBufferAV1* picParam, AV1DecoderFrame const* frame)
    {
        SequenceHeader const& sh = frame->GetSeqHeader();

        FrameHeader const& info =
            frame->GetFrameHeader();

#if UMC_AV1_DECODER_REV >= 5000
        picParam->frame_width_minus1 = (uint16_t)info.FrameWidth - 1;
        picParam->frame_height_minus1 = (uint16_t)info.FrameHeight - 1;

        // fill seq parameters
        picParam->profile = (uint8_t)sh.seq_profile;

        auto& seqInfo = picParam->seq_info_fields.fields;

        seqInfo.still_picture = 0;
        seqInfo.sb_size_128x128 = (sh.sbSize == BLOCK_128X128) ? 1 : 0;
        seqInfo.enable_filter_intra = info.enable_filter_intra;
        seqInfo.enable_intra_edge_filter = info.enable_intra_edge_filter;
        seqInfo.enable_interintra_compound = info.enable_interintra_compound;
        seqInfo.enable_masked_compound = info.enable_masked_compound;
        seqInfo.enable_dual_filter = sh.enable_dual_filter;
        seqInfo.enable_order_hint = sh.enable_order_hint;
        seqInfo.enable_jnt_comp = sh.enable_jnt_comp;
        seqInfo.enable_cdef = 1;
        seqInfo.enable_restoration = 1;
        seqInfo.mono_chrome = sh.mono_chrome;
        seqInfo.color_range = sh.color_range;
        seqInfo.subsampling_x = sh.subsampling_x;
        seqInfo.subsampling_y = sh.subsampling_y;
        seqInfo.chroma_sample_position = sh.chroma_sample_position;
        seqInfo.film_grain_params_present = sh.film_grain_param_present;

        picParam->bit_depth_idx = (sh.BitDepth == 10) ? 1 :
            (sh.BitDepth == 12) ? 2 : 0;
        picParam->order_hint_bits_minus_1 = sh.order_hint_bits_minus1;

        // fill pic params
        auto& picInfo = picParam->pic_info_fields.bits;

        picInfo.frame_type = info.frame_type;
        picInfo.show_frame = info.show_frame;
        picInfo.showable_frame = info.showable_frame;
        picInfo.error_resilient_mode = info.error_resilient_mode;
        picInfo.disable_cdf_update = info.disable_cdf_update;
        picInfo.allow_screen_content_tools = info.allow_screen_content_tools;
        picInfo.force_integer_mv = info.seq_force_integer_mv;
        picInfo.allow_intrabc = info.allow_intrabc;
        picInfo.use_superres = (info.SuperresDenom == SCALE_NUMERATOR) ? 0 : 1;
        picInfo.allow_high_precision_mv = info.allow_high_precision_mv;
        picInfo.is_motion_mode_switchable = info.is_motion_mode_switchable;
        picInfo.use_ref_frame_mvs = info.use_ref_frame_mvs;
        picInfo.disable_frame_end_update_cdf = (info.refresh_frame_context == REFRESH_FRAME_CONTEXT_DISABLED) ? 1 : 0;;
        picInfo.uniform_tile_spacing_flag = info.uniform_tile_spacing_flag;
        picInfo.allow_warped_motion = 0;
        picInfo.refresh_frame_context = info.refresh_frame_context;
        picInfo.large_scale_tile = info.large_scale_tile;

        picParam->order_hint = (uint8_t)info.order_hint;
        picParam->superres_scale_denominator = (uint8_t)info.SuperresDenom;

        picParam->interp_filter = (uint8_t)info.interpolation_filter;

        // fill segmentation params and map
        auto& seg = picParam->seg_info;
        seg.segment_info_fields.bits.enabled = info.segmentation_params.segmentation_enabled;;
        seg.segment_info_fields.bits.temporal_update = info.segmentation_params.segmentation_temporal_update;
        seg.segment_info_fields.bits.update_map = info.segmentation_params.segmentation_update_map;
        memset(&seg.feature_data, 0, sizeof(seg.feature_data)); // TODO: [Global] implement proper setting
        memset(&seg.feature_mask, 0, sizeof(seg.feature_mask)); // TODO: [Global] implement proper setting

        // set current and reference frames
        picParam->current_frame = (VASurfaceID)m_va->GetSurfaceID(frame->GetMemID());
        picParam->current_display_picture = (VASurfaceID)m_va->GetSurfaceID(frame->GetMemID());

        for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
        {
            seg.feature_mask[i] = (uint8_t)info.segmentation_params.FeatureMask[i];
            for (Ipp8u j = 0; j < SEG_LVL_MAX; j++)
                seg.feature_data[i][j] = (int16_t)info.segmentation_params.FeatureData[i][j];
        }

        if (KEY_FRAME == info.frame_type)
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
            {
                picParam->ref_frame_map[i] = VA_INVALID_SURFACE;
            }
        }
        else
        {
            for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
                picParam->ref_frame_map[ref] = (VASurfaceID)m_va->GetSurfaceID(frame->frame_dpb[ref]->GetMemID());

            for (mfxU8 ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
            {
                const Ipp8u idxInDPB = (Ipp8u)info.ref_frame_idx[ref_idx];
                picParam->ref_frame_idx[ref_idx] = idxInDPB;
            }
        }

        picParam->primary_ref_frame = (uint8_t)info.primary_ref_frame;

        // fill loop filter params
        picParam->filter_level[0] = (uint8_t)info.loop_filter_params.loop_filter_level[0];
        picParam->filter_level[1] = (uint8_t)info.loop_filter_params.loop_filter_level[1];
        picParam->filter_level_u = (uint8_t)info.loop_filter_params.loop_filter_level[2];
        picParam->filter_level_v = (uint8_t)info.loop_filter_params.loop_filter_level[3];

        auto& lfInfo = picParam->loop_filter_info_fields.bits;
        lfInfo.sharpness_level = (uint8_t)info.loop_filter_params.loop_filter_sharpness;
        lfInfo.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        lfInfo.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;


        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            picParam->ref_deltas[i] = info.loop_filter_params.loop_filter_ref_deltas[i];
        }
        for (Ipp8u i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam->mode_deltas[i] = info.loop_filter_params.loop_filter_mode_deltas[i];
        }

        // fill quantization params
        picParam->base_qindex = (int16_t)info.base_q_idx;
        picParam->y_dc_delta_q = (int8_t)info.DeltaQYDc;
        picParam->u_dc_delta_q = (int8_t)info.DeltaQUDc;
        picParam->v_dc_delta_q = (int8_t)info.DeltaQVDc;
        picParam->u_ac_delta_q = (int8_t)info.DeltaQUAc;
        picParam->v_ac_delta_q = (int8_t)info.DeltaQVac;

        // fill CDEF
        picParam->cdef_damping_minus_3 = (uint8_t)(info.cdef_damping - 3);
        picParam->cdef_bits = (uint8_t)info.cdef_bits;

        for (Ipp8u i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            picParam->cdef_y_strengths[i] = (uint8_t)info.cdef_y_strength[i];
            picParam->cdef_uv_strengths[i] = (uint8_t)info.cdef_uv_strength[i];
        }

        // fill quantization matrix params
        auto& qmFlags = picParam->qmatrix_fields.bits;
        qmFlags.using_qmatrix = info.using_qmatrix;
        qmFlags.qm_y = info.qm_y;
        qmFlags.qm_u = info.qm_u;
        qmFlags.qm_v = info.qm_v;


        auto& modeCtrl = picParam->mode_control_fields.bits;
        modeCtrl.delta_q_present_flag = info.delta_q_present;
        modeCtrl.log2_delta_q_res = CeilLog2(info.delta_q_res);
        modeCtrl.delta_lf_present_flag = info.delta_lf_present;
        modeCtrl.log2_delta_lf_res = CeilLog2(info.delta_lf_res);
        modeCtrl.delta_lf_multi = info.delta_lf_multi;
        modeCtrl.tx_mode = info.TxMode;
        modeCtrl.reference_mode = info.reference_mode;
        modeCtrl.reduced_tx_set_used = info.reduced_tx_set;

        // fill loop restoration params
        auto& lrInfo = picParam->loop_restoration_fields.bits;
        lrInfo.yframe_restoration_type = info.lr_type[0];
        lrInfo.cbframe_restoration_type = info.lr_type[1];
        lrInfo.crframe_restoration_type = info.lr_type[2];;
        lrInfo.lr_unit_shift = info.lr_unit_shift;
        lrInfo.lr_uv_shift = info.lr_uv_shift;

        // fill global motion params
        for (Ipp8u i = 0; i < INTER_REFS; i++)
        {
            picParam->wm[i].wmtype = static_cast<VATransformationType>(info.global_motion_params[i + 1].wmtype);
            for (Ipp8u j = 0; j < 8; j++)
            {
                picParam->wm[i].wmmat[j] = info.global_motion_params[i + 1].wmmat[j];
                // TODO: [Rev0.5] implement processing of alpha, beta, gamma, delta.
            }
        }

        // fill tile params
        picParam->tile_cols = (uint16_t)info.TileCols;
        picParam->tile_rows = (uint16_t)info.TileRows;

        if (!info.uniform_tile_spacing_flag)
        {
            for (Ipp32u i = 0; i < picParam->tile_cols; i++)
            {
                picParam->width_in_sbs_minus_1[i] =
                    (uint16_t)(info.SbColStarts[i + 1] - info.SbColStarts[i]);
            }

            for (int i = 0; i < picParam->tile_rows; i++)
            {
                picParam->height_in_sbs_minus_1[i] =
                    (uint16_t)(info.SbRowStarts[i + 1] - info.SbRowStarts[i]);
            }
        }

#else // UMC_AV1_DECODER_REV >= 5000
        picParam->frame_width_minus1 = (uint16_t)info.FrameWidth - 1;
        picParam->frame_height_minus1 = (uint16_t)info.FrameHeight - 1;

        picParam->pic_fields.bits.frame_type = info.frame_type;
        picParam->pic_fields.bits.show_frame = info.show_frame;
        picParam->pic_fields.bits.error_resilient_mode = info.error_resilient_mode;
        picParam->pic_fields.bits.intra_only = info.intra_only;
        picParam->pic_fields.bits.extra_plane = 0;

        picParam->pic_fields.bits.allow_high_precision_mv = info.allow_high_precision_mv;
        picParam->pic_fields.bits.sb_size_128x128 = (info.sbSize == BLOCK_128X128) ? 1 : 0;
        picParam->interp_filter = (uint8_t)info.interpolation_filter;
        picParam->seg_info.segment_info_fields.bits.segmentation_enabled = info.segmentation_params.segmentation_enabled;
        picParam->seg_info.segment_info_fields.bits.temporal_update = info.segmentation_params.segmentation_temporal_update;
        picParam->seg_info.segment_info_fields.bits.update_map = info.segmentation_params.segmentation_update_map;

        for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
        {
            picParam->seg_info.feature_mask[i] = (uint8_t)info.segmentation_params.FeatureMask[i];
            for (Ipp8u j = 0; j < SEG_LVL_MAX; j++)
                picParam->seg_info.feature_data[i][j] = (int16_t)info.segmentation_params.FeatureData[i][j];
        }

        picParam->pic_fields.bits.reset_frame_context = info.reset_frame_context;
        picParam->pic_fields.bits.refresh_frame_context = info.refresh_frame_context;
        picParam->pic_fields.bits.frame_context_idx = info.frame_context_idx;

        if (KEY_FRAME == info.frame_type)
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
            {
                picParam->ref_frame_map[i] = VA_INVALID_SURFACE;
            }
        }
        else
        {
            for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
                picParam->ref_frame_map[ref] = (VASurfaceID)m_va->GetSurfaceID(frame->frame_dpb[ref]->GetMemID());

            for (mfxU8 ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
            {
                Ipp8u idxInDPB = (Ipp8u)info.ref_frame_idx[ref_idx];
                picParam->ref_frame_idx[ref_idx] = idxInDPB;
                picParam->ref_frame_sign_bias[ref_idx + 1] = (uint8_t)info.ref_frame_sign_bias[ref_idx];
            }
        }

        picParam->current_frame = (VASurfaceID)m_va->GetSurfaceID(frame->GetMemID());

        picParam->filter_level[0] = (uint8_t)info.loop_filter_params.loop_filter_level[0];
        picParam->filter_level[1] = (uint8_t)info.loop_filter_params.loop_filter_level[1];
        picParam->filter_level_u = (uint8_t)info.loop_filter_params.loop_filter_level[2];
        picParam->filter_level_v = (uint8_t)info.loop_filter_params.loop_filter_level[3];

        picParam->sharpness_level = (uint8_t)info.loop_filter_params.loop_filter_sharpness;
        picParam->uncompressed_frame_header_length_in_bytes = (uint8_t)info.frameHeaderLength;

        picParam->bit_stream_bytes_in_buffer = info.frameDataSize;

        picParam->profile = (uint8_t)info.profile;
        picParam->log2_bit_depth_minus8 = (uint8_t)(info.BitDepth - 8); // DDI for Rev 0.25.2 uses BitDepth instead of log2 - 8
                                                                         // TODO: fix when va_dec_av1.h will be fixed

        picParam->pic_fields.bits.subsampling_x = (uint8_t)info.subsampling_x;
        picParam->pic_fields.bits.subsampling_y = (uint8_t)info.subsampling_y;

        picParam->ref_fields.bits.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        picParam->ref_fields.bits.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;
        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            picParam->ref_deltas[i] = info.loop_filter_params.loop_filter_ref_deltas[i];
        }
        for (Ipp8u i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam->mode_deltas[i] = info.loop_filter_params.loop_filter_mode_deltas[i];
        }

        picParam->ref_fields.bits.use_prev_frame_mvs = info.use_ref_frame_mvs;

        picParam->base_qindex = (int16_t)info.base_q_idx;
        picParam->y_dc_delta_q = (int8_t)info.DeltaQYDc;
        picParam->u_dc_delta_q = (int8_t)info.DeltaQUDc;
        picParam->v_dc_delta_q = (int8_t)info.DeltaQVDc;
        picParam->u_ac_delta_q = (int8_t)info.DeltaQUAc;
        picParam->v_ac_delta_q = (int8_t)info.DeltaQVac;

        memset(&picParam->seg_info.feature_data, 0, sizeof(picParam->seg_info.feature_data)); // TODO: [Global] implement proper setting
        memset(&picParam->seg_info.feature_mask, 0, sizeof(picParam->seg_info.feature_mask)); // TODO: [Global] implement proper setting

        picParam->cdef_pri_damping = (uint8_t)info.cdef_damping;
        picParam->cdef_sec_damping = (uint8_t)info.cdef_sec_damping;
        picParam->cdef_bits = (uint8_t)info.cdef_bits;

        for (Ipp8u i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            picParam->cdef_strengths[i] = (uint8_t)info.cdef_y_strength[i];
            picParam->cdef_uv_strengths[i] = (uint8_t)info.cdef_uv_strength[i];
        }
        picParam->mode_fields.bits.using_qmatrix = info.using_qmatrix;
        picParam->mode_fields.bits.min_qmlevel = info.min_qmlevel;
        picParam->mode_fields.bits.max_qmlevel = info.max_qmlevel;
        picParam->mode_fields.bits.delta_q_present_flag = info.delta_q_present;
        picParam->mode_fields.bits.log2_delta_q_res = CeilLog2(info.delta_q_res);
        picParam->mode_fields.bits.delta_lf_present_flag = info.delta_lf_present;
        picParam->mode_fields.bits.log2_delta_lf_res = CeilLog2(info.delta_lf_res);
        picParam->mode_fields.bits.tx_mode = info.TxMode;
        picParam->mode_fields.bits.reference_mode = info.reference_mode;
        picParam->mode_fields.bits.reduced_tx_set_used = info.reduced_tx_set;
        picParam->mode_fields.bits.loop_filter_across_tiles_enabled = info.loop_filter_across_tiles_enabled;
        picParam->mode_fields.bits.allow_screen_content_tools = info.allow_screen_content_tools;

        picParam->loop_restoration_fields.bits.yframe_restoration_type = info.lr_type[0];
        picParam->loop_restoration_fields.bits.cbframe_restoration_type = info.lr_type[1];
        picParam->loop_restoration_fields.bits.crframe_restoration_type = info.lr_type[2];
        picParam->loop_restoration_fields.bits.lr_unit_shift = info.lr_unit_shift;
        picParam->loop_restoration_fields.bits.lr_uv_shift = info.lr_uv_shift;

        picParam->tile_size_bit_offset = info.tile_group_bit_offset;

        picParam->log2_tile_rows = (uint8_t)info.TileRowsLog2;
        picParam->log2_tile_cols = (uint8_t)info.TileColsLog2;
        // TODO: [Global] add proper calculation of tile_rows/tile_cols during read of uncompressed header
        picParam->tile_cols = (uint16_t)info.TileCols;
        picParam->tile_rows = (uint16_t)info.TileRows;

        picParam->tile_size_bytes = (uint8_t)info.TileSizeBytes;
#endif // UMC_AV1_DECODER_REV >= 5000
    }

#if UMC_AV1_DECODER_REV >= 5000
    void PackerVA::PackTileControlParams(VABitStreamParameterBufferAV1* tileControlParam, TileLocation const& loc)
    {
        tileControlParam->bit_stream_data_offset = (uint32_t)loc.offset;
        tileControlParam->bit_stream_data_size = (uint32_t)loc.size;
        tileControlParam->bit_stream_data_flag = 0;
        tileControlParam->tile_row = (uint16_t)loc.row;
        tileControlParam->tile_column = (uint16_t)loc.col;
        tileControlParam->start_tile_idx = (uint16_t)loc.startIdx;
        tileControlParam->end_tile_idx = (uint16_t)loc.endIdx;
        tileControlParam->anchor_frame_idx = 0;
    }
#else
    void PackerVA::PackBitstreamControlParams(VABitStreamParameterBufferAV1* bsControlParam, AV1DecoderFrame const* frame)
    {
        FrameHeader const& info = frame->GetFrameHeader();

        bsControlParam->bit_stream_data_offset = info.frameHeaderLength;
        bsControlParam->bit_stream_data_size = info.frameDataSize - info.frameHeaderLength;
        bsControlParam->bit_stream_data_flag = 0;
    }
#endif // UMC_AV1_DECODER_REV >= 5000
#endif // UMC_VA_LINUX

} // namespace UMC_AV1_DECODER

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
