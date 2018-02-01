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

    inline
        void mfx_memcpy(void * dst, size_t dstLen, void * src, size_t len)
    {
        memcpy_s(dst, dstLen, src, len);
    }

    Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
    {
#ifdef UMC_VA_DXVA
        return new PackerIntel(va);
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

    void PackerIntel::PackAU(UMC_VP9_DECODER::VP9Bitstream* bs, AV1DecoderFrame const* info)
    {
        UMC::UMCVACompBuffer* compBufPic = NULL;
        DXVA_Intel_PicParams_AV1 *picParam = (DXVA_Intel_PicParams_AV1*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBufPic);
        if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_Intel_PicParams_AV1)))
            throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

        compBufPic->SetDataSize(sizeof(DXVA_Intel_PicParams_AV1));
        memset(picParam, 0, sizeof(DXVA_Intel_PicParams_AV1));

        PackPicParams(picParam, info);

        UMCVACompBuffer* compBufSeg = NULL;
        DXVA_Intel_Tile_Group_AV1_Short *tileGroupParam = (DXVA_Intel_Tile_Group_AV1_Short*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBufSeg);
        if (!tileGroupParam || !compBufSeg || (compBufSeg->GetBufferSize() < sizeof(DXVA_Intel_Tile_Group_AV1_Short)))
            throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

        compBufSeg->SetDataSize(sizeof(DXVA_Intel_Tile_Group_AV1_Short));
        memset(tileGroupParam, 0, sizeof(DXVA_Intel_Tile_Group_AV1_Short));

        PackTileGroupParams(tileGroupParam, info);

        Ipp8u* data;
        Ipp32u length;
        bs->GetOrg(&data, &length);
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

    void PackerIntel::PackPicParams(DXVA_Intel_PicParams_AV1* picParam, AV1DecoderFrame const* frame)
    {
        SequenceHeader const& sh = frame->GetSeqHeader();

#if AV1D_DDI_VERSION >= 11
        picParam->dwFormatAndPictureInfoFlags.fields.frame_id_numbers_present_flag = sh.frame_id_numbers_present_flag;
        picParam->dwFormatAndPictureInfoFlags.fields.use_reference_buffer = sh.frame_id_numbers_present_flag;
#else
        sh;
#endif

        FrameHeader const& info =
            frame->GetFrameHeader();

        picParam->frame_width_minus1 = (USHORT)info.width - 1;
        picParam->frame_height_minus1 = (USHORT)info.height - 1;

        picParam->dwFormatAndPictureInfoFlags.fields.frame_type = info.frameType;
        picParam->dwFormatAndPictureInfoFlags.fields.show_frame = info.showFrame;
        picParam->dwFormatAndPictureInfoFlags.fields.error_resilient_mode = info.errorResilientMode;
        picParam->dwFormatAndPictureInfoFlags.fields.intra_only = info.intraOnly;
        picParam->dwFormatAndPictureInfoFlags.fields.extra_plane = 0;

        picParam->dwFormatAndPictureInfoFlags.fields.allow_high_precision_mv = info.allowHighPrecisionMv;
        picParam->frame_interp_filter = (UCHAR)info.interpFilter;
        picParam->dwFormatAndPictureInfoFlags.fields.frame_parallel_decoding_mode = info.frameParallelDecodingMode;
        picParam->stAV1Segments.enabled = info.segmentation.enabled;;
        picParam->stAV1Segments.temporal_update = info.segmentation.temporalUpdate;
        picParam->stAV1Segments.update_map = info.segmentation.updateMap;
        picParam->stAV1Segments.abs_delta = info.segmentation.absDelta;
        picParam->stAV1Segments.Reserved4Bits = 0;

        for (Ipp8u i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
        {
            picParam->stAV1Segments.feature_mask[i] = (UCHAR)info.segmentation.featureMask[i];
            for (Ipp8u j = 0; j < SEG_LVL_MAX; j++)
                picParam->stAV1Segments.feature_data[i][j] = (SHORT)info.segmentation.featureData[i][j];
        }

        picParam->dwFormatAndPictureInfoFlags.fields.reset_frame_context = info.resetFrameContext;
        picParam->dwFormatAndPictureInfoFlags.fields.refresh_frame_context = info.refreshFrameContext;
        picParam->dwFormatAndPictureInfoFlags.fields.frame_context_idx = info.frameContextIdx;

        if (UMC_VP9_DECODER::KEY_FRAME == info.frameType)
            for (int i = 0; i < NUM_REF_FRAMES; i++)
            {
                picParam->ref_frame_map[i].bPicEntry = UCHAR_MAX;
            }
        else
        {
            for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
                picParam->ref_frame_map[ref].bPicEntry = (UCHAR)frame->frame_dpb[ref]->GetMemID();

            for (mfxU8 ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
            {
                Ipp8u idxInDPB = (Ipp8u)info.activeRefIdx[ref_idx];
                picParam->ref_frame_idx[ref_idx].bPicEntry = idxInDPB;
                picParam->ref_frame_sign_bias[ref_idx + 1] = (UCHAR)info.refFrameSignBias[ref_idx];
#if AV1D_DDI_VERSION < 15
                picParam->ref_frame_width_minus1[ref_idx] = (USHORT)info.sizesOfRefFrame[idxInDPB].width - 1;
                picParam->ref_frame_height_minus1[ref_idx] = (USHORT)info.sizesOfRefFrame[idxInDPB].height - 1;
#endif
            }

#if AV1D_DDI_VERSION < 14
            picParam->num_ref_idx_active = INTER_REFS; // TODO: implement proper setting. Don't understand how to get number of active refs - need to check code of aomdec.
#endif
        }

        picParam->CurrPic.bPicEntry = (UCHAR)frame->GetMemID();
        picParam->filter_level = (UCHAR)info.lf.filterLevel;
        picParam->sharpness_level = (UCHAR)info.lf.sharpnessLevel;
        picParam->UncompressedHeaderLengthInBytes = (UCHAR)info.frameHeaderLength;
#if AV1D_DDI_VERSION < 12
        picParam->compressed_header_size = (USHORT)info.firstPartitionSize;

        for (mfxU8 i = 0; i < UMC_VP9_DECODER::VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
            picParam->mb_segment_tree_probs[i] = info.segmentation.treeProbs[i];

        for (mfxU8 i = 0; i < UMC_VP9_DECODER::VP9_NUM_OF_PREDICTION_PROBS; ++i)
            picParam->segment_pred_probs[i] = info.segmentation.predProbs[i];
#endif

        picParam->BSBytesInBuffer = info.frameDataSize;
        picParam->StatusReportFeedbackNumber = ++m_report_counter;

        picParam->profile = (UCHAR)info.profile;
        picParam->BitDepthMinus8 = (UCHAR)(info.bit_depth - 8);
        picParam->dwFormatAndPictureInfoFlags.fields.subsampling_x = (UCHAR)info.subsamplingX;
        picParam->dwFormatAndPictureInfoFlags.fields.subsampling_y = (UCHAR)info.subsamplingY;

        picParam->wControlInfoFlags.fields.mode_ref_delta_enabled = info.lf.modeRefDeltaEnabled;
        picParam->wControlInfoFlags.fields.mode_ref_delta_update = info.lf.modeRefDeltaUpdate;
        for (Ipp8u i = 0; i < TOTAL_REFS; i++)
        {
            picParam->ref_deltas[i] = info.lf.refDeltas[i];
        }
        for (Ipp8u i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam->mode_deltas[i] = info.lf.modeDeltas[i];
        }

        picParam->wControlInfoFlags.fields.use_prev_frame_mvs = info.usePrevMVs;
        picParam->wControlInfoFlags.fields.ReservedField = 0;

        picParam->base_qindex = (SHORT)info.baseQIndex;
        picParam->y_dc_delta_q = (CHAR)info.y_dc_delta_q;
        picParam->uv_dc_delta_q = (CHAR)info.uv_dc_delta_q;
        picParam->uv_ac_delta_q = (CHAR)info.uv_ac_delta_q;

        memset(&picParam->stAV1Segments.feature_data, 0, sizeof(picParam->stAV1Segments.feature_data)); // TODO: implement proper setting
        memset(&picParam->stAV1Segments.feature_mask, 0, sizeof(&picParam->stAV1Segments.feature_mask)); // TODO: implement proper setting

#if AV1D_DDI_VERSION >= 11
        picParam->cdef_pri_damping = (UCHAR)info.cdefPriDamping;
        picParam->cdef_sec_damping = (UCHAR)info.cdefSecDamping;
        picParam->cdef_bits = (UCHAR)info.cdefBits;
#else
        picParam->dwModeControlFlags.fields.dering_damping = info.cdefDeringDamping;
        picParam->dwModeControlFlags.fields.clpf_damping = info.cdefClpfDamping;
        picParam->dwModeControlFlags.fields.nb_cdef_strengths = info.nbCdefStrengths;
#endif
        for (Ipp8u i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            picParam->cdef_strengths[i] = (UCHAR)info.cdefStrength[i];
            picParam->cdef_uv_strengths[i] = (UCHAR)info.cdefUVStrength[i];
        }
#if AV1D_DDI_VERSION >= 11
        picParam->dwModeControlFlags.fields.using_qmatrix = info.useQMatrix;
#endif
#if UMC_AV1_DECODER_REV >= 251
        picParam->dwModeControlFlags.fields.min_qmlevel = info.minQMLevel;
        picParam->dwModeControlFlags.fields.max_qmlevel = info.maxQMLevel;
#endif
        picParam->dwModeControlFlags.fields.delta_q_present_flag = info.deltaQPresentFlag;
        picParam->dwModeControlFlags.fields.log2_delta_q_res = CeilLog2(info.deltaQRes);
        picParam->dwModeControlFlags.fields.delta_lf_present_flag = info.deltaLFPresentFlag;
        picParam->dwModeControlFlags.fields.log2_delta_lf_res = CeilLog2(info.deltaLFRes);
        picParam->dwModeControlFlags.fields.tx_mode = info.txMode;
        picParam->dwModeControlFlags.fields.reference_mode = info.referenceMode;
        picParam->dwModeControlFlags.fields.reduced_tx_set_used = info.reducedTxSetUsed;
        picParam->dwModeControlFlags.fields.loop_filter_across_tiles_enabled = info.loopFilterAcrossTilesEnabled;
        picParam->dwModeControlFlags.fields.allow_screen_content_tools = info.allowScreenContentTools;
        picParam->dwModeControlFlags.fields.ReservedField = 0;

        picParam->log2_tile_rows = (UCHAR)info.log2TileRows;
        picParam->log2_tile_cols = (UCHAR)info.log2TileColumns;
#if AV1D_DDI_VERSION >= 11
        // TODO: add proper calculation of tile_rows/tile_cols during read of uncompressed header
        picParam->tile_cols = (USHORT)info.tileCols;
        picParam->tile_rows = (USHORT)info.tileRows;
#endif
        picParam->tile_size_bytes = (UCHAR)info.tileSizeBytes;
    }

    void PackerIntel::PackTileGroupParams(DXVA_Intel_Tile_Group_AV1_Short* tileGroupParam, AV1DecoderFrame const* frame)
    {
        FrameHeader const& info =
            frame->GetFrameHeader();

#if AV1D_DDI_VERSION >= 15
        tileGroupParam->BSOBUDataLocation = info.frameHeaderLength;
        // hardcode info about tiles for rev 0.25.1
        tileGroupParam->StartTileIdx = 0;
        tileGroupParam->EndTileIdx = 0;
        tileGroupParam->NumTilesInBuffer = 1;
#else
        tileGroupParam->BSNALunitDataLocation = info.frameHeaderLength;
#endif
        tileGroupParam->TileGroupBytesInBuffer = info.frameDataSize - info.frameHeaderLength;
        tileGroupParam->wBadTileGroupChopping = 0;
    }

#endif // UMC_VA_DXVA

} // namespace UMC_AV1_DECODER

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
