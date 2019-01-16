// Copyright (c) 2014-2019 Intel Corporation
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


#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_tables.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_encode.h"
#include "mfx_av1_bitwriter.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_scan.h"
#include "mfx_av1_prob_opt.h"
#include "mfx_av1_probabilities.h"
#include "mfx_av1_get_context.h"
#include "mfx_av1_binarization.h"
#include "mfx_av1_defs.h"

#if USE_CMODEL_PAK
#include "av1_pak_hw.h"

using namespace AV1Enc;
using namespace MfxEnumShortAliases;

void SetPakFrameState(const AV1VideoParam &par, Frame &frame, CmodelAv1::Av1PakHWPicState &m_pak_pic_state)
{
    const int32_t enable_filter_intra = 0;
    const int32_t enable_intra_edge_filter = 0;
    const int32_t enable_dual_filter = 0;
    const int32_t enable_interintra_compound = 0;
    const int32_t enable_masked_compound = 0;
    const int32_t enable_jnt_comp = 0;
    const int32_t enable_warped_motion = 0;
    const int32_t enable_superres = 0;
    const int32_t enable_restoration = par.lrFlag ? 1 : 0;
    const int32_t enable_cdef = par.cdefFlag ? 1 : 0;
    const int32_t seq_allow_screen_content_tools = 0;
    const int32_t seq_force_integer_mv = 0;
    const int32_t allow_intrabc = 0;
    const int32_t delta_q_present_flag = 0;
    const int32_t reduced_tx_set_used = 1;
    const int32_t skip_mode_flag = 0;

    //DW 1
    m_pak_pic_state.CurFrameSize.WidthInPixelMinus1 = par.Width - 1;//curFrameWidth - 1;
    m_pak_pic_state.CurFrameSize.HeightInPixelMinus1 = par.Height - 1;//curFrameHeight - 1;
    //DW 2
    m_pak_pic_state.SequenceChromaSubSamplingFormat = par.chromaFormatIdc;//m_Params.ChromaFormatIDC;
    int bitDepth = par.src10Enable ? 1 : 0;
    m_pak_pic_state.SequencePixelBitDepthIdc = bitDepth;// (par.bitDepthLuma == 8 ? 0 : (par.bitDepthLuma == 10 ? 1 : 2));//(m_ctrl.BitDepth == AV1_BD_8 ? 0 : (m_ctrl.BitDepth == AV1_BD_10 ? 1 : 2));
    m_pak_pic_state.SequenceFullColorRangeFlag = 0;//m_ctrl.ColorRange; //PAK doesn't use this flag
    m_pak_pic_state.SequenceSuperblockSizeUsed = 0;
    m_pak_pic_state.SequenceEnableOrderHintFlag = par.seqParams.enableOrderHint;//m_ctrl.EnableOrderHint;
    m_pak_pic_state.SequenceOrderHintBitsMinus1 = par.seqParams.orderHintBits - 1;//m_ctrl.OrderHintBits - 1;
    m_pak_pic_state.SequenceEnableFilterIntraFlag = enable_filter_intra;
    m_pak_pic_state.SequenceEnableIntraEdgeFilterFlag = enable_intra_edge_filter;
    m_pak_pic_state.SequenceEnableDualFilterFlag = enable_dual_filter;
    m_pak_pic_state.SequenceEnableInterIntraCompoundFlag = enable_interintra_compound;
    m_pak_pic_state.SequenceEnableMaskedCompoundFlag = enable_masked_compound;
    m_pak_pic_state.SequenceEnableJointCompoundFlag = (enable_jnt_comp && par.seqParams.enableOrderHint) ? 1 : 0;
    // DW 3
    m_pak_pic_state.AllowScreenContentToolsFlag = seq_allow_screen_content_tools;
    m_pak_pic_state.ForceIntegerMVFlag = seq_force_integer_mv;
    m_pak_pic_state.AllowWarpedMotionFlag = enable_warped_motion;
    m_pak_pic_state.UseCDEFFilterFlag = enable_cdef;
    m_pak_pic_state.UseSuperResFlag = enable_superres;
    m_pak_pic_state.UseLoopRestorationFilterFlag = enable_restoration;//((m_Params.LRMode == 0) ? 0 : 1);
    m_pak_pic_state.ApplyFilmGrainFlag = 0; //PAK doesn't use this flag
    m_pak_pic_state.FrameType = (frame.m_picCodeType == MFX_FRAMETYPE_I ? AV1Enc::KEY_FRAME : AV1Enc::NON_KEY_FRAME);//m_ctrl.FrameType;
    m_pak_pic_state.IntraOnlyFlag = frame.IsIntra();//(m_ctrl.FrameType == AV1_KEY_FRAME) || m_ctrl.IntraOnly;
    m_pak_pic_state.ShowFrameFlag = 0; //PAK doesn't use this flag
    m_pak_pic_state.ShowableFrameFlag = 0; //PAK doesn't use this flag
    m_pak_pic_state.ErrorResilientModeFlag = par.errorResilientMode;
    m_pak_pic_state.AllowIntraBCFlag = allow_intrabc;
    m_pak_pic_state.PrimaryReferenceFrameIdx = 7;//m_ctrl.PrimaryRefFrame;  //PAK doesn't use this flag
    // DW 4
    m_pak_pic_state.SegmentationEnableFlag = 0;//m_ctrl.SegOn;
    m_pak_pic_state.SegmentationUpdateMapFlag = 0;//m_ctrl.SegMapUpdate;
    m_pak_pic_state.SegmentationTemporalUpdateFlag = 0;//m_ctrl.SegTemporalUpdate;
    m_pak_pic_state.DeltaQPresentFlag = delta_q_present_flag;
    m_pak_pic_state.DeltaQRes = 0;//m_ctrl.Log2DeltaQRes;
    m_pak_pic_state.SegmentMapIsZeroFlag = 0; // will be set later
    m_pak_pic_state.BaseQindex = frame.m_sliceQpY;//m_ctrl.qIndex;
    m_pak_pic_state.Y_dc_delta_q = 0;//m_ctrl.qDelta[AV1_Y_DC];
    // DW 5
    m_pak_pic_state.U_dc_delta_q = 0;//m_ctrl.qDelta[AV1_U_DC];
    m_pak_pic_state.U_ac_delta_q = 0;//m_ctrl.qDelta[AV1_U_AC];
    m_pak_pic_state.V_dc_delta_q = 0;//m_ctrl.qDelta[AV1_V_DC];
    m_pak_pic_state.V_ac_delta_q = 0;//m_ctrl.qDelta[AV1_V_AC];
    // DW 6
    m_pak_pic_state.AllowHighPrecisionMV = par.allowHighPrecisionMv;
    m_pak_pic_state.FrameLevelReferenceModeSelect = (frame.referenceMode == REFERENCE_MODE_SELECT);//(m_ctrl.CompPredMode == AV1_SINGLE_REFERENCE ? 0 : 1);
    m_pak_pic_state.McompFilterType = CmodelAv1::AV1_SWITCHABLE;//m_ctrl.McompFilterMode;
    m_pak_pic_state.MotionModeSwitchableFlag = 0; //only Simple is supported
    m_pak_pic_state.UseReferenceFrameMVSetFlag = 0;//m_ctrl.EnableOrderHint && !m_ctrl.ErrorResilientMode && m_ctrl.EnableRefFrameMvs && !m_ctrl.IntraOnly && !m_ctrl.FrameType;
    m_pak_pic_state.CurrentFrameOrderHint = frame.curFrameOffset; //m_ctrl.CurFrameOffset;
    // DW 7
    m_pak_pic_state.ReducedTxSetUsed = reduced_tx_set_used;
    assert(par.txMode == AV1Enc::TX_MODE_SELECT);
    m_pak_pic_state.FrameTransformMode = (par.txMode == AV1Enc::TX_MODE_SELECT) ? 2 : 0;
    m_pak_pic_state.SkipModePresentFlag = skip_mode_flag;
    m_pak_pic_state.SkipModeFrame0 = 0;//m_ctrl.RefFrameIdx0;
    m_pak_pic_state.SkipModeFrame1 = 0;//m_ctrl.RefFrameIdx1;

    //DW 8
    CmodelAv1::U32 gm_type[7];

    for (int i = 0; i < 7; i++)
    {
        gm_type[i] = 0;//(CmodelAv1::I32)get_gmtype(m_ctrl.Wmmat[i]);
    }

    m_pak_pic_state.GlobalMotionType0 = gm_type[0];
    m_pak_pic_state.GlobalMotionType1 = gm_type[1];
    m_pak_pic_state.GlobalMotionType2 = gm_type[2];
    m_pak_pic_state.GlobalMotionType3 = gm_type[3];
    m_pak_pic_state.GlobalMotionType4 = gm_type[4];
    m_pak_pic_state.GlobalMotionType5 = gm_type[5];
    m_pak_pic_state.GlobalMotionType6 = gm_type[6];
    m_pak_pic_state.GlobalMotionType7 = 0;

    for (int i = 0; i < 7; i++) {
        memcpy(m_pak_pic_state.WarpParametersArray[i], CmodelAv1::default_warp_params, sizeof(CmodelAv1::default_warp_params));
    }

    //DW 31 - 38
    {
        int i = 0;
        m_pak_pic_state.RefFrameScaleFactor[i].HorizontalScaleFactor = ((par.Width << 14) + (par.Width >> 1)) / par.Width;
        m_pak_pic_state.RefFrameScaleFactor[i].VerticalScaleFactor = ((par.Height << 14) + (par.Height >> 1)) / par.Height;

        m_pak_pic_state.RefFrameSize[i].WidthInPixelMinus1 = par.Width - 1;
        m_pak_pic_state.RefFrameSize[i].HeightInPixelMinus1 = par.Height - 1;
    }

    int32_t signBias = 0;
    for (int32_t i = AV1Enc::AV1_LAST_FRAME; i < AV1Enc::INTER_REFS_PER_FRAME; i++)
    {
        signBias |= ((frame.refFrameSignBiasAv1[i] & 1) << (1 + i));
        if (frame.refFrameIdx[i] >= 0)
        {
            m_pak_pic_state.RefFrameScaleFactor[i + 1].HorizontalScaleFactor = ((par.Width << 14) + (par.Width >> 1)) / par.Width;
            m_pak_pic_state.RefFrameScaleFactor[i + 1].VerticalScaleFactor = ((par.Height << 14) + (par.Height >> 1)) / par.Height;

            m_pak_pic_state.RefFrameSize[i + 1].WidthInPixelMinus1 = par.Width - 1;
            m_pak_pic_state.RefFrameSize[i + 1].HeightInPixelMinus1 = par.Height - 1;

            m_pak_pic_state.ReferenceFrameOrderHint[i + 1] = frame.refFramesAv1[frame.refFrameIdx[i]] ? frame.refFramesAv1[frame.refFrameIdx[i]]->curFrameOffset : 0;
        }
        else
        {
            m_pak_pic_state.RefFrameScaleFactor[i + 1].HorizontalScaleFactor = 0;
            m_pak_pic_state.RefFrameScaleFactor[i + 1].VerticalScaleFactor = 0;
            m_pak_pic_state.RefFrameSize[i + 1].WidthInPixelMinus1 = 0;
            m_pak_pic_state.RefFrameSize[i + 1].HeightInPixelMinus1 = 0;
            m_pak_pic_state.ReferenceFrameOrderHint[i + 1] = 0;
        }
    }
    m_pak_pic_state.ReferenceFrameSignBias = signBias;

    for (int32_t i = AV1Enc::AV1_LAST_FRAME; i < AV1Enc::INTER_REFS_PER_FRAME; i++)
    {
        m_pak_pic_state.ref_frame_side[i] = frame.refFrameSide[i];
    }
    m_pak_pic_state.RDOQEnable = 0;
    m_pak_pic_state.SRC8PAK10Enable = 0;
}

void SetPakTileState(const AV1VideoParam &par, CmodelAv1::Av1PakHWTileState &m_pak_tile_state, int32_t tile)
{
    const int32_t disable_cdf_update = 0;

    m_pak_tile_state.DisableCDFUpdateFlag = disable_cdf_update;
    m_pak_tile_state.DisableFrameContextUpdateFlag = 0;

    m_pak_tile_state.NumOfTileColumnsInFrameMinus1 = par.tileParam.cols - 1;
    m_pak_tile_state.NumOfTileRowsInFrameMinus1 = par.tileParam.rows - 1;

    memset(m_pak_tile_state.tile_heights, 0, sizeof(m_pak_tile_state.tile_heights));
    memset(m_pak_tile_state.tile_widths, 0, sizeof(m_pak_tile_state.tile_widths));
    int i, start_sb;
    for (i = 0, start_sb = 0; i < par.NumTileCols; i++) {
        int32_t tile = i;
        const int32_t tileRow = tile / par.tileParam.cols;
        const int32_t tileCol = tile % par.tileParam.cols;
        int32_t sbRowStart = par.tileParam.rowStart[tileRow];
        int32_t sbColStart = par.tileParam.colStart[tileCol];
        int32_t sbRowEnd = par.tileParam.rowEnd[tileRow];
        int32_t sbColEnd = par.tileParam.colEnd[tileCol];

        m_pak_tile_state.tile_widths[i] = sbColEnd - sbColStart;//m_ctrl.tileWidthsInSb[i];
        m_pak_tile_state.tile_col_start_sb[i] = start_sb;
        start_sb += m_pak_tile_state.tile_widths[i];//m_ctrl.tileWidthsInSb[i];
    }
    m_pak_tile_state.tile_col_start_sb[i] = start_sb;
    for (i = 0, start_sb = 0; i < par.NumTileRows; i++) {
        int32_t tile = i;

        const int32_t tileRow = tile / par.tileParam.cols;
        const int32_t tileCol = tile % par.tileParam.cols;
        int32_t sbRowStart = par.tileParam.rowStart[tileRow];
        int32_t sbColStart = par.tileParam.colStart[tileCol];
        int32_t sbRowEnd = par.tileParam.rowEnd[tileRow];
        int32_t sbColEnd = par.tileParam.colEnd[tileCol];

        m_pak_tile_state.tile_heights[i] = sbRowEnd - sbRowStart;//m_ctrl.tileHeightsInSb[i];
        m_pak_tile_state.tile_row_start_sb[i] = start_sb;
        start_sb += m_pak_tile_state.tile_heights[i];//m_ctrl.tileHeightsInSb[i];
    }
    m_pak_tile_state.tile_row_start_sb[i] = start_sb;

    const int32_t tileRow = tile / par.tileParam.cols;
    const int32_t tileCol = tile % par.tileParam.cols;

    int32_t sbRowStart = par.tileParam.rowStart[tileRow];
    int32_t sbColStart = par.tileParam.colStart[tileCol];
    int32_t sbRowEnd = par.tileParam.rowEnd[tileRow];
    int32_t sbColEnd = par.tileParam.colEnd[tileCol];

    int32_t lastTileCol = tileCol == par.tileParam.cols - 1;
    int32_t lastTileRow = tileRow == par.tileParam.rows - 1;
    int32_t lastTile = lastTileCol && lastTileRow;
    int32_t tileWidth = lastTileCol ? (par.Width - (sbColStart << 6)) : par.tileParam.colWidth[tileCol] * 64;//lastTileCol ? m_frames[AV1_ORIGINAL_FRAME].frameCodedSize.width - (tileStartSbX << 6) : (tileWidthInSb << 6);
    int32_t tileHeight = lastTileRow ? (par.Height - (sbRowStart << 6)) : par.tileParam.rowHeight[tileRow] * 64;//lastTileRow ? m_frames[AV1_ORIGINAL_FRAME].frameCodedSize.height - (tileStartSbY << 6) : (tileHeightInSb << 6);

    int32_t totalTilesInGroup = par.tileParam.cols * par.tileParam.rows;// aya:: fixme
    m_pak_tile_state.FrameTileID = tile;//tileId;
    m_pak_tile_state.TGTileNum = totalTilesInGroup - 1;
    m_pak_tile_state.TileGroupID = 0;//tileGroupId;

    m_pak_tile_state.TileColumnPositionInSBUnit = sbColStart;//tileStartSbX;
    m_pak_tile_state.TileRowPositionInSBUnit = sbRowStart;//tileStartSbY;

    m_pak_tile_state.TileWidthInSuperBlockUnitMinus1 = ((tileWidth + 63) >> 6) - 1;
    m_pak_tile_state.TileHeightInSuperBlockUnitMinus1 = ((tileHeight + 63) >> 6) - 1;

    m_pak_tile_state.TileRowIndependentFlag = 1;
    m_pak_tile_state.IsLastTileOfColumnFlag = lastTileCol ? 1 : 0;
    m_pak_tile_state.IsLastTileOfRowFlag = lastTileRow ? 1 : 0;
    m_pak_tile_state.IsStartTileOfTileGroupFlag = (totalTilesInGroup == 1) ? 1 : 0;
    m_pak_tile_state.IsEndTileOfTileGroupFlag = lastTile ? 1 : 0;//lastTileInGroup ? 1 : 0;
    m_pak_tile_state.IsLastTileOfFrameFlag = lastTile ? 1 : 0;
}


void SetPakInloopFilterState(const AV1VideoParam &par, const Frame &frame, CmodelAv1::Av1PakHWInloopFilterState &m_pak_inloop_filter_state)
{

    m_pak_inloop_filter_state.LumaYDeblockerFilterLevelVertical = frame.m_loopFilterParam.level;//m_ctrl.LoopFilterLevel[0];
    m_pak_inloop_filter_state.LumaYDeblockerFilterLevelHorizontal = frame.m_loopFilterParam.level;//m_ctrl.LoopFilterLevel[1];
    m_pak_inloop_filter_state.ChromaUDeblockerFilterLevel = frame.m_loopFilterParam.level;//m_ctrl.LoopFilterLevel[2];
    m_pak_inloop_filter_state.ChromaVDeblockerFilterLevel = frame.m_loopFilterParam.level;//m_ctrl.LoopFilterLevel[3];
    m_pak_inloop_filter_state.DeblockerFilterSharpnessLevel = frame.m_loopFilterParam.sharpness;//m_ctrl.SharpnessLevel;
    m_pak_inloop_filter_state.DeblockerFilterModeRefDeltaEnableFlag = 0; // PAK doesn't use this flag
    m_pak_inloop_filter_state.DeblockerDeltaLFResolution = 0;//m_ctrl.Log2DeltaLFRes;
    m_pak_inloop_filter_state.DeblockerFilterDeltaLFMultiFlag = 0;//m_ctrl.DeltaLFMulti;
    m_pak_inloop_filter_state.DeblockerFilterDeltaLFPresentFlag = 0;//m_ctrl.DeltaLFPresentFlag;

    m_pak_inloop_filter_state.DeblockerFilterRefDeltas0 = frame.m_loopFilterParam.refDeltas[0];//m_ctrl.refLFDeltas[0];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas1 = frame.m_loopFilterParam.refDeltas[1];//m_ctrl.refLFDeltas[1];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas2 = frame.m_loopFilterParam.refDeltas[2];//m_ctrl.refLFDeltas[2];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas3 = frame.m_loopFilterParam.refDeltas[3];//m_ctrl.refLFDeltas[3];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas4 = 0;//m_ctrl.refLFDeltas[4];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas5 = 0;//m_ctrl.refLFDeltas[5];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas6 = 0;//m_ctrl.refLFDeltas[6];
    m_pak_inloop_filter_state.DeblockerFilterRefDeltas7 = 0;//m_ctrl.refLFDeltas[7];

    m_pak_inloop_filter_state.DeblockerFilterModeDeltas0 = frame.m_loopFilterParam.modeDeltas[0];//m_ctrl.modeLFDeltas[0];
    m_pak_inloop_filter_state.DeblockerFilterModeDeltas1 = frame.m_loopFilterParam.modeDeltas[1];//m_ctrl.modeLFDeltas[1];

    m_pak_inloop_filter_state.SuperResUpscaledFrameWidthMinus1 = frame.av1frameOrigin.frameSize.width - 1;//m_frames[AV1_ORIGINAL_FRAME].frameSize.width - 1;
    m_pak_inloop_filter_state.SuperResDenom = 8;//m_ctrl.SuperResScaleDenominator;
    m_pak_inloop_filter_state.HorzSuperResEnableFlag = 0;//m_Params.EnableSuperResolution; // PAK doesn't use this flag
    m_pak_inloop_filter_state.superres_upscaled_height = frame.av1frameOrigin.frameSize.height;//m_frames[AV1_ORIGINAL_FRAME].frameSize.height;

    m_pak_inloop_filter_state.CDEFMode = par.cdefFlag ? 4 : 0;
    m_pak_inloop_filter_state.CDEFClpfStrength = 0;//m_Params.CDEFClpfStrength; //[0~3]
    m_pak_inloop_filter_state.CDEFDeringStrength = 0;//m_Params.CDEFDeringStrength; //[0~15]
    m_pak_inloop_filter_state.CDEFFilterSkip = 0;//m_Params.CDEFFilterSkip;   //[0 | 1]
    m_pak_inloop_filter_state.CDEFSimulateDblkShift = 0;//m_Params.CDEFSimulateDblkShift;
    m_pak_inloop_filter_state.CDEFReduceSearchBlockSize = 0;//m_Params.CDEFReduceSearchBlockSize;

    if (par.cdefFlag) {

        m_pak_inloop_filter_state.EncoderMode = 1;// NO RANDOM

        m_pak_inloop_filter_state.CDEFYStrength0 = frame.m_cdefParam.cdef_strengths[0];
        m_pak_inloop_filter_state.CDEFYStrength1 = frame.m_cdefParam.cdef_strengths[1];
        m_pak_inloop_filter_state.CDEFYStrength2 = frame.m_cdefParam.cdef_strengths[2];
        m_pak_inloop_filter_state.CDEFYStrength3 = frame.m_cdefParam.cdef_strengths[3];
        m_pak_inloop_filter_state.CDEFYStrength4 = frame.m_cdefParam.cdef_strengths[4];
        m_pak_inloop_filter_state.CDEFYStrength5 = frame.m_cdefParam.cdef_strengths[5];
        m_pak_inloop_filter_state.CDEFYStrength6 = frame.m_cdefParam.cdef_strengths[6];
        m_pak_inloop_filter_state.CDEFYStrength7 = frame.m_cdefParam.cdef_strengths[7];

        m_pak_inloop_filter_state.CDEFBits = frame.m_cdefParam.cdef_bits;

        // in future could be tuned as func(QP)

        m_pak_inloop_filter_state.CDEFUVStrength0 = frame.m_cdefParam.cdef_uv_strengths[0];
        m_pak_inloop_filter_state.CDEFUVStrength1 = frame.m_cdefParam.cdef_uv_strengths[1];
        m_pak_inloop_filter_state.CDEFUVStrength2 = frame.m_cdefParam.cdef_uv_strengths[2];
        m_pak_inloop_filter_state.CDEFUVStrength3 = frame.m_cdefParam.cdef_uv_strengths[3];
        m_pak_inloop_filter_state.CDEFUVStrength4 = frame.m_cdefParam.cdef_uv_strengths[4];
        m_pak_inloop_filter_state.CDEFUVStrength5 = frame.m_cdefParam.cdef_uv_strengths[5];
        m_pak_inloop_filter_state.CDEFUVStrength6 = frame.m_cdefParam.cdef_uv_strengths[6];
        m_pak_inloop_filter_state.CDEFUVStrength7 = frame.m_cdefParam.cdef_uv_strengths[7];
    }

    if (par.lrFlag) {
        m_pak_inloop_filter_state.LRMode = 2;
        m_pak_inloop_filter_state.LRReduceSearchBlockSize = 0;
    }

}

void SetPakSegmentState(CmodelAv1::Av1PakHWSegmentState  m_pak_segment_states[8])
{
    for (int seg = 0; seg < 8; seg++)
    {
        memset(&m_pak_segment_states[seg], 0, sizeof(CmodelAv1::Av1PakHWSegmentState));

        m_pak_segment_states[seg].SegmentID = seg;

        m_pak_segment_states[seg].SegmentLumaYQMLevel = 15;
        m_pak_segment_states[seg].SegmentChromaUQMLevel = 15;
        m_pak_segment_states[seg].SegmentChromaVQMLevel = 15;
    }
}

void copyModeInfo2Av1PakObj_InterMode(const ModeInfo* srcCuCode, int miPitch, CmodelAv1::HWCuCodeAv1* dstBlkCode, int miRow, int miRowMax, int miCol, int miColMax)
{
    int CUSize = srcCuCode->sbType >> 2;
    int TuDepthPart0 = CUSize - srcCuCode->txSize + 1;///srcCuCode->CUSize - srcCuCode->TUSize0 + 1;

    memset(dstBlkCode, 0, sizeof(CmodelAv1::HWCuCodeAv1));
    if (srcCuCode->mode == OUT_OF_PIC || srcCuCode->mode == 205)
        return;

    const int32_t isInter = (srcCuCode->refIdx[0] != INTRA_FRAME);
    assert(isInter);

    dstBlkCode->CuSize = srcCuCode->sbType >> 2;
    dstBlkCode->CuPartMode = 0;
    dstBlkCode->CuPredModePart0 = 1;

    if (TuDepthPart0 == 0) {
        dstBlkCode->TuSplitPart0 = 0;
    }
    else {
        int d0 = 1;

        int bsizeHalf = srcCuCode->sbType - 3;
        int CUSizeHalf = bsizeHalf >> 2;

        const int32_t miWidth = block_size_wide_8x8[bsizeHalf];
        const int32_t miHeight = block_size_high_8x8[bsizeHalf];

        // d1
        //const ModeInfo* miPart1 = srcCuCode + 0 * miPitch + 0;
        int TuDepthPart1 = CUSizeHalf - /*miPart1*/srcCuCode->txSize + 1;
        int d1 = (TuDepthPart1 == 0) ? 0 : 1;

        // d2
        int d2 = 0;
        if (miRow + miHeight < miRowMax) {
            const ModeInfo* miPart2 = srcCuCode + miHeight * miPitch;
            int TuDepthPart2 = CUSizeHalf - miPart2->txSize + 1;
            d2 = (TuDepthPart2 == 0) ? 0 : 1;
        }

        // d3
        int d3 = 0;
        if (miCol + miWidth < miColMax) {
            const ModeInfo* miPart3 = srcCuCode + miWidth;
            int TuDepthPart3 = CUSizeHalf - miPart3->txSize + 1;
            d3 = (TuDepthPart3 == 0) ? 0 : 1;
        }

        // d4
        int d4 = 0;
        if (miRow + miHeight < miRowMax && miCol + miWidth < miColMax) {
            const ModeInfo* miPart4 = srcCuCode + miWidth + miHeight * miPitch;
            int TuDepthPart4 = CUSizeHalf - miPart4->txSize + 1;
            d4 = (TuDepthPart4 == 0) ? 0 : 1;
        }

        //dstBlkCode->TuSplitPart0 = (d4 << 4) + (d3 << 3) + (d2 << 2) + (d1 << 1) + d0;
        dstBlkCode->TuSplitPart0 = (d4 << 4) + (d2 << 3) + (d3 << 2) + (d1 << 1) + d0;
    }

    // which type right now - DCT_DCT due to reduced_tx. looks like restriction of the model
    dstBlkCode->TxTypePart0 = DCT_DCT;//srcCuCode->txType;
    dstBlkCode->MCFilterTypePart0 = srcCuCode->interp0;//first interp for now

    // avg from c-model (INTER)
    dstBlkCode->Round0 = 5;//srcCuCode->QuantRound0 * 2 + 1; //in VP9 roundig is 3 bits
    dstBlkCode->InterCompModePart0 = srcCuCode->refIdx[1] >= 0 ? 1 : 0;//dstBlkCode->L1RefFramePart0 == 0 ? 0 : 1;
    dstBlkCode->CompoundIdxPart0 = 1;
    {
        dstBlkCode->L0BlkMvPart0.MV.mvx = srcCuCode->mv[0].mvx;//srcCuCode->L0_MV0x;
        dstBlkCode->L0BlkMvPart0.MV.mvy = srcCuCode->mv[0].mvy;//srcCuCode->L0_MV0y;
        if (srcCuCode->refIdx[1] >= 0) {
            dstBlkCode->L1BlkMvPart0.MV.mvx = srcCuCode->mv[1].mvx;//srcCuCode->L1_MV0x;
            dstBlkCode->L1BlkMvPart0.MV.mvy = srcCuCode->mv[1].mvy;//srcCuCode->L1_MV0y;
        }

        // [ 0 - LAST, 1 - GOLD, 2 - ALTERF]
        static int mappingToAv1[] = { 1, 4, 7 };
        dstBlkCode->L0RefFramePart0 = mappingToAv1[srcCuCode->refIdx[0]];
        if (srcCuCode->refIdx[1] >= 0) {
            dstBlkCode->L1RefFramePart0 = mappingToAv1[srcCuCode->refIdx[1]];
        }
    }
}

void copyModeInfo2Av1PakObj(const ModeInfo* srcCuCode, int miPitch, CmodelAv1::HWCuCodeAv1* dstBlkCode, int miRow, int miRowMax, int miCol, int miColMax)
{
    int32_t AngleDeltaMap[3] = { 6, 0, 2 }; //VDENC can only output three cases, 0 for -2, 1 for 0, and 2 for 2
    int numTUsinBlock = 0;
    int CUSize = srcCuCode->sbType >> 2;
    int TuDepthPart0 = CUSize - srcCuCode->txSize + 1;///srcCuCode->CUSize - srcCuCode->TUSize0 + 1;
    int TuDepthPart1 = 0;

    memset(dstBlkCode, 0, sizeof(CmodelAv1::HWCuCodeAv1));

    if (srcCuCode->mode == OUT_OF_PIC || srcCuCode->mode == 205)
        return;

    const int32_t isInter = (srcCuCode->refIdx[0] != INTRA_FRAME);
    if (isInter) {
        copyModeInfo2Av1PakObj_InterMode(srcCuCode, miPitch, dstBlkCode, miRow, miRowMax, miCol, miColMax);
        return;
    }
    dstBlkCode->CuSize = srcCuCode->sbType >> 2; //srcCuCode->CUSize;
    dstBlkCode->CuPartMode = 0;
    // aya:: intra - 0, inter - 1
    //const int32_t isInter = (srcCuCode->refIdx[0] != INTRA_FRAME);
    dstBlkCode->CuPredModePart0 = srcCuCode->mode >= AV1_INTRA_MODES ? 1 : 0;//srcCuCode->CUPredMode0;
    assert(dstBlkCode->CuPredModePart0 == 0);

    dstBlkCode->TuSplitPart0 = ((TuDepthPart0 == 0) ? 0 : (TuDepthPart0 == 1 ? 1 : 31));


    dstBlkCode->TxTypePart0 = srcCuCode->txType;

    // from c-model (INTRA)
    dstBlkCode->Round0 = 11;//srcCuCode->QuantRound0 * 2 + 1; //in VP9 roundig is 3 bits

    dstBlkCode->SegmentIdxPart0 = 0;
    dstBlkCode->SegmentPredFlagPart0 = 0;

    if (dstBlkCode->CuPredModePart0 == 0) //intra
    {
        dstBlkCode->IntraLumaModePart0 = srcCuCode->mode;
        dstBlkCode->IntraChromaModePart0 = srcCuCode->modeUV;

        const int32_t delta_y = srcCuCode->angle_delta_y - AV1_MAX_ANGLE_DELTA;
        dstBlkCode->IntraAngleDeltaPart0 = (Ipp32u)(delta_y & 0x7);

        const int32_t delta_uv = srcCuCode->angle_delta_uv - MAX_ANGLE_DELTA;
        dstBlkCode->IntraChromaAngleDeltaPart0 = (Ipp32u)(delta_uv & 0x7);
    }
}

static Ipp8u ZIGZAG_SCAN[64] = {
    0,  1,  8,  9,  2,  3, 10, 11,
    16, 17, 24, 25, 18, 19, 26, 27,
    4,  5, 12, 13,  6,  7, 14, 15,
    20, 21, 28, 29, 22, 23, 30, 31,
    32, 33, 40, 41, 34, 35, 42, 43,
    48, 49, 56, 57, 50, 51, 58, 59,
    36, 37, 44, 45, 38, 39, 46, 47,
    52, 53, 60, 61, 54, 55, 62, 63
};

void copyModeInfo2Av1PakObj_Sb64(AV1VideoParam& par, const ModeInfo* mi_data, int32_t miPitch, CmodelAv1::HWCuCodeAv1* pakObj, Ipp16u *txkTypes, int round0, int sbIndex, CoeffsType* frame_coefs, int chromaShift)
{
    for (int32_t blkIdx = 0; blkIdx < 64; blkIdx++) {
        CmodelAv1::HWCuCodeAv1 *dst = &pakObj[blkIdx];
        memset(dst, 0, sizeof(CmodelAv1::HWCuCodeAv1));
    }

    int32_t i = 0;
    int32_t idx_pakobj = 0;
    for (int32_t blkIdx = 0; blkIdx < 64; blkIdx++)
    {
        const int32_t rasterIdx = av1_scan_z2r4[i];
        const int32_t x4 = (rasterIdx & 15);
        const int32_t y4 = (rasterIdx >> 4);
        const int32_t sbx = x4 << 2;
        const int32_t sby = y4 << 2;
        const ModeInfo *mi = mi_data + (sbx >> 3) + (sby >> 3) * miPitch;
        if (mi->mode == OUT_OF_PIC) {
            // update
            const int32_t num4x4 = num_4x4_blocks_lookup[mi->sbType];
            i += num4x4;
            continue;
        }
        const ModeInfo* src = mi;
        CmodelAv1::HWCuCodeAv1 *dst = &pakObj[idx_pakobj];

#if 1
        // set valid TxType
        const int32_t isInter = (src->refIdx[0] != INTRA_FRAME);
        if (!isInter)
        {
            int32_t blkRow = sby >> 2;
            int32_t blkCol = sbx >> 2;
            int32_t index = (blkRow & 15) * 16 + (blkCol & 15);
            (const_cast<ModeInfo *>(mi))->txType = txkTypes[index];
        }
#endif

        int sbRow = sbIndex / par.sb64Cols;
        int sbCol = sbIndex % par.sb64Cols;
        int ctbPelY = 64 * sbRow;
        int ctbPelX = 64 * sbCol;

        const int32_t miRow = (ctbPelY >> 3) + (y4 >> 1);
        const int32_t miCol = (ctbPelX >> 3) + (x4 >> 1);

        const int32_t miRowMax = par.miRows;
        const int32_t miColMax = par.miCols;

        copyModeInfo2Av1PakObj(src, miPitch, dst, miRow, miRowMax, miCol, miColMax);

        if (isInter && src->skip) {
            dst->BlockZeroCoeffsYPart0 = 1;
            dst->BlockZeroCoeffsUPart0 = 1;
            dst->BlockZeroCoeffsVPart0 = 1;
        }

        // patch
        dst->Round0 = round0;

        dst->lambda[0] = 5464;
        dst->lambda[1] = 4956;

        // updatere
        const int32_t num4x4 = num_4x4_blocks_lookup[mi->sbType];
        i += num4x4;

        idx_pakobj++;
    }
}


void /*H265FrameEncoder::*/SetPakSurfaceState(Frame *m_frame, CmodelAv1::Av1PakHWSurfaceState& m_pak_surface_state)
{
    m_pak_surface_state.CurrentSurfState.pYPlane = m_frame->av1frameOrigin.data_y16b;
    m_pak_surface_state.CurrentSurfState.pUPlane = m_frame->av1frameOrigin.data_u16b;
    m_pak_surface_state.CurrentSurfState.pVPlane = m_frame->av1frameOrigin.data_v16b;
    m_pak_surface_state.CurrentSurfState.pitchPixels = m_frame->av1frameOrigin.pitch;

    for (int i = AV1Enc::AV1_LAST_FRAME; i <= AV1Enc::AV1_ALTREF_FRAME; i++)
    {
        int32_t refIdx = i;//m_frame->refFrameIdx[i];
        if (refIdx >= 0 && m_frame->refFramesAv1[refIdx])
        {
            m_pak_surface_state.RefSurfState[i/* - AV1_LAST_FRAME*/].pYPlane = m_frame->refFramesAv1[refIdx]->av1frameRecon.data_y16b;
            m_pak_surface_state.RefSurfState[i/* - AV1_LAST_FRAME*/].pUPlane = m_frame->refFramesAv1[refIdx]->av1frameRecon.data_u16b;
            m_pak_surface_state.RefSurfState[i/* - AV1_LAST_FRAME*/].pVPlane = m_frame->refFramesAv1[refIdx]->av1frameRecon.data_v16b;
            m_pak_surface_state.RefSurfState[i/* - AV1_LAST_FRAME*/].pitchPixels = m_frame->refFramesAv1[refIdx]->av1frameRecon.pitch;
        }
    }

    m_pak_surface_state.ReconSurfState.pYPlane = m_frame->av1frameRecon.data_y16b;
    m_pak_surface_state.ReconSurfState.pUPlane = m_frame->av1frameRecon.data_u16b;
    m_pak_surface_state.ReconSurfState.pVPlane = m_frame->av1frameRecon.data_v16b;
    m_pak_surface_state.ReconSurfState.pitchPixels = m_frame->av1frameRecon.pitch;

    m_pak_surface_state.PredSegMapSurface.segIdBuf = nullptr;

    m_pak_surface_state.CurSegMapSurface.segIdBuf = m_frame->av1frameOrigin.m_segIdBuf;
    m_pak_surface_state.CurSegMapSurface.segIdBufStride = m_frame->av1frameOrigin.m_segStride;

    m_pak_surface_state.TplMvsSurface;//= m_tpl_mvs;
    m_pak_surface_state.MvRefSurface.mvRefInfo = m_frame->av1frameOrigin.m_mvs;
    m_pak_surface_state.MvRefSurface.mvRefInfoStride = m_frame->av1frameOrigin.m_mvStride;
    m_pak_surface_state.EncoderStreamout;// = m_encoder_stream_out;
}

CmodelAv1::Status InitProbabilities(CmodelAv1::Av1ProbabilityBuffer *probs, Ipp8u initMode)
{
    using namespace CmodelAv1;
    CmodelAv1::Status st = CmodelAv1::UMC_OK;
    //reset the probabilities
    if (initMode == 0)
    {
        memset(probs, 0, sizeof(CmodelAv1::Av1ProbabilityBuffer));
    }

    //Init with default coeff probability
    if (initMode < 2)
    {
#if NEW_AV1_ROOQ
        memcpy(&probs->coef_probs_4x4[0][0], &av1_default_coef_probs_4x4[0][0], sizeof(av1_default_coef_probs_4x4));
        memcpy(&probs->coef_probs_8x8[0][0], &av1_default_coef_probs_8x8[0][0], sizeof(av1_default_coef_probs_8x8));
        memcpy(&probs->coef_probs_16x16[0][0], &av1_default_coef_probs_16x16[0][0], sizeof(av1_default_coef_probs_16x16));
        memcpy(&probs->coef_probs_32x32[0][0], &av1_default_coef_probs_32x32[0][0], sizeof(av1_default_coef_probs_32x32));
#endif
    }

    //init with default mv probs
    for (int i = 0; i < 3; i++) {
        memcpy(&probs->delta_mv_probs[i].joints_cdf[0], &av1_default_joints_cdf[0], sizeof(av1_default_joints_cdf));
        memcpy(&probs->delta_mv_probs[i].mv_int_probs[0], &av1_default_mv_int_probs[0], sizeof(av1_default_mv_int_probs));
        memcpy(&probs->delta_mv_probs[i].mv_fp_probs[0], &av1_default_mv_fp_probs[0], sizeof(av1_default_mv_fp_probs));
        memcpy(&probs->delta_mv_probs[i].mv_hp_probs[0], &av1_default_mv_hp_probs[0], sizeof(av1_default_mv_hp_probs));
    }

    memcpy(&probs->m_cdf.inter_compound_mode_cdf, av1_default_inter_compound_mode_cdf, sizeof(av1_default_inter_compound_mode_cdf));
    memcpy(&probs->m_cdf.y_mode_cdf, av1_default_if_y_mode_cdf, sizeof(av1_default_if_y_mode_cdf));
    memcpy(&probs->m_cdf.uv_mode_cdf, av1_default_uv_mode_cdf, sizeof(av1_default_uv_mode_cdf));
    memcpy(&probs->m_cdf.switchable_interp_cdf, av1_default_switchable_interp_cdf, sizeof(av1_default_switchable_interp_cdf));
    memcpy(&probs->m_cdf.partition_cdf, av1_default_partition_cdf, sizeof(av1_default_partition_cdf));
    memcpy(&probs->m_cdf.intra_ext_tx_cdf, av1_default_intra_ext_tx_cdf, sizeof(av1_default_intra_ext_tx_cdf));
    memcpy(&probs->m_cdf.inter_ext_tx_cdf, av1_default_inter_ext_tx_cdf, sizeof(av1_default_inter_ext_tx_cdf));
    memcpy(&probs->m_cdf.pred_cdf, av1_default_segment_pred_cdf, sizeof(av1_default_segment_pred_cdf));
    memcpy(&probs->m_cdf.spatial_pred_seg_cdf, av1_default_spatial_pred_seg_tree_cdf, sizeof(av1_default_spatial_pred_seg_tree_cdf));
    memcpy(&probs->m_cdf.tx_size_cdf, av1_default_tx_size_cdf, sizeof(av1_default_tx_size_cdf));
    memcpy(&probs->m_cdf.newmv_cdf, av1_default_newmv_cdf, sizeof(av1_default_newmv_cdf));
    memcpy(&probs->m_cdf.zeromv_cdf, av1_default_zeromv_cdf, sizeof(av1_default_zeromv_cdf));
    memcpy(&probs->m_cdf.refmv_cdf, av1_default_refmv_cdf, sizeof(av1_default_refmv_cdf));
    memcpy(&probs->m_cdf.drl_cdf, av1_default_drl_cdf, sizeof(av1_default_drl_cdf));
    memcpy(&probs->m_cdf.delta_q_cdf, av1_default_delta_q_cdf, sizeof(av1_default_delta_q_cdf));
    memcpy(&probs->m_cdf.delta_lf_multi_cdf, av1_default_delta_lf_multi_cdf, sizeof(av1_default_delta_lf_multi_cdf));
    memcpy(&probs->m_cdf.delta_lf_cdf, av1_default_delta_lf_cdf, sizeof(av1_default_delta_lf_cdf));
    memcpy(&probs->m_cdf.intra_inter_cdf, av1_default_intra_inter_cdf, sizeof(av1_default_intra_inter_cdf));
    memcpy(&probs->m_cdf.comp_inter_cdf, av1_default_comp_inter_cdf, sizeof(av1_default_comp_inter_cdf));
    memcpy(&probs->m_cdf.comp_ref_type_cdf, av1_default_comp_ref_type_cdf, sizeof(av1_default_comp_ref_type_cdf));
    memcpy(&probs->m_cdf.uni_comp_ref_cdf, av1_default_uni_comp_ref_cdf, sizeof(av1_default_uni_comp_ref_cdf));
    memcpy(&probs->m_cdf.comp_bwdref_cdf, av1_default_comp_bwdref_cdf, sizeof(av1_default_comp_bwdref_cdf));
    memcpy(&probs->m_cdf.comp_ref_cdf, av1_default_comp_ref_cdf, sizeof(av1_default_comp_ref_cdf));
    memcpy(&probs->m_cdf.single_ref_cdf, av1_default_single_ref_cdf, sizeof(av1_default_single_ref_cdf));
    memcpy(&probs->m_cdf.txfm_partition_cdf, av1_default_txfm_partition_cdf, sizeof(av1_default_txfm_partition_cdf));
    memcpy(&probs->m_cdf.obmc_cdf, av1_default_obmc_cdf, sizeof(av1_default_obmc_cdf));
    memcpy(&probs->m_cdf.motion_mode_cdf, av1_default_motion_mode_cdf, sizeof(av1_default_motion_mode_cdf));
    memcpy(&probs->m_cdf.switchable_restore_cdf, av1_default_switchable_restore_cdf, sizeof(av1_default_switchable_restore_cdf));
    memcpy(&probs->m_cdf.wiener_restore_cdf, av1_default_wiener_restore_cdf, sizeof(av1_default_wiener_restore_cdf));
    memcpy(&probs->m_cdf.sgrproj_restore_cdf, av1_default_sgrproj_restore_cdf, sizeof(av1_default_sgrproj_restore_cdf));
    memcpy(&probs->m_cdf.skip_mode_cdfs, av1_default_skip_mode_cdfs, sizeof(av1_default_skip_mode_cdfs));
    memcpy(&probs->m_cdf.skip_cdfs, av1_default_skip_cdfs, sizeof(av1_default_skip_cdfs));
    memcpy(&probs->m_cdf.compound_idx_cdfs, av1_default_compound_idx_cdfs, sizeof(av1_default_compound_idx_cdfs));
    memcpy(&probs->m_cdf.comp_group_idx_cdfs, av1_default_comp_group_idx_cdfs, sizeof(av1_default_comp_group_idx_cdfs));
    memcpy(&probs->m_cdf.filter_intra_mode_cdf, av1_default_filter_intra_mode_cdf, sizeof(av1_default_filter_intra_mode_cdf));
    memcpy(&probs->m_cdf.filter_intra_cdfs, av1_default_filter_intra_cdfs, sizeof(av1_default_filter_intra_cdfs));
    memcpy(&probs->m_cdf.kf_y_cdf, av1_default_kf_y_mode_cdf, sizeof(av1_default_kf_y_mode_cdf));
    memcpy(&probs->m_cdf.angle_delta_cdf, av1_default_angle_delta_cdf, sizeof(av1_default_angle_delta_cdf));
    memcpy(&probs->m_cdf.cfl_sign_cdf, av1_default_cfl_sign_cdf, sizeof(av1_default_cfl_sign_cdf));
    memcpy(&probs->m_cdf.cfl_alpha_cdf, av1_default_cfl_alpha_cdf, sizeof(av1_default_cfl_alpha_cdf));

    return st;
}

CmodelAv1::Status /*Av1VideoEncoderWebM::*/InitCoeffsCdef(CmodelAv1::Av1ProbabilityBuffer *probs, int32_t base_qindex)
{
    using namespace CmodelAv1;
    Status st = UMC_OK;

    int index = get_q_ctx(base_qindex);
    memcpy(probs->m_cdf.txb_skip_cdf, av1_default_txb_skip_cdfs[index], sizeof(av1_default_txb_skip_cdfs[index]));
    memcpy(probs->m_cdf.eob_extra_cdf, av1_default_eob_extra_cdfs[index], sizeof(av1_default_eob_extra_cdfs[index]));
    memcpy(probs->m_cdf.dc_sign_cdf, av1_default_dc_sign_cdfs[index], sizeof(av1_default_dc_sign_cdfs[index]));
    memcpy(probs->m_cdf.coeff_br_cdf, av1_default_coeff_lps_multi_cdfs[index], sizeof(av1_default_coeff_lps_multi_cdfs[index]));
    memcpy(probs->m_cdf.coeff_base_cdf, av1_default_coeff_base_multi_cdfs[index], sizeof(av1_default_coeff_base_multi_cdfs[index]));
    memcpy(probs->m_cdf.coeff_base_eob_cdf, av1_default_coeff_base_eob_multi_cdfs[index], sizeof(av1_default_coeff_base_eob_multi_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf16, av1_default_eob_multi16_cdfs[index], sizeof(av1_default_eob_multi16_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf32, av1_default_eob_multi32_cdfs[index], sizeof(av1_default_eob_multi32_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf64, av1_default_eob_multi64_cdfs[index], sizeof(av1_default_eob_multi64_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf128, av1_default_eob_multi128_cdfs[index], sizeof(av1_default_eob_multi128_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf256, av1_default_eob_multi256_cdfs[index], sizeof(av1_default_eob_multi256_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf512, av1_default_eob_multi512_cdfs[index], sizeof(av1_default_eob_multi512_cdfs[index]));
    memcpy(probs->m_cdf.eob_flag_cdf1024, av1_default_eob_multi1024_cdfs[index], sizeof(av1_default_eob_multi1024_cdfs[index]));

    return st;
}

void convert8bTo16b(Frame* frame)
{
    for (int32_t y = 0; y < frame->m_par->Height; y++) {
        for (int32_t x = 0; x < frame->m_par->Width; x++) {
            frame->av1frameOrigin.data_y16b[y * frame->av1frameOrigin.pitch + x] = frame->m_origin->y[y * frame->m_origin->pitch_luma_pix + x];
        }
    }

    for (int32_t y = 0; y < frame->m_par->Height / 2; y++) {
        for (int32_t x = 0; x < frame->m_par->Width / 2; x++) {
            frame->av1frameOrigin.data_u16b[y * frame->av1frameOrigin.pitch + x] = frame->m_origin->uv[y * frame->m_origin->pitch_chroma_pix + 2 * x];
            frame->av1frameOrigin.data_v16b[y * frame->av1frameOrigin.pitch + x] = frame->m_origin->uv[y * frame->m_origin->pitch_chroma_pix + 2 * x + 1];
        }
    }
}

#define CHECK_10BIT 1

void convert16bTo16b(Frame* frame)
{
    uint16_t* src10Y = (uint16_t*)(frame->m_origin10->y);

    for (int32_t y = 0; y < frame->m_par->Height; y++) {
        for (int32_t x = 0; x < frame->m_par->Width; x++) {
            frame->av1frameOrigin.data_y16b[y * frame->av1frameOrigin.pitch + x] = src10Y[y * frame->m_origin10->pitch_luma_pix + x];
#if CHECK_10BIT
            frame->av1frameOrigin.data_y16b[y * frame->av1frameOrigin.pitch + x] &= 0x3ff;
#endif
        }
    }

    uint16_t* src10Uv = (uint16_t*)(frame->m_origin10->uv);
    for (int32_t y = 0; y < frame->m_par->Height / 2; y++) {
        for (int32_t x = 0; x < frame->m_par->Width / 2; x++) {
            frame->av1frameOrigin.data_u16b[y * frame->av1frameOrigin.pitch + x] = src10Uv[y * frame->m_origin10->pitch_chroma_pix + 2 * x];
            frame->av1frameOrigin.data_v16b[y * frame->av1frameOrigin.pitch + x] = src10Uv[y * frame->m_origin10->pitch_chroma_pix + 2 * x + 1];
#if CHECK_10BIT
            frame->av1frameOrigin.data_u16b[y * frame->av1frameOrigin.pitch + x] &= 0x03ff;
            frame->av1frameOrigin.data_v16b[y * frame->av1frameOrigin.pitch + x] &= 0x03ff;
#endif
        }
    }
}

void convert16bTo8b(Frame* frame, int shift)
{
    for (int32_t y = 0; y < frame->m_par->Height; y++) {
        for (int32_t x = 0; x < frame->m_par->Width; x++) {
            frame->m_recon->y[y * frame->m_recon->pitch_luma_pix + x] = (frame->av1frameRecon.data_y16b[y * frame->av1frameRecon.pitch + x] + shift) >> shift;
        }
    }

    for (int32_t y = 0; y < frame->m_par->Height / 2; y++) {
        for (int32_t x = 0; x < frame->m_par->Width / 2; x++) {
            frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x] = (frame->av1frameRecon.data_u16b[y * frame->av1frameRecon.pitch + x] + shift) >> shift;
            frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x + 1] = (frame->av1frameRecon.data_v16b[y * frame->av1frameRecon.pitch + x] + shift) >> shift;
        }
    }
}

void Init_Coef_costs_more(CmodelAv1::Av1Prob *coeff_probs_l, unsigned int *coeff_zero, unsigned int *coeff_nonzero, int32_t frameType)
{
    using namespace CmodelAv1;
    coeff_nonzero[3] = av1_prob_cost_for_pak[coeff_probs_l[0]];
    coeff_nonzero[0] = av1_prob_cost_for_pak[256 - coeff_probs_l[0]] + av1_prob_cost_for_pak[coeff_probs_l[1]];
    coeff_nonzero[1] = av1_prob_cost_for_pak[256 - coeff_probs_l[0]] + av1_prob_cost_for_pak[256 - coeff_probs_l[1]] + av1_prob_cost_for_pak[coeff_probs_l[2]];
    coeff_nonzero[2] = av1_prob_cost_for_pak[256 - coeff_probs_l[2]] + coeff_nonzero[1];

    coeff_zero[3] = av1_prob_cost_for_pak[coeff_probs_l[0]];
    coeff_zero[0] = av1_prob_cost_for_pak[coeff_probs_l[1]];
    coeff_zero[1] = av1_prob_cost_for_pak[256 - coeff_probs_l[1]] + av1_prob_cost_for_pak[coeff_probs_l[2]];
    coeff_zero[2] = av1_prob_cost_for_pak[256 - coeff_probs_l[2]] + coeff_zero[1];

    coeff_nonzero[3] += (coeff_nonzero[3] >> 1);
    coeff_nonzero[0] += (coeff_nonzero[0] >> 1);
    coeff_nonzero[1] += (coeff_nonzero[1] >> 1);
    coeff_nonzero[2] += (coeff_nonzero[2] >> 1); //test this combination next

    coeff_zero[3] += (coeff_zero[3] >> 1);
    coeff_zero[0] += (coeff_zero[0] >> 1);
    coeff_zero[1] += (coeff_zero[1] >> 1);
    coeff_zero[2] += (coeff_zero[2] >> 1); //test this combination next

    coeff_zero[3] = coeff_nonzero[3]; //(coeff_zero[3] >> 1);
    coeff_zero[0] = coeff_nonzero[0]; //(coeff_zero[0] >> 1);
    coeff_zero[1] = coeff_nonzero[1];
    coeff_zero[2] = coeff_nonzero[2];

#if Max_Rounding_Test
    if (!frameType)
    {
        coeff_nonzero[1] = (int)(coeff_nonzero[1] * 2.0);//1.6// 2.5;
        coeff_nonzero[2] = (int)(coeff_nonzero[2] * 2.0);//1.6// 2.5;
    }
#endif
}



void AV1FrameEncoder::PackTile_viaCmodel(int32_t tile)
{
    int numTiles = m_videoParam.tileParam.cols * m_videoParam.tileParam.rows;
    int numFinishedTilesUpdated = vm_interlocked_inc32(&(m_frame->m_numFinishedTiles));
    if (numFinishedTilesUpdated < numTiles) {
        return;
    }

    CmodelAv1::g_debugTraceCabacFrameNumber = 1;

    CRandNum              *m_rand = nullptr;

    CmodelAv1::Av1ProbabilityBuffer m_idealizedProbs;
    CmodelAv1::Av1PakHWSurfaceState  m_pak_surface_state;
    CmodelAv1::Av1PakHWPicState      m_pak_pic_state;
    CmodelAv1::Av1PakHWTileState     m_pak_tile_state;
    CmodelAv1::Av1PakHWInloopFilterState m_pak_inloop_filter_state;
    CmodelAv1::Av1PakHWSegmentState  m_pak_segment_states[8];

    CmodelAv1::Status status = 0;
    CmodelAv1::Av1Pak* m_Pak = new CmodelAv1::Av1Pak(status);

    if (m_videoParam.src10Enable) {
        convert16bTo16b(m_frame);//<=== input as src10
    } else {
        convert8bTo16b(m_frame);//<=== input as src8
    }

    int passCount = m_videoParam.cdefFlag ? 2 : 1;

    for (int pass = 0; pass < passCount; pass++) {
        SetPakSurfaceState(m_frame, m_pak_surface_state);
        m_Pak->LoadSurfaceState(&m_pak_surface_state);

        SetPakFrameState(m_videoParam, *m_frame, m_pak_pic_state);
        m_Pak->LoadFrameState(&m_pak_pic_state);

        SetPakInloopFilterState(m_videoParam, *m_frame, m_pak_inloop_filter_state);
        m_Pak->LoadInloopFilterState(&m_pak_inloop_filter_state);

        SetPakTileState(m_videoParam, m_pak_tile_state, /*tile*/0);
        m_Pak->LoadTileState(&m_pak_tile_state);

        SetPakSegmentState(m_pak_segment_states);
        for (int i = 0; i<8; i++) m_Pak->LoadSegmentState(&m_pak_segment_states[i]);

        // set post-processing
        {
            m_Pak->InitDeblock();

            CmodelAv1::CDEF_OP cdef_op = CmodelAv1::CDEF_OP_NONE;
            if (m_videoParam.cdefFlag) {
                cdef_op = (pass == 0) ? CmodelAv1::CDEF_OP_SEARCH : CmodelAv1::CDEF_OP_FILTER;
            }
            m_Pak->InitCdef(m_rand, cdef_op);

            CmodelAv1::LR_OP lr_op = CmodelAv1::LR_OP_NONE;
            if (m_videoParam.lrFlag) {
                lr_op = (pass == 0) ? CmodelAv1::LR_OP_SEARCH : CmodelAv1::LR_OP_FILTER;
            }
            m_Pak->InitLr(m_rand, lr_op);
        }

        m_Pak->CreateCompressedFrameStart(nullptr, nullptr);

        for (int32_t tile = 0; tile < numTiles; tile++) {

            InitProbabilities(&m_idealizedProbs, 0);
            InitCoeffsCdef(&m_idealizedProbs, m_frame->m_sliceQpY);
            m_Pak->LoadFrameProbs(&m_idealizedProbs);

            {
                m_Pak->reset_loop_restoration();
            }

            SetPakTileState(m_videoParam, m_pak_tile_state, tile);
            m_Pak->LoadTileState(&m_pak_tile_state);

            // [bs preparation]
            CompressedBuf &compTile = m_compressedTiles[tile];
            compTile.size = 0;

            CmodelAv1::Av1PakHWOutputInfo    m_frameOut;
            m_frameOut.buffer = compTile.buf;
            m_frameOut.bytes_written = 0;
            m_frameOut.buffer_size = compTile.size;

            m_Pak->SetBitstream(&m_frameOut);

            // [packing]
            const int32_t tileRow = tile / m_videoParam.tileParam.cols;
            const int32_t tileCol = tile % m_videoParam.tileParam.cols;
            const int32_t sbRowStart = m_videoParam.tileParam.rowStart[tileRow];
            const int32_t sbColStart = m_videoParam.tileParam.colStart[tileCol];
            const int32_t sbRowEnd = m_videoParam.tileParam.rowEnd[tileRow];
            const int32_t sbColEnd = m_videoParam.tileParam.colEnd[tileCol];

            for (int32_t r = sbRowStart, sb = 0; r < sbRowEnd; r++) {
                for (int32_t c = sbColStart; c < sbColEnd; c++, sb++) {
                    int32_t sby = r;
                    int32_t sbx = c;
                    ModeInfo *miSb = m_frame->m_modeInfo + (c << 3) + (r << 3) * m_videoParam.miPitch;
                    CmodelAv1::HWSbCodeAv1 *curSB = &m_frame->av1frameOrigin.m_FrameLSBData.LSBData[sby * /*picWidthInSb*/m_videoParam.PicWidthInCtbs + sbx];

                    int32_t ctbAddr = sby * m_videoParam.PicWidthInCtbs + sbx;
                    Ipp16u *txkTypes = m_frame->m_txkTypes4x4 + (ctbAddr * 256);

                    //int32_t round0 = (m_videoParam.qparamY[m_frame->m_sliceQpY].round[0] + m_videoParam.qparamY[m_frame->m_sliceQpY].round[1]) >> 1;
                    //int32_t round0 = m_videoParam.qparamY[m_frame->m_sliceQpY].round[1];
                    int32_t round0 = m_frame->m_lcuRound[ctbAddr]; //9;//round0 = round0 * 32 /
                    copyModeInfo2Av1PakObj_Sb64(m_videoParam, miSb, m_videoParam.miPitch, curSB->BlkCode, txkTypes, round0, ctbAddr, m_coeffWork, m_videoParam.chromaShift);

                    int32_t tileStartSbX = sbColStart;
                    int32_t tileWidthInSb = sbColEnd - sbColStart;
                    int32_t tileStartSbY = sbRowStart;
                    int32_t tileHeightInSb = sbRowEnd - sbRowStart;
                    m_Pak->UpdateBoundaryInfo(sbx, sby, tileStartSbX, tileStartSbX + tileWidthInSb, tileStartSbY, tileStartSbY + tileHeightInSb);
                    m_Pak->CreateCompressedFrameOneLsb(m_frame->av1frameOrigin.m_FrameLSBData.LSBData/*&LSBData*/, &m_frameOut, sbx, sby);
                }
            }
            // write outbuf
            compTile.size = m_frameOut.bytes_written;
        }
        m_Pak->CreateCompressedFrameEnd(m_frame->av1frameOrigin.m_FrameLSBData.LSBData, nullptr/*out_hFrameStats*/); // <=== deblocking, cdef, LR here
    }

    int shift = m_videoParam.src10Enable ? 2 : 0;
    convert16bTo8b(m_frame, shift);
    PadRectLumaAndChroma(*m_frame->m_recon, m_videoParam.fourcc, 0, 0, m_videoParam.Width, m_videoParam.Height);


    delete m_Pak;
}

#endif // CMODEL

#endif //(MFX_ENABLE_AV1_VIDEO_ENCODE)