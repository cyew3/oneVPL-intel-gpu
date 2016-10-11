//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2016 Intel Corporation. All Rights Reserved.
//
//#define USE_TEMPTEMP

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

//#define SVC_DUMP_RECON

// #include "talx.h"

#include "umc_defs.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_to_ipp.h"
#include "umc_h264_bme.h"
#include "umc_h264_resample.h"
#include "mfxdefs.h"
#include "mfx_h264_enc_common.h"
#include "mfx_h264_encode.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "umc_h264_brc.h"
#include "umc_svc_brc.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "mfx_ext_buffers.h"
#include <new>

#ifdef VM_SLICE_THREADING_H264
#include "vm_event.h"
#include "vm_sys_info.h"
#endif // VM_SLICE_THREADING_H264

////extern int rd_frame_num;

#define CHECK_VERSION(ver)
#define CHECK_CODEC_ID(id, myid)
#define CHECK_FUNCTION_ID(id, myid)

//Defines from UMC encoder
#define MV_SEARCH_TYPE_FULL             0
#define MV_SEARCH_TYPE_CLASSIC_LOG      1
#define MV_SEARCH_TYPE_LOG              2
#define MV_SEARCH_TYPE_EPZS             3
#define MV_SEARCH_TYPE_FULL_ORTHOGONAL  4
#define MV_SEARCH_TYPE_LOG_ORTHOGONAL   5
#define MV_SEARCH_TYPE_TTS              6
#define MV_SEARCH_TYPE_NEW_EPZS         7
#define MV_SEARCH_TYPE_UMH              8
#define MV_SEARCH_TYPE_SQUARE           9
#define MV_SEARCH_TYPE_FTS             10
#define MV_SEARCH_TYPE_SMALL_DIAMOND   11

#define MV_SEARCH_TYPE_SUBPEL_FULL      0
#define MV_SEARCH_TYPE_SUBPEL_HALF      1
#define MV_SEARCH_TYPE_SUBPEL_SQUARE    2
#define MV_SEARCH_TYPE_SUBPEL_HQ        3
#define MV_SEARCH_TYPE_SUBPEL_DIAMOND   4

#define H264_MAXREFDIST 32

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
#pragma warning(disable:2259)
#endif
//////////////////////////////////////////////////////////////////////////

#define CHECK_OPTION(input, output, corcnt) \
  if ( input != MFX_CODINGOPTION_OFF &&     \
      input != MFX_CODINGOPTION_ON  &&      \
      input != MFX_CODINGOPTION_UNKNOWN ) { \
    output = MFX_CODINGOPTION_UNKNOWN;      \
    (corcnt) ++;                            \
  } else output = input;

#define CHECK_TRI_OPTION(input, output, corcnt) \
if (input != MFX_CODINGOPTION_OFF &&      \
    input != MFX_CODINGOPTION_ON &&       \
    input != MFX_CODINGOPTION_ADAPTIVE && \
    input != MFX_CODINGOPTION_UNKNOWN ) { \
    output = MFX_CODINGOPTION_UNKNOWN;    \
    (corcnt) ++;                          \
} else output = input;

#define CORRECT_FLAG(flag, value, corcnt) \
  if ( flag != 0 && flag != 1 ) { \
    flag = value;      \
    (corcnt) ++; }

#define CHECK_ZERO(input, corcnt) \
  if ( input != 0 ) { \
    input =0 ;      \
    (corcnt) ++; }

#define CHECK_EXTBUF_SIZE(ebuf, errcounter) if ((ebuf).Header.BufferSz != sizeof(ebuf)) {(errcounter) = (errcounter) + 1;}

#define IS_MULTIPLE(x,of) \
    ((of)<=1 || (x)<=1 || (x)/(of)*(of) == (x))
#define ARE_MULTIPLE(x,y) \
    ((x)<=1 || (y)<=1 || (x)/(y)*(y) == (x) || (y)/(x)*(x) == (y))


static mfxI32 simplify(mfxI32* pvar1, mfxI32 *pstep1, mfxI32 var2)
{
    mfxI32 var1 = *pvar1;
    mfxI32 step1 = *pstep1;
    if (ARE_MULTIPLE(var2, var1))
        return 2;
    if (step1 < 1) step1 = 1;
    if (var1 < var2) {
        while(!IS_MULTIPLE(var2, var1) && var1 <= var2)
            var1 += step1;
    } else if (var1 > var2) {
        while(!IS_MULTIPLE(var1, var2) && var1 <= var2 * *pvar1)
            var1 += step1;
    }
    if (!ARE_MULTIPLE(var2, var1))
        return 0;

    *pvar1 = var1;
    *pstep1 = step1;
    return 1;
}

#ifdef VM_SLICE_THREADING_H264
Status MFXVideoENCODEH264::ThreadedSliceCompress(Ipp32s numTh){
#ifdef H264_NEW_THREADING
    const H264CoreEncoder_8u16s* core_enc = m_layer->enc;
    void* state = m_layer->enc;
    EnumSliceType default_slice_type = m_taskParams.threads_data[numTh].default_slice_type;
    Ipp32s slice_qp_delta_default = m_taskParams.threads_data[numTh].slice_qp_delta_default;
    Ipp32s slice = m_taskParams.threads_data[numTh].slice;
#else
    const H264CoreEncoder_8u16s* core_enc = threadSpec[numTh].core_enc;
    void* state = threadSpec[numTh].state;
    EnumSliceType default_slice_type = threadSpec[numTh].default_slice_type;
    Ipp32s slice_qp_delta_default = threadSpec[numTh].slice_qp_delta_default;
    Ipp32s slice = threadSpec[numTh].slice;
#endif // H264_NEW_THREADING

    core_enc->m_Slices[slice].m_slice_qp_delta = (Ipp8s)slice_qp_delta_default;
    core_enc->m_Slices[slice].m_slice_number = slice;
    core_enc->m_Slices[slice].m_slice_type = default_slice_type; // Pass to core encoder
    H264CoreEncoder_UpdateRefPicList_8u16s(state, core_enc->m_Slices + slice, &core_enc->m_pCurrentFrame->m_refPicListCommon[core_enc->m_field_index], core_enc->m_SliceHeader);
#ifdef SLICE_CHECK_LIMIT
re_encode_slice:
#endif // SLICE_CHECK_LIMIT
    Ipp32s slice_bits = H264BsBase_GetBsOffset(core_enc->m_Slices[slice].m_pbitstream);
    EnumPicCodType pic_type = INTRAPIC;
    if (core_enc->m_info.rate_controls.method == H264_RCM_VBR_SLICE || core_enc->m_info.rate_controls.method == H264_RCM_CBR_SLICE){
        pic_type = (core_enc->m_Slices[slice].m_slice_type == INTRASLICE) ? INTRAPIC : (core_enc->m_Slices[slice].m_slice_type == PREDSLICE) ? PREDPIC : BPREDPIC;
        core_enc->m_Slices[slice].m_slice_qp_delta = (Ipp8s)(H264_AVBR_GetQP(&core_enc->avbr, pic_type) - core_enc->m_PicParamSet->pic_init_qp);
        core_enc->m_Slices[slice].m_iLastXmittedQP = core_enc->m_PicParamSet->pic_init_qp + core_enc->m_Slices[slice].m_slice_qp_delta;
    }

    /* Fill SVC data */
    if (!core_enc->m_svc_layer.isActive || m_layer->m_InterLayerParam.ScanIdxPresent != MFX_CODINGOPTION_ON) {
        core_enc->m_Slices[slice].m_scan_idx_start = 0;
        core_enc->m_Slices[slice].m_scan_idx_end = 15;
    } else {
        core_enc->m_Slices[slice].m_scan_idx_start =
             m_layer->m_InterLayerParam.QualityLayer[core_enc->m_svc_layer.svc_ext.quality_id].ScanIdxStart;
        core_enc->m_Slices[slice].m_scan_idx_end = 
            m_layer->m_InterLayerParam.QualityLayer[core_enc->m_svc_layer.svc_ext.quality_id].ScanIdxEnd;
    }

    if (!core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag)
    {
        switch (m_layer->m_InterLayerParam.BasemodePred) {
        case MFX_CODINGOPTION_ON:
            core_enc->m_Slices[slice].m_adaptive_prediction_flag = 0;
            core_enc->m_Slices[slice].m_default_base_mode_flag = 1;
            break;
        case MFX_CODINGOPTION_OFF:
            core_enc->m_Slices[slice].m_adaptive_prediction_flag = 0;
            core_enc->m_Slices[slice].m_default_base_mode_flag = 0;
            break;
        case MFX_CODINGOPTION_ADAPTIVE:
        default:
            core_enc->m_Slices[slice].m_adaptive_prediction_flag = 1;
            core_enc->m_Slices[slice].m_default_base_mode_flag = 0;
            break;
        }

        switch (m_layer->m_InterLayerParam.MotionPred) {
        case MFX_CODINGOPTION_ON:
            core_enc->m_Slices[slice].m_adaptive_motion_prediction_flag = 0;
            core_enc->m_Slices[slice].m_default_motion_prediction_flag = 1;
            break;
        case MFX_CODINGOPTION_OFF:
            core_enc->m_Slices[slice].m_adaptive_motion_prediction_flag = 0;
            core_enc->m_Slices[slice].m_default_motion_prediction_flag = 0;
            break;
        case MFX_CODINGOPTION_ADAPTIVE:
        default:
            core_enc->m_Slices[slice].m_adaptive_motion_prediction_flag = 1;
            core_enc->m_Slices[slice].m_default_motion_prediction_flag = 0;
            break;
        }
        // MFX_CODINGOPTION_OFF anyway
        core_enc->m_Slices[slice].m_adaptive_motion_prediction_flag = 0;
        core_enc->m_Slices[slice].m_default_motion_prediction_flag = 0;

        switch (m_layer->m_InterLayerParam.ResidualPred) {
        case MFX_CODINGOPTION_ON:
            core_enc->m_Slices[slice].m_adaptive_residual_prediction_flag = 0;
            core_enc->m_Slices[slice].m_default_residual_prediction_flag = 1;
            break;
        case MFX_CODINGOPTION_OFF:
            core_enc->m_Slices[slice].m_adaptive_residual_prediction_flag = 0;
            core_enc->m_Slices[slice].m_default_residual_prediction_flag = 0;
            break;
        case MFX_CODINGOPTION_ADAPTIVE:
        default:
            core_enc->m_Slices[slice].m_adaptive_residual_prediction_flag = 1;
            core_enc->m_Slices[slice].m_default_residual_prediction_flag = 0;
            break;
        }

        core_enc->m_Slices[slice].m_tcoeff_level_prediction_flag =
            (m_layer->m_InterLayerParam.QualityLayer[core_enc->m_svc_layer.svc_ext.quality_id].TcoeffPredictionFlag == MFX_CODINGOPTION_ON);

        core_enc->m_Slices[slice].m_slice_skip_flag = 0;
        core_enc->m_Slices[slice].m_base_pred_weight_table_flag = 1;
        core_enc->m_Slices[slice].m_num_mbs_in_slice_minus1 = 0; /* is used only if m_slice_skip_flag != 0 */

        if (core_enc->m_svc_layer.svc_ext.quality_id == 0)
        {
            if (m_layer->ref_layer != NULL)
            {
                mfxU16 RefLayerDid = m_layer->m_InterLayerParam.RefLayerDid;
                mfxU16 RefLayerQid = m_layer->ref_layer->enc->QualityNum - 1;

                core_enc->m_Slices[slice].m_ref_layer_dq_id = (RefLayerDid << 4) + RefLayerQid;
            }

            core_enc->m_Slices[slice].m_constrained_intra_resampling_flag = 0;
            core_enc->m_Slices[slice].m_ref_layer_chroma_phase_x_plus1 = 0;
            core_enc->m_Slices[slice].m_ref_layer_chroma_phase_y_plus1 = 0;
            core_enc->m_Slices[slice].m_disable_inter_layer_deblocking_filter_idc =
                (m_layer->m_InterLayerParam.DisableDeblockingFilter == MFX_CODINGOPTION_ON) ?
                DEBLOCK_FILTER_OFF : DEBLOCK_FILTER_ON;
            core_enc->m_Slices[slice].m_inter_layer_slice_alpha_c0_offset_div2 = 0;
            core_enc->m_Slices[slice].m_inter_layer_slice_beta_offset_div2 = 0;

            core_enc->m_Slices[slice].m_scaled_ref_layer_left_offset =
                m_layer->m_InterLayerParam.ScaledRefLayerOffsets[0];
            core_enc->m_Slices[slice].m_scaled_ref_layer_top_offset =
                m_layer->m_InterLayerParam.ScaledRefLayerOffsets[1];
            core_enc->m_Slices[slice].m_scaled_ref_layer_right_offset =
                m_layer->m_InterLayerParam.ScaledRefLayerOffsets[2];
            core_enc->m_Slices[slice].m_scaled_ref_layer_bottom_offset =
                m_layer->m_InterLayerParam.ScaledRefLayerOffsets[3];
        }
    }


    // Compress one slice
    if (core_enc->m_is_cur_pic_afrm)
        core_enc->m_Slices[slice].status = H264CoreEncoder_Compress_Slice_MBAFF_8u16s(state, core_enc->m_Slices + slice);
    else {
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
        if (core_enc->useMBT)
            core_enc->m_Slices[slice].status = H264CoreEncoder_Compress_Slice_MBT(state, core_enc->m_Slices + slice);
        else
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT
        core_enc->m_Slices[slice].status = H264CoreEncoder_Compress_Slice_8u16s(state, core_enc->m_Slices + slice, core_enc->m_Slices[slice].m_slice_number == core_enc->m_info.num_slices*core_enc->m_field_index);
#ifdef SLICE_CHECK_LIMIT
        if( core_enc->m_MaxSliceSize){
            Ipp32s numMBs = core_enc->m_HeightInMBs*core_enc->m_WidthInMBs;
            numSliceMB += core_enc->m_Slices[slice].m_MB_Counter;
            dst->SetDataSize(dst->GetDataSize() + H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(core_enc->m_Slices[slice].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize(), (ePic_Class != DISPOSABLE_PIC),
#if defined ALPHA_BLENDING_H264
                (alpha) ? NAL_UT_LAYERNOPART :
#endif // ALPHA_BLENDING_H264
                ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE), startPicture, 0));
            if (numSliceMB != numMBs) {
                core_enc->m_NeedToCheckMBSliceEdges = true;
                H264ENC_MAKE_NAME(H264BsReal_Reset)(core_enc->m_Slices[slice].m_pbitstream);
                core_enc->m_Slices[slice].m_slice_number++;
                goto re_encode_slice;
            } else
                core_enc->m_Slices[slice].m_slice_number = slice;
        }
#endif // SLICE_CHECK_LIMIT
    }
    if (core_enc->m_info.rate_controls.method == H264_RCM_VBR_SLICE || core_enc->m_info.rate_controls.method == H264_RCM_CBR_SLICE)
        H264_AVBR_PostFrame((UMC_H264_ENCODER::H264_AVBR *)&core_enc->avbr, pic_type, (Ipp32s)slice_bits); // NOT thread safe! but unused slice RC method
#ifdef H264_STAT
    slice_bits = H264BsBase_GetBsOffset(core_enc->m_Slices[slice].m_pbitstream) - slice_bits;
    core_enc->hstats.slice_sizes.push_back( slice_bits );
#endif
    return core_enc->m_Slices[slice].status;
}

#ifdef VM_MB_THREADING_H264
#define PIXBITS 8
#define PIXTYPE Ipp8u
static void PadMB_Luma_8u16s(PIXTYPE *pP, Ipp32s pitchPixels, Ipp32s padFlag, Ipp32s padSize)
{
    if (padFlag & 1) {
        // left
        PIXTYPE *p = pP;
        for (Ipp32s i = 0; i < 16; i ++) {
#if PIXBITS == 8
            memset(p - padSize, p[0], padSize);
#else
            for (Ipp32s j = 0; j < padSize; j ++)
                p[j-padSize] = p[0];
#endif
            p += pitchPixels;
        }
    }
    if (padFlag & 2) {
        // right
        PIXTYPE *p = pP + 16;
        for (Ipp32s i = 0; i < 16; i ++) {
#if PIXBITS == 8
            memset(p, p[-1], padSize);
#else
            for (Ipp32s j = 0; j < padSize; j ++)
                p[j] = p[-1];
#endif
            p += pitchPixels;
        }
    }
    if (padFlag & 4) {
        // top
        PIXTYPE *p = pP - pitchPixels;
        Ipp32s s = 16;
        if (padFlag & 1) {
            // top-left
            p -= padSize;
            pP -= padSize;
            s += padSize;
        } else if (padFlag & 2) {
            // top-right
            s += padSize;
        }
        for (Ipp32s i = 0; i < padSize; i ++) {
            MFX_INTERNAL_CPY(p, pP, s * sizeof(PIXTYPE));
            p -= pitchPixels;
        }
        if (padFlag & 1)
            pP += padSize;
    }
    if (padFlag & 8) {
        // bottom
        PIXTYPE *p = pP + 16 * pitchPixels;
        pP = p - pitchPixels;
        Ipp32s s = 16;
        if (padFlag & 1) {
            // bottom-left
            p -= padSize;
            pP -= padSize;
            s += padSize;
        } else if (padFlag & 2) {
            // bottom-right
            s += padSize;
        }
        for (Ipp32s i = 0; i < padSize; i ++) {
            MFX_INTERNAL_CPY(p, pP, s * sizeof(PIXTYPE));
            p += pitchPixels;
        }
    }
}

#ifdef USE_NV12
static void PadMB_Chroma_8u16s(PIXTYPE *pU, Ipp32s pitchPixels, Ipp32s szX, Ipp32s szY, Ipp32s padFlag, Ipp32s padSize)
{
    //Ipp32s
    if (padFlag & 1) {
        // left
        PIXTYPE *pu = pU;
        for (Ipp32s i = 0; i < szY; i ++) {
            for (Ipp32s j = 0; j < (padSize >> 1); j ++)
            {
                pu[(j << 1) - padSize] = pu[0];
                pu[(j << 1) - padSize + 1] = pu[1];
            }
            pu += pitchPixels;
        }
    }
    if (padFlag & 2) {
        // right
        PIXTYPE *pu = pU + szX;
        for (Ipp32s i = 0; i < szY; i ++) {
            for (Ipp32s j = 0; j < (padSize >> 1); j ++)
            {
                pu[j << 1] = pu[-2];
                pu[(j << 1) + 1] = pu[-1];
            }
            pu += pitchPixels;
        }
    }
    if (padFlag & 4) {
        // top
        PIXTYPE *pu = pU - pitchPixels;
        Ipp32s s = szX;
        if (padFlag & 1) {
            // top-left
            pu -= padSize;
            pU -= padSize;
            s += padSize;
        } else if (padFlag & 2) {
            // top-right
            s += padSize;
        }
        for (Ipp32s i = 0; i < (padSize >> 1); i ++) {
            MFX_INTERNAL_CPY(pu, pU, s * sizeof(PIXTYPE));
            pu -= pitchPixels;
        }
        if (padFlag & 1) {
            pU += padSize;
        }
    }
    if (padFlag & 8) {
        // bottom
        PIXTYPE *pu = pU + szY * pitchPixels;
        pU = pu - pitchPixels;
        Ipp32s s = szX;
        if (padFlag & 1) {
            // bottom-left
            pu -= padSize;
            pU -= padSize;
            s += padSize;
        } else if (padFlag & 2) {
            // bottom-right
            s += padSize;
        }
        for (Ipp32s i = 0; i < (padSize >> 1); i ++) {
            MFX_INTERNAL_CPY(pu, pU, s * sizeof(PIXTYPE));
            pu += pitchPixels;
        }
    }
}
#else // USE_NV12
static void H264ENC_MAKE_NAME(PadMB_Chroma)(PIXTYPE *pU, PIXTYPE *pV, Ipp32s pitchPixels, Ipp32s szX, Ipp32s szY, Ipp32s padFlag, Ipp32s padSize)
{
    if (padFlag & 1) {
        // left
        PIXTYPE *pu = pU;
        PIXTYPE *pv = pV;
        for (Ipp32s i = 0; i < szY; i ++) {
#if PIXBITS == 8
            memset(pu - padSize, pu[0], padSize);
            memset(pv - padSize, pv[0], padSize);
#else
            for (Ipp32s j = 0; j < padSize; j ++) {
                pu[j-padSize] = pu[0];
                pv[j-padSize] = pv[0];
            }
#endif
            pu += pitchPixels;
            pv += pitchPixels;
        }
    }
    if (padFlag & 2) {
        // right
        PIXTYPE *pu = pU + szX;
        PIXTYPE *pv = pV + szX;
        for (Ipp32s i = 0; i < szY; i ++) {
#if PIXBITS == 8
            memset(pu, pu[-1], padSize);
            memset(pv, pv[-1], padSize);
#else
            for (Ipp32s j = 0; j < padSize; j ++) {
                pu[j] = pu[-1];
                pv[j] = pv[-1];
            }
#endif
            pu += pitchPixels;
            pv += pitchPixels;
        }
    }
    if (padFlag & 4) {
        // top
        PIXTYPE *pu = pU - pitchPixels;
        PIXTYPE *pv = pV - pitchPixels;
        Ipp32s s = szX;
        if (padFlag & 1) {
            // top-left
            pu -= padSize;
            pU -= padSize;
            pv -= padSize;
            pV -= padSize;
            s += padSize;
        } else if (padFlag & 2) {
            // top-right
            s += padSize;
        }
        for (Ipp32s i = 0; i < padSize; i ++) {
            MFX_INTERNAL_CPY(pu, pU, s * sizeof(PIXTYPE));
            MFX_INTERNAL_CPY(pv, pV, s * sizeof(PIXTYPE));
            pu -= pitchPixels;
            pv -= pitchPixels;
        }
        if (padFlag & 1) {
            pU += padSize;
            pV += padSize;
        }
    }
    if (padFlag & 8) {
        // bottom
        PIXTYPE *pu = pU + szY * pitchPixels;
        PIXTYPE *pv = pV + szY * pitchPixels;
        pU = pu - pitchPixels;
        pV = pv - pitchPixels;
        Ipp32s s = szX;
        if (padFlag & 1) {
            // bottom-left
            pu -= padSize;
            pU -= padSize;
            pv -= padSize;
            pV -= padSize;
            s += padSize;
        } else if (padFlag & 2) {
            // bottom-right
            s += padSize;
        }
        for (Ipp32s i = 0; i < padSize; i ++) {
            MFX_INTERNAL_CPY(pu, pU, s * sizeof(PIXTYPE));
            MFX_INTERNAL_CPY(pv, pV, s * sizeof(PIXTYPE));
            pu += pitchPixels;
            pv += pitchPixels;
        }
    }
}
#endif // USE_NV12


#ifdef FRAME_INTERPOLATION

static void InterpolateHP_MB_8u16s(PIXTYPE *pY, Ipp32s step, Ipp32s planeSize, Ipp32s bitDepth)
{
    static IppiSize sz = {16, 16};
    PIXTYPE *pB = pY + planeSize;
    PIXTYPE *pH = pB + planeSize;
    PIXTYPE *pJ = pH + planeSize;

        ippiInterpolateLuma_H264_8u16s(pY, step, pB, step, 2, 0, sz, bitDepth);
        ippiInterpolateLuma_H264_8u16s(pY, step, pH, step, 0, 2, sz, bitDepth);
        ippiInterpolateLuma_H264_8u16s(pY, step, pJ, step, 2, 2, sz, bitDepth);

}
#endif
#undef PIXBITS
#undef PIXTYPE

mfxStatus CheckAndSetLayerBitrates(struct LayerHRDParams HRDParams[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS],
                                   mfxExtSVCSeqDesc *svcInfo,
                                   mfxI32 maxDid,
                                   mfxU16 rateControlMethod)

{
    mfxStatus sts = MFX_ERR_NONE;
    bool haveLayersToSet = false;
    mfxI32 toprow[MAX_TEMP_LEVELS];
    mfxI32 ii, did, qid, tid;
    if (rateControlMethod != MFX_RATECONTROL_CBR &&
        rateControlMethod != MFX_RATECONTROL_VBR)
        return MFX_ERR_NONE;

    for (ii = 0; ii < svcInfo->DependencyLayer[maxDid].TemporalNum; ii++)
        toprow[ii] = IPP_MAX_32S;
    mfxI32 rightvalue = IPP_MAX_32S;
    for (ii = maxDid; ii >= 0; ii--) {
        if (svcInfo->DependencyLayer[ii].Active) {
            for (qid = svcInfo->DependencyLayer[ii].QualityNum - 1; qid >= 0; qid--) {
                mfxI32 rightvalue = IPP_MAX_32S;
                for (tid = svcInfo->DependencyLayer[ii].TemporalNum - 1; tid >= 0; tid--) {
                    if (HRDParams[ii][qid][tid].TargetKbps >= (mfxU32)rightvalue)
                        HRDParams[ii][qid][tid].TargetKbps = 0;
                    if (HRDParams[ii][qid][tid].TargetKbps >= (mfxU32)toprow[tid])
                        HRDParams[ii][qid][tid].TargetKbps = 0;
                    if (HRDParams[ii][qid][tid].TargetKbps)
                        toprow[tid] = rightvalue = HRDParams[ii][qid][tid].TargetKbps;
                    else {
                        toprow[tid] = rightvalue = IPP_MIN(toprow[tid], rightvalue);
                        haveLayersToSet = true;
                    }
                }
            }
        }
    }

    if (haveLayersToSet) {
        for (tid = svcInfo->DependencyLayer[maxDid].TemporalNum - 1; tid >= 0; tid--) {
            mfxI32 topvalue = 0, topdid = 0, topqid = 0, botvalue = -1, botdid, botqid;
            mfxI32 zerolayers = 0;
            mfxI32 di, qi;
            mfxF64 calcvalue, delta;
            for (did = maxDid; did >= 0; did--) {
                if (!svcInfo->DependencyLayer[did].Active)
                    continue;
                for (qid = svcInfo->DependencyLayer[did].QualityNum - 1; qid >= 0; qid--) {
                    if ((HRDParams[did][qid][tid].TargetKbps > 0) || ((qid | did) == 0)) {
                        if (HRDParams[did][qid][tid].TargetKbps == 0) { // base layer (not set)
                            zerolayers++;
                            qid = -1;
                        }
                        if (zerolayers > 0) {
                            botvalue = (qid >= 0 ? HRDParams[did][qid][tid].TargetKbps : 0);
                            botdid = did;
                            botqid = qid;

                            // set values for intermediate layers
                            // ....

                            if (topdid > botdid) {
                                mfxI32 n0 = svcInfo->DependencyLayer[botdid].QualityNum - 1 - botqid;
                                mfxF64 s0 = (mfxF32)(svcInfo->DependencyLayer[botdid].Width * svcInfo->DependencyLayer[botdid].Height);
                                mfxF64 fn = (mfxF64)n0;
                                mfxF64 si;
                                mfxI32 ni;

                                for (di = botdid + 1; di < topdid; di++) {
                                    if (!svcInfo->DependencyLayer[di].Active)
                                        continue;
                                    ni = svcInfo->DependencyLayer[di].QualityNum;
                                    si = (mfxF32)(svcInfo->DependencyLayer[di].Width * svcInfo->DependencyLayer[di].Height);
                                    fn += ni * si / s0;
                                }

                                ni = topqid + 1;
                                si = (mfxF32)(svcInfo->DependencyLayer[topdid].Width * svcInfo->DependencyLayer[topdid].Height);
                                fn += ni * si / s0;

                                mfxF64 delta0 = (topvalue - botvalue) / fn;
                                calcvalue = (mfxF64)botvalue;
                                for (qi = botqid + 1; qi <= svcInfo->DependencyLayer[botdid].QualityNum - 1; qi++) {
                                    calcvalue += delta0;
                                    HRDParams[botdid][qi][tid].TargetKbps = (mfxU32)calcvalue;
                                }
                                for (di = botdid + 1; di < topdid; di++) {
                                    if (!svcInfo->DependencyLayer[di].Active)
                                        continue;
                                    si = (mfxF64)(svcInfo->DependencyLayer[di].Width * svcInfo->DependencyLayer[di].Height);
                                    delta = delta0 * si / s0;
                                    for (qi = 0; qi < svcInfo->DependencyLayer[di].QualityNum; qi++) {
                                        calcvalue += delta;
                                        HRDParams[di][qi][tid].TargetKbps = (mfxU32)calcvalue;
                                    }
                                }
                                for (qi = 0; qi < topqid; qi++) {
                                    si = (mfxF64)(svcInfo->DependencyLayer[di].Width * svcInfo->DependencyLayer[di].Height);
                                    delta = delta0 * si / s0;
                                    calcvalue += delta;
                                    HRDParams[di][qi][tid].TargetKbps = (mfxU32)calcvalue;
                                }

                            } else {
                                delta = ((mfxF64)topvalue - botvalue) / (topqid - botqid);
                                calcvalue = botvalue;

                                for (qi = botqid + 1; qi < topqid; qi++) {
                                    calcvalue += delta;
                                    HRDParams[did][qi][tid].TargetKbps = (mfxU32)calcvalue;
                                }
                            }

                            topvalue = botvalue;
                            topdid = botdid;
                            topqid = botqid;
                            zerolayers = 0;
                        } else {
                            topvalue = HRDParams[did][qid][tid].TargetKbps;
                            topdid = did;
                            topqid = qid;
                        }
                    } else
                        zerolayers++;

                }
            }
        }

        for (tid = svcInfo->DependencyLayer[maxDid].TemporalNum - 2; tid >= 0; tid--) {
            mfxF64 calcvalue, frateratio = (mfxF64)svcInfo->TemporalScale[tid] / svcInfo->TemporalScale[tid+1];
            mfxF64 curvalue, delta, botvalue;
            for (did = maxDid; did >= 0; did--) {
                if (!svcInfo->DependencyLayer[did].Active)
                    continue;
                if (svcInfo->DependencyLayer[did].TemporalNum - 1 > tid) {
                    for (qid = svcInfo->DependencyLayer[did].QualityNum - 1; qid >= 0; qid--) {
                        curvalue = (mfxF64)HRDParams[did][qid][tid].TargetKbps;
                        rightvalue = HRDParams[did][qid][tid + 1].TargetKbps;
                        calcvalue = rightvalue * frateratio;
                        delta = rightvalue - calcvalue;

                        if (curvalue > rightvalue - 0.25*delta) {
                            curvalue = rightvalue - 0.25*delta;
                            if (qid > 0) {
                                botvalue = (mfxF64)HRDParams[did][qid-1][tid].TargetKbps;
                            } else if (did > 0) {
                                mfxI32 lowerdid;
                                botvalue = 0;
                                for (lowerdid = did - 1; lowerdid >= 0; lowerdid--) {
                                    if (svcInfo->DependencyLayer[lowerdid].Active) {
                                        botvalue = (mfxF64)HRDParams[lowerdid][svcInfo->DependencyLayer[lowerdid].QualityNum - 1][tid].TargetKbps;
                                        break;
                                    }
                                }
                            } else
                                botvalue = 0;
                            if (curvalue < botvalue) {
                                mfxF64 newbotvalue = curvalue;
                                curvalue = rightvalue - 0.125*delta;
                                mfxI32 dd, qq;
                                mfxF64 scale = botvalue == 0 ? 0 : newbotvalue / botvalue;
                                dd = qid > 0 ? did : did - 1;
                                for (; dd >= 0; dd--) {
                                    if (!svcInfo->DependencyLayer[dd].Active)
                                        continue;
                                    qq = (dd == did ? qid - 1 : svcInfo->DependencyLayer[dd].QualityNum-1);
                                    for (; qq >=0; qq--) {
                                        HRDParams[dd][qq][tid].TargetKbps = (mfxU64)(HRDParams[dd][qq][tid].TargetKbps * scale);
                                    }
                                }
                            }
                            HRDParams[did][qid][tid].TargetKbps = (mfxU64)curvalue;
                        }
                    }
                }
            }
        }
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    mfxU32 haveBitrates = 0;
    for (did = maxDid; did >= 0; did--) {
        for (qid = svcInfo->DependencyLayer[did].QualityNum - 1; qid >= 0; qid--) {
            for (tid = 0; tid < svcInfo->DependencyLayer[did].TemporalNum; tid++) {
                haveBitrates |= HRDParams[did][qid][tid].TargetKbps;
                if (rateControlMethod == MFX_RATECONTROL_VBR) {
                    if (HRDParams[did][qid][tid].MaxKbps < HRDParams[did][qid][tid].TargetKbps) {
                        HRDParams[did][qid][tid].MaxKbps = HRDParams[did][qid][tid].TargetKbps;
                        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                    }
                }
            }
        }
    }
    if (!haveBitrates)
        sts = MFX_ERR_INVALID_VIDEO_PARAM;

    return sts;
}

mfxStatus CheckAndSetLayerHRDBuffers(struct LayerHRDParams HRDParams[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS],
                                   mfxExtSVCSeqDesc *svcInfo,
                                   mfxI32 maxDid,
                                   mfxF64 framerate,
                                   mfxU16 rateControlMethod)
{
    mfxStatus sts = MFX_ERR_NONE;
    bool haveLayersToSet = false;
    mfxI32 toprow[MAX_TEMP_LEVELS];
    mfxI32 ii, did, qid, tid;
    if (rateControlMethod != MFX_RATECONTROL_CBR &&
        rateControlMethod != MFX_RATECONTROL_VBR)
        return MFX_ERR_NONE;
    for (ii = 0; ii < svcInfo->DependencyLayer[maxDid].TemporalNum; ii++)
        toprow[ii] = IPP_MAX_32S;
    for (ii = maxDid; ii >= 0; ii--) {
        if (svcInfo->DependencyLayer[ii].Active) {
            for (qid = svcInfo->DependencyLayer[ii].QualityNum - 1; qid >= 0; qid--) {
                for (tid = svcInfo->DependencyLayer[ii].TemporalNum - 1; tid >= 0; tid--) {
                    mfxF64 curFR = framerate * svcInfo->TemporalScale[tid];
                    mfxI32 curBuf = HRDParams[ii][qid][tid].BufferSizeInKB;
                    if (ii < maxDid || qid < svcInfo->DependencyLayer[ii].QualityNum - 1) {
                        mfxI32 topBitrate = (qid < svcInfo->DependencyLayer[ii].QualityNum - 1) ? HRDParams[ii][qid+1][tid].TargetKbps : HRDParams[ii+1][0][tid].TargetKbps;
                        // target bitrate must be set by now
                        if (toprow[tid] - curBuf < (topBitrate - (mfxI32)HRDParams[ii][qid][tid].TargetKbps) * 0.5 / curFR) // 4 frames
                            HRDParams[ii][qid][tid].BufferSizeInKB = 0;
                        else if (curBuf < HRDParams[ii][qid][tid].TargetKbps * 0.5 / curFR)
                            HRDParams[ii][qid][tid].BufferSizeInKB = 0;
                    } else if (curBuf < HRDParams[ii][qid][tid].TargetKbps * 0.5 / curFR)
                        HRDParams[ii][qid][tid].BufferSizeInKB = HRDParams[ii][qid][tid].TargetKbps * 0.5 / curFR;
                    if (HRDParams[ii][qid][tid].BufferSizeInKB)
                        toprow[tid] = HRDParams[ii][qid][tid].BufferSizeInKB;
                    else
                        haveLayersToSet = true;
                }
            }
        }
    }
// check initial delays right away?
// makes sense to set all buffer sizes according to the top one and then check that we don't INCREASE the given (sane) sizes?

    if (haveLayersToSet) {
        for (tid = svcInfo->DependencyLayer[maxDid].TemporalNum - 1; tid >= 0; tid--) {
            mfxI32 topvalue = 0, topdid = 0, topqid = 0, botvalue = -1, botdid, botqid;
            mfxI32 zerolayers = 0;
            mfxI32 di, qi;
            mfxF64 calcvalue, delta;
            for (did = maxDid; did >= 0; did--) {
                if (!svcInfo->DependencyLayer[did].Active)
                    continue;
                for (qid = svcInfo->DependencyLayer[did].QualityNum - 1; qid >= 0; qid--) {
                    if ((HRDParams[did][qid][tid].BufferSizeInKB > 0) || ((qid | did) == 0)) {
                        if (HRDParams[did][qid][tid].BufferSizeInKB == 0) { // base layer (not set)
                            zerolayers++;
                            qid = -1;
                        }
                        if (zerolayers > 0) {
                            botvalue = (qid >= 0 ? HRDParams[did][qid][tid].BufferSizeInKB : 0);
                            botdid = did;
                            botqid = qid;

                            // set values for intermediate layers
                            // ....

                            if (topdid > botdid) {
                                mfxI32 n0 = svcInfo->DependencyLayer[botdid].QualityNum - 1 - botqid;
                                mfxF64 s0 = (mfxF32)(svcInfo->DependencyLayer[botdid].Width * svcInfo->DependencyLayer[botdid].Height);
                                mfxF64 fn = (mfxF64)n0;
                                mfxF64 si;
                                mfxI32 ni;

                                for (di = botdid + 1; di < topdid; di++) {
                                    if (!svcInfo->DependencyLayer[di].Active)
                                        continue;
                                    ni = svcInfo->DependencyLayer[di].QualityNum;
                                    si = (mfxF32)(svcInfo->DependencyLayer[di].Width * svcInfo->DependencyLayer[di].Height);
                                    fn += ni * si / s0;
                                }

                                ni = topqid + 1;
                                si = (mfxF32)(svcInfo->DependencyLayer[topdid].Width * svcInfo->DependencyLayer[topdid].Height);
                                fn += ni * si / s0;

                                mfxF64 delta0 = (topvalue - botvalue) / fn;
                                calcvalue = (mfxF64)botvalue;
                                for (qi = botqid + 1; qi <= svcInfo->DependencyLayer[botdid].QualityNum - 1; qi++) {
                                    calcvalue += delta0;
                                    HRDParams[botdid][qi][tid].BufferSizeInKB = (mfxU32)calcvalue;
                                }
                                for (di = botdid + 1; di < topdid; di++) {
                                    if (!svcInfo->DependencyLayer[di].Active)
                                        continue;
                                    si = (mfxF64)(svcInfo->DependencyLayer[di].Width * svcInfo->DependencyLayer[di].Height);
                                    delta = delta0 * si / s0;
                                    for (qi = 0; qi < svcInfo->DependencyLayer[di].QualityNum; qi++) {
                                        calcvalue += delta;
                                        HRDParams[di][qi][tid].BufferSizeInKB = (mfxU32)calcvalue;
                                    }
                                }
                                for (qi = 0; qi < topqid; qi++) {
                                    si = (mfxF64)(svcInfo->DependencyLayer[di].Width * svcInfo->DependencyLayer[di].Height);
                                    delta = delta0 * si / s0;
                                    calcvalue += delta;
                                    HRDParams[di][qi][tid].BufferSizeInKB = (mfxU32)calcvalue;
                                }

                            } else {
                                delta = ((mfxF64)topvalue - botvalue) / (topqid - botqid);
                                calcvalue = botvalue;

                                for (qi = botqid + 1; qi < topqid; qi++) {
                                    calcvalue += delta;
                                    HRDParams[did][qi][tid].BufferSizeInKB = (mfxU32)calcvalue;
                                }
                            }

                            topvalue = botvalue;
                            topdid = botdid;
                            topqid = botqid;
                            zerolayers = 0;
                        } else {
                            topvalue = HRDParams[did][qid][tid].BufferSizeInKB;
                            topdid = did;
                            topqid = qid;
                        }
                    } else
                        zerolayers++;

                }
            }
        }
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    mfxU32 haveBufferSizes = 0;
    for (did = maxDid; did >= 0; did--) {
        for (qid = svcInfo->DependencyLayer[did].QualityNum - 1; qid >= 0; qid--) {
            for (tid = 0; tid < svcInfo->DependencyLayer[did].TemporalNum; tid++) {
                mfxF64 curFR = framerate * svcInfo->TemporalScale[tid];
                haveBufferSizes |= HRDParams[did][qid][tid].BufferSizeInKB;
                if (HRDParams[did][qid][tid].InitialDelayInKB > HRDParams[did][qid][tid].BufferSizeInKB) {
                    HRDParams[did][qid][tid].InitialDelayInKB = HRDParams[did][qid][tid].BufferSizeInKB;
                    sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
                if (HRDParams[did][qid][tid].InitialDelayInKB == 0) {
                    HRDParams[did][qid][tid].InitialDelayInKB = (rateControlMethod == MFX_RATECONTROL_VBR) ? HRDParams[did][qid][tid].BufferSizeInKB : HRDParams[did][qid][tid].BufferSizeInKB >> 1;
                    sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
                else if (HRDParams[did][qid][tid].InitialDelayInKB < (mfxU32)(HRDParams[did][qid][tid].TargetKbps / 8 / curFR)) {
                    HRDParams[did][qid][tid].InitialDelayInKB = (mfxU32)(HRDParams[did][qid][tid].TargetKbps / 8 / curFR);
                    sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
            }
        }
    }

    if (!haveBufferSizes)
        sts = MFX_ERR_INVALID_VIDEO_PARAM;

    return sts;
}


Status MFXVideoENCODEH264::ThreadCallBackVM_MBT(threadSpecificDataH264 &tsd)
{
    ThreadDataVM_MBT_8u16s *td = &tsd.mbt_data;
    const H264CoreEncoder_8u16s *core_enc = td->core_enc;
    void *state = td->core_enc;
    H264Slice_8u16s *curr_slice = td->curr_slice;
    H264BsReal_8u16s *pBitstream = (H264BsReal_8u16s *)curr_slice->m_pbitstream;
    Ipp32s uNumMBs = core_enc->m_HeightInMBs * core_enc->m_WidthInMBs;
    Ipp32s uFirstMB = core_enc->m_field_index * uNumMBs;
    //Ipp32s MBYAdjust = (core_enc->m_field_index) ? core_enc->m_HeightInMBs : 0;
    bool doPadding = /*(core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC) || (core_enc->m_info.treat_B_as_reference && core_enc->m_pCurrentFrame->m_RefPic)*/ false;
    bool doDeblocking = doPadding && (curr_slice->m_disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF);
    Status status = UMC_OK;
    Ipp32s m_first_mb_in_slice = curr_slice->m_first_mb_in_slice + uFirstMB;
    Ipp32s m_last_mb_in_slice = curr_slice->m_last_mb_in_slice + uFirstMB;
    Ipp32s first_slice_row = m_first_mb_in_slice / core_enc->m_WidthInMBs;
    Ipp32s last_slice_row = m_last_mb_in_slice / core_enc->m_WidthInMBs;
    Ipp32s MBYAdjust = 0;
    if (core_enc->m_field_index)
        MBYAdjust = core_enc->m_HeightInMBs;

#ifdef USE_NV12
    Ipp32s ch_X = 16, ch_Y = 8, ch_P = LUMA_PADDING;
#else // USE_NV12
    Ipp32s ch_X = 8, ch_Y = 8, ch_P = LUMA_PADDING >> 1;
#endif // USE_NV12
    if (core_enc->m_PicParamSet->chroma_format_idc == 2) {
        ch_Y = 16;
        ch_P = LUMA_PADDING;
    } else if (core_enc->m_PicParamSet->chroma_format_idc == 3) {
        ch_X = ch_Y = 16;
        ch_P = LUMA_PADDING;
    }

    {
        Ipp32s tRow, tCol, tNum, nBits;
        Ipp8u *pBuffS=0, *pBuffM;
        Ipp32u bitOffS=0, bitOffM;
        Status sts = UMC_OK;
        tNum = td->threadNum;
        if ((core_enc->m_PicParamSet->entropy_coding_mode == 1) &&
#ifdef H264_NEW_THREADING
            (m_taskParams.parallel_executing_threads > 1) &&
            !vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&m_taskParams.cabac_encoding_in_progress), 1, 0))
#else
            (core_enc->m_info.numThreads > 2) && (tNum == (core_enc->m_info.numThreads - 1)))
#endif // H264_NEW_THREADING
        {
            H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
            for (tRow = first_slice_row; tRow <= last_slice_row; tRow ++) {
                for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++) {
                    Ipp32s uMB = tRow * core_enc->m_WidthInMBs + tCol;
                    VM_ASSERT(uMB <= m_last_mb_in_slice);
                    cur_mb.uMB = uMB;
                    cur_mb.uMBx = static_cast<Ipp16u>(tCol);
                    cur_mb.uMBy = static_cast<Ipp16u>(tRow - MBYAdjust);
#ifndef H264_INTRA_REFRESH
                    cur_mb.lambda = lambda_sq[curr_slice->m_iLastXmittedQP];
#endif // H264_INTRA_REFRESH
                    cur_mb.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
                    cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                    cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << curr_slice->m_is_cur_mb_field;
                    H264CoreEncoder_UpdateCurrentMBInfo_8u16s(state, curr_slice);
                    cur_mb.cabac_data = &core_enc->gd_MBT[uMB].bc;
                    cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
#ifdef H264_INTRA_REFRESH
                    cur_mb.lambda = lambda_sq[cur_mb.lumaQP];
#endif // H264_INTRA_REFRESH
                    cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                    cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
                    while (core_enc->mbReady_MBT[tRow] < tCol);
            
#ifdef H264_RECODE_PCM
                    H264BsReal_StoreIppCABACState_8u16s(pBitstream);
#endif //H264_RECODE_PCM

                    ////if (rd_frame_num == 1) {
                    ////    printf("r frame = %d, did = %d, qid = %d, x = %d, y = %d\n", rd_frame_num,
                    ////        core_enc->m_svc_layer.svc_ext.dependency_id,
                    ////        core_enc->m_svc_layer.svc_ext.quality_id,
                    ////        cur_mb.uMBx, cur_mb.uMBy); /* RD */
                    ////    fflush(stdout);
                    ////}

#ifdef H264_RECODE_PCM
                    if(core_enc->m_mbPCM[uMB])
                    {
                        H264CoreEncoder_Put_MBHeader_Real_8u16s(state, curr_slice);   // PCM values are written in the MB Header.
                    } else
#endif //H264_RECODE_PCM
                        sts = H264CoreEncoder_Put_MB_Real_8u16s(state, curr_slice);
                    if (sts != UMC_OK) {
                        status = sts;
                        break;
                    }
                    H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(pBitstream, (uMB == uFirstMB + uNumMBs - 1) || (core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB + 1].slice_id != curr_slice->m_slice_number));
                    H264CoreEncoder_ReconstuctCBP_8u16s(&cur_mb);
                    
#ifdef H264_RECODE_PCM
                    if (   core_enc->m_PicParamSet->entropy_coding_mode 
                        && !core_enc->m_mbPCM[uMB]
                        && 3200 < H264BsReal_GetNotStoredStreamSize_CABAC_8u16s(pBitstream))
                    {
                        vm_interlocked_inc32((volatile Ipp32u*)&core_enc->m_nPCM);
                        core_enc->m_mbPCM[uMB] = 1;
                    }
#endif //H264_RECODE_PCM
                }
            }
        } else {
            tRow = *td->incRow;
            H264Slice_8u16s *cur_s = &core_enc->m_Slices_MBT[tNum];
            cur_s->m_MB_Counter = 0;
            H264CurrentMacroblockDescriptor_8u16s &cur_mb = cur_s->m_cur_mb;
            while (tRow <= last_slice_row) {
                vm_mutex_lock((vm_mutex *)&core_enc->mutexIncRow); // cast from const vm_mutex *
                tRow = *td->incRow;
                (*td->incRow) ++;
#ifdef MB_THREADING_TW
                if (tRow < last_slice_row)
                    for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++)
                        vm_mutex_lock((vm_mutex *)core_enc->mMutexMT + tRow * core_enc->m_WidthInMBs + tCol);
#endif // MB_THREADING_TW
                vm_mutex_unlock((vm_mutex *)&core_enc->mutexIncRow); // cast from const vm_mutex *
                if (tRow <= last_slice_row) {
                    cur_s->m_uSkipRun = 0;
                    if (core_enc->m_PicParamSet->entropy_coding_mode == 0) {
                        H264BsReal_Reset_8u16s(cur_s->m_pbitstream);
                        H264BsBase_GetState(cur_s->m_pbitstream, &pBuffS, &bitOffS);
                    }
                    Ipp32s fsr = -1;
                    for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++) {
                        Ipp32s uMB = tRow * core_enc->m_WidthInMBs + tCol;
                        cur_s->m_MB_Counter++;
                        cur_mb.uMB = uMB;
                        cur_mb.uMBx = tCol;
                        cur_mb.uMBy = tRow - MBYAdjust;
#ifndef H264_INTRA_REFRESH
                        cur_mb.lambda = lambda_sq[cur_s->m_iLastXmittedQP];
#endif // H264_INTRA_REFRESH
                        cur_mb.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
                        cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field];
                        cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << cur_s->m_is_cur_mb_field;
#ifdef MB_THREADING_TW
                        if ((tRow > first_slice_row) && (tCol < (core_enc->m_WidthInMBs - 1))) {
                            vm_mutex_lock((vm_mutex *)core_enc->mMutexMT + (tRow - 1) * core_enc->m_WidthInMBs + tCol + 1);
                            vm_mutex_unlock((vm_mutex *)core_enc->mMutexMT + (tRow - 1) * core_enc->m_WidthInMBs + tCol + 1);
                        }
#else
                        if ((tRow > first_slice_row) && (tCol < (core_enc->m_WidthInMBs - 1)))
                            while (core_enc->mbReady_MBT[tRow-1] <= tCol);
#endif // MB_THREADING_TW
                        H264CoreEncoder_LoadPredictedMBInfo_8u16s(state, cur_s);
                        H264CoreEncoder_UpdateCurrentMBInfo_8u16s(state, cur_s);
                        cur_mb.cabac_data = &core_enc->gd_MBT[uMB].bc;
                        cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
#ifdef H264_INTRA_REFRESH
                        cur_mb.lambda = lambda_sq[cur_mb.lumaQP];
#endif // H264_INTRA_REFRESH
                        cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                        cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
                        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 0);
                        pSetMB8x8TSBaseFlag(cur_mb.GlobalMacroblockInfo, // used in base mode
                            pGetMB8x8TSPackFlag(cur_mb.GlobalMacroblockInfo));

                        H264CoreEncoder_setInCropWindow_8u16s(state, cur_s);

            //if (core_enc->m_uFrames_Num == 1// && cur_mb.uMBx == 1 && cur_mb.uMBy == 0
            //    &&core_enc->m_svc_layer.svc_ext.dependency_id == 0
            //    //                && core_enc->m_svc_layer.svc_ext.quality_id == 1
            //    ) {
            //        printf("");
            //}

            ////{
            ////    printf("frame = %i, did = %d, qid = %d, x = %i, y = %i\n", core_enc->m_uFrames_Num,
            ////        core_enc->m_svc_layer.svc_ext.dependency_id,
            ////        core_enc->m_svc_layer.svc_ext.quality_id,
            ////        cur_mb.uMBx, cur_mb.uMBy); /* RD */
            ////    fflush(stdout);
            ////}

                        if (core_enc->m_svc_layer.isActive) {
                            MFX_INTERNAL_CPY(cur_s->m_cur_mb.intra_types_save, cur_s->m_cur_mb.intra_types, 16*sizeof(T_AIMode));
                        }

#ifdef H264_RECODE_PCM
                        if(core_enc->m_mbPCM[uMB])
                        {
                            cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_PCM;
                            cur_mb.LocalMacroblockInfo->cbp_luma = 0xffff;

                            memset(cur_mb.MacroblockCoeffsInfo->numCoeff, 16, 24);

                            Ipp32s  k;     // block number, 0 to 15
                            for (k = 0; k < 16; k++) {
                                cur_mb.intra_types[k] = 2;
                                cur_mb.MVs[LIST_0]->MotionVectors[k] = null_mv;
                                cur_mb.MVs[LIST_1]->MotionVectors[k] = null_mv;
                                cur_mb.RefIdxs[LIST_0]->RefIdxs[k] = -1;
                                cur_mb.RefIdxs[LIST_1]->RefIdxs[k] = -1;
                            }

                            H264CoreEncoder_Reconstruct_PCM_MB_8u16s(state, curr_slice);
                        } else {
#endif //H264_RECODE_PCM

                            if (core_enc->m_svc_layer.isActive)
                                SVC_MB_Decision(state, cur_s);
                            else
                                H264CoreEncoder_MB_Decision_8u16s(state, cur_s);

                            cur_mb.LocalMacroblockInfo->cbp_bits = 0;
                            cur_mb.LocalMacroblockInfo->cbp_bits_chroma = 0;
                            if (core_enc->m_svc_layer.isActive) // SVC case
                                H264CoreEncoder_CEncAndRecMB_SVC_8u16s(state, cur_s);
                            else // AVC case
                                H264CoreEncoder_CEncAndRecMB_8u16s(state, cur_s);
                        
#ifdef H264_RECODE_PCM
                        }
#endif //H264_RECODE_PCM
                        if (core_enc->m_PicParamSet->entropy_coding_mode && cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) {
                            cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x8;
                        }
                        if (core_enc->m_PicParamSet->entropy_coding_mode == 0) {
                            sts = H264CoreEncoder_Put_MB_Real_8u16s(state, cur_s);
                            if (sts != UMC_OK) {
                                status = sts;
                                break;
                            }
                            if (fsr == -1 && cur_s->m_uSkipRun == 0)
                                fsr = tCol;
                        }
                        if (tRow > 0) {
                            if (doDeblocking)
                            {
                                if (tCol > 0)
                                    H264CoreEncoder_DeblockSlice_8u16s((void*)core_enc, cur_s, uMB - 1 * core_enc->m_WidthInMBs - 1, 1);
                                if (tCol == core_enc->m_WidthInMBs - 1)
                                    H264CoreEncoder_DeblockSlice_8u16s((void*)core_enc, cur_s, uMB - 1 * core_enc->m_WidthInMBs, 1);
                            }
                        }
#ifndef NO_PADDING
                        if (doPadding) {
                            if (tRow > first_slice_row + 1) {
                                Ipp32s pRow = tRow - 2;
                                Ipp32s pCol = tCol;
                                Ipp32s pMB = pRow * core_enc->m_WidthInMBs + pCol;
                                Ipp32s padFlag = 0;
                                if (pCol == 0)
                                    padFlag = 1;
                                if (pCol == core_enc->m_WidthInMBs - 1)
                                    padFlag = 2;
                                if (pRow == 0)
                                    padFlag |= 4;
                                //if (pRow == core_enc->m_HeightInMBs - 1)
                                //    padFlag |= 8;
                                if (padFlag) {
                                    PadMB_Luma_8u16s(core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[pMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], cur_mb.mbPitchPixels, padFlag, LUMA_PADDING);
                                    if (core_enc->m_PicParamSet->chroma_format_idc)
#ifdef USE_NV12
                                        PadMB_Chroma_8u16s(core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], cur_mb.mbPitchPixels, ch_X, ch_Y, padFlag, ch_P);
#else // USE_NV12
                                        H264ENC_MAKE_NAME(PadMB_Chroma)(core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], core_enc->m_pReconstructFrame->m_pVPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], cur_mb.mbPitchPixels, ch_X, ch_Y, padFlag, ch_P);
#endif // USE_NV12
                                }
                            }
#ifdef FRAME_INTERPOLATION
                            if (core_enc->m_Analyse & ANALYSE_ME_SUBPEL)
                            {
                                Ipp32s pitchPixels = core_enc->m_pReconstructFrame->m_pitchPixels;
                                Ipp32s planeSize = core_enc->m_pReconstructFrame->m_PlaneSize;
                                Ipp32s bitDepth = core_enc->m_PicParamSet->bit_depth_luma;
                                if (tRow > 2) {
                                    Ipp32s pRow = tRow - 3;
                                    Ipp32s pCol = tCol;
                                    Ipp32s pMB = pRow * core_enc->m_WidthInMBs + pCol;
                                    Ipp8u *pY = core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[pMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field];
                                    InterpolateHP_MB_8u16s(pY, pitchPixels, planeSize, bitDepth);
                                    Ipp32s padFlag = 0;
                                    if (pCol == 0)
                                        padFlag = 1;
                                    if (pCol == core_enc->m_WidthInMBs - 1)
                                        padFlag = 2;
                                    if (pRow == 0)
                                        padFlag |= 4;
                                    //if (pRow == core_enc->m_HeightInMBs - 1)
                                    //    padFlag |= 8;
                                    if (padFlag & 1)
                                        // left
                                        InterpolateHP_MB_8u16s(pY - 16, pitchPixels, planeSize, bitDepth);
                                    if (padFlag & 2)
                                        // right
                                        InterpolateHP_MB_8u16s(pY + 16, pitchPixels, planeSize, bitDepth);
                                    if (padFlag & 4) {
                                        // top
                                        InterpolateHP_MB_8u16s(pY - 16 * pitchPixels, pitchPixels, planeSize, bitDepth);
                                        if (padFlag & 1)
                                            // top-left
                                            InterpolateHP_MB_8u16s(pY - 16 * pitchPixels - 16, pitchPixels, planeSize, bitDepth);
                                        else if (padFlag & 2)
                                            // top-right
                                            InterpolateHP_MB_8u16s(pY - 16 * pitchPixels + 16, pitchPixels, planeSize, bitDepth);
                                    }
                                    /*
                                    if (padFlag & 8) {
                                        // bottom
                                        H264ENC_MAKE_NAME(InterpolateHP_MB)(pY + 16 * pitchPixels, pitchPixels, planeSize, bitDepth);
                                    if (padFlag & 1)
                                        // bottom-left
                                        H264ENC_MAKE_NAME(InterpolateHP_MB)(pY + 16 * pitchPixels - 16, pitchPixels, planeSize, bitDepth);
                                    else if (padFlag & 2)
                                        // bottom-right
                                        H264ENC_MAKE_NAME(InterpolateHP_MB)(pY + 16 * pitchPixels + 16, pitchPixels, planeSize, bitDepth);
                                    }
                                    */
                                }
                            }
#endif // FRAME_INTERPOLATION
                        }
#endif // !NO_PADDING
#ifdef MB_THREADING_TW
                        if (tCol < (core_enc->m_WidthInMBs - 1))
                            vm_mutex_unlock(core_enc->mMutexMT + tRow * core_enc->m_WidthInMBs + tCol);
#endif // MB_THREADING_TW
                        if (tCol < (core_enc->m_WidthInMBs - 1))
                            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(core_enc->mbReady_MBT + tRow));
                    }
                    if (core_enc->m_info.entropy_coding_mode == 0) {
                        core_enc->lSR_MBT[tRow] = cur_s->m_uSkipRun;
                        H264BsBase_GetState(curr_slice->m_pbitstream, &pBuffM, &bitOffM);
                        nBits = H264BsBase_GetBsOffset(cur_s->m_pbitstream);
                        if (tRow == first_slice_row) {
                            ippsCopy_1u(pBuffS, bitOffS, pBuffM, bitOffM, nBits);
                        } else {
                            if (core_enc->lSR_MBT[tRow - 1] == 0) {
                                ippsCopy_1u(pBuffS, bitOffS, pBuffM, bitOffM, nBits);
                            } else if (core_enc->lSR_MBT[tRow] == core_enc->m_WidthInMBs) {
                                core_enc->lSR_MBT[tRow] += core_enc->lSR_MBT[tRow - 1];
                            } else {
                                Ipp32s l = -1;
                                Ipp32u c = fsr + 1;
                                while (c) {
                                    c >>= 1;
                                    l ++;
                                }
                                l = 1 + (l << 1);
                                nBits -= l;
                                H264BsReal_PutVLCCode_8u16s(pBitstream, fsr + core_enc->lSR_MBT[tRow - 1]);
                                H264BsBase_GetState(curr_slice->m_pbitstream, &pBuffM, &bitOffM);
                                pBuffS += (l + bitOffS) >> 3;
                                bitOffS = (l + bitOffS) & 7;
                                ippsCopy_1u(pBuffS, bitOffS, pBuffM, bitOffM, nBits);
                            }
                        }
                        H264BsBase_UpdateState(curr_slice->m_pbitstream, nBits);
#ifndef NO_FINAL_SKIP_RUN
                        if (tRow == last_slice_row && core_enc->lSR_MBT[tRow] != 0)
                            H264BsReal_PutVLCCode_8u16s((H264BsReal_8u16s*)curr_slice->m_pbitstream, core_enc->lSR_MBT[tRow]);
#endif // NO_FINAL_SKIP_RUN
                        /*
                        if (tRow == (core_enc->m_HeightInMBs - 1) && (cur_s->m_uSkipRun != 0)
                            H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)((H264BsRealType*)cur_s->m_pbitstream, cur_s->m_uSkipRun);
                        H264BsBase_GetState(curr_slice->m_pbitstream, &pBuffM, &bitOffM);
                        Ipp32s nBits = H264BsBase_GetBsOffset(cur_s->m_pbitstream);
                        ippsCopy_1u(pBuffS, bitOffS, pBuffM, bitOffM, nBits);
                        H264BsBase_UpdateState(curr_slice->m_pbitstream, nBits);
                        */
                    }
#ifdef MB_THREADING_TW
                    vm_mutex_unlock(core_enc->mMutexMT + tRow * core_enc->m_WidthInMBs + tCol - 1);
#endif // MB_THREADING_TW
                    vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(core_enc->mbReady_MBT + tRow));
                }
                tRow ++;
                if (sts != UMC_OK)
                    tRow = *td->incRow = core_enc->m_HeightInMBs;
            }
        }
    }
    return status;
}

Status MFXVideoENCODEH264::H264CoreEncoder_Compress_Slice_MBT(void* state, H264Slice_8u16s *curr_slice)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    H264BsReal_8u16s* pBitstream = (H264BsReal_8u16s *)curr_slice->m_pbitstream;
    Ipp32s i;
    Status status = UMC_OK;
    //Ipp32s MBYAdjust = (core_enc->m_field_index) ? core_enc->m_HeightInMBs : 0;
    bool doPadding = /*(core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC) || (core_enc->m_info.treat_B_as_reference && core_enc->m_pCurrentFrame->m_RefPic)*/ false;
    bool doDeblocking = doPadding && (curr_slice->m_disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF);
    Ipp32s nThreads = core_enc->m_info.numThreads;
    Ipp32s m_first_mb_in_slice = curr_slice->m_first_mb_in_slice + core_enc->m_HeightInMBs * core_enc->m_WidthInMBs * core_enc->m_field_index;
    Ipp32s m_last_mb_in_slice = curr_slice->m_last_mb_in_slice + core_enc->m_HeightInMBs * core_enc->m_WidthInMBs * core_enc->m_field_index;
    Ipp32s MBYAdjust = 0;
    if (core_enc->m_field_index)
        MBYAdjust = core_enc->m_HeightInMBs;

    curr_slice->m_InitialOffset = core_enc->m_InitialOffsets[core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]];
    curr_slice->m_is_cur_mb_field = core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;
    curr_slice->m_is_cur_mb_bottom_field = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index] == 1;
    // curr_slice->m_use_transform_for_intra_decision = core_enc->m_info.use_transform_for_intra_decision ? (curr_slice->m_slice_type == INTRASLICE) : false;
    curr_slice->m_use_transform_for_intra_decision = 1;
    // curr_slice->m_Intra_MB_Counter = 0;
    // curr_slice->m_MB_Counter = 0;
    curr_slice->m_uSkipRun = 0;
    curr_slice->m_slice_alpha_c0_offset = (Ipp8s)core_enc->m_info.deblocking_filter_alpha;
    curr_slice->m_slice_beta_offset = (Ipp8s)core_enc->m_info.deblocking_filter_beta;
    curr_slice->m_disable_deblocking_filter_idc = core_enc->m_info.deblocking_filter_idc;
    curr_slice->m_cabac_init_idc = core_enc->m_info.cabac_init_idc;
    curr_slice->m_iLastXmittedQP = core_enc->m_PicParamSet->pic_init_qp + curr_slice->m_slice_qp_delta;
    for (i = 0; i < nThreads; i ++) {
#ifdef H264_NEW_THREADING
        core_enc->m_Slices_MBT[i].m_MB_Counter = 0;
#endif // H264_NEW_THREADING
        core_enc->m_Slices_MBT[i].m_slice_qp_delta = curr_slice->m_slice_qp_delta;
        core_enc->m_Slices_MBT[i].m_slice_number = curr_slice->m_slice_number;
        core_enc->m_Slices_MBT[i].m_slice_type = curr_slice->m_slice_type;
        core_enc->m_Slices_MBT[i].m_InitialOffset = curr_slice->m_InitialOffset;
        core_enc->m_Slices_MBT[i].m_is_cur_mb_field = curr_slice->m_is_cur_mb_field;
        core_enc->m_Slices_MBT[i].m_is_cur_mb_bottom_field = curr_slice->m_is_cur_mb_bottom_field;
        core_enc->m_Slices_MBT[i].m_use_transform_for_intra_decision = curr_slice->m_use_transform_for_intra_decision;
        core_enc->m_Slices_MBT[i].m_slice_alpha_c0_offset = curr_slice->m_slice_alpha_c0_offset;
        core_enc->m_Slices_MBT[i].m_slice_beta_offset = curr_slice->m_slice_beta_offset;
        core_enc->m_Slices_MBT[i].m_disable_deblocking_filter_idc = curr_slice->m_disable_deblocking_filter_idc;
        core_enc->m_Slices_MBT[i].m_cabac_init_idc = curr_slice->m_cabac_init_idc;
        core_enc->m_Slices_MBT[i].m_iLastXmittedQP = curr_slice->m_iLastXmittedQP;

        // SVC extension
        core_enc->m_Slices_MBT[i].m_constrained_intra_resampling_flag = curr_slice->m_constrained_intra_resampling_flag;
        core_enc->m_Slices_MBT[i].m_ref_layer_chroma_phase_x_plus1 = curr_slice->m_ref_layer_chroma_phase_x_plus1;
        core_enc->m_Slices_MBT[i].m_ref_layer_chroma_phase_y_plus1 = curr_slice->m_ref_layer_chroma_phase_y_plus1;
        core_enc->m_Slices_MBT[i].m_slice_skip_flag = curr_slice->m_slice_skip_flag;
        core_enc->m_Slices_MBT[i].m_adaptive_prediction_flag = curr_slice->m_adaptive_prediction_flag;
        core_enc->m_Slices_MBT[i].m_default_base_mode_flag = curr_slice->m_default_base_mode_flag;
        core_enc->m_Slices_MBT[i].m_adaptive_motion_prediction_flag = curr_slice->m_adaptive_motion_prediction_flag;
        core_enc->m_Slices_MBT[i].m_default_motion_prediction_flag = curr_slice->m_default_motion_prediction_flag;
        core_enc->m_Slices_MBT[i].m_adaptive_residual_prediction_flag = curr_slice->m_adaptive_residual_prediction_flag;
        core_enc->m_Slices_MBT[i].m_default_residual_prediction_flag = curr_slice->m_default_residual_prediction_flag;
        core_enc->m_Slices_MBT[i].m_tcoeff_level_prediction_flag = curr_slice->m_tcoeff_level_prediction_flag;
        core_enc->m_Slices_MBT[i].m_scan_idx_start = curr_slice->m_scan_idx_start;
        core_enc->m_Slices_MBT[i].m_scan_idx_end = curr_slice->m_scan_idx_end;
        core_enc->m_Slices_MBT[i].m_base_pred_weight_table_flag = curr_slice->m_base_pred_weight_table_flag;
        core_enc->m_Slices_MBT[i].m_ref_layer_dq_id = curr_slice->m_ref_layer_dq_id;
        core_enc->m_Slices_MBT[i].m_disable_inter_layer_deblocking_filter_idc = curr_slice->m_disable_inter_layer_deblocking_filter_idc;
        core_enc->m_Slices_MBT[i].m_inter_layer_slice_alpha_c0_offset_div2 = curr_slice->m_inter_layer_slice_alpha_c0_offset_div2;
        core_enc->m_Slices_MBT[i].m_inter_layer_slice_beta_offset_div2 = curr_slice->m_inter_layer_slice_beta_offset_div2;
        core_enc->m_Slices_MBT[i].m_scaled_ref_layer_left_offset = curr_slice->m_scaled_ref_layer_left_offset;
        core_enc->m_Slices_MBT[i].m_scaled_ref_layer_top_offset = curr_slice->m_scaled_ref_layer_top_offset;
        core_enc->m_Slices_MBT[i].m_scaled_ref_layer_right_offset = curr_slice->m_scaled_ref_layer_right_offset;
        core_enc->m_Slices_MBT[i].m_scaled_ref_layer_bottom_offset = curr_slice->m_scaled_ref_layer_bottom_offset;
        core_enc->m_Slices_MBT[i].m_num_mbs_in_slice_minus1 = curr_slice->m_num_mbs_in_slice_minus1;

        if (curr_slice->m_slice_type != INTRASLICE) {
            core_enc->m_Slices_MBT[i].m_NumRefsInL0List = curr_slice->m_NumRefsInL0List;
            core_enc->m_Slices_MBT[i].m_NumRefsInL1List = curr_slice->m_NumRefsInL1List;
            core_enc->m_Slices_MBT[i].m_NumRefsInLTList = curr_slice->m_NumRefsInLTList;
            core_enc->m_Slices_MBT[i].num_ref_idx_active_override_flag = curr_slice->num_ref_idx_active_override_flag;
            core_enc->m_Slices_MBT[i].num_ref_idx_l0_active = curr_slice->num_ref_idx_l0_active;
            core_enc->m_Slices_MBT[i].num_ref_idx_l1_active = curr_slice->num_ref_idx_l1_active;
            core_enc->m_Slices_MBT[i].m_use_intra_mbs_only = curr_slice->m_use_intra_mbs_only;
            core_enc->m_Slices_MBT[i].m_TempRefPicList[0][0] = curr_slice->m_TempRefPicList[0][0];
            core_enc->m_Slices_MBT[i].m_TempRefPicList[0][1] = curr_slice->m_TempRefPicList[0][1];
            core_enc->m_Slices_MBT[i].m_TempRefPicList[1][0] = curr_slice->m_TempRefPicList[1][0];
            core_enc->m_Slices_MBT[i].m_TempRefPicList[1][1] = curr_slice->m_TempRefPicList[1][1];
            if (curr_slice->m_slice_type != PREDSLICE) {
                MFX_INTERNAL_CPY(core_enc->m_Slices_MBT[i].MapColMBToList0, curr_slice->MapColMBToList0, sizeof(curr_slice->MapColMBToList0));
                MFX_INTERNAL_CPY(core_enc->m_Slices_MBT[i].DistScaleFactor, curr_slice->DistScaleFactor, sizeof(curr_slice->DistScaleFactor));
                MFX_INTERNAL_CPY(core_enc->m_Slices_MBT[i].DistScaleFactorMV, curr_slice->DistScaleFactorMV, sizeof(curr_slice->DistScaleFactorMV));
            }
        }
        core_enc->m_Slices_MBT[i].m_Intra_MB_Counter = 0;
    }
    curr_slice->m_MB_Counter = 0;
    H264BsReal_PutSliceHeader_8u16s(
        pBitstream,
        core_enc->m_SliceHeader,
        *core_enc->m_PicParamSet,
        *core_enc->m_SeqParamSet,
        core_enc->m_PicClass,
        curr_slice,
        core_enc->m_DecRefPicMarkingInfo[core_enc->m_field_index], core_enc->m_ReorderInfoL0[core_enc->m_field_index], core_enc->m_ReorderInfoL1[core_enc->m_field_index],
        &core_enc->m_svc_layer.svc_ext);

    if (core_enc->m_info.entropy_coding_mode) {
        if (curr_slice->m_slice_type == INTRASLICE)
            H264BsReal_InitializeContextVariablesIntra_CABAC_8u16s(pBitstream, curr_slice->m_iLastXmittedQP);
        else
            H264BsReal_InitializeContextVariablesInter_CABAC_8u16s(pBitstream, curr_slice->m_iLastXmittedQP, curr_slice->m_cabac_init_idc);
    }
    for (i = 0; i < core_enc->m_HeightInMBs * ((core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE) + 1); i ++)
        core_enc->mbReady_MBT[i] = -1;

    Ipp32s row = m_first_mb_in_slice / core_enc->m_WidthInMBs;
    Ipp32s first_slice_row = row;

    for (i = 0; i < nThreads; i ++) {
        if (core_enc->m_info.entropy_coding_mode)
            memcpy_s(core_enc->m_Slices_MBT[i].m_pbitstream->context_array, CABAC_CONTEXT_ARRAY_LEN * sizeof(CABAC_CONTEXT), curr_slice->m_pbitstream->context_array, CABAC_CONTEXT_ARRAY_LEN * sizeof(CABAC_CONTEXT));
#ifdef H264_NEW_THREADING
        m_taskParams.threads_data[i].mbt_data.core_enc = core_enc;
        m_taskParams.threads_data[i].mbt_data.curr_slice = curr_slice;
        m_taskParams.threads_data[i].mbt_data.threadNum = i;
        m_taskParams.threads_data[i].mbt_data.incRow = &row;
#else
        threadSpec[i].mbt_data.core_enc = core_enc;
        threadSpec[i].mbt_data.curr_slice = curr_slice;
        threadSpec[i].mbt_data.threadNum = i;
        threadSpec[i].mbt_data.incRow = &row;

        if (i == nThreads - 1)
            ThreadCallBackVM_MBT(threadSpec[nThreads - 1]);
        else
            vm_event_signal(&threads[i]->start_event);
#endif // H264_NEW_THREADING
    }

#ifdef H264_NEW_THREADING
    m_taskParams.cabac_encoding_in_progress = 0;
    // Start parallel region
    m_taskParams.single_thread_done = true;
    // Signal scheduler that all busy threads should be unleashed
    m_core->INeedMoreThreadsInside(this);
    status = ThreadCallBackVM_MBT(m_taskParams.threads_data[0]);
    curr_slice->m_MB_Counter = core_enc->m_Slices_MBT[0].m_MB_Counter;
#else
    curr_slice->m_MB_Counter = core_enc->m_Slices_MBT[nThreads - 1].m_MB_Counter;
#endif // H264_NEW_THREADING

#ifdef H264_NEW_THREADING
    // Disable parallel task execution
    m_taskParams.single_thread_done = false;

    bool do_cabac_here = false;
    if (!vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&m_taskParams.cabac_encoding_in_progress), 1, 0))
        do_cabac_here = true;

    // Wait for other threads to finish encoding their last macroblock row
    while(m_taskParams.parallel_executing_threads > 0)
        vm_event_wait(&m_taskParams.parallel_region_done);

    for (i = 1; i < nThreads; i++)
        curr_slice->m_MB_Counter += core_enc->m_Slices_MBT[i].m_MB_Counter;
#else
    for (i = 0; i < nThreads - 1; i ++)
    {
        vm_event_wait(&threads[i]->stop_event);
        curr_slice->m_MB_Counter += core_enc->m_Slices_MBT[i].m_MB_Counter;
    }
#endif // H264_NEW_THREADING

#ifdef USE_NV12
    Ipp32s ch_X = 16, ch_Y = 8, ch_P = LUMA_PADDING;
#else // USE_NV12
    Ipp32s ch_X = 8, ch_Y = 8, ch_P = LUMA_PADDING >> 1;
#endif // USE_NV12
    if (core_enc->m_PicParamSet->chroma_format_idc == 2) {
        ch_Y = 16;
        ch_P = LUMA_PADDING;
    } else if (core_enc->m_PicParamSet->chroma_format_idc == 3) {
        ch_X = ch_Y = 16;
        ch_P = LUMA_PADDING;
    }

    Ipp32s last_slice_row = m_last_mb_in_slice / core_enc->m_WidthInMBs;
    if ((core_enc->m_PicParamSet->entropy_coding_mode == 1) &&
#ifdef H264_NEW_THREADING
        do_cabac_here)
#else
        (nThreads <= 2))
#endif
    {
        Ipp32s tRow, tCol;
        Status sts = UMC_OK;
        H264CurrentMacroblockDescriptor_8u16s &cur_mb = curr_slice->m_cur_mb;
        for (tRow = first_slice_row; tRow <= last_slice_row; tRow ++) {
            for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++) {
                Ipp32s uMB = tRow * core_enc->m_WidthInMBs + tCol;
                VM_ASSERT(uMB <= m_last_mb_in_slice);
                cur_mb.uMB = uMB;
                cur_mb.uMBx = tCol;
                cur_mb.uMBy = tRow - MBYAdjust;
#ifndef H264_INTRA_REFRESH
                cur_mb.lambda = lambda_sq[curr_slice->m_iLastXmittedQP];
#endif // H264_INTRA_REFRESH
                cur_mb.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
                cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << curr_slice->m_is_cur_mb_field;
                H264CoreEncoder_UpdateCurrentMBInfo_8u16s(state, curr_slice);
                cur_mb.cabac_data = &core_enc->gd_MBT[uMB].bc;
                cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
#ifdef H264_INTRA_REFRESH
                cur_mb.lambda = lambda_sq[cur_mb.lumaQP];
#endif // H264_INTRA_REFRESH
                cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);

#ifdef H264_RECODE_PCM
                H264BsReal_StoreIppCABACState_8u16s(pBitstream);
#endif //H264_RECODE_PCM

                ////if (rd_frame_num == 1) {
                ////    printf("r frame = %d, did = %d, qid = %d, x = %d, y = %d\n", rd_frame_num,
                ////        core_enc->m_svc_layer.svc_ext.dependency_id,
                ////        core_enc->m_svc_layer.svc_ext.quality_id,
                ////        cur_mb.uMBx, cur_mb.uMBy); /* RD */
                ////    fflush(stdout);
                ////}

#ifdef H264_RECODE_PCM
                if(core_enc->m_mbPCM[uMB])
                {
                    H264CoreEncoder_Put_MBHeader_Real_8u16s(state, curr_slice);   // PCM values are written in the MB Header.
                } else
#endif //H264_RECODE_PCM
                    sts = H264CoreEncoder_Put_MB_Real_8u16s(state, curr_slice);
                if (sts != UMC_OK) {
                    status = sts;
                    break;
                }
                H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(pBitstream, (uMB == m_first_mb_in_slice + (Ipp32s)curr_slice->m_MB_Counter - 1) || (core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB + 1].slice_id != curr_slice->m_slice_number));
                H264CoreEncoder_ReconstuctCBP_8u16s(&cur_mb);

#ifdef H264_RECODE_PCM
                if (   core_enc->m_PicParamSet->entropy_coding_mode 
                    && !core_enc->m_mbPCM[uMB]
                    && 3200 < H264BsReal_GetNotStoredStreamSize_CABAC_8u16s(pBitstream))
                {
                    core_enc->m_nPCM ++;
                    core_enc->m_mbPCM[uMB] = 1;
                }
#endif //H264_RECODE_PCM
            }
        }
    }
    Ipp32s last_row_MBs = (m_last_mb_in_slice + 1) % core_enc->m_WidthInMBs;
    last_row_MBs = last_row_MBs == 0 ? core_enc->m_WidthInMBs : last_row_MBs;
    Ipp32s last_row_length = MIN(last_row_MBs, m_last_mb_in_slice + 1 - m_first_mb_in_slice);
    Ipp32s last_row_start_mb = MAX(m_first_mb_in_slice, m_last_mb_in_slice + 1 - last_row_length);
    if (doDeblocking && (core_enc->m_info.num_slices - 1 == curr_slice->m_slice_number))
        H264CoreEncoder_DeblockSlice_8u16s(core_enc, curr_slice, last_row_start_mb, last_row_length);
#ifndef NO_PADDING
    if (doPadding) {
        H264Slice_8u16s *cur_s = curr_slice;
        for (Ipp32s pRow = MAX(first_slice_row, last_slice_row - 2); pRow <= last_slice_row; pRow ++) {
            for (Ipp32s pCol = 0; pCol < core_enc->m_WidthInMBs; pCol ++) {
                Ipp32s padFlag = 0;
                if (pCol == 0)
                    padFlag = 1;
                if (pCol == core_enc->m_WidthInMBs - 1)
                    padFlag = 2;
                if (pRow == 0)
                    padFlag |= 4;
                if (pRow == core_enc->m_HeightInMBs - 1)
                    padFlag |= 8;
                Ipp32u pMB = pRow * core_enc->m_WidthInMBs + pCol;
                if (padFlag) {
                    PadMB_Luma_8u16s(core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[pMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], core_enc->m_pReconstructFrame->m_pitchPixels, padFlag, LUMA_PADDING);
                    if (core_enc->m_PicParamSet->chroma_format_idc)
#ifdef USE_NV12
                        PadMB_Chroma_8u16s(core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], core_enc->m_pReconstructFrame->m_pitchPixels, ch_X, ch_Y, padFlag, ch_P);
#else // USE_NV12
                        H264ENC_MAKE_NAME(PadMB_Chroma)(core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], core_enc->m_pReconstructFrame->m_pVPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], pitchPixels, ch_X, ch_Y, padFlag, ch_P);
#endif // USE_NV12
                }
            }
        }
#ifdef FRAME_INTERPOLATION
        if ((core_enc->m_Analyse & ANALYSE_ME_SUBPEL) && (core_enc->m_info.num_slices - 1 == curr_slice->m_slice_number))
        {
            Ipp32s pitchPixels = core_enc->m_pReconstructFrame->m_pitchPixels;
            Ipp32s planeSize = core_enc->m_pReconstructFrame->m_PlaneSize;
            Ipp32s bitDepth = core_enc->m_PicParamSet->bit_depth_luma;

            for (Ipp32s pRow = MAX(first_slice_row, last_slice_row - 3); pRow <= last_slice_row; pRow ++) {
                for (Ipp32s pCol = 0; pCol < core_enc->m_WidthInMBs; pCol ++) {
                    Ipp32u pMB = pRow * core_enc->m_WidthInMBs + pCol;
                    Ipp8u *pY = core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[pMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field];
                    InterpolateHP_MB_8u16s(pY, pitchPixels, planeSize, bitDepth);
                    Ipp32s padFlag = 0;
                    if (pCol == 0)
                        padFlag = 1;
                    if (pCol == core_enc->m_WidthInMBs - 1)
                        padFlag = 2;
                    if (pRow == 0)
                        padFlag |= 4;
                    if (pRow == core_enc->m_HeightInMBs - 1)
                        padFlag |= 8;
                    if (padFlag & 1)
                        // left
                        InterpolateHP_MB_8u16s(pY - 16, pitchPixels, planeSize, bitDepth);
                    if (padFlag & 2)
                        // right
                        InterpolateHP_MB_8u16s(pY + 16, pitchPixels, planeSize, bitDepth);
                    if (padFlag & 4) {
                        // top
                        InterpolateHP_MB_8u16s(pY - 16 * pitchPixels, pitchPixels, planeSize, bitDepth);
                        if (padFlag & 1)
                            // top-left
                            InterpolateHP_MB_8u16s(pY - 16 * pitchPixels - 16, pitchPixels, planeSize, bitDepth);
                        else if (padFlag & 2)
                            // top-right
                            InterpolateHP_MB_8u16s(pY - 16 * pitchPixels + 16, pitchPixels, planeSize, bitDepth);
                    }
                    if (padFlag & 8) {
                        // bottom
                        InterpolateHP_MB_8u16s(pY + 16 * pitchPixels, pitchPixels, planeSize, bitDepth);
                        if (padFlag & 1)
                            // bottom-left
                            InterpolateHP_MB_8u16s(pY + 16 * pitchPixels - 16, pitchPixels, planeSize, bitDepth);
                        else if (padFlag & 2)
                            // bottom-right
                            InterpolateHP_MB_8u16s(pY + 16 * pitchPixels + 16, pitchPixels, planeSize, bitDepth);
                    }
                }
            }
            Ipp8u *pB = core_enc->m_pReconstructFrame->m_pYPlane + planeSize;
            Ipp8u *pH = pB + planeSize;
            Ipp8u *pJ = pH + planeSize;
            ExpandPlane_8u16s(pB - 16 * pitchPixels - 16, (core_enc->m_WidthInMBs + 2) * 16, (core_enc->m_HeightInMBs + 2) * 16, pitchPixels, LUMA_PADDING - 16);
            ExpandPlane_8u16s(pH - 16 * pitchPixels - 16, (core_enc->m_WidthInMBs + 2) * 16, (core_enc->m_HeightInMBs + 2) * 16, pitchPixels, LUMA_PADDING - 16);
            ExpandPlane_8u16s(pJ - 16 * pitchPixels - 16, (core_enc->m_WidthInMBs + 2) * 16, (core_enc->m_HeightInMBs + 2) * 16, pitchPixels, LUMA_PADDING - 16);
        }
#endif // FRAME_INTERPOLATION
    }
#endif // !NO_PADDING
    for (i = 0; i < core_enc->m_info.numThreads; i ++)
        curr_slice->m_Intra_MB_Counter += core_enc->m_Slices_MBT[i].m_Intra_MB_Counter;
    if (core_enc->m_PicParamSet->entropy_coding_mode)
        H264BsReal_TerminateEncode_CABAC_8u16s(pBitstream);
    else
        H264BsBase_WriteTrailingBits(&pBitstream->m_base);

    return status;
}
#endif // VM_MB_THREADING_H264
#endif // VM_SLICE_THREADING_H264

static mfxStatus h264enc_ConvertStatus(UMC::Status status)
{
    switch( status ){
        case UMC::UMC_ERR_NULL_PTR:
            return MFX_ERR_NULL_PTR;
        case UMC::UMC_ERR_NOT_ENOUGH_DATA:
            return MFX_ERR_MORE_DATA;
        case UMC::UMC_ERR_NOT_ENOUGH_BUFFER:
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        case UMC::UMC_ERR_ALLOC:
            return MFX_ERR_MEMORY_ALLOC;
        case UMC::UMC_OK:
            return MFX_ERR_NONE;
        default:
            return MFX_ERR_UNSUPPORTED;
    }
}


inline Ipp32u h264enc_ConvertBitrate(mfxU16 TargetKbps)
{
    return (TargetKbps * 1000);
}

//////////////////////////////////////////////////////////////////////////

MFXVideoENCODEH264::MFXVideoENCODEH264(VideoCORE *core, mfxStatus *stat)
: VideoENCODE(),
  m_taskParams(),
  m_core(core),
  m_allocator_id(NULL),
  m_allocator(NULL),
  //m_frameCountSync(0),
  //m_totalBits(0),
  //m_encodedFrames(0),
  m_fieldOutput(false),
  m_fieldOutputState(0),
  m_fieldOutputStatus(MFX_ERR_NONE),
  m_initValues(),
  //m_Initialized(false),
  //m_putEOSeq(false),
  //m_putEOStream(false),
  //m_putTimingSEI(false),
  //m_putDecRefPicMarkinfRepSEI(false),
  //m_resetRefList(false),
  m_separateSEI(false),
  //m_useAuxInput(false),
  m_useSysOpaq(false),
  m_useVideoOpaq(false),
  m_isOpaque(false),
  m_ConstQP(0)
{
    Status res = UMC_OK;
    ippStaticInit();
#ifdef VM_SLICE_THREADING_H264
    threadsAllocated = 0;
    threadSpec = 0;
    threads = 0;
#endif // VM_SLICE_THREADING_H264
    memset(&m_extBitstream, 0, sizeof(mfxBitstream));
    memset(&m_temporalLayers, 0, sizeof(AVCTemporalLayers));
    *stat = MFX_ERR_NONE;

    memset(&m_base, 0, sizeof(m_base));
    m_layer = &m_base;
    for(int i=0; i<MAX_DEP_LAYERS; i++) m_svc_layers[i]=0;
    m_svc_count = 0;
    m_selectedPOC = POC_UNSPECIFIED;
    m_maxDid = 0;

#ifdef USE_TEMPTEMP
    m_refctrl = 0;
    m_temptemplen = 0;
    m_usetemptemp = 0;
#endif // USE_TEMPTEMP

    memset(&m_svc_ScalabilityInfoSEI, 0, sizeof(m_svc_ScalabilityInfoSEI));
    allocated_tmpBuf = 0;
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
    H264ENC_CALL_NEW(res, H264CoreEncoder, m_base.enc);
#undef H264ENC_MAKE_NAME
    if (res != UMC_OK) *stat = MFX_ERR_NOT_INITIALIZED;
    m_tmpBuf0 = 0;
    m_tmpBuf1 = 0;
    m_tmpBuf2 = 0;
    m_layerSync = 0;
    m_internalFrameOrders = 0;
}

MFXVideoENCODEH264::~MFXVideoENCODEH264()
{
    Close();
    if( m_base.enc != NULL ){
        H264CoreEncoder_Destroy_8u16s(m_base.enc);
        H264_Free(m_base.enc);
        m_base.enc = NULL;
    }
    for(int i=0; i<MAX_DEP_LAYERS; i++) {
        if (!m_svc_layers[i]) continue;
        if (m_svc_layers[i]->enc) {
            H264CoreEncoder_Destroy_8u16s(m_svc_layers[i]->enc);
            H264_Free(m_svc_layers[i]->enc);
        }
        H264_Free(m_svc_layers[i]);
        m_svc_layers[i] = 0;
    }
}

#ifdef H264_NEW_THREADING
mfxStatus MFXVideoENCODEH264::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus status = EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams);
    h264Layer* layer = m_layerSync;

    pEntryPoint->pRoutine = TaskRoutine;
    pEntryPoint->pCompleteProc = TaskCompleteProc;
    pEntryPoint->pState = this;
    pEntryPoint->requiredNumThreads = layer->enc->m_info.numThreads;
    pEntryPoint->pRoutineName = "EncodeAVC";

    mfxFrameSurface1 *realSurface = GetOriginalSurface(surface);

    if (surface && !realSurface)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (MFX_ERR_NONE == status || static_cast<int>(MFX_ERR_MORE_DATA_RUN_TASK) == static_cast<int>(status) || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status || MFX_ERR_MORE_BITSTREAM == status)
    {
        // lock surface. If input surface is opaque core will lock both opaque and associated realSurface
        if (realSurface) {
            // lock surface only for 1st call for field-based output
            if (!m_fieldOutput || (m_fieldOutput && m_fieldOutputState != FIELD_OUTPUT_SECOND_FIELD))
                m_core->IncreaseReference(&(realSurface->Data));
        }

        EncodeTaskInputParams *m_pTaskInputParams = (EncodeTaskInputParams*)H264_Malloc(sizeof(EncodeTaskInputParams));
        // MFX_ERR_MORE_DATA_RUN_TASK means that frame will be buffered and will be encoded later. Output bitstream isn't required for this task
        m_pTaskInputParams->bs = (static_cast<int>(status) == static_cast<int>(MFX_ERR_MORE_DATA_RUN_TASK)) ? 0 : bs;
        m_pTaskInputParams->ctrl = ctrl;
        m_pTaskInputParams->surface = surface;
        m_pTaskInputParams->picState = m_fieldOutput ? m_fieldOutputState : 0; // field-based output: tell async part is it called for 1st or 2nd field
        pEntryPoint->pParam = m_pTaskInputParams;
    }

    return status;
}

Status MFXVideoENCODEH264::EncodeNextSliceTask(mfxI32 thread_number, mfxI32 *slice_number)
{
    mfxI32 slice = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_taskParams.slice_counter)) - 1;
    *slice_number = slice;

    if (slice < m_taskParams.slice_number_limit)
    {
        m_taskParams.threads_data[thread_number].slice = slice;
        return ThreadedSliceCompress(thread_number);
    }
    else
        return UMC_OK;
}

mfxStatus MFXVideoENCODEH264::TaskRoutine(void *pState, void *pParam, mfxU32 /*threadNumber*/, mfxU32 /*callNumber*/)
{
    //H264ENC_UNREFERENCED_PARAMETER(threadNumber);
    //H264ENC_UNREFERENCED_PARAMETER(callNumber);
//    TAL_SCOPED_TASK();
    if (pState == NULL || pParam == NULL)
        return MFX_ERR_NULL_PTR;

    MFXVideoENCODEH264 *th = static_cast<MFXVideoENCODEH264 *>(pState);

    EncodeTaskInputParams *pTaskParams = (EncodeTaskInputParams*)pParam;

    if (th->m_taskParams.parallel_encoding_finished)
        return MFX_TASK_DONE;

    mfxI32 single_thread_selected = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.single_thread_selected), 1, 0);
    if (!single_thread_selected)
    {
        // This task performs all single threaded regions
        mfxEncodeInternalParams intParams, *pIntParams = 0;
        if (th->m_fieldOutput) {
            memset(&intParams, 0, sizeof(mfxEncodeInternalParams));
            intParams.InternalFlags = pTaskParams->picState;
            pIntParams = &intParams;
        }
        mfxStatus status = th->EncodeFrame(pTaskParams->ctrl, pIntParams, pTaskParams->surface, pTaskParams->bs);
        th->m_taskParams.parallel_encoding_finished = true;
        H264_Free(pParam);

        return status;
    }
    else
    {
        if (th->m_taskParams.single_thread_done)
        {
            // Here we get only when thread that performs single thread work sent event
            // In this case m_taskParams.thread_number is set to 0, this is the number of single thread task
            // All other tasks receive their number > 0
            mfxI32 thread_number = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.thread_number));

            // Some thread finished its work and was called again, but there is no work left for it since
            // if it finished, the only work remaining is being done by other currently working threads. No
            // work is possible so thread goes away waiting for maybe another parallel region
            if (thread_number >= th->m_layer->enc->m_info.numThreads)
                return MFX_TASK_BUSY;

            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));
            Status umc_status = UMC_OK;

            if (!th->m_layer->enc->useMBT)
            {
                mfxI32 slice_number;
                do
                {
                    umc_status = th->EncodeNextSliceTask(thread_number, &slice_number);
                }
                while (UMC_OK == umc_status && slice_number < th->m_taskParams.slice_number_limit);
            }
            else
                umc_status = th->ThreadCallBackVM_MBT(th->m_taskParams.threads_data[thread_number]);

            vm_interlocked_dec32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));

            // We're done encoding our share of slices or macroblock rows in this task, let single thread to continue
            vm_event_signal(&th->m_taskParams.parallel_region_done);

            if (UMC_OK == umc_status)
                return MFX_TASK_WORKING;
            else
                return h264enc_ConvertStatus(umc_status);
        }
        else
            return MFX_TASK_BUSY;
    }
}

mfxStatus MFXVideoENCODEH264::TaskCompleteProc(void *pState, void* /*pParam*/, mfxStatus /*taskRes*/)
{
    //H264ENC_UNREFERENCED_PARAMETER(pParam);
    //H264ENC_UNREFERENCED_PARAMETER(taskRes);
    if (pState == NULL)
        return MFX_ERR_NULL_PTR;

    MFXVideoENCODEH264 *th = static_cast<MFXVideoENCODEH264 *>(pState);

    th->m_taskParams.single_thread_selected = 0;
    th->m_taskParams.thread_number = 0;
    th->m_taskParams.parallel_encoding_finished = false;
    for (mfxI32 i = 0; i < th->m_layer->enc->m_info.numThreads; i++)
    {
        th->m_taskParams.threads_data[i].core_enc = NULL;
        th->m_taskParams.threads_data[i].state = NULL;
        th->m_taskParams.threads_data[i].slice = 0;
        th->m_taskParams.threads_data[i].slice_qp_delta_default = 0;
        th->m_taskParams.threads_data[i].default_slice_type = INTRASLICE;
    }
    th->m_taskParams.single_thread_done = false;

    return MFX_TASK_DONE;
}
#else
#ifdef VM_SLICE_THREADING_H264
Ipp32u VM_THREAD_CALLCONVENTION MFXVideoENCODEH264::ThreadWorkingRoutine(void* ptr)
{
  threadInfoH264* th = (threadInfoH264*)ptr;

  for (;;) {
    // wait for start
    if (VM_OK != vm_event_wait(&th->start_event)) {
      vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("ThreadWorkingRoutine result wait start_event\n"));
    }

    if (VM_TIMEOUT != vm_event_timed_wait(&th->quit_event, 0)) {
      break;
    }

    if (th->m_lpOwner->m_layer->enc->useMBT)
        th->m_lpOwner->ThreadCallBackVM_MBT(th->m_lpOwner->threadSpec[th->numTh - 1]);
    else
        th->m_lpOwner->ThreadedSliceCompress(th->numTh);

    if (VM_OK != vm_event_signal(&th->stop_event)) {
      vm_debug_trace(VM_DEBUG_PROGRESS, VM_STRING("ThreadWorkingRoutine result signal stop_event\n"));
    }

    //vm_time_sleep(0);
  }

  return 0;
}
#endif // VM_SLICE_THREADING_H264
#endif // H264_NEW_THREADING

mfxFrameSurface1* MFXVideoENCODEH264::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_isOpaque)
        return m_core->GetNativeSurface(surface);
    return surface;
}

// function MFXVideoENCODEH264::AnalyzePicStruct checks combination of (init PicStruct, runtime PicStruct) and converts this combination to internal format.
// Returns: parameter
//                      picStructForEncoding - internal format that used for encoding, pointer can be NULL
//          statuses
//                      MFX_ERR_NONE                       combination is applicable for encoding
//                      MFX_WRN_INCOMPATIBLE_VIDEO_PARAM   warning, combination isn't applicable, default PicStruct is used for encoding
//                      MFX_ERR_UNDEFINED_BEHAVIOR         error, combination isn't applicable, default PicStruct for encoding can't be choosen
//                      MFX_ERR_INVALID_VIDEO_PARAM        error, PicStruct was initialized incorrectly
mfxStatus MFXVideoENCODEH264::AnalyzePicStruct(h264Layer *layer, const mfxU16 picStructInSurface, mfxU16 *picStructForEncoding)
{
    H264CoreEncoder_8u16s* cur_enc = layer->enc;
    mfxVideoParam *vp = &layer->m_mfxVideoParam;

    mfxStatus st = MFX_ERR_NONE;
    mfxU16 picStruct = MFX_PICSTRUCT_UNKNOWN;
    if (vp->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_UNKNOWN && picStructInSurface == MFX_PICSTRUCT_UNKNOWN)
        st = MFX_ERR_UNDEFINED_BEHAVIOR;
    else {
        switch (vp->mfx.FrameInfo.PicStruct) {
            case MFX_PICSTRUCT_PROGRESSIVE:
                switch (picStructInSurface) { // applicable PicStructs for init progressive
                    case MFX_PICSTRUCT_PROGRESSIVE: // coded and displayed as progressive frame
                    case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_DOUBLING:
                    case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_TRIPLING:
                        picStruct = picStructInSurface;
                        break;
                    default: // if picStructInSurface isn't applicable or MFX_PICSTRUCT_UNKNOWN
                        picStruct = MFX_PICSTRUCT_PROGRESSIVE; // default: coded and displayed as progressive frame
                        if (picStructInSurface != MFX_PICSTRUCT_UNKNOWN)
                            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                        break;
                }
                break;
            case MFX_PICSTRUCT_FIELD_TFF:
                if (m_fieldOutput)
                    picStruct = MFX_PICSTRUCT_FIELD_TFF;
                else {
                    switch (picStructInSurface) { // applicable PicStructs for init TFF
                        case MFX_PICSTRUCT_PROGRESSIVE:
                            picStruct = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF; // coded as frame, displayed as field TFF
                            break;
                        case MFX_PICSTRUCT_FIELD_TFF:
                            picStruct = picStructInSurface; // coded as two fields or MBAFF, displayed as fields TFF
                            break;
                        default: // if picStructInSurface isn't applicable or MFX_PICSTRUCT_UNKNOWN
                            picStruct = MFX_PICSTRUCT_FIELD_TFF; // default: coded as two fields or MBAFF, displayed as fields TFF
                            if (picStructInSurface != MFX_PICSTRUCT_UNKNOWN)
                                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                            break;
                    }
                }
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
                if (m_fieldOutput)
                    picStruct = MFX_PICSTRUCT_FIELD_BFF;
                else {
                    switch (picStructInSurface) { // applicable PicStructs for init BFF
                        case MFX_PICSTRUCT_PROGRESSIVE:
                            picStruct = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF; // coded as frame, displayed as field BFF
                            break;
                        case MFX_PICSTRUCT_FIELD_BFF:
                            picStruct = picStructInSurface; // coded as two fields or MBAFF, displayed as fields BFF
                            break;
                        default: // if picStructInSurface isn't applicable or MFX_PICSTRUCT_UNKNOWN
                            picStruct = MFX_PICSTRUCT_FIELD_BFF; // default: coded as two fields or MBAFF, displayed as fields BFF
                            if (picStructInSurface != MFX_PICSTRUCT_UNKNOWN)
                                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                            break;
                    }
                }
                break;
            case MFX_PICSTRUCT_UNKNOWN:
                if (m_fieldOutput) {
                    if (picStructInSurface & MFX_PICSTRUCT_FIELD_BFF)
                        picStruct = MFX_PICSTRUCT_FIELD_BFF;
                    else
                        picStruct = MFX_PICSTRUCT_FIELD_TFF;
                } else {
                    switch (picStructInSurface) { // applicable PicStructs for init UNKNOWN
                        case MFX_PICSTRUCT_PROGRESSIVE:
                            picStruct = MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF; // coded as frame, displayed as field TFF
                            break;
                        case MFX_PICSTRUCT_FIELD_TFF:
                        case MFX_PICSTRUCT_FIELD_BFF:
                        case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF:
                        case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF:
                        case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED:
                        case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_REPEATED:
                            picStruct = picStructInSurface;
                            break;
                        default: // if picStructInSurface isn't applicable
                            if (cur_enc->m_info.coding_type == 2) // MBAFF
                                picStruct = MFX_PICSTRUCT_PROGRESSIVE; // default: coded as MBAFF, displayed as progressive frame
                            else if (cur_enc->m_info.coding_type == 3) // PicAFF
                                picStruct = MFX_PICSTRUCT_FIELD_TFF; // default: coded as two fields
                            else {
                                picStruct = MFX_PICSTRUCT_PROGRESSIVE;
                                VM_ASSERT(false);
                            }
                            if (picStructInSurface != MFX_PICSTRUCT_UNKNOWN)
                                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                            break;
                    }
                }
                break;
            default:
                st = MFX_ERR_INVALID_VIDEO_PARAM;
                break;
        }
    }

    if (picStructForEncoding)
        *picStructForEncoding = picStruct;
    return st;
}

mfxStatus MFXVideoENCODEH264::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams * /*pInternalParams*/)

{
    mfxU32 minBufferSize;
    h264Layer *layer = &m_base;
    bool lastDid = true; // TODO can be deleted?
    //H264ENC_UNREFERENCED_PARAMETER(pInternalParams);

    // if input is svc layer - find proper encoder by Id
    mfxU16 internalPicStruct = MFX_PICSTRUCT_UNKNOWN;
    mfxU16 isFieldEncoding = 0;
    mfxStatus st = MFX_ERR_NONE;
    if (surface && m_svc_count > 0) {
        mfxU32 pos;
        mfxFrameInfo *fr = &surface->Info;
//        lastDid = false;
        for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
            if (!m_svc_layers[pos]) continue;
            struct SVCLayerParams *lsps = &m_svc_layers[pos]->enc->m_svc_layer;
            sNALUnitHeaderSVCExtension* hd = &lsps->svc_ext;
            if (fr->FrameId.DependencyId == hd->dependency_id)
            {
                layer = m_svc_layers[pos];
                if( !layer->m_Initialized ) return MFX_ERR_NOT_INITIALIZED;
//                if( fr->FrameId.TemporalId == layer->enc->TempIdMax )
//                    lastDid = true;
                break;
            }
        }
    }
    m_layerSync = layer; // pass to caller

    if( !m_base.m_Initialized ) return MFX_ERR_NOT_INITIALIZED;
    if (bs == 0) return MFX_ERR_NULL_PTR;
    /* RD: not clear with HRD ??????????????????? */

    //if (m_mfxVideoParam.mfx.EncodedOrder)
    //{
    //    if (ctrl == 0) return MFX_ERR_NULL_PTR;
    //    if (surface == 0) return MFX_ERR_NULL_PTR;
    //}

    //Check for enough bitstream buffer size
    if( bs->MaxLength < (bs->DataOffset + bs->DataLength))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (!surface && m_svc_count) {
        // For now find any buffered, avoid most checking
        // because can't define exactly which layer to be chosen in async. part
        mfxU32 pos;
        for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
            h264Layer *lr = m_svc_layers[pos];
            if (!lr) continue;
            if (lr->m_frameCountBufferedSync) {
                lr->m_frameCountBufferedSync--;
                lr->m_frameCountSync++;
                return MFX_ERR_NONE;
            }
        }
        // nothing buffered
        return MFX_ERR_MORE_DATA;
    }

    H264CoreEncoder_8u16s* cur_enc = layer->enc;
    mfxVideoParam *vp = &layer->m_mfxVideoParam;

    minBufferSize = layer->enc->requiredBsBufferSize;

    if(((bs->MaxLength - (bs->DataOffset + bs->DataLength) < minBufferSize) && cur_enc->m_info.rate_controls.method != H264_RCM_QUANT) || bs->MaxLength - bs->DataOffset == 0)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    if (bs->Data == 0)
        return MFX_ERR_NULL_PTR;

    // field-based output: just skip further check for 2nd field
    // if frame was buffered by 1st then expect new frame
    if (m_fieldOutput && m_fieldOutputState == FIELD_OUTPUT_FIRST_FIELD && m_fieldOutputStatus == MFX_ERR_MORE_BITSTREAM){
        m_fieldOutputState = FIELD_OUTPUT_SECOND_FIELD;
        m_fieldOutputStatus = MFX_ERR_NONE;
        return MFX_ERR_NONE; // field-based output: repeat return status from 1st field call
    }

    if (surface) { // check frame parameters
        if (surface->Info.Width < cur_enc->m_info.info.clip_info.width ||
            surface->Info.Height < cur_enc->m_info.info.clip_info.height ||
            //surface->Info.CropW > 0 && surface->Info.CropW != cur_enc->m_info.info.clip_info.width ||
            //surface->Info.CropH > 0 && surface->Info.CropH != cur_enc->m_info.info.clip_info.height ||
            surface->Info.ChromaFormat != vp->mfx.FrameInfo.ChromaFormat)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        // check PicStruct in surface
        st = AnalyzePicStruct(layer, surface->Info.PicStruct, &internalPicStruct);

        if (st < MFX_ERR_NONE) // error occured
            return st;

        //surface size > coding size - valid case, no warning required
        /*if (surface->Info.Width != cur_enc->m_info.info.clip_info.width ||
            surface->Info.Height != cur_enc->m_info.info.clip_info.height)
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;*/

        isFieldEncoding = (cur_enc->m_info.coding_type == CODINGTYPE_PICAFF || cur_enc->m_info.coding_type == CODINGTYPE_INTERLACE) && !(internalPicStruct & MFX_PICSTRUCT_PROGRESSIVE);

        // field-based output: PicStruct should represent interlaced encoding
        if (m_fieldOutput && !isFieldEncoding)
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (surface->Data.Y) {
            if ((surface->Info.FourCC == MFX_FOURCC_YV12 && (!surface->Data.U || !surface->Data.V)) ||
                (surface->Info.FourCC == MFX_FOURCC_NV12 && !surface->Data.UV))
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (surface->Data.Pitch >= 0x8000 || !surface->Data.Pitch)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        } else {
            if ((surface->Info.FourCC == MFX_FOURCC_YV12 && (surface->Data.U || surface->Data.V)) ||
                (surface->Info.FourCC == MFX_FOURCC_NV12 && surface->Data.UV))
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    // field-based output: tell async part to return 1st field
    if (m_fieldOutput)
        m_fieldOutputState = FIELD_OUTPUT_FIRST_FIELD;

    // check external payloads
    if (ctrl && ctrl->Payload && ctrl->NumPayload > 0) {
        for( mfxI32 i = 0; i<ctrl->NumPayload; i++ ){
            if (ctrl->Payload[i] == 0 || (ctrl->Payload[i]->NumBit & 0x07)) // check if byte-aligned
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            // check for valid payload type
            if (ctrl->Payload[i]->Type <= SEI_TYPE_PIC_TIMING  ||
                (ctrl->Payload[i]->Type >= SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION  && ctrl->Payload[i]->Type <= SEI_TYPE_SPARE_PIC) ||
                (ctrl->Payload[i]->Type >= SEI_TYPE_SUB_SEQ_INFO && ctrl->Payload[i]->Type <= SEI_TYPE_SUB_SEQ_CHARACTERISTICS) ||
                ctrl->Payload[i]->Type == SEI_TYPE_MOTION_CONSTRAINED_SLICE_GROUP_SET ||
                (ctrl->Payload[i]->Type > SEI_TYPE_TONE_MAPPING_INFO && ctrl->Payload[i]->Type != SEI_TYPE_FRAME_PACKING_ARRANGEMENT))
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    // check ref pic list control info
    if (ctrl && ctrl->ExtParam && ctrl->NumExtParam > 0) {
        mfxExtAVCRefListCtrl *pRefPicListCtrl = 0;
        // try to get ref pic list control info
        pRefPicListCtrl = (mfxExtAVCRefListCtrl*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);
        // ref pic list control info is ignored for B-frames
        if (pRefPicListCtrl) {
            // ref list ctrl will be ignored in async part for Lync, field encoding and encoding with B-frames
            // signal about it with warning
            if (isFieldEncoding || m_temporalLayers.NumLayers || cur_enc->m_info.B_frame_rate)
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            for (mfxU8 i = 0; i < 32; i ++)
                if (pRefPicListCtrl->PreferredRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && pRefPicListCtrl->PreferredRefList[i].PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            for (mfxU8 i = 0; i < 16; i ++)
               if (pRefPicListCtrl->RejectedRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && pRefPicListCtrl->RejectedRefList[i].PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
                   return MFX_ERR_UNDEFINED_BEHAVIOR;
            for (mfxU8 i = 0; i < 16; i ++)
               if (pRefPicListCtrl->LongTermRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && pRefPicListCtrl->LongTermRefList[i].PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
                   return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
#ifdef H264_INTRA_REFRESH
        mfxExtCodingOption2* pCO2 = (mfxExtCodingOption2*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_CODING_OPTION2);

        if (pCO2) {
            if (m_base.enc->m_refrType == 0) // refresh is disabled
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            if (pCO2->IntRefQPDelta < -51 || pCO2->IntRefQPDelta > 51) {
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }
        }
#endif // H264_INTRA_REFRESH
    }

    *reordered_surface = surface;

    if( vp->mfx.EncodedOrder ) {
        if (!surface) {
            if (m_fieldOutput)
                m_fieldOutputStatus = MFX_ERR_MORE_DATA;
            return MFX_ERR_MORE_DATA;
        }
        if (!ctrl) // for EncodedOrder application should provide FrameType via ctrl
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        else if ((!layer->m_frameCountSync && surface->Data.FrameOrder) || (layer->m_frameCountSync && !surface->Data.FrameOrder))
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        if (ctrl) {
            mfxU16 firstFieldType = ctrl->FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B);
            mfxU16 secondFieldType = ctrl->FrameType & (MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xB);
            if ((firstFieldType != MFX_FRAMETYPE_I && firstFieldType != MFX_FRAMETYPE_P && firstFieldType != MFX_FRAMETYPE_B) ||
                (secondFieldType != 0 && secondFieldType != MFX_FRAMETYPE_xI && secondFieldType != MFX_FRAMETYPE_xP && secondFieldType != MFX_FRAMETYPE_xB))
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            else if (isFieldEncoding && secondFieldType != (firstFieldType << 8) && (firstFieldType != MFX_FRAMETYPE_I || secondFieldType != MFX_FRAMETYPE_xP)) {
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            if (!layer->m_frameCountSync && (firstFieldType != MFX_FRAMETYPE_I))
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        layer->m_frameCountSync++;

        if (m_fieldOutput && m_fieldOutputState == FIELD_OUTPUT_FIRST_FIELD)
            return MFX_ERR_MORE_BITSTREAM; // ask for bitstream for 2nd field

        return st;
    }
    else {
        if (m_svc_count>1) { // check for proper order of spatial layers
            Ipp8u did = layer->enc->m_svc_layer.svc_ext.dependency_id;
            mfxU32 num = layer->m_frameCountSync * layer->enc->rateDivider;
            bool checkprev = (layer->m_frameCountSync > 0);
            // if have lower layer's frame with same counter - OK,
            // otherwise check if top layer has previous counter
            // note, m_frameCountSync points to abs number of the frame before is incremented
            if (did > 0) {
                h264Layer* prevl = m_svc_layers[did-1];
                checkprev = ((prevl->m_frameCountSync - 1) * prevl->enc->rateDivider != num);
            }
            if (checkprev) {
                h264Layer* topl = m_svc_layers[m_maxDid];
                if (layer->m_frameCountSync == 0 ||
                    topl->m_frameCountSync * topl->enc->rateDivider != num)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (ctrl && (ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN)) {
            if ((ctrl->FrameType & 0x0f) != MFX_FRAMETYPE_I )
                return MFX_ERR_INVALID_VIDEO_PARAM;
            if (layer->m_frameCountSync == 0 && (ctrl->FrameType & MFX_FRAMETYPE_IDR) == 0)
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        layer->m_frameCountSync++;
        mfxU32 noahead = (cur_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ? 0 : 1;
        h264Layer *toplayer = m_svc_layers[m_maxDid] ? m_svc_layers[m_maxDid] : layer;
        if (!surface) {
            if (layer->m_frameCountBufferedSync == 0) { // buffered frames to be encoded
                if (m_fieldOutput)
                    m_fieldOutputStatus = MFX_ERR_MORE_DATA;
                return MFX_ERR_MORE_DATA;
            }
            layer->m_frameCountBufferedSync --;
        }
        else if (layer->m_frameCountSync > noahead && toplayer->m_frameCountBufferedSync <
            (mfxU32)toplayer->enc->m_info.B_frame_rate + 1 - noahead ) {
            layer->m_frameCountBufferedSync++;
            if (m_fieldOutput)
                m_fieldOutputStatus = MFX_ERR_MORE_DATA;
            return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
        }
    }

    if (m_fieldOutput && m_fieldOutputState == FIELD_OUTPUT_FIRST_FIELD) {
        m_fieldOutputStatus = MFX_ERR_MORE_BITSTREAM;
        return MFX_ERR_MORE_BITSTREAM; // ask for bitstream for 2nd field
    }

    if (!lastDid && surface) // encode only when all depId are here
        return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;

    return st;
}

#ifdef USE_TEMPTEMP
mfxStatus MFXVideoENCODEH264::InitGopTemplate(mfxVideoParam* par)
{
    // create refs control for temporal scalability, to minimize ref count
    // excluding B refs
    mfxI32 i0, maxdid = -1;
    mfxI32 maxtid = 0;
    //mfxU32 pos = 0;

    if (par->mfx.EncodedOrder || par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        return MFX_ERR_UNSUPPORTED;

    for ( i0 = 0; i0 < MAX_DEP_LAYERS; i0++ ) {
        if (m_svc_layers[i0])
            maxdid = i0;
    }
    if (maxdid>0)
        maxtid = m_svc_layers[maxdid]->m_InterLayerParam.TemporalNum;

    {
        mfxI32 refForLevel[MAX_TEMP_LEVELS] = {0,}; // pos for last ref, valid for tID
        mfxI32 window[16];                          // sliding window buffer
        mfxI32 refsInWindow = 0;                    // window fullness
        mfxI32 maxnumref = par->mfx.NumRefFrame;
        mfxI32 lentemp = maxdid<0 ? 1 :
            m_svc_layers[maxdid]->enc->TemporalScaleList[maxtid /*- 1*/];
        mfxI32 lenGOP = par->mfx.GopPicSize;
        mfxI32 refdist = par->mfx.GopRefDist;
        assert(lentemp<=lenGOP || !(lentemp%lenGOP));
        assert(lenGOP<=lentemp || !(lenGOP%lentemp));
        assert(maxnumref <= 16);
        mfxI32 pos, gop, len = MAX(lentemp, lenGOP);
        m_temptempwidth = len;   // step back when went to the end of template
        if (!(par->mfx.GopOptFlag | MFX_GOP_CLOSED) && refdist > 1)
            len *= 2; // to have closed and !closed patterns; 1-st for closed, 2nd for open
        SVCRefControl* refctrl = m_refctrl = (SVCRefControl*)H264_Malloc((len+1)*sizeof(*m_refctrl));
        if (!refctrl)
            return MFX_ERR_MEMORY_ALLOC;
        m_temptempstart = 0;
        m_temptemplen = len;
        m_usetemptemp = 1; // 0; //disabled
        m_maxTempScale = lentemp;
        memset(refctrl, 0, (len+1)*sizeof(*refctrl));
        refctrl[len].isRef = 1; //(layer->m_mfxVideoParam.mfx.GopOptFlag & MFX_GOP_CLOSED) ? 0 : 1;

        // set all temporal id
        for (pos=0; pos<len; pos++) {
            mfxI32 tid, tstep = 1;
            for(tid = 0; tid < maxtid; tid++) {
                tstep = lentemp / m_svc_layers[maxdid]->enc->TemporalScaleList[tid];
                if (!(pos%tstep)) break;
            }
            refctrl[pos].tid = tid;
            refctrl[pos].pcType = BPREDPIC; // other in the next loop
        }

        for (gop=0; gop<len; gop+=lenGOP) { // mark I refs
            for (pos=0; pos<lenGOP; pos+=refdist) { // mark P refs
                //mfxI32 tid = refctrl[gop+pos].tid;
                refctrl[gop+pos].isRef = 1; // can be off if lack of refs
                refctrl[gop+pos].pcType = PREDPIC;
            }
            refctrl[gop+0].pcType = INTRAPIC;
        }

        // put first frame (I) to sl.window
        refctrl[0].codorder = 0;
        window[0] = 0;
        refsInWindow = 1;
        //refForLevel[0] = 0;

        mfxI32 codorder = 1; // first I was added

        for (;;) {
            mfxI32 bb;
            // find B having bwd ref or last I or P
            for (bb = 1; bb<len && refctrl[bb].codorder; bb++); // first unassigned
            if (bb==len) break; // done

            mfxI32 left = 0, right=0, hole, firstST=0;
            mfxI32 mintid = maxtid;
            pos  = 0; // if remained 0 means need to find B closest to center of B row

            if (refctrl[bb].pcType != BPREDPIC) {
                pos = bb;
                mintid = refctrl[pos].tid;
            } else {
                for(left = bb-1; left>0 && !refctrl[left].isRef; left--); // find last fwd ref
                if (left == bb-1) { // can replace layers refs if later ref has lower tid
                    mfxI32 tid;
                    for(tid = refctrl[left].tid; tid <= maxtid; tid++)
                        refForLevel[tid] = left;
                }
                for (right = bb+1; right<len && !refctrl[right].isRef; right++); // find first bwd ref
                if (right<len && !refctrl[right].codorder) {
                    // it is P (or I) to encode next
                    pos = right;
                    mintid = refctrl[pos].tid;
                } else {
                    if (right == len && !(par->mfx.GopOptFlag | MFX_GOP_CLOSED))
                        break; // will jump GOPlen back
                    // right is encoded (or absent), select B from (sub)row
                    if (right == bb+1 /*right - left == 2*/) { // not reference B
                        pos = bb;
                        mintid = refctrl[pos].tid;
                    } else {
                        // find min temporal id in the B sub-row
                        for (bb = left+1; bb<right; bb++)
                            if (refctrl[bb].tid < mintid)
                                mintid = refctrl[bb].tid;
                    }
                }
            }

            hole = refsInWindow;
            // try to find a place for reference frame in the sliding window
            if (refsInWindow == maxnumref) {
                // need a frame to remove from dpb
                for(firstST=0; firstST<refsInWindow && refctrl[window[firstST]].isLT; firstST++);
                // find first removable ST frame in window, turn to LT if still needed
                for (hole = firstST; hole < refsInWindow; hole++) {
                    if (refForLevel[refctrl[window[hole]].tid] > window[hole]) {
                        // there is a newer ref for its tid, can delete
                        break;
                    }
                }
                if (hole == refsInWindow) {
                    // find among LT to replace, can change only higher tid
                    mfxI32 highestref = 0, highestreftid=0;
                    for (hole = 0; hole<firstST; hole++) {
                        mfxI32 tid = refctrl[window[hole]].tid;
                        if(tid >= mintid && refForLevel[tid] > window[hole]) {
                            // there is a newer ref, can delete
                            break;
                        }
                        // find ref with highest tid, not less than current
                        if (tid > highestreftid) {
                            highestref = hole;
                            highestreftid = tid;
                        }
                    }
                    if (hole == firstST) { // nothing to delete lossless
                        if (highestreftid >= mintid && pos>0 && refctrl[pos].isRef) {
                            hole = highestref;
                        } else
                            hole = refsInWindow; // no place in ref list
                    }
                }
            }

            if (pos == 0 && hole < maxnumref) {
                // find a frame in the B sub-row closest to its center
                mfxI32 minii=0, bdist=-1;
                for (bb = left+1; bb<right; bb++) {
                    mfxI32 cdist = (bb-left)*(right-bb); // the greater the closer to center
                    if (refctrl[bb].tid == mintid && cdist > bdist) {
                        minii = bb;
                        bdist = cdist;
                    }
                }
                assert(minii);
                pos = minii;
                refctrl[pos].isRef = 1;
            }

            if (hole == maxnumref) { // can't insert to sliding window
                if (pos == 0) {
                    // take first notcoded in B row to encode
                    for (pos = left+1; refctrl[pos].codorder > 0; pos++);
                }
                refctrl[pos].isRef = 0;
            }

            if (refctrl[pos].pcType == BPREDPIC) {
                bool hasrightref = ((right<len) &&
                    (refctrl[right].pcType != INTRAPIC ||
                    !(par->mfx.GopOptFlag | MFX_GOP_CLOSED)));
                if (!hasrightref && !(par->mfx.GopOptFlag | MFX_GOP_STRICT)) {
                    refctrl[pos].pcType = PREDPIC; // not reference P, just P predicted
                }
            }

            if (refctrl[pos].isRef) {
                if ( refsInWindow == maxnumref) {
                    // mark previous frames as LT to keep them
                    while (firstST < hole)
                        refctrl[window[firstST++]].isLT = 1;
                    // slide:
                    //if (refsInWindow == maxnumref) {
                        refctrl[window[hole]].deadline = pos;
                        for (; hole+1 < refsInWindow; hole++)
                            window[hole] = window[hole+1];
                        refsInWindow --;
                    //}
                }
                // add the new frame to the end of the window//, update newest refs
                window[refsInWindow] = pos;
                refsInWindow ++;
                //mfxI32 tid;
                //for(tid = refctrl[pos].tid; tid <= maxtid; tid++)
                //    refForLevel[tid] = pos; // ??? left or right
            }

            if (refctrl[pos].pcType != INTRAPIC)
                refctrl[refForLevel[mintid]].lastuse = pos; // for left

            if (refctrl[pos].pcType == BPREDPIC && right < len && refctrl[right].isRef && refctrl[right].tid <= mintid)
                refctrl[right].lastuse = pos;

            refctrl[pos].codorder = ++codorder;
        } // end template loop


    }
    return MFX_ERR_NONE;
}
#endif // USE_TEMPTEMP

#define PIXTYPE Ipp8u

mfxU8 GetNumReorderFrames(mfxU32 BFrameRate, bool BPyramid){
    mfxU8 n = !!BFrameRate;

    if(BPyramid && n--){
        while(BFrameRate){
            BFrameRate >>= 1;
            n ++;
        }
    }
    return n;
}

mfxStatus MFXVideoENCODEH264::Init(mfxVideoParam* par_in)
{
    mfxStatus st, QueryStatus;
    UMC::VideoBrcParams brcParams;
    H264EncoderParams videoParams;
    Status status;
    H264CoreEncoder_8u16s* enc = m_base.enc;

    if (m_base.m_Initialized)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    if( par_in == NULL ) return MFX_ERR_NULL_PTR;

    st = CheckExtBuffers_H264enc( par_in->ExtParam, par_in->NumExtParam );
    if (st != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOption* opts = GetExtCodingOptions( par_in->ExtParam, par_in->NumExtParam );
    mfxExtCodingOptionSPSPPS* optsSP = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtPictureTimingSEI* picTimingSei = (mfxExtPictureTimingSEI*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );
    mfxExtAvcTemporalLayers* tempLayers = (mfxExtAvcTemporalLayers*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
    mfxExtCodingOption2* opts2 = (mfxExtCodingOption2*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );
    if(GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
        return MFX_ERR_INVALID_VIDEO_PARAM;
    mfxExtCodingOptionDDI* extOptDdi = NULL;
    extOptDdi = (mfxExtCodingOptionDDI *) GetExtBuffer(par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_DDI);
    if (extOptDdi)
    {
        m_ConstQP = extOptDdi->ConstQP;
    }

    mfxExtSVCSeqDesc* pSvcLayers = (mfxExtSVCSeqDesc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
    mfxExtSVCRateControl *rc = (mfxExtSVCRateControl*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );
    bool hasTemporal = false;

    /* Check */
    if (pSvcLayers) {
        mfxU32 i0;
        bool hasActive = false;
        for ( i0 = 0; i0 < MAX_DEP_LAYERS; i0++ ) {
            if (pSvcLayers->DependencyLayer[i0].Active) {
                hasActive = true;
                if (pSvcLayers->DependencyLayer[i0].TemporalNum > 1) {
                    hasTemporal = true; // more than single temporal layer
                    break;
                }
            }
        }
        if (!hasActive)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxVideoParam checked;
    mfxExtCodingOption checked_ext;
    mfxExtCodingOptionSPSPPS checked_extSP;
    mfxExtVideoSignalInfo checked_videoSignalInfo;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtPictureTimingSEI checked_picTimingSei;
    mfxExtAvcTemporalLayers checked_tempLayers;
    mfxExtSVCSeqDesc checked_svcLayers;
    mfxExtSVCRateControl checked_svcRC;
    mfxExtCodingOption2 checked_ext2;
    mfxExtBuffer *ptr_checked_ext[9] = {0,};
    mfxU16 ext_counter = 0;
    checked = *par_in;
    if (opts) {
        checked_ext = *opts;
        ptr_checked_ext[ext_counter++] = &checked_ext.Header;
    } else {
        memset(&checked_ext, 0, sizeof(checked_ext));
        checked_ext.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
        checked_ext.Header.BufferSz = sizeof(checked_ext);
    }
    if (optsSP) {
        checked_extSP = *optsSP;
        ptr_checked_ext[ext_counter++] = &checked_extSP.Header;
    } else {
        memset(&checked_extSP, 0, sizeof(checked_extSP));
        checked_extSP.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
        checked_extSP.Header.BufferSz = sizeof(checked_extSP);
    }
    if (videoSignalInfo) {
        checked_videoSignalInfo = *videoSignalInfo;
        ptr_checked_ext[ext_counter++] = &checked_videoSignalInfo.Header;
    } else {
        memset(&checked_videoSignalInfo, 0, sizeof(checked_videoSignalInfo));
        checked_videoSignalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
        checked_videoSignalInfo.Header.BufferSz = sizeof(checked_videoSignalInfo);
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (picTimingSei) {
        checked_picTimingSei = *picTimingSei;
        ptr_checked_ext[ext_counter++] = &checked_picTimingSei.Header;
    }
    if (tempLayers) {
        checked_tempLayers = *tempLayers;
        ptr_checked_ext[ext_counter++] = &checked_tempLayers.Header;
    }
    if (pSvcLayers) {
        checked_svcLayers = *pSvcLayers;
        ptr_checked_ext[ext_counter++] = &checked_svcLayers.Header;
    }
    if (rc) {
        checked_svcRC = *rc;
        ptr_checked_ext[ext_counter++] = &checked_svcRC.Header;
    }
    if (opts2) {
        checked_ext2 = *opts2;
        ptr_checked_ext[ext_counter++] = &checked_ext2.Header;
    }
    checked.ExtParam = ptr_checked_ext;
    checked.NumExtParam = ext_counter;

    st = Query(par_in, &checked);

    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && st != MFX_WRN_PARTIAL_ACCELERATION) {
        if (st == MFX_ERR_UNSUPPORTED && optsSP == 0) // SPS/PPS header isn't attached. Error is in mfxVideoParam. Return MFX_ERR_INVALID_VIDEO_PARAM
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else if (st == MFX_ERR_UNSUPPORTED) // SPSPPS header is attached. Return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        else
            return st;
    }

    QueryStatus = st;

    // workaround for default entropy_coding_mode_flag with opts == 0
    // need to set correct profile for TU 5
    if (!opts) {
        ptr_checked_ext[ext_counter++] = &checked_ext.Header;
        checked.NumExtParam = ext_counter;
    }

    mfxVideoInternalParam inInt = checked;
    mfxVideoInternalParam* par = &inInt;
    opts = &checked_ext;
    if (picTimingSei)
        picTimingSei = &checked_picTimingSei;

    if (tempLayers && par->mfx.EncodedOrder) {
        tempLayers = 0;
        QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (tempLayers)
        tempLayers = &checked_tempLayers;
    if (pSvcLayers)
        pSvcLayers = &checked_svcLayers;
    if (rc)
        rc = &checked_svcRC;
    //optsSP = &checked_extSP;

#ifdef H264_INTRA_REFRESH
    enc->m_QPDelta = 0;
    enc->m_refrType = 0;
    enc->m_stripeWidth = 0;
#endif // H264_INTRA_REFRESH

    if (opts2) {
        opts2 = &checked_ext2;
#ifdef H264_INTRA_REFRESH
        enc->m_refrType = opts2->IntRefType;
        if(opts2->IntRefType) {
            if(par->mfx.GopRefDist == 0)
                par->mfx.GopRefDist = 1;
            mfxU16 refreshSizeInMBs =  enc->m_refrType == HORIZ_REFRESH ? par->mfx.FrameInfo.Height >> 4 : par->mfx.FrameInfo.Width >> 4;
            enc->m_stopRefreshFrames = 0;
            if (opts2->IntRefCycleSize <= refreshSizeInMBs) {
                enc->m_stripeWidth = (refreshSizeInMBs + opts2->IntRefCycleSize - 1) / opts2->IntRefCycleSize;
                mfxU16 refreshTime = (refreshSizeInMBs + enc->m_stripeWidth - 1) / enc->m_stripeWidth;
                enc->m_stopRefreshFrames = opts2->IntRefCycleSize - refreshTime;
            } else {
                enc->m_stopRefreshFrames = opts2->IntRefCycleSize - refreshSizeInMBs;
                enc->m_stripeWidth = 1;
            }
            enc->m_refrSpeed = enc->m_stripeWidth;
            enc->m_periodCount = 0;
            enc->m_stopFramesCount = 0;
            enc->m_QPDelta = opts2->IntRefQPDelta;
        } else {
            enc->m_stripeWidth = 0;
            enc->m_refrSpeed = 0;
            enc->m_periodCount = 0;
            enc->m_stopFramesCount = 0;
            enc->m_QPDelta = 0;
        }
#endif // H264_INTRA_REFRESH
    }

    // no HRD syntax in bitstream if HRD-conformance is OFF
    if (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF) {
        opts->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
        opts->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    // MSDK doesn't support true VCL conformance. We use NAL HRD settings for VCL conformance in case of VBR
    if (opts && opts->VuiVclHrdParameters == MFX_CODINGOPTION_ON && par->mfx.RateControlMethod != MFX_RATECONTROL_VBR) {
        QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        opts->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (opaqAllocReq)
        opaqAllocReq = &checked_opaqAllocReq;

    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        if (opaqAllocReq == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else {
            // check memory type in opaque allocation request
            if (!(opaqAllocReq->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET) && !(opaqAllocReq->In.Type  & MFX_MEMTYPE_SYSTEM_MEMORY))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            if ((opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (opaqAllocReq->In.Type  & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            // use opaque surfaces. Need to allocate
            m_isOpaque = true;
        }
    }

    // return an error if requested opaque memory type isn't equal to native
    if (m_isOpaque && (opaqAllocReq->In.Type & MFX_MEMTYPE_FROM_ENCODE) && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_allocator = new mfx_UMC_MemAllocator;
    if (!m_allocator) return MFX_ERR_MEMORY_ALLOC;

    UMC::MemoryAllocatorParams umcAllocParams;
    m_allocator->InitMem(&umcAllocParams, m_core);

    ////if(!m_VideoParams_id) m_core->AllocBuffer(sizeof(H264EncoderParams),0,&m_VideoParams_id);
    ////if(!m_VideoParams_id) return MFX_ERR_MEMORY_ALLOC;

    //// m_core->LockBuffer(m_VideoParams_id, &mem1);
    ////if (!mem1) return MFX_ERR_MEMORY_ALLOC;

    ////m_VideoParams = new (mem1) H264EncoderParams;
    ////if (!m_VideoParams) return MFX_ERR_MEMORY_ALLOC;

    videoParams.m_Analyse_on = 0;
    videoParams.m_Analyse_restrict = 0;
    memset(&videoParams.m_SeqParamSet, 0, sizeof(UMC::H264SeqParamSet));
    memset(&videoParams.m_PicParamSet, 0, sizeof(UMC::H264PicParamSet));
    videoParams.use_ext_sps = videoParams.use_ext_pps = false;
    videoParams.m_ext_SPS_id = videoParams.m_ext_PPS_id = 0;
    memset(videoParams.m_ext_constraint_flags, 0, sizeof(videoParams.m_ext_constraint_flags));
    st = ConvertVideoParam_H264enc(par, &videoParams); // converts all with preference rules
    if (st < MFX_ERR_NONE)
        return st;
    else if (st != MFX_ERR_NONE)
        QueryStatus = st;

    m_initValues.FrameWidth = par->mfx.FrameInfo.Width;
    m_initValues.FrameHeight = par->mfx.FrameInfo.Height;

    if (m_isOpaque && opaqAllocReq->In.NumSurface < videoParams.B_frame_rate + 1)
        return MFX_ERR_INVALID_VIDEO_PARAM;

     // Allocate Opaque frames and frame for copy from video memory (if any)
    memset(&m_base.m_auxInput, 0, sizeof(m_base.m_auxInput));
    m_base.m_useAuxInput = false;
    m_useSysOpaq = false;
    m_useVideoOpaq = false;
    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_isOpaque) {
        bool bOpaqVideoMem = m_isOpaque && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
        bool bNeedAuxInput = (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) || bOpaqVideoMem;
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        request.Info              = par->mfx.FrameInfo;

        // try to allocate opaque surfaces in video memory for another component in transcoding chain
        if (bOpaqVideoMem) {
            memset(&m_base.m_response_alien, 0, sizeof(m_base.m_response_alien));
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin =request.NumFrameSuggested = (mfxU16)opaqAllocReq->In.NumSurface;

            st = m_core->AllocFrames(&request,
                                     &m_base.m_response_alien,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);

            if (MFX_ERR_NONE != st &&
                MFX_ERR_UNSUPPORTED != st) // unsupported means that current Core couldn;t allocate the surfaces
                return st;

            if (m_base.m_response_alien.NumFrameActual < request.NumFrameMin)
                return MFX_ERR_MEMORY_ALLOC;

            if (st != MFX_ERR_UNSUPPORTED)
                m_useVideoOpaq = true;
        }

        // allocate all we need in system memory
        memset(&m_base.m_response, 0, sizeof(m_base.m_response));
        if (bNeedAuxInput) {
            // allocate additional surface in system memory for FastCopy from video memory
            request.Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
            request.NumFrameMin       = 1;
            request.NumFrameSuggested = 1;
            st = m_core->AllocFrames(&request, &m_base.m_response);
            MFX_CHECK_STS(st);
        } else {
            // allocate opaque surfaces in system memory
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin       = opaqAllocReq->In.NumSurface;
            request.NumFrameSuggested = opaqAllocReq->In.NumSurface;
            st = m_core->AllocFrames(&request,
                                     &m_base.m_response,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);
            MFX_CHECK_STS(st);
        }

        if (m_base.m_response.NumFrameActual < request.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        if (bNeedAuxInput) {
            m_base.m_useAuxInput = true;
            m_base.m_auxInput.Data.MemId = m_base.m_response.mids[0];
            m_base.m_auxInput.Info = request.Info;
        } else
            m_useSysOpaq = true;

        //st = m_core->LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        //MFX_CHECK_STS(st);
    }

    if (videoParams.info.clip_info.width == 0 || videoParams.info.clip_info.height == 0 ||
        par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 ||
        ((videoParams.rate_controls.method != H264_RCM_QUANT && videoParams.info.bitrate == 0) && !pSvcLayers) ||
        videoParams.info.framerate == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // to set profile/level and related vars
    // first copy fields which could have been changed from TU
    mfxVideoInternalParam checkedSetByTU = mfxVideoInternalParam();
    if (par->mfx.CodecProfile == MFX_PROFILE_UNKNOWN && videoParams.transform_8x8_mode_flag)
        par->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    if (par->mfx.GopRefDist == 0) {
        checkedSetByTU.mfx.GopRefDist = 1;
        par->mfx.GopRefDist = (mfxU16)(videoParams.B_frame_rate + 1);
    }
    if (par->mfx.NumRefFrame == 0) {
        checkedSetByTU.mfx.NumRefFrame = 1;
        par->mfx.NumRefFrame = (mfxU16)videoParams.num_ref_frames;
    }
    if (checked_ext.CAVLC == MFX_CODINGOPTION_UNKNOWN)
        checked_ext.CAVLC = (mfxU16)(videoParams.entropy_coding_mode ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);

    st = CheckProfileLevelLimits_H264enc(par, false, &checkedSetByTU);
    if (st != MFX_ERR_NONE)
        QueryStatus = st;

    if (par->mfx.CodecProfile > MFX_PROFILE_AVC_BASELINE && par->mfx.CodecLevel >= MFX_LEVEL_AVC_3)
        videoParams.use_direct_inference = 1;
    // get back mfx params to umc
    videoParams.profile_idc = (UMC::H264_PROFILE_IDC)ConvertCUCProfileToUMC( par->mfx.CodecProfile );
    videoParams.level_idc = ConvertCUCLevelToUMC( par->mfx.CodecLevel );
    videoParams.num_ref_frames = par->mfx.NumRefFrame;
    if (par->mfx.GopRefDist > 3 && (videoParams.num_ref_frames > ((par->mfx.GopRefDist - 1) / 2 + 1))) { // enable B-refs
        videoParams.treat_B_as_reference = 1;
    }
    if ((opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF) || par->mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
        videoParams.info.bitrate = par->calcParam.TargetKbps * 1000;
    else
        videoParams.info.bitrate = par->calcParam.MaxKbps * 1000;
    if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || checked_ext.FramePicture == MFX_CODINGOPTION_ON)
        videoParams.coding_type = 0;

    // apply level-dependent restriction on vertical MV range
    mfxU16 maxVertMV = GetMaxVmvR(par->mfx.CodecLevel);
    if (maxVertMV && maxVertMV < videoParams.max_mv_length_y)
        videoParams.max_mv_length_y = maxVertMV;

    if (((videoParams.profile_idc == H264_BASE_PROFILE) &&
        (par->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE)) || tempLayers)
        videoParams.m_ext_constraint_flags[1] = 1;
    else if (videoParams.profile_idc == H264_HIGH_PROFILE &&
        par->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        videoParams.m_ext_constraint_flags[4] = videoParams.m_ext_constraint_flags[5] = 1;
    else if (videoParams.profile_idc == H264_HIGH_PROFILE &&
        par->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH)
        videoParams.m_ext_constraint_flags[4] = 1;

    //Reset frame count
    m_base.m_frameCountSync = 0;
    m_base.m_frameCountBufferedSync = 0;
    m_base.m_frameCount = 0;
    m_base.m_frameCountBuffered = 0;
    m_base.m_lastIDRframe = 0;
    m_base.m_lastIframe = 0;
    m_base.m_lastIframeEncCount = 0;
    m_base.m_lastRefFrame = 0;
    m_internalFrameOrders = false;

    m_fieldOutputState = 0;
    m_fieldOutputStatus = MFX_ERR_NONE;

    if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        videoParams.coding_type = 0; // frames only
    else {
        if( opts && opts->FramePicture== MFX_CODINGOPTION_ON )
            videoParams.coding_type = 3; // MBAFF isn't supported. Encode as 2 fields if MBAFF is requested. TODO: change 3 to 2 when MBAFF will be implemented
        else
            videoParams.coding_type = 3; // PicAFF
    }

    if (videoParams.m_Analyse_on){
        if (videoParams.transform_8x8_mode_flag == 0)
            videoParams.m_Analyse_on &= ~ANALYSE_I_8x8;
        if (videoParams.entropy_coding_mode == 0)
            videoParams.m_Analyse_on &= ~(ANALYSE_RD_OPT | ANALYSE_RD_MODE | ANALYSE_B_RD_OPT);
        if (videoParams.coding_type && videoParams.coding_type != 2) // TODO: remove coding_type != 2 when MBAFF is implemented
            videoParams.m_Analyse_on &= ~ANALYSE_ME_AUTO_DIRECT;
        if (par->mfx.GopOptFlag & MFX_GOP_STRICT)
            videoParams.m_Analyse_on &= ~ANALYSE_FRAME_TYPE;
    }

    m_base.m_mfxVideoParam.mfx = par->mfx;
    m_base.m_mfxVideoParam.calcParam = par->calcParam;
    m_base.m_mfxVideoParam.IOPattern = par->IOPattern;
    m_base.m_mfxVideoParam.Protected = 0;
    m_base.m_mfxVideoParam.AsyncDepth = par->AsyncDepth;

    // set like in par-files
    if( par->mfx.RateControlMethod == MFX_RATECONTROL_CBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_CBR;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_CQP )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_QUANT;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_AVBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_AVBR;
    else
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_VBR;

    // apply restrictions on MV length from external SPS
    if (videoParams.use_ext_sps == true && videoParams.m_SeqParamSet.vui_parameters_present_flag &&
        videoParams.m_SeqParamSet.vui_parameters.bitstream_restriction_flag)
    {
        mfxI32 xLimit = 1 << videoParams.m_SeqParamSet.vui_parameters.log2_max_mv_length_horizontal;
        xLimit -= 1; // assure that MV is less then 2^n - 1
        xLimit >>= 2;
        if (xLimit && xLimit < videoParams.max_mv_length_x)
            videoParams.max_mv_length_x = xLimit;
        mfxI32 yLimit = 1 << videoParams.m_SeqParamSet.vui_parameters.log2_max_mv_length_vertical;
        yLimit -= 1; // assure that MV is less then 2^n - 1
        yLimit >>= 2;
        if (yLimit && yLimit < videoParams.max_mv_length_y)
            videoParams.max_mv_length_y = yLimit;

    }


    /*videoParams.rate_controls.quantI = 30;
    videoParams.rate_controls.quantP = 30;
    videoParams.rate_controls.quantB = 30;*/

    status = H264CoreEncoder_Init_8u16s(enc, &videoParams, m_allocator);
    st = h264enc_ConvertStatus(status);
    MFX_CHECK_STS(st);

    // init brc (if not const QP)
    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP && !pSvcLayers) {
        mfxVideoParam tmp = *par;
        par->GetCalcParams(&tmp);
        st = ConvertVideoParam_Brc(&tmp, &brcParams);
        MFX_CHECK_STS(st);
        brcParams.GOPPicSize = videoParams.key_frame_controls.interval;
        brcParams.GOPRefDist = videoParams.B_frame_rate + 1;
        brcParams.profile = videoParams.profile_idc;
        brcParams.level = videoParams.level_idc;

        m_base.m_brc[0] = new UMC::H264BRC;
        status = m_base.m_brc[0]->Init(&brcParams);

        if (m_ConstQP)
        {
            m_base.m_brc[0]->SetQP(m_ConstQP, I_PICTURE);
            m_base.m_brc[0]->SetQP(m_ConstQP, P_PICTURE);
            m_base.m_brc[0]->SetQP(m_ConstQP, B_PICTURE);
        }

        st = h264enc_ConvertStatus(status);
        MFX_CHECK_STS(st);
        enc->brc = m_base.m_brc[0];

        enc->brc->GetParams(&brcParams);
        if (brcParams.BRCMode != BRC_AVBR)
            enc->requiredBsBufferSize = brcParams.HRDBufferSizeBytes;
        else
            enc->requiredBsBufferSize = videoParams.info.clip_info.width * videoParams.info.clip_info.height * 3 / 2;
    }
    else { // const QP
        m_base.m_brc[0] = NULL;
        enc->brc = NULL;
        enc->requiredBsBufferSize = videoParams.info.clip_info.width * videoParams.info.clip_info.height * 3 / 2;
    }

    // Set low initial QP values to prevent BRC panic mode if superfast mode
    if(par->mfx.TargetUsage==MFX_TARGETUSAGE_BEST_SPEED && m_base.m_brc[0]) {
        if(m_base.m_brc[0]->GetQP(I_PICTURE) < 30) {
            m_base.m_brc[0]->SetQP(30, I_PICTURE); m_base.m_brc[0]->SetQP(30, B_PICTURE);
        }
    }

    enc->recodeFlag = 0;

    enc->free_slice_mode_enabled = (par->mfx.NumSlice == 0);
    if (enc->free_slice_mode_enabled)
        m_base.m_mfxVideoParam.mfx.NumSlice = 0;

    //Set specific parameters
    enc->m_info.write_access_unit_delimiters = 1;
    if( opts != NULL ){
        if( opts->MaxDecFrameBuffering != MFX_CODINGOPTION_UNKNOWN){
            if ( opts->MaxDecFrameBuffering < enc->m_info.num_ref_frames)
                opts->MaxDecFrameBuffering = enc->m_info.num_ref_frames;
            if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag = true;
            enc->m_SeqParamSet->vui_parameters.motion_vectors_over_pic_boundaries_flag = 1;
            enc->m_SeqParamSet->vui_parameters.max_bytes_per_pic_denom = 2;
            enc->m_SeqParamSet->vui_parameters.max_bits_per_mb_denom = 1;
            enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_horizontal = 11;
            enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_vertical = 11;
            enc->m_SeqParamSet->vui_parameters.num_reorder_frames = GetNumReorderFrames(enc->m_info.B_frame_rate, !!enc->m_info.treat_B_as_reference);
            enc->m_SeqParamSet->vui_parameters.max_dec_frame_buffering = opts->MaxDecFrameBuffering;
        }
        if( opts->AUDelimiter == MFX_CODINGOPTION_OFF ){
            enc->m_info.write_access_unit_delimiters = 0;
        //}else{
        //    enc->m_info.write_access_unit_delimiters = 0;
        }

        m_base.m_putEOSeq     = ( opts->EndOfSequence == MFX_CODINGOPTION_ON );
        m_base.m_putEOStream  = ( opts->EndOfStream   == MFX_CODINGOPTION_ON );
        m_base.m_putTimingSEI = ( opts->PicTimingSEI  != MFX_CODINGOPTION_OFF );
        m_base.m_resetRefList = ( opts->ResetRefList  == MFX_CODINGOPTION_ON );
        m_base.m_putDecRefPicMarkinfRepSEI = ( opts->RefPicMarkRep == MFX_CODINGOPTION_ON );
#ifdef H264_INTRA_REFRESH
        m_base.m_putRecoveryPoint = ( opts->RecoveryPointSEI == MFX_CODINGOPTION_ON );
#endif // H264_INTRA_REFRESH

        if( opts->FieldOutput == MFX_CODINGOPTION_ON) {
            if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || opts->FramePicture == MFX_CODINGOPTION_ON) {
                m_fieldOutput = false;
                QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            } else
                m_fieldOutput = true;
        }

        if (opts->SingleSeiNalUnit == MFX_CODINGOPTION_OFF) m_separateSEI = true;
        else m_separateSEI = false; // default behavior is to put all SEI in one NAL unit
    }

    m_base.m_putClockTimestamp = (picTimingSei != NULL);

    if (m_base.m_putClockTimestamp) {
        // fill clock timestamp info
        for (mfxU8 i = 0; i < 3; i ++) {
            enc->m_SEIData.PictureTiming.clock_timestamp_flag[i] = picTimingSei->TimeStamp[i].ClockTimestampFlag;
            enc->m_SEIData.PictureTiming.ct_type[i] = picTimingSei->TimeStamp[i].CtType;
            enc->m_SEIData.PictureTiming.nuit_field_based_flag[i] = picTimingSei->TimeStamp[i].NuitFieldBasedFlag;
            enc->m_SEIData.PictureTiming.counting_type[i] = picTimingSei->TimeStamp[i].CountingType;
            enc->m_SEIData.PictureTiming.full_timestamp_flag[i] = picTimingSei->TimeStamp[i].FullTimestampFlag;
            enc->m_SEIData.PictureTiming.discontinuity_flag[i] = picTimingSei->TimeStamp[i].DiscontinuityFlag;
            enc->m_SEIData.PictureTiming.cnt_dropped_flag[i] = picTimingSei->TimeStamp[i].CntDroppedFlag;
            enc->m_SEIData.PictureTiming.n_frames[i] = picTimingSei->TimeStamp[i].NFrames;
            enc->m_SEIData.PictureTiming.seconds_flag[i] = picTimingSei->TimeStamp[i].SecondsFlag;
            enc->m_SEIData.PictureTiming.seconds_value[i] = picTimingSei->TimeStamp[i].SecondsValue;
            enc->m_SEIData.PictureTiming.minutes_flag[i] = picTimingSei->TimeStamp[i].MinutesFlag;
            enc->m_SEIData.PictureTiming.minutes_value[i] = picTimingSei->TimeStamp[i].MinutesValue;
            enc->m_SEIData.PictureTiming.hours_flag[i] = picTimingSei->TimeStamp[i].HoursFlag;
            enc->m_SEIData.PictureTiming.hours_value[i] = picTimingSei->TimeStamp[i].HoursValue;
            enc->m_SEIData.PictureTiming.time_offset[i] = picTimingSei->TimeStamp[i].TimeOffset;
        }
    }

    // init temporal layers info
    memset(&m_temporalLayers, 0, sizeof(AVCTemporalLayers));
    if (tempLayers) {
        mfxU16 i = 0;
        while (i < 4 && tempLayers->Layer[i].Scale) {
            m_temporalLayers.LayerScales[i] = tempLayers->Layer[i].Scale;
            m_temporalLayers.LayerPIDs[i] = tempLayers->BaseLayerPID + i;
            i ++;
        }
        m_temporalLayers.NumLayers = i;

        if (m_temporalLayers.NumLayers) {
            // B-frames aren't supported for current temporal layers implementation
            if (enc->m_info.B_frame_rate) {
                enc->m_info.B_frame_rate = 0;
                QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            if ((enc->m_info.num_ref_frames == 3 || enc->m_info.num_ref_frames == 2) && m_temporalLayers.NumLayers > 3) {
                m_temporalLayers.NumLayers = 3;
                QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            else if (enc->m_info.num_ref_frames == 1 && m_temporalLayers.NumLayers > 2) {
                m_temporalLayers.NumLayers = 2;
                QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
        }

        for (i = 0; i < m_temporalLayers.NumLayers; i ++) {
            m_temporalLayers.LayerNumRefs[i] = (i == 0) ? m_temporalLayers.NumLayers : (m_temporalLayers.NumLayers - (i + 1));
        }
    }

    // set VUI aspect ratio info
    if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
    if( par->mfx.FrameInfo.AspectRatioW != 0 && par->mfx.FrameInfo.AspectRatioH != 0 ){
        // enc->m_info.aspect_ratio_idc contains aspect_ratio_idc from attachecd sps/pps buffer
        enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = enc->m_info.aspect_ratio_idc == 255 ? 0 : (Ipp8u)ConvertSARtoIDC_H264enc(par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);
        if (enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 0) {
            enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = 255; //Extended SAR
            enc->m_SeqParamSet->vui_parameters.sar_width = par->mfx.FrameInfo.AspectRatioW;
            enc->m_SeqParamSet->vui_parameters.sar_height = par->mfx.FrameInfo.AspectRatioH;
        }
    }

    // set VUI video signal info
    if (videoSignalInfo != NULL) {
        if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = 1;
        enc->m_SeqParamSet->vui_parameters.video_format = videoSignalInfo->VideoFormat;
        enc->m_SeqParamSet->vui_parameters.video_full_range_flag = videoSignalInfo->VideoFullRange ? 1 : 0;
        enc->m_SeqParamSet->vui_parameters.colour_description_present_flag = videoSignalInfo->ColourDescriptionPresent ? 1 : 0;
        if (enc->m_SeqParamSet->vui_parameters.colour_description_present_flag) {
            enc->m_SeqParamSet->vui_parameters.colour_primaries = videoSignalInfo->ColourPrimaries;
            enc->m_SeqParamSet->vui_parameters.transfer_characteristics = videoSignalInfo->TransferCharacteristics;
            enc->m_SeqParamSet->vui_parameters.matrix_coefficients = videoSignalInfo->MatrixCoefficients;
        }
    }

    // set VUI timing info
    if (par->mfx.FrameInfo.FrameRateExtD && par->mfx.FrameInfo.FrameRateExtN) {
        if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.timing_info_present_flag = 1;
        enc->m_SeqParamSet->vui_parameters.num_units_in_tick = par->mfx.FrameInfo.FrameRateExtD;
        enc->m_SeqParamSet->vui_parameters.time_scale = par->mfx.FrameInfo.FrameRateExtN * 2;
        enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = 1;
    }
    else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (tempLayers && m_temporalLayers.NumLayers > 2)
        enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = 1;
    else if (enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag == 0)
        enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = hasTemporal;
    enc->m_SeqParamSet->vui_parameters.chroma_loc_info_present_flag = 0;

    if (opts != NULL)
    {
        if( opts->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN || opts->VuiNalHrdParameters == MFX_CODINGOPTION_ON)
        {
            enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
            if (enc->m_info.use_ext_sps == false && opts->PicTimingSEI != MFX_CODINGOPTION_OFF) enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP && par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
                enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 1;
            }
            else {
                enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
                if (opts->PicTimingSEI == MFX_CODINGOPTION_OFF)
                    enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 0;
            }
        }else {
            enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
            if (opts->PicTimingSEI != MFX_CODINGOPTION_OFF)
                enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
        }

        if( opts->VuiVclHrdParameters == MFX_CODINGOPTION_ON)
        {
            enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
            if (enc->m_info.use_ext_sps == false && opts->PicTimingSEI != MFX_CODINGOPTION_OFF) enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag = 1;
        }else {
            enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag = 0;
            if (opts->PicTimingSEI != MFX_CODINGOPTION_OFF)
                enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
        }
    }

    enc->m_FieldStruct = par->mfx.FrameInfo.PicStruct;

    // MSDK doesn't support true VCL conformance. We use NAL HRD settings for VCL conformance in case of VBR
    if (!pSvcLayers)
    if (enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag == 1 ||
        enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag)
    {
        VideoBrcParams  curBRCParams;

        enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_cnt_minus1 = 0;
        enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale = 0;
        enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale = 3;

        if (enc->brc)
        {
            enc->brc->GetParams(&curBRCParams);

            if (curBRCParams.maxBitrate)
                enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_value_minus1[0] =
                (curBRCParams.maxBitrate >> (6 + enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale)) - 1;
            else
                status = UMC_ERR_FAILED;

            if (curBRCParams.HRDBufferSizeBytes)
                enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_value_minus1[0] =
                (curBRCParams.HRDBufferSizeBytes  >> (1 + enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale)) - 1;
            else
                status = UMC_ERR_FAILED;

            enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0] = (curBRCParams.BRCMode == BRC_CBR) ? 1 : 0;
        }
        else
            status = UMC_ERR_NOT_ENOUGH_DATA;

        if (enc->m_info.use_ext_sps) {
            enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = enc->m_info.m_SeqParamSet.vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1;
            enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = enc->m_info.m_SeqParamSet.vui_parameters.hrd_params.cpb_removal_delay_length_minus1;
            enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1 = enc->m_info.m_SeqParamSet.vui_parameters.hrd_params.dpb_output_delay_length_minus1;
            enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length = enc->m_info.m_SeqParamSet.vui_parameters.hrd_params.time_offset_length;
        } else {
            enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = 23; // default
            enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = 23; // default
            enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1 = 23; // default
            enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length = 24; // default
        }

        if (enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag)
            enc->m_SeqParamSet->vui_parameters.vcl_hrd_params = enc->m_SeqParamSet->vui_parameters.hrd_params;
    }

#ifdef H264_HRD_CPB_MODEL
    enc->m_SEIData.m_tickDuration = ((Ipp64f)enc->m_SeqParamSet->vui_parameters.num_units_in_tick) / enc->m_SeqParamSet->vui_parameters.time_scale;
#endif // H264_HRD_CPB_MODEL

    // Init extCodingOption first
    if (opts != NULL)
        m_base.m_extOption = *opts;
    else {
        memset(&m_base.m_extOption, 0, sizeof(m_base.m_extOption));
        m_base.m_extOption.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
        m_base.m_extOption.Header.BufferSz = sizeof(m_base.m_extOption);
    }
    m_base.m_pExtBuffer = &m_base.m_extOption.Header;
    m_base.m_mfxVideoParam.ExtParam = &m_base.m_pExtBuffer;
    m_base.m_mfxVideoParam.NumExtParam = 1;

    st = ConvertVideoParamBack_H264enc(&m_base.m_mfxVideoParam, enc);
    MFX_CHECK_STS(st);

    // allocate empty frames to cpb
    UMC::Status ps = H264EncoderFrameList_Extend_8u16s(
        &enc->m_cpb,
        m_base.m_mfxVideoParam.mfx.NumRefFrame + (m_base.m_mfxVideoParam.mfx.EncodedOrder ? 1 : m_base.m_mfxVideoParam.mfx.GopRefDist),
        enc->m_info.info.clip_info.width,
        enc->m_info.info.clip_info.height,
        enc->m_PaddedSize,
        UMC::NV12,
        enc->m_info.num_slices * ((enc->m_info.coding_type == 1 || enc->m_info.coding_type == 3) + 1)
#if defined ALPHA_BLENDING_H264
        , enc->m_SeqParamSet->aux_format_idc
#endif
    );
    if (ps != UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;


    // allocate frame for recode
    // do it always to allow switching  // if (enc->m_Analyse & ANALYSE_RECODE_FRAME /*&& core_enc->m_pReconstructFrame == NULL //could be size change*/){ //make init reconstruct buffer
    enc->m_pReconstructFrame_allocated = (H264EncoderFrame_8u16s *)H264_Malloc(sizeof(H264EncoderFrame_8u16s));
    if (!enc->m_pReconstructFrame_allocated)
        return MFX_ERR_MEMORY_ALLOC;
    H264EncoderFrame_Create_8u16s(enc->m_pReconstructFrame_allocated, &enc->m_cpb.m_pHead->m_data, enc->memAlloc,
#if defined ALPHA_BLENDING_H264
        enc->m_SeqParamSet->aux_format_idc,
#endif // ALPHA_BLENDING_H264
        0);
    if (H264EncoderFrame_allocate_8u16s(enc->m_pReconstructFrame_allocated, enc->m_PaddedSize, enc->m_info.num_slices * ((enc->m_info.coding_type == 1 || enc->m_info.coding_type == 3) + 1)))
        return MFX_ERR_MEMORY_ALLOC;

    // allocate bitstream buffer for 2nd field in case of field-based output
    if (m_fieldOutput) {
        mfxU32 bsSize = par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height * 3;
        m_extBitstream.Data = (mfxU8*)H264_Malloc(bsSize);
        m_extBitstream.MaxLength = bsSize;
    }

    if (pSvcLayers) {
        mfxU32 layersON = 0;
        mfxU32 i0, i1;
        mfxU32 pos = 0;
        mfxI32 maxdid=0, maxtid=0;

        for ( i0 = 0; i0 < MAX_DEP_LAYERS; i0++ ) {
            if (pSvcLayers->DependencyLayer[i0].Active) {
                UMC::Status res;
                m_svc_layers[i0] = (h264Layer *)H264_Malloc( sizeof(h264Layer));
                if( m_svc_layers[i0] == NULL ) return MFX_ERR_MEMORY_ALLOC;
                memset(m_svc_layers[i0], 0, sizeof(h264Layer));

#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
                H264ENC_CALL_NEW(res, H264CoreEncoder, m_svc_layers[i0]->enc);
#undef H264ENC_MAKE_NAME
                if (res != UMC::UMC_OK) return MFX_ERR_MEMORY_ALLOC;

                m_svc_layers[i0]->enc->m_svc_layer.svc_ext.dependency_id = i0;
                m_svc_layers[i0]->m_InterLayerParam = pSvcLayers->DependencyLayer[i0];

                m_svc_layers[i0]->enc->QualityNum = pSvcLayers->DependencyLayer[i0].QualityNum;
                m_svc_layers[i0]->enc->TempIdMax = pSvcLayers->DependencyLayer[i0].TemporalNum-1;
                for (i1 = 0; i1 < MAX_TEMP_LEVELS; i1++)
                    m_svc_layers[i0]->enc->TemporalScaleList[i1] = 0;
                for (i1 = 0; i1 < pSvcLayers->DependencyLayer[i0].TemporalNum; i1++) {
                    mfxU16 tid = pSvcLayers->DependencyLayer[i0].TemporalId[i1];
                    m_svc_layers[i0]->enc->TemporalScaleList[i1] = pSvcLayers->TemporalScale[tid]; // skipping unused tid
                }

                maxdid = i0;
                maxtid = m_svc_layers[maxdid]->m_InterLayerParam.TemporalNum-1;
                m_svc_layers[i0]->enc->svc_ScalabilityInfoSEI = &m_svc_ScalabilityInfoSEI;
                layersON++;
            } else {
                m_svc_layers[i0] = 0;
            }
        }
        m_maxDid = maxdid;


#ifdef USE_TEMPTEMP
        InitGopTemplate (par); // call after m_svc_layers are assigned
#endif // USE_TEMPTEMP
        

        if (layersON && par->mfx.RateControlMethod != MFX_RATECONTROL_CQP && par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR /* avbr ??? */ ) {
            mfxI32 ii;
            mfxI32 did, qid, tid;
            memset((void*)m_HRDparams, 0, sizeof(m_HRDparams));

            for (ii = 0; ii < rc->NumLayers; ii++) {

                did = rc->Layer[ii].DependencyId;
                qid = rc->Layer[ii].QualityId;
                tid = rc->Layer[ii].TemporalId;
                mfxI32 tidx;
                for (tidx=0; tidx<pSvcLayers->DependencyLayer[did].TemporalNum; tidx++)
                    if (tid == pSvcLayers->DependencyLayer[did].TemporalId[tidx])
                        break;

                if (pSvcLayers->DependencyLayer[did].Active && 
                    (qid < pSvcLayers->DependencyLayer[did].QualityNum) && (tidx < pSvcLayers->DependencyLayer[did].TemporalNum)) {
                        m_HRDparams[did][qid][tidx].BufferSizeInKB   = rc->Layer[ii].CbrVbr.BufferSizeInKB;
                        m_HRDparams[did][qid][tidx].TargetKbps       = rc->Layer[ii].CbrVbr.TargetKbps;
                        m_HRDparams[did][qid][tidx].InitialDelayInKB = rc->Layer[ii].CbrVbr.InitialDelayInKB;
                        m_HRDparams[did][qid][tidx].MaxKbps          = rc->Layer[ii].CbrVbr.MaxKbps;
                }

            }

            st = CheckAndSetLayerBitrates(m_HRDparams, pSvcLayers, m_maxDid, par->mfx.RateControlMethod);
            if (st < 0)
                return st;


            if (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD) {
                mfxF64 framerate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD;
                st = CheckAndSetLayerHRDBuffers(m_HRDparams, pSvcLayers, m_maxDid, framerate, par->mfx.RateControlMethod);
                if (st < 0)
                    return st;
            }

            for (ii = 0; ii < rc->NumLayers; ii++) { // ??? kta
                did = rc->Layer[ii].DependencyId;
                qid = rc->Layer[ii].QualityId;
                tid = rc->Layer[ii].TemporalId;
                mfxI32 tidx;
                for (tidx=0; tidx<pSvcLayers->DependencyLayer[did].TemporalNum; tidx++)
                    if (tid == pSvcLayers->DependencyLayer[did].TemporalId[tidx])
                        break;
                if(tidx >= pSvcLayers->DependencyLayer[did].TemporalNum)
                    tidx = MAX_TEMP_LEVELS-1;

                rc->Layer[ii].CbrVbr.TargetKbps       = m_HRDparams[did][qid][tidx].TargetKbps;
                rc->Layer[ii].CbrVbr.MaxKbps          = m_HRDparams[did][qid][tidx].MaxKbps;
                rc->Layer[ii].CbrVbr.BufferSizeInKB   = m_HRDparams[did][qid][tidx].BufferSizeInKB;
                rc->Layer[ii].CbrVbr.InitialDelayInKB = m_HRDparams[did][qid][tidx].InitialDelayInKB;
            }
        }

        if (layersON) {
            struct h264Layer *biggest = 0, *biggest_prev = 0;
            mfxU32 ll;
            mfxU32 maxbufsize = 0;
            for (ll = 0; ll < MAX_DEP_LAYERS; ll++) {
                struct h264Layer *cur = m_svc_layers[ll];
                if (cur) {
                    struct h264Layer *ref = m_svc_layers[cur->m_InterLayerParam.RefLayerDid];
                    if(ref && cur != ref) {
                        cur->ref_layer = ref;
                        ref->src_layer = cur; // do not if all layers are provided(?), what if few reference one?
                    } else
                        cur->ref_layer = 0;
                    // Init layer's encoder, suppose ref was initialized before cur
                    cur->m_mfxVideoParam = m_base.m_mfxVideoParam; // to take only common parameters
                    cur->m_extOption = m_base.m_extOption;
                    mfxI32 maxTempScale = m_svc_layers[maxdid]->enc->TemporalScaleList[maxtid];
                    cur->enc->rateDivider = maxTempScale / cur->enc->TemporalScaleList[cur->enc->TempIdMax];
                    if(maxtid > 0)
                        cur->m_mfxVideoParam.mfx.NumRefFrame = checked.mfx.NumRefFrame;

                    st = InitSVCLayer(rc, ll, cur);
                    MFX_CHECK_STS(st);
                    m_svc_count ++;
                    if (cur->enc->requiredBsBufferSize > maxbufsize)
                        maxbufsize = cur->enc->requiredBsBufferSize;
                    if(!biggest ||
                        biggest->m_mfxVideoParam.mfx.FrameInfo.Width * biggest->m_mfxVideoParam.mfx.FrameInfo.Height <
                        cur->m_mfxVideoParam.mfx.FrameInfo.Width * cur->m_mfxVideoParam.mfx.FrameInfo.Height) {
                        biggest_prev = biggest;
                        biggest = cur;
                    }
                }
            }
            if (maxbufsize > 0) {
                m_base.enc->requiredBsBufferSize = maxbufsize;
                m_base.m_mfxVideoParam.calcParam.BufferSizeInKB = (maxbufsize + 999)/1000; // so that GetVideoParam return the max buffer size over all spatial layers (???) kta
            }
            // disable deblocking in SVC for awhile, except Temporal scalability
            for (pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
                if (!m_svc_layers[pos]) continue;
                if (m_svc_count > 0 /*|| m_svc_layers[pos]->enc->QualityNum > 1*/) // TODO check about QualityNum
                    m_svc_layers[pos]->enc->m_info.deblocking_filter_idc =
                        (m_svc_layers[pos]->m_InterLayerParam.DisableDeblockingFilter == MFX_CODINGOPTION_ON) ?
                        DEBLOCK_FILTER_OFF : DEBLOCK_FILTER_ON;
            }

            if (biggest) {
                PIXTYPE *yPlane, *uPlane, *vPlane;
                Ipp32s sz2 = 0, len2 = 0;
                // big buffers for svc resampling, provided from upper component
                Ipp32s sz = (biggest->m_mfxVideoParam.mfx.FrameInfo.Width + 4) * (biggest->m_mfxVideoParam.mfx.FrameInfo.Height + 4);
                sz = sz * 3 / 2; // 420 only
                Ipp32s len = sizeof(Ipp32s)*(2*sz + biggest->m_mfxVideoParam.mfx.FrameInfo.Height+32 +2*ALIGN_VALUE);
                Ipp32s pitchInBytes = 0;

                if (biggest_prev) {
                    pitchInBytes = CalcPitchFromWidth(biggest_prev->m_mfxVideoParam.mfx.FrameInfo.Width, sizeof(PIXTYPE));
                    sz2 = (pitchInBytes + 4) * (biggest_prev->m_mfxVideoParam.mfx.FrameInfo.Height + 32);
                    len2 = sizeof(PIXTYPE)*(sz2*3/2 +2*ALIGN_VALUE);
                }

                allocated_tmpBuf = (Ipp32s*)H264_Malloc(len + len2); // upmost layer size
                if( !allocated_tmpBuf ) return MFX_ERR_MEMORY_ALLOC;

                m_tmpBuf0 = align_pointer<Ipp32s*> (allocated_tmpBuf, ALIGN_VALUE);
                m_tmpBuf2 = align_pointer<Ipp32s*> (m_tmpBuf0 + sz, ALIGN_VALUE);
                m_tmpBuf1 = align_pointer<Ipp32s*> (m_tmpBuf2 + sz, ALIGN_VALUE);
                if (biggest_prev) {
                    yPlane = align_pointer<PIXTYPE*> ((Ipp8u*)m_tmpBuf0 + len, ALIGN_VALUE);
                    uPlane = align_pointer<PIXTYPE*> (yPlane + sz2, ALIGN_VALUE);
                    yPlane += 16 + pitchInBytes * 16;
                    uPlane += 16 + pitchInBytes * 8;
                    vPlane = uPlane + 1; // NV12 only
                } else {
                    yPlane = uPlane = vPlane = NULL;
                }

                for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
                    if (!m_svc_layers[pos]) continue;
                    m_svc_layers[pos]->enc->m_tmpBuf0 = m_tmpBuf0;
                    m_svc_layers[pos]->enc->m_tmpBuf1 = m_tmpBuf1;
                    m_svc_layers[pos]->enc->m_tmpBuf2 = m_tmpBuf2;
                    m_svc_layers[pos]->enc->m_pYDeblockingILPlane = yPlane;
                    m_svc_layers[pos]->enc->m_pUDeblockingILPlane = uPlane;
                    m_svc_layers[pos]->enc->m_pVDeblockingILPlane = vPlane;
                }
            }
        }

        st = SetScalabilityInfoSE();
        MFX_CHECK_STS(st);
    }
    else {
        enc->QualityNum = 1;
        enc->TempIdMax = 0;
        enc->m_spatial_resolution_change = false;
        enc->m_restricted_spatial_resolution_change = true;
        //enc->m_next_spatial_resolution_change = false;
        enc->rateDivider = 1; //m_maxTempScale / layer->enc->TemporalScaleList[layer->enc->TempIdMax];
#ifdef USE_TEMPTEMP
        InitGopTemplate (par); // call after m_svc_layers are assigned or for AVC
        enc->refctrl = m_refctrl; // For temporal; contains index in GOP of the last frame, which refers to it (display order)
        enc->temptempstart = &m_temptempstart; // FrameTag for first frame in temporal template
        enc->temptemplen = m_temptemplen; // length of the temporal template
        enc->temptempwidth = m_temptempwidth; // step back when went to the end of template
        enc->usetemptemp = &m_usetemptemp; // allow use
#endif // USE_TEMPTEMP
    }

#ifdef H264_NEW_THREADING
    m_taskParams.threads_data = NULL;

    vm_event_set_invalid(&m_taskParams.parallel_region_done);
    if (VM_OK != vm_event_init(&m_taskParams.parallel_region_done, 0, 0))
        return MFX_ERR_MEMORY_ALLOC;

    m_taskParams.threads_data = (threadSpecificDataH264*)H264_Malloc(sizeof(threadSpecificDataH264)  * m_base.m_mfxVideoParam.mfx.NumThread);
    if (0 == m_taskParams.threads_data)
        return MFX_ERR_MEMORY_ALLOC;

    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;
    for (mfxI32 i = 0; i < enc->m_info.numThreads; i++)
    {
        m_taskParams.threads_data[i].core_enc = NULL;
        m_taskParams.threads_data[i].state = NULL;
        m_taskParams.threads_data[i].slice = 0;
        m_taskParams.threads_data[i].slice_qp_delta_default = 0;
        m_taskParams.threads_data[i].default_slice_type = INTRASLICE;
    }
    m_taskParams.single_thread_done = false;
#else
#ifdef VM_SLICE_THREADING_H264
    // threads preparation
    Ipp32s i;
    Ipp32s numThreads = par->mfx.NumThread;

    if (numThreads != 1) {
        if (numThreads == 0)
            numThreads = vm_sys_info_get_cpu_num();
        if (numThreads < 1) {
            numThreads = 1;
        }

        Ipp32s maxThreads;
        if (enc->useMBT)
            maxThreads = numThreads;
        else
            maxThreads = enc->m_info.num_slices;

        if (maxThreads < 1 ){
            maxThreads = 1;
        }

        if (numThreads > maxThreads) {
            numThreads = maxThreads;
        }
    }

    // add threads if was fewer
    if(threadsAllocated < numThreads) {
        threadSpecificDataH264* cur_threadSpec = threadSpec;
        threadSpec = (threadSpecificDataH264*)H264_Malloc(sizeof(threadSpecificDataH264)  * numThreads);
        // copy existing
        if(threadsAllocated) {
            ippsCopy_8u((Ipp8u*)cur_threadSpec, (Ipp8u*)threadSpec,
            threadsAllocated*sizeof(threadSpecificDataH264));
            H264_Free(cur_threadSpec);
        }
        if(numThreads > 1) {
            threadInfoH264** cur_threads = threads;
            threads = (threadInfoH264**)H264_Malloc(sizeof(threadInfoH264*) * (numThreads - 1));
            if(threadsAllocated > 1) {
                for (i = 0; i < threadsAllocated - 1; i++)
                    threads[i] = cur_threads[i];
                H264_Free(cur_threads);
            }
        }
      // init newly allocated
      for (i = threadsAllocated; i < numThreads; i++) {
          threadSpec[i].core_enc = NULL;
          threadSpec[i].state = NULL;
          threadSpec[i].slice = 0;
          threadSpec[i].slice_qp_delta_default = 0;
          threadSpec[i].default_slice_type = INTRASLICE;

          if(i>0) {
              threads[i-1] = (threadInfoH264*)H264_Malloc(sizeof(threadInfoH264));
              vm_event_set_invalid(&threads[i-1]->start_event);
              if (VM_OK != vm_event_init(&threads[i-1]->start_event, 0, 0))
                  return h264enc_ConvertStatus(UMC_ERR_ALLOC);
              vm_event_set_invalid(&threads[i-1]->stop_event);
              if (VM_OK != vm_event_init(&threads[i-1]->stop_event, 0, 0))
                  return h264enc_ConvertStatus(UMC_ERR_ALLOC);
              vm_event_set_invalid(&threads[i-1]->quit_event);
              if (VM_OK != vm_event_init(&threads[i-1]->quit_event, 0, 0))
                  return h264enc_ConvertStatus(UMC_ERR_ALLOC);
              vm_thread_set_invalid(&threads[i-1]->thread);
              threads[i-1]->numTh = i;
              threads[i-1]->m_lpOwner = this;
              if (0 == vm_thread_create(&threads[i-1]->thread,
                                    ThreadWorkingRoutine,
                                    threads[i-1])) {
              return h264enc_ConvertStatus(UMC_ERR_ALLOC);
              }
          }
      }
      threadsAllocated = numThreads;
    }
#endif // VM_SLICE_THREADING_H264
#endif // H264_NEW_THREADING
    st = h264enc_ConvertStatus(status);
    if (st == MFX_ERR_NONE)
    {
        m_base.m_Initialized = true;
        return (QueryStatus != MFX_ERR_NONE) ? QueryStatus : (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
    }
    else
        return st;
}
#undef PIXTYPE

mfxStatus MFXVideoENCODEH264::Close()
{
    H264CoreEncoder_8u16s* enc = m_base.enc;
    if( !m_base.m_Initialized ) return MFX_ERR_NOT_INITIALIZED;

    mfxU32 pos;
    for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
        if (!m_svc_layers[pos]) continue;
        CloseSVCLayer( m_svc_layers[pos]);
    }
    m_svc_count = 0;

    ReleaseBufferedFrames(&m_base);
    H264CoreEncoder_Close_8u16s(enc);
    H264EncoderFrameList_clearFrameList_8u16s(&enc->m_dpb);
    H264EncoderFrameList_clearFrameList_8u16s(&enc->m_cpb);
    if (enc->m_pReconstructFrame_allocated) {
        H264EncoderFrame_Destroy_8u16s(enc->m_pReconstructFrame_allocated);
        H264_Free(enc->m_pReconstructFrame_allocated);
        enc->m_pReconstructFrame_allocated = 0;
    }

    if (m_base.m_brc[0])
    {
      m_base.m_brc[0]->Close();
      delete m_base.m_brc[0];
      m_base.m_brc[0] = 0;
    }

    ////if (m_VideoParams_id){
    ////    m_core->UnlockBuffer(m_VideoParams_id);
    ////    m_core->FreeBuffer(m_VideoParams_id);
    ////    m_VideoParams_id = 0;
    ////}
    if (m_allocator) {
        delete m_allocator;
        m_allocator = 0;
    }

#ifdef H264_NEW_THREADING
    vm_event_destroy(&m_taskParams.parallel_region_done);
    if (m_taskParams.threads_data)
    {
        H264_Free(m_taskParams.threads_data);
        m_taskParams.threads_data = 0;
    }
#else
#ifdef VM_SLICE_THREADING_H264
    Ipp32s i;
    /*
  if (!threadSpec) {
    return UMC_ERR_NULL_PTR;
  }
*/
    if (threadsAllocated) {
    // let all threads to exit
        if (threadsAllocated > 1) {
            for (i = 0; i < threadsAllocated - 1; i++) {
                vm_event_signal(&threads[i]->quit_event);
                vm_event_signal(&threads[i]->start_event);
            }

            for (i = 0; i < threadsAllocated - 1; i++)
            {
                if (&threads[i]->thread) {
                    vm_thread_close(&threads[i]->thread);
                    vm_thread_set_invalid(&threads[i]->thread);
                }
                if (vm_event_is_valid(&threads[i]->start_event)) {
                    vm_event_destroy(&threads[i]->start_event);
                    vm_event_set_invalid(&threads[i]->start_event);
                }
                if (vm_event_is_valid(&threads[i]->stop_event)) {
                    vm_event_destroy(&threads[i]->stop_event);
                    vm_event_set_invalid(&threads[i]->stop_event);
                }
                if (vm_event_is_valid(&threads[i]->quit_event)) {
                    vm_event_destroy(&threads[i]->quit_event);
                    vm_event_set_invalid(&threads[i]->quit_event);
                }
                H264_Free(threads[i]);
            }

            H264_Free(threads);
        }

        for(i = 0; i < threadsAllocated; i++)
        {
            threadSpec[i].core_enc = NULL;
            threadSpec[i].state = NULL;
            threadSpec[i].slice = 0;
            threadSpec[i].slice_qp_delta_default = 0;
            threadSpec[i].default_slice_type = INTRASLICE;
        }

        H264_Free(threadSpec);
        threadsAllocated = 0;
    }
#endif // VM_SLICE_THREADING_H264
#endif // H264_NEW_THREADING

    if (m_fieldOutput && m_extBitstream.Data != 0) {
        H264_Free(m_extBitstream.Data);
        m_extBitstream.Data = 0;
    }

    if (m_base.m_useAuxInput || m_useSysOpaq) {
        //st = m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        m_core->FreeFrames(&m_base.m_response);
        m_base.m_response.NumFrameActual = 0;
        m_base.m_useAuxInput = false;
        m_useSysOpaq = false;
    }

    if (m_useVideoOpaq) {
        m_core->FreeFrames(&m_base.m_response_alien);
        m_base.m_response_alien.NumFrameActual = 0;
        m_useVideoOpaq = false;
    }


    if (allocated_tmpBuf) {
        H264_Free(allocated_tmpBuf);
        allocated_tmpBuf = 0;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::ReleaseBufferedFrames(h264Layer* layer)
{
    H264CoreEncoder_8u16s* enc = layer->enc;
    if (enc->m_pNextFrame) { // return future frame to cpb array
        H264EncoderFrameList_insertAtCurrent_8u16s(&enc->m_cpb, enc->m_pNextFrame);
        enc->m_pNextFrame = 0;
    }

    UMC_H264_ENCODER::sH264EncoderFrame_8u16s *pCurr = enc->m_cpb.m_pHead;

    while (pCurr) {
        if (!pCurr->m_wasEncoded && pCurr->m_pAux[1]) {
            mfxFrameSurface1* surf = (mfxFrameSurface1*)pCurr->m_pAux[1];
            m_core->DecreaseReference(&(surf->Data));
            pCurr->m_pAux[0] = pCurr->m_pAux[1] = 0; // to prevent repeated Release
        }
        pCurr = pCurr->m_pFutureFrame;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::Reset(mfxVideoParam *par_in)
{
    mfxStatus st, QueryStatus;
    UMC::H264EncoderParams videoParams;
    UMC::VideoBrcParams brcParams;
    Status status;
    H264CoreEncoder_8u16s* enc = m_base.enc;

    if( !m_base.m_Initialized ) return MFX_ERR_NOT_INITIALIZED;

    if( par_in == NULL ) return MFX_ERR_NULL_PTR;
    st = CheckExtBuffers_H264enc( par_in->ExtParam, par_in->NumExtParam );
    if (st != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOption* opts = GetExtCodingOptions( par_in->ExtParam, par_in->NumExtParam );
    mfxExtCodingOptionSPSPPS* optsSP = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtPictureTimingSEI* picTimingSei = (mfxExtPictureTimingSEI*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );
    mfxExtAvcTemporalLayers* tempLayers = (mfxExtAvcTemporalLayers*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
    mfxExtCodingOption2* opts2 = (mfxExtCodingOption2*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );
    if(GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
        return MFX_ERR_INVALID_VIDEO_PARAM;
    mfxExtSVCSeqDesc* pSvcLayers = (mfxExtSVCSeqDesc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
    mfxExtSVCRateControl *rc = (mfxExtSVCRateControl*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );
    bool hasTemporal = false;

    /* Check */
    if (pSvcLayers) {
        mfxU32 i0;
        bool hasActive = false;
        for ( i0 = 0; i0 < MAX_DEP_LAYERS; i0++ ) {
            if (pSvcLayers->DependencyLayer[i0].Active) {
                hasActive = true;
                if (pSvcLayers->DependencyLayer[i0].TemporalNum > 1) {
                    hasTemporal = true; // more than single temporal layer
                    break;
                }
            }
        }
        if (!hasActive)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxVideoParam checked;
    mfxExtCodingOption checked_ext;
    mfxExtCodingOptionSPSPPS checked_extSP;
    mfxExtVideoSignalInfo checked_videoSignalInfo;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtPictureTimingSEI checked_picTimingSei;
    mfxExtAvcTemporalLayers checked_tempLayers;
    mfxExtSVCSeqDesc checked_svcLayers;
    mfxExtSVCRateControl checked_svcRC;
    mfxExtCodingOption2 checked_ext2;
    mfxExtBuffer *ptr_checked_ext[9] = {0,};
    mfxU16 ext_counter = 0;
    checked = *par_in;
    if (opts) {
        checked_ext = *opts;
        ptr_checked_ext[ext_counter++] = &checked_ext.Header;
    } else {
        checked_ext = m_base.m_extOption;
        ptr_checked_ext[ext_counter++] = &checked_ext.Header;
    }
    if (optsSP) {
        checked_extSP = *optsSP;
        ptr_checked_ext[ext_counter++] = &checked_extSP.Header;
    } else {
        memset(&checked_extSP, 0, sizeof(checked_extSP));
        checked_extSP.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
        checked_extSP.Header.BufferSz = sizeof(checked_extSP);
    }
    if (videoSignalInfo) {
        checked_videoSignalInfo = *videoSignalInfo;
        ptr_checked_ext[ext_counter++] = &checked_videoSignalInfo.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (picTimingSei) {
        checked_picTimingSei = *picTimingSei;
        ptr_checked_ext[ext_counter++] = &checked_picTimingSei.Header;
    }
    if (tempLayers) {
        checked_tempLayers = *tempLayers;
        ptr_checked_ext[ext_counter++] = &checked_tempLayers.Header;
    }
    if (pSvcLayers) {
        checked_svcLayers = *pSvcLayers;
        ptr_checked_ext[ext_counter++] = &checked_svcLayers.Header;
    }
    if (rc) {
        checked_svcRC = *rc;
        ptr_checked_ext[ext_counter++] = &checked_svcRC.Header;
    }
    if (opts2) {
        checked_ext2 = *opts2;
        ptr_checked_ext[ext_counter++] = &checked_ext2.Header;
    }

    checked.ExtParam = ptr_checked_ext;
    checked.NumExtParam = ext_counter;
    
    SetDefaultParamForReset(checked, m_base.m_mfxVideoParam);

    st = Query(&checked, &checked);

    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && st != MFX_WRN_PARTIAL_ACCELERATION) {
        if (st == MFX_ERR_UNSUPPORTED && optsSP == 0) // SPS/PPS header isn't attached. Error is in mfxVideoParam. Return MFX_ERR_INVALID_VIDEO_PARAM
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else if (st == MFX_ERR_UNSUPPORTED) // SPSPPS header is attached. Return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        else
            return st;
    }
    QueryStatus = st;
    mfxVideoInternalParam inInt = checked;
    mfxVideoInternalParam* par = &inInt;
    opts = &checked_ext;
    if (picTimingSei)
        picTimingSei = &checked_picTimingSei;

    if (tempLayers && par->mfx.EncodedOrder) {
        tempLayers = 0;
        QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (tempLayers)
        tempLayers = &checked_tempLayers;
    if (pSvcLayers)
        pSvcLayers = &checked_svcLayers;
    if (rc)
        rc = &checked_svcRC;
    //optsSP = &checked_extSP;

#ifdef H264_INTRA_REFRESH
    enc->m_QPDelta = 0;
#endif // H264_INTRA_REFRESH

    if (opts2) {
        opts2 = &checked_ext2;
#ifdef H264_INTRA_REFRESH
        enc->m_refrType = opts2->IntRefType;
        if(enc->m_refrType) {
            if(par->mfx.GopRefDist == 0)
                par->mfx.GopRefDist = 1;
            mfxU16 refreshSizeInMBs =  enc->m_refrType == HORIZ_REFRESH ? par->mfx.FrameInfo.Height >> 4 : par->mfx.FrameInfo.Width >> 4;
            enc->m_stopRefreshFrames = 0;
            if (opts2->IntRefCycleSize <= refreshSizeInMBs) {
                enc->m_stripeWidth = (refreshSizeInMBs + opts2->IntRefCycleSize - 1) / opts2->IntRefCycleSize;
                mfxU16 refreshTime = (refreshSizeInMBs + enc->m_stripeWidth - 1) / enc->m_stripeWidth;
                enc->m_stopRefreshFrames = opts2->IntRefCycleSize - refreshTime;
            } else {
                enc->m_stopRefreshFrames = opts2->IntRefCycleSize - refreshSizeInMBs;
                enc->m_stripeWidth = 1;
            }
            enc->m_refrSpeed = enc->m_stripeWidth;
            enc->m_periodCount = 0;
            enc->m_stopFramesCount = 0;
            enc->m_QPDelta = opts2->IntRefQPDelta;
        } else {
            enc->m_stopRefreshFrames = 0;
            enc->m_refrSpeed = 0;
            enc->m_periodCount = 0;
            enc->m_stopFramesCount = 0;
            enc->m_QPDelta = 0;
        }
#endif // H264_INTRA_REFRESH
    }

    // no HRD syntax in bitstream if HRD-conformance is OFF
    if (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF) {
        opts->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
        opts->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    // MSDK doesn't support true VCL conformance. We use NAL HRD settings for VCL conformance in case of VBR
    if (opts && opts->VuiVclHrdParameters == MFX_CODINGOPTION_ON && par->mfx.RateControlMethod != MFX_RATECONTROL_VBR) {
        QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        opts->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0 ||
        par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 || par->calcParam.TargetKbps == 0 ||
        par->mfx.FrameInfo.FrameRateExtN == 0 || par->mfx.FrameInfo.FrameRateExtD == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // checks for opaque memory
    if (!(m_base.m_mfxVideoParam.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) && (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if (opaqAllocReq != 0 && opaqAllocReq->In.NumSurface != m_base.m_mfxVideoParam.mfx.GopRefDist)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    enc->m_info.m_Analyse_on = 0;
    enc->m_info.m_Analyse_restrict = 0;
    enc->m_info.m_Analyse_ex = 0;

    // convert new params (including parameters from attached SPS/PPS headers (if any))
    memset(&videoParams.m_SeqParamSet, 0, sizeof(UMC::H264SeqParamSet));
    memset(&videoParams.m_PicParamSet, 0, sizeof(UMC::H264PicParamSet));
    videoParams.use_ext_sps = videoParams.use_ext_pps = false;
    videoParams.m_ext_SPS_id = videoParams.m_ext_PPS_id = 0;
    memset(videoParams.m_ext_constraint_flags, 0, sizeof(videoParams.m_ext_constraint_flags));
    st = ConvertVideoParam_H264enc(par, &videoParams); // converts all with preference rules
    if (st < MFX_ERR_NONE)
        return st;
    else if (st != MFX_ERR_NONE)
        QueryStatus = st;

    // to set profile/level and related vars
    // first copy fields which could have been changed from TU
    mfxVideoInternalParam checkedSetByTU = mfxVideoInternalParam();
    if (par->mfx.CodecProfile == MFX_PROFILE_UNKNOWN && videoParams.transform_8x8_mode_flag)
        par->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    if (par->mfx.GopRefDist == 0) {
        checkedSetByTU.mfx.GopRefDist = 1;
        par->mfx.GopRefDist = (mfxU16)(videoParams.B_frame_rate + 1);
    }
    if (par->mfx.NumRefFrame == 0) {
        checkedSetByTU.mfx.NumRefFrame = 1;
        par->mfx.NumRefFrame = (mfxU16)videoParams.num_ref_frames;
    }
    if (checked_ext.CAVLC == MFX_CODINGOPTION_UNKNOWN)
        checked_ext.CAVLC = (mfxU16)(videoParams.entropy_coding_mode ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);

    // to set profile/level and related vars
    st = CheckProfileLevelLimits_H264enc(par, true);
    if (st != MFX_ERR_NONE)
        QueryStatus = st;

    if (par->mfx.CodecProfile > MFX_PROFILE_AVC_BASELINE && par->mfx.CodecLevel >= MFX_LEVEL_AVC_3)
        videoParams.use_direct_inference = 1;
    // get back mfx params to umc
    videoParams.profile_idc = (UMC::H264_PROFILE_IDC)ConvertCUCProfileToUMC( par->mfx.CodecProfile );
    videoParams.level_idc = ConvertCUCLevelToUMC( par->mfx.CodecLevel );
    videoParams.num_ref_frames = par->mfx.NumRefFrame;

    // apply level-dependent restriction on vertical MV range
    mfxU16 maxVertMV = GetMaxVmvR(par->mfx.CodecLevel);
    if (maxVertMV && maxVertMV < videoParams.max_mv_length_y)
        videoParams.max_mv_length_y = maxVertMV;

    // get previous (before Reset) encoding params
    st = ConvertVideoParamBack_H264enc(&m_base.m_mfxVideoParam, enc);

    // check that new params don't require allocation of additional memory
    if (par->mfx.FrameInfo.Width > m_initValues.FrameWidth ||
        par->mfx.FrameInfo.Height > m_initValues.FrameHeight ||
        m_base.m_mfxVideoParam.mfx.FrameInfo.ChromaFormat != par->mfx.FrameInfo.ChromaFormat ||
        m_base.m_mfxVideoParam.mfx.GopRefDist < (videoParams.B_frame_rate + 1) ||
        (par->mfx.NumSlice != 0 && enc->m_info.num_slices != par->mfx.NumSlice) ||
        m_base.m_mfxVideoParam.mfx.NumRefFrame < videoParams.num_ref_frames ||
        (m_base.m_mfxVideoParam.mfx.CodecProfile & 0xFF) != (par->mfx.CodecProfile & 0xFF) ||
        (par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && enc->m_info.coding_type == 0) )
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if ((videoParams.profile_idc == H264_BASE_PROFILE &&
        par->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE) || tempLayers)
        videoParams.m_ext_constraint_flags[1] = 1;
    else if (videoParams.profile_idc == H264_HIGH_PROFILE &&
        par->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        videoParams.m_ext_constraint_flags[4] = videoParams.m_ext_constraint_flags[5] = 1;
    else if (videoParams.profile_idc == H264_HIGH_PROFILE &&
        par->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH)
        videoParams.m_ext_constraint_flags[4] = 1;

    // save obsolete SPS
    H264SeqParamSet  obsoleteSeqParamSet = *enc->m_SeqParamSet;

    // apply new params
    enc->m_info = videoParams;

    MFX_CHECK_STS(st);

    mfxVideoParam tmp = *par;
    par->GetCalcParams(&tmp);

    st = ConvertVideoParam_Brc(&tmp, &brcParams);
    MFX_CHECK_STS(st);
    brcParams.GOPPicSize = enc->m_info.key_frame_controls.interval;
    brcParams.GOPRefDist = enc->m_info.B_frame_rate + 1;
    brcParams.profile = enc->m_info.profile_idc;
    brcParams.level = enc->m_info.level_idc;

    // ignore brc params to preserve current BRC HRD states in case of HRD-compliant encoding
    if ((m_base.m_extOption.NalHrdConformance ==  MFX_CODINGOPTION_OFF ||
        (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF)) &&
        par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        if (m_base.m_brc[0]){
            m_base.m_brc[0]->Close();
        }else
            m_base.m_brc[0] = new UMC::H264BRC;
        status = m_base.m_brc[0]->Init(&brcParams);
        st = h264enc_ConvertStatus(status);
        MFX_CHECK_STS(st);
    } else if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        if (m_base.m_brc[0])
            status = m_base.m_brc[0]->Reset(&brcParams);
        else {
            m_base.m_brc[0] = new UMC::H264BRC;
            status = m_base.m_brc[0]->Init(&brcParams);
        }
        if (status == UMC_ERR_INVALID_PARAMS)
            st = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        else
            st = h264enc_ConvertStatus(status);
        MFX_CHECK_STS(st);
    }

    if (par->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
        m_base.m_brc[0] = NULL;
        enc->brc = NULL;
    }
    else
        enc->brc = m_base.m_brc[0];
    //enc->brc = NULL;

    // Set low initial QP values to prevent BRC panic mode if superfast mode
    if(par->mfx.TargetUsage==MFX_TARGETUSAGE_BEST_SPEED && m_base.m_brc[0]) {
        if(m_base.m_brc[0]->GetQP(I_PICTURE) < 30) {
            m_base.m_brc[0]->SetQP(30, I_PICTURE); m_base.m_brc[0]->SetQP(30, B_PICTURE);
        }
    }

    if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        enc->m_info.coding_type = 0; // frames only
    else {
        if( opts && opts->FramePicture== MFX_CODINGOPTION_ON )
            enc->m_info.coding_type = 2; // MBAFF
        else
            enc->m_info.coding_type = 3; // PicAFF
    }

    if (enc->m_info.m_Analyse_on){
        if (enc->m_info.transform_8x8_mode_flag == 0)
            enc->m_info.m_Analyse_on &= ~ANALYSE_I_8x8;
        if (enc->m_info.entropy_coding_mode == 0)
            enc->m_info.m_Analyse_on &= ~(ANALYSE_RD_OPT | ANALYSE_RD_MODE | ANALYSE_B_RD_OPT);
        if (enc->m_info.coding_type && enc->m_info.coding_type != 2) // TODO: remove coding_type != 2 when MBAFF is implemented
            enc->m_info.m_Analyse_on &= ~ANALYSE_ME_AUTO_DIRECT;
        if (par->mfx.GopOptFlag & MFX_GOP_STRICT)
            enc->m_info.m_Analyse_on &= ~ANALYSE_FRAME_TYPE;
    }

    // set like in par-files
    if( par->mfx.RateControlMethod == MFX_RATECONTROL_CBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_CBR;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_CQP )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_QUANT;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_AVBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_AVBR;
    else
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_VBR;

    if (videoParams.use_ext_sps == false) {
        H264CoreEncoder_SetSequenceParameters_8u16s(enc);
    }

    // apply restrictions on MV length from external SPS
    if (videoParams.use_ext_sps == true && videoParams.m_SeqParamSet.vui_parameters_present_flag &&
        videoParams.m_SeqParamSet.vui_parameters.bitstream_restriction_flag)
    {
        mfxI32 xLimit = 1 << videoParams.m_SeqParamSet.vui_parameters.log2_max_mv_length_horizontal;
        xLimit -= 1; // assure that MV is less then 2^n - 1
        xLimit >>= 2;
        if (xLimit && xLimit < videoParams.max_mv_length_x)
            videoParams.max_mv_length_x = xLimit;
        mfxI32 yLimit = 1 << videoParams.m_SeqParamSet.vui_parameters.log2_max_mv_length_vertical;
        yLimit -= 1; // assure that MV is less then 2^n - 1
        yLimit >>= 2;
        if (yLimit && yLimit < videoParams.max_mv_length_y)
            videoParams.max_mv_length_y = yLimit;

    }

    // set VUI aspect ration info
    if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
    if( par->mfx.FrameInfo.AspectRatioW != 0 && par->mfx.FrameInfo.AspectRatioH != 0 ){
        // enc->m_info.aspect_ratio_idc contains aspect_ratio_idc from attachecd sps/pps buffer
        enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = enc->m_info.aspect_ratio_idc == 255 ? 0 : (Ipp8u)ConvertSARtoIDC_H264enc(par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);
        if (enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 0) {
            enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = 255; //Extended SAR
            enc->m_SeqParamSet->vui_parameters.sar_width = par->mfx.FrameInfo.AspectRatioW;
            enc->m_SeqParamSet->vui_parameters.sar_height = par->mfx.FrameInfo.AspectRatioH;
        }
    }

    // set VUI video signal info
    if (videoSignalInfo != NULL) {
        if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = 1;
        enc->m_SeqParamSet->vui_parameters.video_format = videoSignalInfo->VideoFormat;
        enc->m_SeqParamSet->vui_parameters.video_full_range_flag = videoSignalInfo->VideoFullRange ? 1 : 0;
        enc->m_SeqParamSet->vui_parameters.colour_description_present_flag = videoSignalInfo->ColourDescriptionPresent ? 1 : 0;
        if (enc->m_SeqParamSet->vui_parameters.colour_description_present_flag) {
            enc->m_SeqParamSet->vui_parameters.colour_primaries = videoSignalInfo->ColourPrimaries;
            enc->m_SeqParamSet->vui_parameters.transfer_characteristics = videoSignalInfo->TransferCharacteristics;
            enc->m_SeqParamSet->vui_parameters.matrix_coefficients = videoSignalInfo->MatrixCoefficients;
        }
    } else {
        enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = 0;
        enc->m_SeqParamSet->vui_parameters.video_format = 0;
        enc->m_SeqParamSet->vui_parameters.video_full_range_flag = 0;
        enc->m_SeqParamSet->vui_parameters.colour_description_present_flag = 0;
        enc->m_SeqParamSet->vui_parameters.colour_primaries = 0;
        enc->m_SeqParamSet->vui_parameters.transfer_characteristics = 0;
        enc->m_SeqParamSet->vui_parameters.matrix_coefficients = 0;
    }

    // set VUI timing info
    if (par->mfx.FrameInfo.FrameRateExtD && par->mfx.FrameInfo.FrameRateExtN) {
        if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.timing_info_present_flag = 1;
        enc->m_SeqParamSet->vui_parameters.num_units_in_tick = par->mfx.FrameInfo.FrameRateExtD;
        enc->m_SeqParamSet->vui_parameters.time_scale = par->mfx.FrameInfo.FrameRateExtN * 2;
        enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = 1;
    }
    else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // reset temporal layers info
    if (tempLayers) {
        memset(&m_temporalLayers, 0, sizeof(AVCTemporalLayers));
        mfxU16 i = 0;
        while (i < 4 && tempLayers->Layer[i].Scale) {
            m_temporalLayers.LayerScales[i] = tempLayers->Layer[i].Scale;
            m_temporalLayers.LayerPIDs[i] = tempLayers->BaseLayerPID + i;
            i ++;
        }
        m_temporalLayers.NumLayers = i;

        if ((enc->m_info.num_ref_frames == 3 || enc->m_info.num_ref_frames == 2) && m_temporalLayers.NumLayers > 3) {
            m_temporalLayers.NumLayers = 3;
            QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        else if (enc->m_info.num_ref_frames == 1 && m_temporalLayers.NumLayers > 2) {
            m_temporalLayers.NumLayers = 2;
            QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (m_temporalLayers.NumLayers > 2)
        enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = 1;
    else if (enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag == 0)
        enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = hasTemporal;
    enc->m_SeqParamSet->vui_parameters.chroma_loc_info_present_flag = 0;

    if (opts != NULL)
    {
        if( opts->MaxDecFrameBuffering != MFX_CODINGOPTION_UNKNOWN){
            if ( opts->MaxDecFrameBuffering < enc->m_info.num_ref_frames)
                opts->MaxDecFrameBuffering = enc->m_info.num_ref_frames;
            if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag = true;
            enc->m_SeqParamSet->vui_parameters.motion_vectors_over_pic_boundaries_flag = 1;
            enc->m_SeqParamSet->vui_parameters.max_bytes_per_pic_denom = 2;
            enc->m_SeqParamSet->vui_parameters.max_bits_per_mb_denom = 1;
            enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_horizontal = 11;
            enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_vertical = 11;
            enc->m_SeqParamSet->vui_parameters.num_reorder_frames = GetNumReorderFrames(enc->m_info.B_frame_rate, !!enc->m_info.treat_B_as_reference);
            enc->m_SeqParamSet->vui_parameters.max_dec_frame_buffering = opts->MaxDecFrameBuffering;
        }

        if( opts->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN || opts->VuiNalHrdParameters == MFX_CODINGOPTION_ON)
        {
            enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
            if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP && par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
                enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 1;
            }
            else {
                enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
                if (opts->PicTimingSEI != MFX_CODINGOPTION_ON)
                    enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 0;
            }
        }else {
            enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
            if (opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
        }

        if( opts->VuiVclHrdParameters == MFX_CODINGOPTION_ON)
        {
            enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
            if (enc->m_info.use_ext_sps == false) enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag = 1;
        }else {
            enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag = 0;
            if (opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
        }

        if (opts->SingleSeiNalUnit == MFX_CODINGOPTION_OFF) m_separateSEI = true;
        else m_separateSEI = false; // default behavior is to put all SEI in one NAL unit
    }

    // MSDK doesn't support true VCL conformance. We use NAL HRD settings for VCL conformance in case of VBR
    if (!pSvcLayers)
    if (enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag == 1 ||
        enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag == 1)
    {
        VideoBrcParams  curBRCParams;

        enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_cnt_minus1 = 0;
        enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale = 0;
        enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale = 3;

        if (enc->brc)
        {
            enc->brc->GetParams(&curBRCParams);

            if (curBRCParams.maxBitrate)
                enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_value_minus1[0] =
                (curBRCParams.maxBitrate >> (6 + enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale)) - 1;
            else
                status = UMC_ERR_FAILED;

            if (curBRCParams.HRDBufferSizeBytes)
                enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_value_minus1[0] =
                (curBRCParams.HRDBufferSizeBytes  >> (1 + enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale)) - 1;
            else
                status = UMC_ERR_FAILED;

            enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0] = (curBRCParams.BRCMode == BRC_CBR) ? 1 : 0;
        }
        else
            status = UMC_ERR_NOT_ENOUGH_DATA;

        enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = 23; // default
        enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = 23; // default
        enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1 = 23; // default
        enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length = 24; // default

        if (enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag == 1)
            enc->m_SeqParamSet->vui_parameters.vcl_hrd_params = enc->m_SeqParamSet->vui_parameters.hrd_params;
    }

    mfxU8 isBitrateChanged = m_base.m_mfxVideoParam.calcParam.TargetKbps != par->calcParam.TargetKbps || (par->calcParam.MaxKbps && m_base.m_mfxVideoParam.calcParam.MaxKbps != par->calcParam.MaxKbps);
    mfxU8 isNewSPSRequired = (mfxU8)memcmp(enc->m_SeqParamSet, &obsoleteSeqParamSet, sizeof(H264SeqParamSet));
    // don't reset enc pipeline in the only case: no need in new SPS but bitrate was changed
    // this is used to change bitrate w/o starting new sequence
    enc->m_info.reset_encoding_pipeline = isNewSPSRequired || !isBitrateChanged;

    if (enc->m_info.reset_encoding_pipeline)
        ReleaseBufferedFrames(&m_base); // Release locked frames

    enc->m_info.is_resol_changed = enc->m_SeqParamSet->frame_width_in_mbs != obsoleteSeqParamSet.frame_width_in_mbs ||
        enc->m_SeqParamSet->frame_height_in_mbs != obsoleteSeqParamSet.frame_height_in_mbs ? true : false;

    m_base.m_mfxVideoParam.mfx = par->mfx;
    m_base.m_mfxVideoParam.IOPattern = par->IOPattern;
    if (opts != 0)
        m_base.m_extOption = *opts;

    if (enc->m_info.reset_encoding_pipeline) {
         //Reset frame count
        m_base.m_frameCountSync = 0;
        m_base.m_frameCountBufferedSync = 0;
        m_base.m_frameCount = 0;
        m_base.m_frameCountBuffered = 0;
        m_base.m_lastIDRframe = 0;
        m_base.m_lastIframe = 0;
        m_base.m_lastRefFrame = 0;
        m_base.m_lastIframeEncCount = 0;

        m_fieldOutputState = 0;
        m_fieldOutputStatus = MFX_ERR_NONE;
    }

    m_internalFrameOrders = false;

    status = H264CoreEncoder_Reset_8u16s(enc);

    enc->m_FieldStruct = par->mfx.FrameInfo.PicStruct;

    //Set specific parameters
    enc->m_info.write_access_unit_delimiters = 1;
    if( opts != NULL ){
        if( opts->AUDelimiter == MFX_CODINGOPTION_OFF ){
            enc->m_info.write_access_unit_delimiters = 0;
        //}else{
        //    enc->m_info.write_access_unit_delimiters = 0;
        }

        if( opts->EndOfSequence == MFX_CODINGOPTION_ON ) m_base.m_putEOSeq = true;
        else m_base.m_putEOSeq = false;

        if( opts->EndOfStream == MFX_CODINGOPTION_ON ) m_base.m_putEOStream = true;
        else m_base.m_putEOStream = false;

        if( opts->PicTimingSEI == MFX_CODINGOPTION_OFF ) m_base.m_putTimingSEI = false;
        else m_base.m_putTimingSEI = true;

        if( opts->ResetRefList == MFX_CODINGOPTION_ON ) m_base.m_resetRefList = true;
        else m_base.m_resetRefList = false;

#ifdef H264_INTRA_REFRESH
        if (opts->RecoveryPointSEI == MFX_CODINGOPTION_ON) m_base.m_putRecoveryPoint = true;
        else m_base.m_putRecoveryPoint = false;
#endif // H264_INTRA_REFRESH


        if( opts->FieldOutput == MFX_CODINGOPTION_ON) {
            if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || opts->FramePicture == MFX_CODINGOPTION_ON) {
                m_fieldOutput = false;
                QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            } else if (pSvcLayers)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            else if (m_fieldOutput == false)
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            else
                m_fieldOutput = true;
        }
    }

    m_base.m_putClockTimestamp = (picTimingSei != NULL);

    if (m_base.m_putClockTimestamp) {
        // fill clock timestamp info
        for (mfxU8 i = 0; i < 3; i ++) {
            enc->m_SEIData.PictureTiming.clock_timestamp_flag[i] = picTimingSei->TimeStamp[i].ClockTimestampFlag;
            enc->m_SEIData.PictureTiming.ct_type[i] = picTimingSei->TimeStamp[i].CtType;
            enc->m_SEIData.PictureTiming.nuit_field_based_flag[i] = picTimingSei->TimeStamp[i].NuitFieldBasedFlag;
            enc->m_SEIData.PictureTiming.counting_type[i] = picTimingSei->TimeStamp[i].CountingType;
            enc->m_SEIData.PictureTiming.full_timestamp_flag[i] = picTimingSei->TimeStamp[i].FullTimestampFlag;
            enc->m_SEIData.PictureTiming.discontinuity_flag[i] = picTimingSei->TimeStamp[i].DiscontinuityFlag;
            enc->m_SEIData.PictureTiming.cnt_dropped_flag[i] = picTimingSei->TimeStamp[i].CntDroppedFlag;
            enc->m_SEIData.PictureTiming.n_frames[i] = picTimingSei->TimeStamp[i].NFrames;
            enc->m_SEIData.PictureTiming.seconds_flag[i] = picTimingSei->TimeStamp[i].SecondsFlag;
            enc->m_SEIData.PictureTiming.seconds_value[i] = picTimingSei->TimeStamp[i].SecondsValue;
            enc->m_SEIData.PictureTiming.minutes_flag[i] = picTimingSei->TimeStamp[i].MinutesFlag;
            enc->m_SEIData.PictureTiming.minutes_value[i] = picTimingSei->TimeStamp[i].MinutesValue;
            enc->m_SEIData.PictureTiming.hours_flag[i] = picTimingSei->TimeStamp[i].HoursFlag;
            enc->m_SEIData.PictureTiming.hours_value[i] = picTimingSei->TimeStamp[i].HoursValue;
            enc->m_SEIData.PictureTiming.time_offset[i] = picTimingSei->TimeStamp[i].TimeOffset;
        }
    }

    if (m_temporalLayers.NumLayers) {
        // B-frames aren't supported for current temporal layers implementation
        if (enc->m_info.B_frame_rate) {
            enc->m_info.B_frame_rate = 0;
            QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        // can't encode more then 2 temporal layers with 1 reference frame
        if (m_temporalLayers.NumLayers > 2 && enc->m_info.num_ref_frames == 1) {
            m_temporalLayers.NumLayers = 2;
            QueryStatus = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        for (mfxU16 i = 0; i < m_temporalLayers.NumLayers; i ++) {
            m_temporalLayers.LayerNumRefs[i] = (i == 0) ? m_temporalLayers.NumLayers : (m_temporalLayers.NumLayers - (i + 1));
        }
    }

    if (pSvcLayers) {
        mfxU32 layersON;
        mfxU32 i0, i1;
        mfxU32 pos;

        for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
            if (!m_svc_layers[pos]) continue;
            CloseSVCLayer(m_svc_layers[pos]);
            H264_Free(m_svc_layers[pos]);
            m_svc_layers[pos] = 0;
            // TO DO: make it smart to avoid reallocation
        }
        m_svc_count = 0;
        layersON = 0;
        for ( i0 = 0; i0 < MAX_DEP_LAYERS; i0++ ) {
            if (pSvcLayers->DependencyLayer[i0].Active) {
                UMC::Status res;
                m_svc_layers[i0] = (h264Layer *)H264_Malloc( sizeof(h264Layer));
                if( m_svc_layers[i0] == NULL ) return MFX_ERR_MEMORY_ALLOC;
                memset(m_svc_layers[i0], 0, sizeof(h264Layer));

#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
                H264ENC_CALL_NEW(res, H264CoreEncoder, m_svc_layers[i0]->enc);
#undef H264ENC_MAKE_NAME
                if (res != UMC::UMC_OK) return MFX_ERR_MEMORY_ALLOC;

                m_svc_layers[i0]->enc->m_svc_layer.svc_ext.dependency_id = i0;
                m_svc_layers[i0]->m_InterLayerParam = pSvcLayers->DependencyLayer[i0];

                m_svc_layers[i0]->enc->QualityNum = pSvcLayers->DependencyLayer[i0].QualityNum;
                m_svc_layers[i0]->enc->TempIdMax = pSvcLayers->DependencyLayer[i0].TemporalNum-1;
                for (i1 = 0; i1 < MAX_TEMP_LEVELS; i1++)
                    m_svc_layers[i0]->enc->TemporalScaleList[i1] = 0;
                for (i1 = 0; i1 < pSvcLayers->DependencyLayer[i0].TemporalNum; i1++) {
                    mfxU16 tid = pSvcLayers->DependencyLayer[i0].TemporalId[i1];
                    m_svc_layers[i0]->enc->TemporalScaleList[i1] = pSvcLayers->TemporalScale[tid]; // skipping unused tid
                }

                m_svc_layers[i0]->enc->svc_ScalabilityInfoSEI = &m_svc_ScalabilityInfoSEI;
                layersON++;
            } else {
                m_svc_layers[i0] = 0;
            }
        }


        if (layersON && par->mfx.RateControlMethod != MFX_RATECONTROL_CQP && par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR /* avbr ??? */ ) {
            mfxI32 ii;
            mfxI32 did, qid, tid;

            for (ii = 0; ii < rc->NumLayers; ii++) {

                did = rc->Layer[ii].DependencyId;
                qid = rc->Layer[ii].QualityId;
                tid = rc->Layer[ii].TemporalId;
                mfxI32 tidx;
                for (tidx=0; tidx<pSvcLayers->DependencyLayer[did].TemporalNum; tidx++)
                    if (tid == pSvcLayers->DependencyLayer[did].TemporalId[tidx])
                        break;

                if (pSvcLayers->DependencyLayer[did].Active && 
                    (qid < pSvcLayers->DependencyLayer[did].QualityNum) && (tidx < pSvcLayers->DependencyLayer[did].TemporalNum)) {
                        m_HRDparams[did][qid][tidx].BufferSizeInKB   = rc->Layer[ii].CbrVbr.BufferSizeInKB;
                        m_HRDparams[did][qid][tidx].TargetKbps       = rc->Layer[ii].CbrVbr.TargetKbps;
                        m_HRDparams[did][qid][tidx].InitialDelayInKB = rc->Layer[ii].CbrVbr.InitialDelayInKB;
                        m_HRDparams[did][qid][tidx].MaxKbps          = rc->Layer[ii].CbrVbr.MaxKbps;
                }

            }

            st = CheckAndSetLayerBitrates(m_HRDparams, pSvcLayers, m_maxDid, par->mfx.RateControlMethod);
            if (st < 0)
                return st;


            if (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD) {
                mfxF64 framerate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD;
                st = CheckAndSetLayerHRDBuffers(m_HRDparams, pSvcLayers, m_maxDid, framerate, par->mfx.RateControlMethod);
                if (st < 0)
                    return st;
            }

            for (ii = 0; ii < rc->NumLayers; ii++) { // ??? kta
                did = rc->Layer[ii].DependencyId;
                qid = rc->Layer[ii].QualityId;
                tid = rc->Layer[ii].TemporalId;
                mfxI32 tidx;
                for (tidx=0; tidx<pSvcLayers->DependencyLayer[did].TemporalNum; tidx++)
                    if (tid == pSvcLayers->DependencyLayer[did].TemporalId[tidx])
                        break;
                if(tidx >= pSvcLayers->DependencyLayer[did].TemporalNum)
                    tidx = MAX_TEMP_LEVELS-1;

                rc->Layer[ii].CbrVbr.TargetKbps       = m_HRDparams[did][qid][tidx].TargetKbps;
                rc->Layer[ii].CbrVbr.MaxKbps          = m_HRDparams[did][qid][tidx].MaxKbps;
                rc->Layer[ii].CbrVbr.BufferSizeInKB   = m_HRDparams[did][qid][tidx].BufferSizeInKB;
                rc->Layer[ii].CbrVbr.InitialDelayInKB = m_HRDparams[did][qid][tidx].InitialDelayInKB;
            }
        }

        if (layersON) {
            mfxU32 ll;
            mfxU32 maxbufsize = 0;
            for (ll = 0; ll < MAX_DEP_LAYERS; ll++) {
                struct h264Layer *cur = m_svc_layers[ll];
                if (cur) {
                    struct h264Layer *ref = m_svc_layers[cur->m_InterLayerParam.RefLayerDid];
                    if(ref && cur != ref) {
                        cur->ref_layer = ref;
                        ref->src_layer = cur; // do not if all layers are provided(?), what if few reference one?
                    } else
                        cur->ref_layer = 0;
                    // Init layer's encoder, suppose ref was initialized before cur
                    cur->m_mfxVideoParam = m_base.m_mfxVideoParam; // to take only common parameters
                    cur->m_extOption = m_base.m_extOption;
                    st = InitSVCLayer(rc, ll, cur);
                    MFX_CHECK_STS(st);
                    m_svc_count ++;
                    if (cur->enc->requiredBsBufferSize > maxbufsize)
                        maxbufsize = cur->enc->requiredBsBufferSize;
                }
            }
            if (maxbufsize > 0) {
                m_base.enc->requiredBsBufferSize = maxbufsize;
                m_base.m_mfxVideoParam.mfx.BufferSizeInKB = (maxbufsize + 999)/1000; // so that GetVideoParam return the max buffer size over all spatial layers (???) kta
            }
            // disable deblocking in SVC for awhile, except Temporal scalability
            for (pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
                if (!m_svc_layers[pos]) continue;
                if (m_svc_count > 0 /*|| m_svc_layers[pos]->enc->QualityNum > 1*/) // TODO check about QualityNum
                    m_svc_layers[pos]->enc->m_info.deblocking_filter_idc = DEBLOCK_FILTER_OFF;
            }
        }

        st = SetScalabilityInfoSE();
        MFX_CHECK_STS(st);
    }

#ifdef H264_HRD_CPB_MODEL
    enc->m_SEIData.m_tickDuration = ((Ipp64f)enc->m_SeqParamSet->vui_parameters.num_units_in_tick) / enc->m_SeqParamSet->vui_parameters.time_scale;
#endif // H264_HRD_CPB_MODEL

#ifdef H264_NEW_THREADING
    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;

    for (mfxI32 i = 0; i < enc->m_info.numThreads; i++)
    {
        m_taskParams.threads_data[i].core_enc = NULL;
        m_taskParams.threads_data[i].state = NULL;
        m_taskParams.threads_data[i].slice = 0;
        m_taskParams.threads_data[i].slice_qp_delta_default = 0;
        m_taskParams.threads_data[i].default_slice_type = INTRASLICE;
    }

    m_taskParams.single_thread_done = false;
#endif // H264_NEW_THREADING

    st = ConvertVideoParamBack_H264enc(&m_base.m_mfxVideoParam, enc);
    MFX_CHECK_STS(st);

    if (enc->free_slice_mode_enabled)
        m_base.m_mfxVideoParam.mfx.NumSlice = 0;

    if( status == UMC_OK ) m_base.m_Initialized = true;

    st = h264enc_ConvertStatus(status);
    if (st == MFX_ERR_NONE)
    {
        if (m_base.m_mfxVideoParam.AsyncDepth != par->AsyncDepth) {
            m_base.m_mfxVideoParam.AsyncDepth = par->AsyncDepth;
            st = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        if (QueryStatus != MFX_ERR_NONE && QueryStatus != MFX_WRN_PARTIAL_ACCELERATION)
            return QueryStatus;
        else
            return st;
    }
    else
        return st;

}


mfxStatus MFXVideoENCODEH264::Query(mfxVideoParam *par_in, mfxVideoParam *par_out)
{
    mfxU32 isCorrected = 0;
    mfxU32 isInvalid = 0;
    mfxStatus st;
    MFX_CHECK_NULL_PTR1(par_out)
    CHECK_VERSION(par_in->Version);
    CHECK_VERSION(par_out->Version);
    struct LayerHRDParams hrdParams[MAX_DEP_LAYERS][MAX_QUALITY_LEVELS][MAX_TEMP_LEVELS];

    //First check for zero input
    if( par_in == NULL ){ //Set ones for filed that can be configured
        mfxVideoParam *out = par_out;
        memset( &out->mfx, 0, sizeof( mfxInfoMFX ));
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.CodecId = MFX_CODEC_AVC; // restore cleared mandatory
        out->mfx.CodecLevel = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.NumThread = 1;
        out->mfx.TargetUsage = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
        out->mfx.IdrInterval = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.InitialDelayInKB = 1;
        out->mfx.BufferSizeInKB = 1;
        out->mfx.TargetKbps = 1;
        out->mfx.MaxKbps = 1;
        out->mfx.NumSlice = 1;
        out->mfx.NumRefFrame = 1;
        out->mfx.EncodedOrder = 1;
        out->AsyncDepth = 1;
        out->IOPattern = 1;
        out->Protected = 0;
        // ignore all reserved

        //Extended coding options
        st = CheckExtBuffers_H264enc( out->ExtParam, out->NumExtParam );
        MFX_CHECK_STS(st);
        //if (... CheckExtBuffers_H264enc( out->ExtParam, out->NumExtParam ))
        //    return MFX_ERR_UNSUPPORTED;
        mfxExtCodingOption* opts = GetExtCodingOptions( out->ExtParam, out->NumExtParam );
        if ( opts != NULL ){
            mfxExtBuffer HeaderCopy = opts->Header;
            memset( opts, 0, sizeof( mfxExtCodingOption ));
            opts->Header = HeaderCopy;

            opts->RateDistortionOpt = 1;
            opts->MECostType = 1;
            opts->MESearchType = 1;
            opts->MVSearchWindow.x = 1;
            opts->MVSearchWindow.y = 1;
            opts->EndOfSequence = 1;
            opts->FramePicture = 1;
            opts->CAVLC = 1;
            opts->SingleSeiNalUnit = 1;
            //opts->RefPicListReordering = 1;
            //opts->ResetRefList = 1;
            opts->IntraPredBlockSize = 1;
            opts->InterPredBlockSize = 1;
            opts->MVPrecision = 1;
            opts->MaxDecFrameBuffering = 1;
            opts->AUDelimiter = 1;
            opts->EndOfStream = 1;
            opts->PicTimingSEI = 1;
            opts->NalHrdConformance = 1;
            opts->VuiNalHrdParameters = 1;
            opts->VuiVclHrdParameters = 1;
            opts->FieldOutput = 1;
        }
        mfxExtCodingOption2* opts2 = (mfxExtCodingOption2*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
        if ( opts2 != NULL ){
            mfxExtBuffer HeaderCopy = opts2->Header;
            memset( opts2, 0, sizeof( mfxExtCodingOption2 ));
            opts2->Header = HeaderCopy;

#ifdef H264_INTRA_REFRESH
            opts2->IntRefCycleSize = 1;
            opts2->IntRefQPDelta = 1;
            opts2->IntRefType = 1;
#endif // H264_INTRA_REFRESH
            opts2->BitrateLimit = 1;
        }
        mfxExtCodingOptionSPSPPS* optsSP = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
        if ( optsSP != NULL ){
            mfxExtBuffer HeaderCopy = optsSP->Header;
            memset( optsSP, 0, sizeof( mfxExtCodingOptionSPSPPS ));
            optsSP->Header = HeaderCopy;
            //optsSP->SPSBuffer = 1;
            optsSP->SPSBufSize = 1;
            optsSP->SPSId = 1;
            //optsSP->PPSBuffer = 1;
            optsSP->PPSBufSize = 1;
            optsSP->PPSId = 1;
        }
        mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
        if (videoSignalInfo != 0) {
            mfxExtBuffer HeaderCopy = videoSignalInfo->Header;
            memset( videoSignalInfo, 0, sizeof( mfxExtVideoSignalInfo ));
            videoSignalInfo->Header = HeaderCopy;
            videoSignalInfo->ColourDescriptionPresent = 1;
            videoSignalInfo->ColourPrimaries = 1;
            videoSignalInfo->MatrixCoefficients = 1;
            videoSignalInfo->TransferCharacteristics = 1;
            videoSignalInfo->VideoFormat = 1;
            videoSignalInfo->VideoFullRange = 1;
        }
        mfxExtPictureTimingSEI* picTimingSei = (mfxExtPictureTimingSEI*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );
        if (picTimingSei != 0) {
            for (mfxU8 i = 0; i < 3; i ++) {
                picTimingSei->TimeStamp[i].ClockTimestampFlag = 1;
                picTimingSei->TimeStamp[i].NuitFieldBasedFlag = 1;
                picTimingSei->TimeStamp[i].DiscontinuityFlag =  1;
                picTimingSei->TimeStamp[i].CountingType =       1;
                picTimingSei->TimeStamp[i].FullTimestampFlag =  0;
                picTimingSei->TimeStamp[i].NFrames =            0;
                picTimingSei->TimeStamp[i].SecondsFlag =        0;
                picTimingSei->TimeStamp[i].SecondsValue =       0;
                picTimingSei->TimeStamp[i].MinutesFlag =        0;
                picTimingSei->TimeStamp[i].MinutesValue =       0;
                picTimingSei->TimeStamp[i].HoursFlag =          0;
                picTimingSei->TimeStamp[i].HoursValue =         0;
                picTimingSei->TimeStamp[i].TimeOffset =         0;
            }
        }
        mfxExtAvcTemporalLayers* tempLayers = (mfxExtAvcTemporalLayers*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
        if ( tempLayers != 0 ) {
            tempLayers->BaseLayerPID = 1;

            for (int i = 0; i < 4; i ++)
                tempLayers->Layer[i].Scale = 1;

            for (int i = 4; i < 8; i ++)
                tempLayers->Layer[i].Scale = 1;
        }
        // Scene analysis info from VPP is not used in Query and Init
        mfxExtVppAuxData* optsSA = (mfxExtVppAuxData*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA );
        if ( optsSA != NULL ){
            //optsSA->RepeatedFrame = 0;
            //optsSA->SceneChangeRate = 0;
            //optsSA->SpatialComplexity = 0;
            //optsSA->TemporalComplexity = 0;
            return MFX_ERR_UNSUPPORTED;
        }
        mfxExtMVCSeqDesc *depinfo_out = (mfxExtMVCSeqDesc*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
        if (depinfo_out != NULL) {
            mfxExtBuffer HeaderCopy = depinfo_out->Header;
            memset( depinfo_out, 0, sizeof( mfxExtMVCSeqDesc ));
            depinfo_out->Header = HeaderCopy;

            depinfo_out->NumView = 1;
            depinfo_out->NumViewId = 1;
            depinfo_out->NumOP = 1;
            depinfo_out->NumRefsTotal = 1;
        }
        mfxExtSVCSeqDesc *svcinfo_out = (mfxExtSVCSeqDesc*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
        if (svcinfo_out != NULL) {
            mfxExtBuffer HeaderCopy = svcinfo_out->Header;
            memset( svcinfo_out, 0, sizeof( mfxExtSVCSeqDesc ));
            svcinfo_out->Header = HeaderCopy;

            svcinfo_out->TemporalScale[0] = 1;
        }
        mfxExtSVCRateControl *svcRC_out = (mfxExtSVCRateControl*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );
        if (svcRC_out != NULL) {
            mfxExtBuffer HeaderCopy = svcRC_out->Header;
            memset( svcRC_out, 0, sizeof( mfxExtSVCRateControl ));
            svcRC_out->Header = HeaderCopy;

            svcRC_out->RateControlMethod = 1;
            svcRC_out->NumLayers = 1;
        }
        if (svcinfo_out || svcRC_out) {
            out->mfx.FrameInfo.Width = 0;
            out->mfx.FrameInfo.Height = 0;
            out->mfx.FrameInfo.CropX = 0;
            out->mfx.FrameInfo.CropY = 0;
            out->mfx.FrameInfo.CropW = 0;
            out->mfx.FrameInfo.CropH = 0;
            out->mfx.GopPicSize = 0;
            out->mfx.GopRefDist = 0;
            out->mfx.GopOptFlag = 0;
            out->mfx.IdrInterval = 0;
            out->mfx.RateControlMethod = 0;
            out->mfx.InitialDelayInKB = 0;
            out->mfx.BufferSizeInKB = 0;
            out->mfx.TargetKbps = 0;
            out->mfx.MaxKbps = 0;
        }
    } else { //Check options for correctness

        mfxU16 cropSampleMask, correctCropTop, correctCropBottom;
        mfxVideoInternalParam inInt = *par_in;
        mfxVideoInternalParam outInt = *par_out;
        mfxVideoInternalParam *in = &inInt;
        mfxVideoInternalParam *out = &outInt;
        mfxVideoInternalParam par_SPSPPS = *par_in;

        H264SeqParamSet seq_parms;
        H264PicParamSet pic_parms;

        memset(&seq_parms, 0, sizeof(seq_parms));
        memset(&pic_parms, 0, sizeof(pic_parms));

        if ( in->mfx.CodecId != MFX_CODEC_AVC)
            return MFX_ERR_UNSUPPORTED;

        // Check compatibility of input and output Ext buffers // only if not SVC
        if ( ! GetExtBuffer(  in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC ) &&
             ! GetExtBuffer(  in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL ))
        if ((in->NumExtParam != out->NumExtParam) || (in->NumExtParam &&((in->ExtParam == 0) != (out->ExtParam == 0))))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        st = CheckExtBuffers_H264enc( in->ExtParam, in->NumExtParam );
        MFX_CHECK_STS(st);
        st = CheckExtBuffers_H264enc( out->ExtParam, out->NumExtParam );
        MFX_CHECK_STS(st);

        if(GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
            return MFX_ERR_UNSUPPORTED;
        if(GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
            return MFX_ERR_UNSUPPORTED;

        const mfxExtCodingOption* opts_in = GetExtCodingOptions( in->ExtParam, in->NumExtParam );
        mfxExtCodingOption*       opts_out = GetExtCodingOptions( out->ExtParam, out->NumExtParam );
        mfxExtCodingOptionSPSPPS* optsSP_in  = (mfxExtCodingOptionSPSPPS*)GetExtBuffer(  in->ExtParam,  in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
        mfxExtCodingOptionSPSPPS* optsSP_out = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
        mfxExtVideoSignalInfo*    videoSignalInfo_in = (mfxExtVideoSignalInfo*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
        mfxExtVideoSignalInfo*    videoSignalInfo_out = (mfxExtVideoSignalInfo*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
        mfxExtPictureTimingSEI*   picTimingSei_in = (mfxExtPictureTimingSEI*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );
        mfxExtPictureTimingSEI*   picTimingSei_out = (mfxExtPictureTimingSEI*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );
        mfxExtAvcTemporalLayers*  tempLayers_in = (mfxExtAvcTemporalLayers*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
        mfxExtAvcTemporalLayers*  tempLayers_out = (mfxExtAvcTemporalLayers*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
        const mfxExtSVCSeqDesc*   svcinfo_in = (mfxExtSVCSeqDesc*)GetExtBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
        mfxExtSVCSeqDesc*         svcinfo_out = (mfxExtSVCSeqDesc*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
        const mfxExtSVCRateControl* svcRC_in = (mfxExtSVCRateControl*)GetExtBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );
        mfxExtSVCRateControl*     svcRC_out = (mfxExtSVCRateControl*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );
        mfxExtCodingOption2*      opts2_in = (mfxExtCodingOption2*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );
        mfxExtCodingOption2*      opts2_out = (mfxExtCodingOption2*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );

        if (opts_in            ) CHECK_EXTBUF_SIZE( *opts_in,             isInvalid)
        if (opts_out           ) CHECK_EXTBUF_SIZE( *opts_out,            isInvalid)
        if (optsSP_in          ) CHECK_EXTBUF_SIZE( *optsSP_in,           isInvalid)
        if (optsSP_out         ) CHECK_EXTBUF_SIZE( *optsSP_out,          isInvalid)
        if (videoSignalInfo_in ) CHECK_EXTBUF_SIZE( *videoSignalInfo_in,  isInvalid)
        if (videoSignalInfo_out) CHECK_EXTBUF_SIZE( *videoSignalInfo_out, isInvalid)
        if (picTimingSei_in    ) CHECK_EXTBUF_SIZE( *picTimingSei_in,     isInvalid)
        if (picTimingSei_out   ) CHECK_EXTBUF_SIZE( *picTimingSei_out,    isInvalid)
        if (tempLayers_in      ) CHECK_EXTBUF_SIZE( *tempLayers_in,       isInvalid)
        if (tempLayers_out     ) CHECK_EXTBUF_SIZE( *tempLayers_out,      isInvalid)
        if (svcinfo_in         ) CHECK_EXTBUF_SIZE( *svcinfo_in,          isInvalid)
        if (svcinfo_out        ) CHECK_EXTBUF_SIZE( *svcinfo_out,         isInvalid)
        if (svcRC_in           ) CHECK_EXTBUF_SIZE( *svcRC_in,            isInvalid)
        if (svcRC_out          ) CHECK_EXTBUF_SIZE( *svcRC_out,           isInvalid)
        if (opts2_in           ) CHECK_EXTBUF_SIZE( *opts2_in,            isInvalid)
        if (opts2_out          ) CHECK_EXTBUF_SIZE( *opts2_out,           isInvalid)
        if (isInvalid)
            return MFX_ERR_UNSUPPORTED;

        if ((opts_in==0) != (opts_out==0) || (optsSP_in==0) != (optsSP_out==0) ||
            (videoSignalInfo_in == 0) != (videoSignalInfo_out == 0) ||
            (picTimingSei_in == 0) != (picTimingSei_out == 0) ||
            (tempLayers_in == 0) != (tempLayers_out == 0) ||
            (svcRC_in == 0) != (svcRC_out == 0) ||
            (opts2_in == 0) != (opts2_out == 0) ||
            (svcinfo_in == 0) != (svcinfo_out == 0))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        // Check attached SPSPPS header
        if (optsSP_in) {
            if (((optsSP_in->SPSBuffer == 0) != (optsSP_out->SPSBuffer == 0)) ||
                ((optsSP_in->PPSBuffer == 0) != (optsSP_out->PPSBuffer == 0)) ||
                (optsSP_in->SPSBuffer && (optsSP_out->SPSBufSize < optsSP_out->SPSBufSize)) ||
                (optsSP_in->PPSBuffer && (optsSP_out->PPSBufSize < optsSP_out->PPSBufSize)))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            st = LoadSPSPPS(&par_SPSPPS, seq_parms, pic_parms);

            if (st == MFX_ERR_NOT_FOUND && tempLayers_in && tempLayers_out) {
                // if temporal layers are specified SPSID and PPSID are enough
                optsSP_out->SPSId = optsSP_in->SPSId;
                optsSP_out->PPSId = optsSP_in->PPSId;
                st = MFX_ERR_NONE;
            } else if (st != MFX_ERR_NONE)
                return MFX_ERR_UNSUPPORTED;

            if (optsSP_in->SPSBuffer)
                MFX_INTERNAL_CPY(optsSP_out->SPSBuffer, optsSP_in->SPSBuffer, optsSP_in->SPSBufSize);
            if (optsSP_in->PPSBuffer)
                MFX_INTERNAL_CPY(optsSP_out->PPSBuffer, optsSP_in->PPSBuffer, optsSP_in->PPSBufSize);
        }

        if (in->mfx.FrameInfo.FourCC != 0 && in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12) {
            out->mfx.FrameInfo.FourCC = 0;
            isInvalid ++;
        }
        else out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;

        if (in->Protected != 0)
            return MFX_ERR_UNSUPPORTED;
        out->Protected = 0;

        out->AsyncDepth = in->AsyncDepth;

        //Check for valid framerate, height and width
        if( (in->mfx.FrameInfo.Width & 15) || in->mfx.FrameInfo.Width > 16384 ) {
            out->mfx.FrameInfo.Width = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
        if( (in->mfx.FrameInfo.Height & 15) || in->mfx.FrameInfo.Height > 16384 ) {
            out->mfx.FrameInfo.Height = 0;
            isInvalid ++;
        } else out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
        if( (!in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD) ||
            (in->mfx.FrameInfo.FrameRateExtN && !in->mfx.FrameInfo.FrameRateExtD) ||
            (in->mfx.FrameInfo.FrameRateExtD && ((mfxF64)in->mfx.FrameInfo.FrameRateExtN / in->mfx.FrameInfo.FrameRateExtD) > 172)) {
            isInvalid ++;
            out->mfx.FrameInfo.FrameRateExtN = out->mfx.FrameInfo.FrameRateExtD = 0;
        } else {
            out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
            out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;
        }

        switch(in->IOPattern) {
            case 0:
            case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
            case MFX_IOPATTERN_IN_VIDEO_MEMORY:
            case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
                out->IOPattern = in->IOPattern;
                break;
            default:
                if (in->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) {
                    isCorrected ++;
                    out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
                } else if (in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
                    isCorrected ++;
                    out->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
                } else {
                    isInvalid ++;
                    out->IOPattern = 0;
                }
        }

        // profile and level can be corrected
        switch( in->mfx.CodecProfile ){
            case MFX_PROFILE_AVC_MAIN:
            case MFX_PROFILE_AVC_BASELINE:
            case MFX_PROFILE_AVC_HIGH:
            case MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
            case MFX_PROFILE_AVC_PROGRESSIVE_HIGH:
            case MFX_PROFILE_AVC_CONSTRAINED_HIGH:
            case MFX_PROFILE_AVC_SCALABLE_BASELINE:
            case MFX_PROFILE_AVC_SCALABLE_HIGH:
            case MFX_PROFILE_UNKNOWN:
                out->mfx.CodecProfile = in->mfx.CodecProfile;
                break;
            default:
                isCorrected ++;
                out->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
                break;
        }
        switch( in->mfx.CodecLevel ){
            case MFX_LEVEL_AVC_1:
            case MFX_LEVEL_AVC_1b:
            case MFX_LEVEL_AVC_11:
            case MFX_LEVEL_AVC_12:
            case MFX_LEVEL_AVC_13:
            case MFX_LEVEL_AVC_2:
            case MFX_LEVEL_AVC_21:
            case MFX_LEVEL_AVC_22:
            case MFX_LEVEL_AVC_3:
            case MFX_LEVEL_AVC_31:
            case MFX_LEVEL_AVC_32:
            case MFX_LEVEL_AVC_4:
            case MFX_LEVEL_AVC_41:
            case MFX_LEVEL_AVC_42:
            case MFX_LEVEL_AVC_5:
            case MFX_LEVEL_AVC_51:
            case MFX_LEVEL_AVC_52:
            case MFX_LEVEL_UNKNOWN:
                out->mfx.CodecLevel = in->mfx.CodecLevel;
                break;
            default:
                isCorrected ++;
                out->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
                break;
        }

        out->mfx.NumThread = in->mfx.NumThread;
        //if( out->mfx.NumThread < 1 ) out->mfx.NumThread = 1;

#if defined (__ICL)
        //error #186: pointless comparison of unsigned integer with zero
#pragma warning(disable:186)
#endif

        if( in->mfx.TargetUsage < MFX_TARGETUSAGE_UNKNOWN || in->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED) {
            isCorrected ++;
            out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
        }
        else out->mfx.TargetUsage = in->mfx.TargetUsage;

        if (!svcinfo_in) { // SVC takes from svcinfo_in
            if( in->mfx.GopPicSize < 0 ) {
                out->mfx.GopPicSize = 0;
                isCorrected ++;
            } else out->mfx.GopPicSize = in->mfx.GopPicSize;

            if( in->mfx.GopRefDist  < 0 || in->mfx.GopRefDist > H264_MAXREFDIST) {
                if (in->mfx.GopRefDist > H264_MAXREFDIST)
                    out->mfx.GopRefDist = H264_MAXREFDIST;
                else
                    out->mfx.GopRefDist = 1;
                isCorrected ++;
            }
            else out->mfx.GopRefDist = in->mfx.GopRefDist;

            if ((in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT)) != in->mfx.GopOptFlag) {
                out->mfx.GopOptFlag = 0;
                isInvalid ++;
            } else out->mfx.GopOptFlag = in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);
            out->mfx.IdrInterval = in->mfx.IdrInterval;
        } // if (!svcinfo_in)

        if (opts2_in) {
#ifdef H264_INTRA_REFRESH
            if (opts2_in->IntRefType) {
                if (out->mfx.GopRefDist > 1 || opts2_in->IntRefType == 2) {
                    opts2_out->IntRefType = 0;
                    isCorrected ++;
                } else if (opts2_in->IntRefType > 2) {
                    opts2_out->IntRefType = 0;
                    isInvalid ++;
                }
                else opts2_out->IntRefType = opts2_in->IntRefType;

                if (out->mfx.GopPicSize && out->mfx.GopPicSize <= opts2_in->IntRefCycleSize) {
                    opts2_out->IntRefCycleSize = 0;
                    isCorrected ++;
                } else opts2_out->IntRefCycleSize = opts2_in->IntRefCycleSize;

                if (opts2_in->IntRefQPDelta < -51 || opts2_in->IntRefQPDelta > 51) {
                    opts2_out->IntRefQPDelta = 0;
                    isCorrected ++;
                } else opts2_out->IntRefQPDelta = opts2_in->IntRefQPDelta;
            }
#endif // H264_INTRA_REFRESH

            CHECK_OPTION(opts2_in->BitrateLimit, opts2_out->BitrateLimit, isCorrected);
            CHECK_OPTION(opts2_in->MBBRC,        opts2_out->MBBRC,        isCorrected);
            CHECK_OPTION(opts2_in->ExtBRC,       opts2_out->ExtBRC,       isCorrected);

            // frame size limit isn't supported at the moment
            if (opts2_in->MaxFrameSize) {
                opts2_out->MaxFrameSize = 0;
                isCorrected ++;
            }

            if (opts2_in->MBBRC == MFX_CODINGOPTION_ON) {
                opts2_out->MBBRC = MFX_CODINGOPTION_OFF;
                isCorrected ++;
            }

            if (opts2_in->ExtBRC == MFX_CODINGOPTION_ON) {
                opts2_out->ExtBRC = MFX_CODINGOPTION_OFF;
                isCorrected ++;
            }

            if (opts2_in->LookAheadDepth) {
                opts2_out->LookAheadDepth = 0;
                isCorrected ++;
            }

            if (opts2_in->Trellis) {
                opts2_out->Trellis = 0;
                isCorrected ++;
            }
        }

        mfxU16 RCMethod = (in->mfx.RateControlMethod == MFX_RATECONTROL_CBR || in->mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                           in->mfx.RateControlMethod == MFX_RATECONTROL_CQP || in->mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
                           ? in->mfx.RateControlMethod : 0;
        if (svcRC_in && svcRC_out) {
            if (svcRC_in->RateControlMethod != MFX_RATECONTROL_CBR && svcRC_in->RateControlMethod != MFX_RATECONTROL_VBR &&
                svcRC_in->RateControlMethod != MFX_RATECONTROL_CQP && svcRC_in->RateControlMethod != MFX_RATECONTROL_AVBR) {
                isInvalid++;
            } else {
                RCMethod = svcRC_in->RateControlMethod;
            }
            svcRC_out->RateControlMethod = RCMethod;
        } else if (in->mfx.RateControlMethod != RCMethod) {
            isInvalid ++;
        }
        out->mfx.RateControlMethod = RCMethod;

        if (!svcRC_in) { // SVC takes from svcRC_in
            out->calcParam.BufferSizeInKB = in->calcParam.BufferSizeInKB;
            if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
                out->calcParam.TargetKbps = in->calcParam.TargetKbps;
                out->calcParam.InitialDelayInKB = in->calcParam.InitialDelayInKB;
                if (out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.FrameRateExtD && out->calcParam.TargetKbps &&
                    (opts2_out == 0 || (opts2_out && opts2_in->BitrateLimit != MFX_CODINGOPTION_OFF))) {
                    // last denominator 700 gives about 1 Mbps for 1080p x 30
                    mfxU32 minBitRate = (mfxU32)((mfxF64)out->mfx.FrameInfo.Width * out->mfx.FrameInfo.Height * 12 // size of raw image (luma + chroma 420) in bits
                                                 * out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD / 1000 / 700);
                    if (minBitRate > out->calcParam.TargetKbps) {
                        out->calcParam.TargetKbps = (mfxU32)minBitRate;
                        isCorrected ++;
                    }
                    mfxU32 AveFrameKB = out->calcParam.TargetKbps * out->mfx.FrameInfo.FrameRateExtD / out->mfx.FrameInfo.FrameRateExtN / 8;
                    if (out->calcParam.BufferSizeInKB != 0 && out->calcParam.BufferSizeInKB < 2 * AveFrameKB) {
                        out->calcParam.BufferSizeInKB = (mfxU32)(2 * AveFrameKB);
                        isCorrected ++;
                    }
                    if (out->calcParam.InitialDelayInKB != 0 && out->calcParam.InitialDelayInKB < AveFrameKB) {
                        out->calcParam.InitialDelayInKB = (mfxU32)AveFrameKB;
                        isCorrected ++;
                    }
                }

                if (in->calcParam.MaxKbps != 0 && in->calcParam.MaxKbps < out->calcParam.TargetKbps) {
                    out->calcParam.MaxKbps = out->calcParam.TargetKbps;
                } else
                    out->calcParam.MaxKbps = in->calcParam.MaxKbps;
            }
            // check for correct QPs for const QP mode
            else if (out->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
                if (in->mfx.QPI > 51) {
                    in->mfx.QPI = 0;
                    isInvalid ++;
                }
                if (in->mfx.QPP > 51) {
                    in->mfx.QPP = 0;
                    isInvalid ++;
                }
                if (in->mfx.QPB > 51) {
                    in->mfx.QPB = 0;
                    isInvalid ++;
                }
            }
            else {
                out->calcParam.TargetKbps = in->calcParam.TargetKbps;
                out->mfx.Accuracy = in->mfx.Accuracy;
                out->mfx.Convergence = in->mfx.Convergence;
            }
        } // if (!svcRC_in)

        out->mfx.EncodedOrder = in->mfx.EncodedOrder;

        out->mfx.NumSlice = in->mfx.NumSlice;
        if ( in->mfx.NumSlice > 0 && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.Width)
        {
            if (in->mfx.NumSlice > ((out->mfx.FrameInfo.Height * out->mfx.FrameInfo.Width) >> 8))
            {
                out->mfx.NumSlice = 0;
                isInvalid ++;
            }
        }
        // in->mfx.NumSlice is 0 if free slice mode to be used
        if( in->mfx.NumSlice > 0 && out->mfx.FrameInfo.Height) {
            // slices to be limited to rows
            if ((in->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))  == MFX_PICSTRUCT_PROGRESSIVE) {
                if ( in->mfx.NumSlice > out->mfx.FrameInfo.Height >> 4)
                    out->mfx.NumSlice = out->mfx.FrameInfo.Height >> 4;
            } else {
                if ( in->mfx.NumSlice > out->mfx.FrameInfo.Height >> 5)
                    out->mfx.NumSlice = out->mfx.FrameInfo.Height >> 5;
            }
            if (out->mfx.NumSlice != in->mfx.NumSlice)
                isCorrected ++;
        }

        // Check crops
        if (in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height && in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropH = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
        if (in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropW = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
        if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropX = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
        if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropY = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        // Assume 420 checking horizontal crop to be even
        if ((in->mfx.FrameInfo.CropX & 1) && (in->mfx.FrameInfo.CropW & 1)) {
            if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
            isCorrected ++;
        } else if (in->mfx.FrameInfo.CropX & 1)
        {
            if (out->mfx.FrameInfo.CropX == in->mfx.FrameInfo.CropX) // not to correct CropX forced to zero
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX + 1;
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 2;
            isCorrected ++;
        }
        else if (in->mfx.FrameInfo.CropW & 1) {
            if (out->mfx.FrameInfo.CropW) // can't decrement zero CropW
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW - 1;
            isCorrected ++;
        }

        // Assume 420 checking vertical crop to be even for progressive and multiple of 4 for other PicStruct
        if ((in->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))  == MFX_PICSTRUCT_PROGRESSIVE)
            cropSampleMask = 1;
        else
            cropSampleMask = 3;

        correctCropTop = (in->mfx.FrameInfo.CropY + cropSampleMask) & ~cropSampleMask;
        correctCropBottom = (in->mfx.FrameInfo.CropY + out->mfx.FrameInfo.CropH) & ~cropSampleMask;

        if ((in->mfx.FrameInfo.CropY & cropSampleMask) || (out->mfx.FrameInfo.CropH & cropSampleMask)) {
            if (out->mfx.FrameInfo.CropY == in->mfx.FrameInfo.CropY) // not to correct CropY forced to zero
                out->mfx.FrameInfo.CropY = correctCropTop;
            if (correctCropBottom >= out->mfx.FrameInfo.CropY)
                out->mfx.FrameInfo.CropH = correctCropBottom - out->mfx.FrameInfo.CropY;
            else // CropY < cropSample
                out->mfx.FrameInfo.CropH = 0;
            isCorrected ++;
        }

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if (in->mfx.FrameInfo.ChromaFormat != 0 && in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420) {
            isCorrected ++;
            out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        } else
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;

        if( in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF &&
            in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN ) {
            out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
            isCorrected ++;
        } else out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;

        if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && (out->mfx.FrameInfo.Height & 16)) {
            out->mfx.FrameInfo.PicStruct = 0;
            out->mfx.FrameInfo.Height = 0;
            isInvalid ++;
        }

        out->mfx.NumRefFrame = in->mfx.NumRefFrame;

        //Extended coding options
        if (opts_in && opts_out) {
            CHECK_OPTION(opts_in->RateDistortionOpt, opts_out->RateDistortionOpt, isCorrected);
            // probably bit-combinations are possible
            if (opts_in->MVSearchWindow.x >= 0 && opts_in->MVSearchWindow.y >= 0) {
                opts_out->MVSearchWindow = opts_in->MVSearchWindow;
                if (opts_in->MVSearchWindow.x > 2*2048) {opts_out->MVSearchWindow.x = 2*2048; isCorrected ++;}
                if (opts_in->MVSearchWindow.y > 2*512)  {opts_out->MVSearchWindow.y = 2*512;  isCorrected ++;}
            } else {
                opts_out->MVSearchWindow.x = opts_out->MVSearchWindow.y = 0;
                isCorrected ++;
            }
            opts_out->FieldOutput = opts_in->FieldOutput;
            opts_out->MESearchType = opts_in->MESearchType;
            opts_out->MECostType = opts_in->MECostType;
            CHECK_OPTION (opts_in->EndOfSequence, opts_out->EndOfSequence, isCorrected);
            CHECK_OPTION (opts_in->FramePicture, opts_out->FramePicture, isCorrected);
            CHECK_OPTION (opts_in->CAVLC, opts_out->CAVLC, isCorrected);
            if (optsSP_in) {
                if (opts_out->CAVLC == MFX_CODINGOPTION_ON && pic_parms.entropy_coding_mode) {
                    opts_out->CAVLC = MFX_CODINGOPTION_OFF;
                    isCorrected ++;
                } else if (opts_out->CAVLC == MFX_CODINGOPTION_OFF && pic_parms.entropy_coding_mode == 0) {
                    opts_out->CAVLC = MFX_CODINGOPTION_ON;
                    isCorrected ++;
                }
            }
            CHECK_OPTION (opts_in->SingleSeiNalUnit, opts_out->SingleSeiNalUnit, isCorrected);
            //CHECK_OPTION (opts_in->RefPicListReordering, opts_out->RefPicListReordering);
            CHECK_OPTION (opts_in->ResetRefList, opts_out->ResetRefList, isCorrected);
            opts_out->IntraPredBlockSize = opts_in->IntraPredBlockSize &
                (MFX_BLOCKSIZE_MIN_16X16|MFX_BLOCKSIZE_MIN_8X8|MFX_BLOCKSIZE_MIN_4X4);
            if (opts_out->IntraPredBlockSize != opts_in->IntraPredBlockSize)
                isCorrected ++;
            opts_out->InterPredBlockSize = opts_in->InterPredBlockSize &
                (MFX_BLOCKSIZE_MIN_16X16|MFX_BLOCKSIZE_MIN_8X8|MFX_BLOCKSIZE_MIN_4X4);
            if (opts_out->InterPredBlockSize != opts_in->InterPredBlockSize)
                isCorrected ++;
            opts_out->MVPrecision = opts_in->MVPrecision &
                (MFX_MVPRECISION_INTEGER|MFX_MVPRECISION_HALFPEL|MFX_MVPRECISION_QUARTERPEL);
            if (opts_out->MVPrecision != opts_in->MVPrecision)
                isCorrected ++;
            opts_out->MaxDecFrameBuffering = opts_in->MaxDecFrameBuffering; // limits??
            CHECK_OPTION (opts_in->AUDelimiter, opts_out->AUDelimiter, isCorrected);
            CHECK_OPTION (opts_in->EndOfStream, opts_out->EndOfStream, isCorrected);
            CHECK_OPTION (opts_in->PicTimingSEI, opts_out->PicTimingSEI, isCorrected);
            if (opts_out->PicTimingSEI == MFX_CODINGOPTION_ON && optsSP_in &&
                seq_parms.vui_parameters_present_flag == 1 && seq_parms.vui_parameters.pic_struct_present_flag == 0) {
                    opts_out->PicTimingSEI = MFX_CODINGOPTION_OFF;
                    isCorrected ++;
            }
            CHECK_OPTION (opts_in->NalHrdConformance, opts_out->NalHrdConformance, isCorrected);
            CHECK_OPTION (opts_in->VuiNalHrdParameters, opts_out->VuiNalHrdParameters, isCorrected);
            CHECK_OPTION (opts_in->VuiVclHrdParameters, opts_out->VuiVclHrdParameters, isCorrected);
            CHECK_OPTION (opts_in->FieldOutput, opts_out->FieldOutput, isCorrected);
        } else if (opts_out) {
            memset(opts_out, 0, sizeof(*opts_out));
        }

        if (optsSP_in && optsSP_out) {
            *optsSP_out = *optsSP_in;
            if ((optsSP_out->SPSBuffer==0) != (optsSP_out->SPSBufSize==0)) {
                optsSP_out->SPSBuffer = 0; optsSP_out->SPSBufSize = 0;
                isInvalid++;
            }
            if ((optsSP_out->PPSBuffer==0) != (optsSP_out->PPSBufSize==0)) {
                optsSP_out->PPSBuffer = 0; optsSP_out->PPSBufSize = 0;
                isInvalid++;
            }
        } else if (optsSP_out) {
            memset(optsSP_out, 0, sizeof(*optsSP_out));
        }

        // check video signal info
        if (videoSignalInfo_in && videoSignalInfo_out) {
            *videoSignalInfo_out = *videoSignalInfo_in;
            if (videoSignalInfo_out->VideoFormat > 5) {
                videoSignalInfo_out->VideoFormat = 0;
                isInvalid++;
            }
            if (videoSignalInfo_out->ColourDescriptionPresent) {
                if (videoSignalInfo_out->ColourPrimaries == 0 || videoSignalInfo_out->ColourPrimaries > 8) {
                    videoSignalInfo_out->ColourPrimaries = 0;
                    isInvalid++;
                }
                if (videoSignalInfo_out->TransferCharacteristics == 0 || videoSignalInfo_out->TransferCharacteristics > 12) {
                    videoSignalInfo_out->TransferCharacteristics = 0;
                    isInvalid++;
                }
                if (videoSignalInfo_out->MatrixCoefficients > 8 ) {
                    videoSignalInfo_out->MatrixCoefficients = 0;
                    isInvalid++;
                }
            }
        }

        // check compatibility of temporal layers
        if (tempLayers_in && tempLayers_out) {
            tempLayers_out->BaseLayerPID = tempLayers_in->BaseLayerPID;

            mfxU16 numLayers = 0;

            for (int i = 0; i < 8; i ++) {
                tempLayers_out->Layer[i].Scale = tempLayers_in->Layer[i].Scale;
                if (tempLayers_out->Layer[i].Scale) {
                    numLayers ++;
                    if ((i && tempLayers_out->Layer[i - 1].Scale == 0) || (i > 3)) {
                        tempLayers_out->Layer[i].Scale = 0;
                        isInvalid++;
                    }
                }
            }

            if (tempLayers_out->BaseLayerPID + numLayers - 1 > 0x3f) { // check that priority_id fits into 6 bits
                tempLayers_out->BaseLayerPID = 0;
                isInvalid++;
            }

            if (tempLayers_out->Layer[0].Scale != 1) {
                tempLayers_out->Layer[0].Scale = 0;
                isInvalid++;
            }
            if (tempLayers_out->Layer[1].Scale && tempLayers_out->Layer[1].Scale != 2) {
                tempLayers_out->Layer[1].Scale = 0;
                isInvalid++;
            }
            if (tempLayers_out->Layer[2].Scale && tempLayers_out->Layer[2].Scale != 4) {
                tempLayers_out->Layer[2].Scale = 0;
                isInvalid++;
            }
            if (tempLayers_out->Layer[3].Scale && tempLayers_out->Layer[3].Scale != 8) {
                tempLayers_out->Layer[3].Scale = 0;
                isInvalid++;
            }
        }

        if (picTimingSei_in && picTimingSei_out) {
            *picTimingSei_out = *picTimingSei_in;
            for (mfxU8 i = 0; i < 3; i ++) {
                CORRECT_FLAG (picTimingSei_out->TimeStamp[i].ClockTimestampFlag, 0, isCorrected);
                if (picTimingSei_out->TimeStamp[i].CtType > 6 && picTimingSei_out->TimeStamp[i].CtType != 0xffff) {
                    picTimingSei_out->TimeStamp[i].CountingType = 0;
                    isCorrected ++;
                }
                CORRECT_FLAG (picTimingSei_out->TimeStamp[i].NuitFieldBasedFlag, 1, isCorrected);
                CORRECT_FLAG (picTimingSei_out->TimeStamp[i].DiscontinuityFlag, 1, isCorrected);
                if (picTimingSei_out->TimeStamp[i].CountingType > 6) {
                    picTimingSei_out->TimeStamp[i].CountingType = 0;
                    isCorrected ++;
                }
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].FullTimestampFlag, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].NFrames, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].SecondsFlag, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].SecondsValue, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].MinutesFlag, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].MinutesValue, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].HoursFlag, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].HoursValue, isCorrected);
                CHECK_ZERO ( picTimingSei_out->TimeStamp[i].TimeOffset, isCorrected);
            }
        }

        mfxI32 maxDid = 1;
        if (svcinfo_in && svcinfo_out) {
            *svcinfo_out = *svcinfo_in;
            mfxI32 tid, did, qid, i, numdid;
            mfxI32 maxtid = sizeof(svcinfo_out->TemporalScale)/sizeof(svcinfo_out->TemporalScale[0]);
            mfxI32 maxdid = sizeof(svcinfo_out->DependencyLayer)/sizeof(svcinfo_out->DependencyLayer[0]);
            mfxI32 maxqid = sizeof(svcinfo_out->DependencyLayer[0].QualityLayer) /
                sizeof(svcinfo_out->DependencyLayer[0].QualityLayer[0]);
            mfxI32 maxfoundTID = 0;

            for ( did = 0, numdid = 0; did < maxdid; did++) {
                if (svcinfo_in->DependencyLayer[did].Active == 0)
                    continue;

                if (svcinfo_in->DependencyLayer[did].TemporalNum == 0 || svcinfo_in->DependencyLayer[did].TemporalNum > maxtid) {
                    isCorrected++;
                    svcinfo_out->DependencyLayer[did].TemporalNum = 1;
                } else svcinfo_out->DependencyLayer[did].TemporalNum = svcinfo_in->DependencyLayer[did].TemporalNum;

                for (tid = 1; tid < svcinfo_out->DependencyLayer[did].TemporalNum; tid++) {
                    mfxI32 curTID = svcinfo_in->DependencyLayer[did].TemporalId[tid];
                    if (curTID > maxtid) continue;
                    if (maxfoundTID < curTID)
                        maxfoundTID = curTID;
                }
            }

            if(svcinfo_in->TemporalScale[0] != 1)
                isCorrected++;
            svcinfo_out->TemporalScale[0] = 1;
            mfxU16 currscale, prevscale = 1; // to avoid zero division on error
            for ( tid = 1; tid <= maxfoundTID; tid++ ) {
                currscale = svcinfo_in->TemporalScale[tid];
                if (currscale <= prevscale ||
                    currscale / prevscale * prevscale != currscale) {
                        isInvalid++;
                        svcinfo_out->TemporalScale[tid] = 0;
                        if (!currscale) currscale = prevscale; // replace 0 with last nonzero
                } else svcinfo_out->TemporalScale[tid] = currscale;
                prevscale = currscale;
            }
            for ( ; tid < maxtid ; tid++ ) {
                if(svcinfo_in->TemporalScale[tid] != 0)
                    isCorrected++;
                svcinfo_out->TemporalScale[tid] = 0;
            }
            for ( did = 0, numdid = 0; did < maxdid; did++) {
                if (svcinfo_in->DependencyLayer[did].Active == 0)
                    continue;
                mfxI32 hasTCoeff = 0;
                numdid ++;
                maxDid = did;
                if( (svcinfo_in->DependencyLayer[did].Width & 15) || svcinfo_in->DependencyLayer[did].Width > 4096 ) {
                    svcinfo_out->DependencyLayer[did].Width = 0;
                    isInvalid ++;
                } else svcinfo_out->DependencyLayer[did].Width = svcinfo_in->DependencyLayer[did].Width;
                if( (svcinfo_in->DependencyLayer[did].Height & 15) || svcinfo_in->DependencyLayer[did].Height > 4096 ) {
                    svcinfo_out->DependencyLayer[did].Height = 0;
                    isInvalid ++;
                } else svcinfo_out->DependencyLayer[did].Height = svcinfo_in->DependencyLayer[did].Height;

                // Check crops
                if (svcinfo_in->DependencyLayer[did].CropH > svcinfo_in->DependencyLayer[did].Height && svcinfo_in->DependencyLayer[did].Height) {
                    svcinfo_out->DependencyLayer[did].CropH = 0;
                    isInvalid ++;
                } else
                    svcinfo_out->DependencyLayer[did].CropH = svcinfo_in->DependencyLayer[did].CropH;
                if (svcinfo_in->DependencyLayer[did].CropW > svcinfo_in->DependencyLayer[did].Width && svcinfo_in->DependencyLayer[did].Width) {
                    svcinfo_out->DependencyLayer[did].CropW = 0;
                    isInvalid ++;
                } else
                    svcinfo_out->DependencyLayer[did].CropW = svcinfo_in->DependencyLayer[did].CropW;
                if (svcinfo_in->DependencyLayer[did].CropX + svcinfo_in->DependencyLayer[did].CropW > svcinfo_in->DependencyLayer[did].Width) {
                    svcinfo_out->DependencyLayer[did].CropX = 0;
                    isInvalid ++;
                } else
                    svcinfo_out->DependencyLayer[did].CropX = svcinfo_in->DependencyLayer[did].CropX;
                if (svcinfo_in->DependencyLayer[did].CropY + svcinfo_in->DependencyLayer[did].CropH > svcinfo_in->DependencyLayer[did].Height) {
                    svcinfo_out->DependencyLayer[did].CropY = 0;
                    isInvalid ++;
                } else
                    svcinfo_out->DependencyLayer[did].CropY = svcinfo_in->DependencyLayer[did].CropY;

                // Assume 420 checking horizontal crop to be even
                if ((svcinfo_in->DependencyLayer[did].CropX & 1) && (svcinfo_in->DependencyLayer[did].CropW & 1)) {
                    if (svcinfo_out->DependencyLayer[did].CropX == svcinfo_in->DependencyLayer[did].CropX) // not to correct CropX forced to zero
                        svcinfo_out->DependencyLayer[did].CropX = svcinfo_in->DependencyLayer[did].CropX + 1;
                    if (svcinfo_out->DependencyLayer[did].CropW) // can't decrement zero CropW
                        svcinfo_out->DependencyLayer[did].CropW = svcinfo_in->DependencyLayer[did].CropW - 1;
                    isCorrected ++;
                } else if (svcinfo_in->DependencyLayer[did].CropX & 1)
                {
                    if (svcinfo_out->DependencyLayer[did].CropX == svcinfo_in->DependencyLayer[did].CropX) // not to correct CropX forced to zero
                        svcinfo_out->DependencyLayer[did].CropX = svcinfo_in->DependencyLayer[did].CropX + 1;
                    if (svcinfo_out->DependencyLayer[did].CropW) // can't decrement zero CropW
                        svcinfo_out->DependencyLayer[did].CropW = svcinfo_in->DependencyLayer[did].CropW - 2;
                    isCorrected ++;
                }
                else if (svcinfo_in->DependencyLayer[did].CropW & 1) {
                    if (svcinfo_out->DependencyLayer[did].CropW) // can't decrement zero CropW
                        svcinfo_out->DependencyLayer[did].CropW = svcinfo_in->DependencyLayer[did].CropW - 1;
                    isCorrected ++;
                }


                // if( svcinfo_out->DependencyLayer[did].GopPicSize < 0 ) // noting to check
                if( svcinfo_out->DependencyLayer[did].GopRefDist > H264_MAXREFDIST) {
                    svcinfo_out->DependencyLayer[did].GopRefDist = H264_MAXREFDIST;
                    isCorrected ++;
                }
                if (svcinfo_out->DependencyLayer[did].GopOptFlag & ~(MFX_GOP_CLOSED | MFX_GOP_STRICT)) {
                    isInvalid ++;
                    svcinfo_out->DependencyLayer[did].GopOptFlag = 0; //svcinfo_out->DependencyLayer[did].GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);
                }


                if (svcinfo_in->DependencyLayer[did].RefLayerDid > did) {
                    svcinfo_out->DependencyLayer[did].RefLayerDid = did;
                    isInvalid ++;
                } else svcinfo_out->DependencyLayer[did].RefLayerDid = svcinfo_in->DependencyLayer[did].RefLayerDid;

                mfxU16 refdid = svcinfo_out->DependencyLayer[did].RefLayerDid;
                if (svcinfo_out->DependencyLayer[refdid].Active == 0)
                    isInvalid++;

                //if (refdid < did) {
                //    if (svcinfo_in->DependencyLayer[did].RefLayerQid > svcinfo_out->DependencyLayer[refdid].QualityNum-1) {
                //        svcinfo_out->DependencyLayer[did].RefLayerQid = svcinfo_out->DependencyLayer[refdid].QualityNum-1;
                //        isCorrected ++;
                //    } else svcinfo_out->DependencyLayer[did].RefLayerQid = svcinfo_in->DependencyLayer[did].RefLayerQid;
                //}
                if (svcinfo_in->DependencyLayer[did].RefLayerQid != 0)
                    isInvalid ++;
                svcinfo_out->DependencyLayer[did].RefLayerQid = 0;

                CHECK_TRI_OPTION (svcinfo_in->DependencyLayer[did].BasemodePred, svcinfo_out->DependencyLayer[did].BasemodePred, isCorrected);
                CHECK_TRI_OPTION (svcinfo_in->DependencyLayer[did].MotionPred, svcinfo_out->DependencyLayer[did].MotionPred, isCorrected);
                CHECK_TRI_OPTION (svcinfo_in->DependencyLayer[did].ResidualPred, svcinfo_out->DependencyLayer[did].ResidualPred, isCorrected);

                // TODO add check for DisableInterLayerDeblockingFilterIdc
                CHECK_OPTION (svcinfo_in->DependencyLayer[did].DisableDeblockingFilter, svcinfo_out->DependencyLayer[did].DisableDeblockingFilter, isCorrected);
                CHECK_OPTION (svcinfo_in->DependencyLayer[did].ScanIdxPresent, svcinfo_out->DependencyLayer[did].ScanIdxPresent, isCorrected);

                for (i = 0; i < 4; i++) {
                    if (svcinfo_in->DependencyLayer[did].ScaledRefLayerOffsets[i] & 1) {
                        isInvalid++;
                        svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[i] = 0;
                    } else
                        svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[i] = svcinfo_in->DependencyLayer[did].ScaledRefLayerOffsets[i];
                }
                mfxI32 scaledw = svcinfo_out->DependencyLayer[did].Width -
                                svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[0] -
                                svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[2];
                if (scaledw < svcinfo_out->DependencyLayer[refdid].Width) {
                    isInvalid++;
                    svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[0] = 0;
                    svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[2] = 0;
                }
                mfxI32 scaledh = svcinfo_out->DependencyLayer[did].Height -
                    svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[1] -
                    svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[3];
                if (scaledh < svcinfo_out->DependencyLayer[refdid].Height) {
                    isInvalid++;
                    svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[1] = 0;
                    svcinfo_out->DependencyLayer[did].ScaledRefLayerOffsets[3] = 0;
                }

                if (svcinfo_in->DependencyLayer[did].QualityNum == 0 || svcinfo_in->DependencyLayer[did].QualityNum > maxqid) {
                    isCorrected++;
                    svcinfo_out->DependencyLayer[did].QualityNum = 1;
                } else svcinfo_out->DependencyLayer[did].QualityNum = svcinfo_in->DependencyLayer[did].QualityNum;

                for (qid = 0; qid < svcinfo_out->DependencyLayer[did].QualityNum; qid++) {
                    if (svcinfo_in->DependencyLayer[did].ScanIdxPresent == MFX_CODINGOPTION_ON) {
                        if (svcinfo_in->DependencyLayer[did].QualityLayer[qid].ScanIdxStart > 15) {
                            isCorrected++;
                            svcinfo_out->DependencyLayer[did].QualityLayer[qid].ScanIdxStart = 15;
                        } else
                            svcinfo_out->DependencyLayer[did].QualityLayer[qid].ScanIdxStart = svcinfo_in->DependencyLayer[did].QualityLayer[qid].ScanIdxStart;
                        if (svcinfo_in->DependencyLayer[did].QualityLayer[qid].ScanIdxEnd > 15) {
                            isCorrected++;
                            svcinfo_out->DependencyLayer[did].QualityLayer[qid].ScanIdxEnd = 15;
                        } else
                            svcinfo_out->DependencyLayer[did].QualityLayer[qid].ScanIdxEnd = svcinfo_in->DependencyLayer[did].QualityLayer[qid].ScanIdxEnd;
                    }
                    CHECK_OPTION (svcinfo_in->DependencyLayer[did].QualityLayer[qid].TcoeffPredictionFlag, svcinfo_out->DependencyLayer[did].QualityLayer[qid].TcoeffPredictionFlag, isCorrected);
                    svcinfo_out->DependencyLayer[did].QualityLayer[qid].TcoeffPredictionFlag = svcinfo_in->DependencyLayer[did].QualityLayer[qid].TcoeffPredictionFlag;
                    if (svcinfo_in->DependencyLayer[did].QualityLayer[qid].TcoeffPredictionFlag == MFX_CODINGOPTION_ON)
                        hasTCoeff ++;
                }
                // Check for TcoeffPredictionFlag
                // must be 0 if no MB matching because of zoom or offset
                if (svcinfo_in->DependencyLayer[did].QualityLayer[0].TcoeffPredictionFlag == MFX_CODINGOPTION_ON &&
                    ((did == 0) ||
                     (svcinfo_in->DependencyLayer[did].ScaledRefLayerOffsets[0] & 15) ||
                     (svcinfo_in->DependencyLayer[did].ScaledRefLayerOffsets[1] & 15) ||
                     (scaledw != svcinfo_out->DependencyLayer[refdid].Width) ||
                     (scaledh != svcinfo_out->DependencyLayer[refdid].Height)) )
                {
                     isCorrected++;
                     svcinfo_out->DependencyLayer[did].QualityLayer[0].TcoeffPredictionFlag = 0;
                     hasTCoeff --;
                }

                if (hasTCoeff && svcinfo_in->DependencyLayer[did].ResidualPred == MFX_CODINGOPTION_OFF) {
                    isCorrected++;
                    svcinfo_out->DependencyLayer[did].ResidualPred = MFX_CODINGOPTION_ADAPTIVE;
                }

                for (tid = 1; tid < svcinfo_out->DependencyLayer[did].TemporalNum; tid++) {
                    if (svcinfo_out->TemporalScale[tid] <= 1
//                        || svcinfo_in->DependencyLayer[did].TemporalId[tid] != tid // beta condition, TemporalId seems to be useless
                        || svcinfo_in->DependencyLayer[did].TemporalId[tid] <= svcinfo_out->DependencyLayer[did].TemporalId[tid-1]
                    ) {
                        isInvalid++;
                        //svcinfo_out->DependencyLayer[did].TemporalNum = tid; // how to signal?
                        svcinfo_out->DependencyLayer[did].TemporalId[tid] = svcinfo_out->DependencyLayer[did].TemporalId[tid-1] + 1;
                    } else {
                        svcinfo_out->DependencyLayer[did].TemporalId[tid] = svcinfo_in->DependencyLayer[did].TemporalId[tid];
                        //if (maxfoundTID < svcinfo_out->DependencyLayer[did].TemporalId[tid])
                        //    maxfoundTID = svcinfo_out->DependencyLayer[did].TemporalId[tid];
                    }
                }
                // check if Temporal scale at the level is aligned to GOPRefDist
                for (tid = 0; tid < svcinfo_out->DependencyLayer[did].TemporalNum - 1; tid++) {
                    mfxI32 rate = svcinfo_out->TemporalScale[svcinfo_out->DependencyLayer[did].TemporalId[tid]];
                    if (rate == 0) continue;
                    mfxI32 rateDivider = svcinfo_out->TemporalScale[maxfoundTID] / rate;
                    if (!ARE_MULTIPLE(rateDivider, svcinfo_out->DependencyLayer[did].GopRefDist)) {
                        svcinfo_out->DependencyLayer[did].GopRefDist = 1;
                        isCorrected ++;
                        break;
                    }
                }
            }
            if (numdid==0)
                isInvalid ++;

            // Check restrictions from SVC to AVC
            if( out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
                out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN ) {
                    out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
                    isInvalid ++;
            }

            out->mfx.NumThread = 1; // Temporary disable internal threading. Problems with quality layers

            // Check NumRefFrame and GOP params when temporal
            if (maxfoundTID) {
                mfxI32 templen = svcinfo_out->TemporalScale[maxfoundTID];
                mfxI32 gopsize = svcinfo_out->DependencyLayer[maxDid].GopPicSize;
                mfxI32 gopdist = svcinfo_out->DependencyLayer[maxDid].GopRefDist;
                if (!IS_MULTIPLE(gopsize, gopdist)) {
                    mfxI32 newsize = gopsize/gopdist*gopdist;
                    if (ARE_MULTIPLE(newsize, templen)) {
                        isCorrected ++;
                        gopsize = newsize;
                    } else if (ARE_MULTIPLE(newsize+gopdist, templen)) {
                        isCorrected ++;
                        gopsize = newsize+gopdist;
                    } else {
                        gopsize = 0;
                        isInvalid ++;
                    }
                    svcinfo_out->DependencyLayer[maxDid].GopPicSize = gopsize;
                }
                if (!ARE_MULTIPLE(gopsize, templen)) {
                    mfxI32 res = simplify(&gopsize, &gopdist, templen);
                    if (!res)
                        isInvalid ++;
                    else if (res==1)
                        isCorrected ++;
                }
                svcinfo_out->DependencyLayer[maxDid].GopPicSize = gopsize;
                svcinfo_out->DependencyLayer[maxDid].GopRefDist = gopdist;
            }

            // Check profiles: restrictions for scalable_base
            // Refuse EncodingOrder
        } else if (svcinfo_out) {
            memset(svcinfo_out, 0, sizeof(*svcinfo_out));
        }

        bool rcValid = true;
        if (svcRC_in && svcRC_out) {
            mfxI32 i;
            mfxI32 did, qid, tid;
            memset((void*)hrdParams, 0, sizeof(hrdParams));
            //*svcRC_out = *svcRC_in;
            // svcRC_out->RateControlMethod = RCMethod; // already done
            if (svcRC_in->NumLayers >= sizeof(svcRC_in->Layer)/sizeof(svcRC_in->Layer[0])) {
                svcRC_out->NumLayers = 0;
                isInvalid ++;
                rcValid = false;
            } else svcRC_out->NumLayers = svcRC_in->NumLayers;

            for (i=0; i<svcRC_out->NumLayers; i++) {
                if ((did = svcRC_in->Layer[i].DependencyId) >= MAX_DEP_LAYERS ||
                    (qid = svcRC_in->Layer[i].QualityId) >= MAX_QUALITY_LEVELS ||
                    (tid = svcRC_in->Layer[i].TemporalId) >= MAX_TEMP_LEVELS) {
                    isInvalid++;
                    rcValid = false;
                    continue;
                }
                if (svcRC_in->RateControlMethod == MFX_RATECONTROL_CBR ||
                    svcRC_in->RateControlMethod == MFX_RATECONTROL_VBR) {
                    if (svcinfo_in && svcinfo_out) {
                        mfxI32 tidx;
                        for (tidx=0; tidx<svcinfo_out->DependencyLayer[did].TemporalNum; tidx++)
                            if (tid == svcinfo_out->DependencyLayer[did].TemporalId[tidx])
                                break;

                        if (svcinfo_out->DependencyLayer[did].Active && 
                            (qid < svcinfo_out->DependencyLayer[did].QualityNum) && (tidx<svcinfo_out->DependencyLayer[did].TemporalNum)) {
                            hrdParams[did][qid][tidx].BufferSizeInKB   = svcRC_in->Layer[i].CbrVbr.BufferSizeInKB;
                            hrdParams[did][qid][tidx].TargetKbps       = svcRC_in->Layer[i].CbrVbr.TargetKbps;
                            hrdParams[did][qid][tidx].InitialDelayInKB = svcRC_in->Layer[i].CbrVbr.InitialDelayInKB;
                            hrdParams[did][qid][tidx].MaxKbps          = svcRC_in->Layer[i].CbrVbr.MaxKbps;
                        } else {
                            isCorrected++;
                        }
                    } else {
                        hrdParams[did][qid][tid].BufferSizeInKB   = svcRC_in->Layer[i].CbrVbr.BufferSizeInKB;
                        hrdParams[did][qid][tid].TargetKbps       = svcRC_in->Layer[i].CbrVbr.TargetKbps;
                        hrdParams[did][qid][tid].InitialDelayInKB = svcRC_in->Layer[i].CbrVbr.InitialDelayInKB;
                        hrdParams[did][qid][tid].MaxKbps          = svcRC_in->Layer[i].CbrVbr.MaxKbps;
                    }
                } // TODO more RC params to be checked

                else if (svcRC_in->RateControlMethod == MFX_RATECONTROL_CQP) {
                // check for correct QPs for const QP mode
                    svcRC_out->Layer[i].Cqp = svcRC_in->Layer[i].Cqp;
                    if (svcRC_out->Layer[i].Cqp.QPI > 51) {
                        svcRC_out->Layer[i].Cqp.QPI = 0;
                        isInvalid ++;
                    }
                    if (svcRC_out->Layer[i].Cqp.QPP > 51) {
                        svcRC_out->Layer[i].Cqp.QPP = 0;
                        isInvalid ++;
                    }
                    if (svcRC_out->Layer[i].Cqp.QPB > 51) {
                        svcRC_out->Layer[i].Cqp.QPB = 0;
                        isInvalid ++;
                    }
                }
                else if (svcRC_in->RateControlMethod == MFX_RATECONTROL_AVBR) {
                    svcRC_out->Layer[i].Avbr = svcRC_in->Layer[i].Avbr;
                }
            }

            mfxStatus sts;

// QP ?
// set higher level values as well?

            if (rcValid) {
                sts = CheckAndSetLayerBitrates(hrdParams, svcinfo_out, maxDid, svcRC_out->RateControlMethod);

                if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                    isCorrected++;
                else if (sts != MFX_ERR_NONE) {
                    rcValid = false;
                    isInvalid++;
                }
                if (rcValid && out->mfx.FrameInfo.FrameRateExtN != 0 && out->mfx.FrameInfo.FrameRateExtD != 0) {
                    mfxF64 framerate = (mfxF64)out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD;
                    sts = CheckAndSetLayerHRDBuffers(hrdParams, svcinfo_out, maxDid, framerate, svcRC_out->RateControlMethod);
                    if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                        isCorrected++;
                    else if (sts != MFX_ERR_NONE) {
                        rcValid = false;
                        isInvalid++;
                    }
                }
                //for (i = 0; i < svcRC_in->NumLayers; i++) {
                //    did = svcRC_out->Layer[i].DependencyId;
                //    qid = svcRC_out->Layer[i].QualityId;
                //    tid = svcRC_out->Layer[i].TemporalId;

                //    svcRC_out->Layer[i].CbrVbr.TargetKbps       = hrdParams[did][qid][tid].TargetKbps;
                //    svcRC_out->Layer[i].CbrVbr.MaxKbps          = hrdParams[did][qid][tid].MaxKbps;
                //    svcRC_out->Layer[i].CbrVbr.BufferSizeInKB   = hrdParams[did][qid][tid].BufferSizeInKB;
                //    svcRC_out->Layer[i].CbrVbr.InitialDelayInKB = hrdParams[did][qid][tid].InitialDelayInKB;
                //}
            }

            if (svcRC_in->RateControlMethod == MFX_RATECONTROL_CBR ||
                svcRC_in->RateControlMethod == MFX_RATECONTROL_VBR) {
                memset(svcRC_out->Layer, 0, sizeof(svcRC_out->Layer));
                i = 0;
                svcRC_out->NumLayers = 0;
                for (did = 0; did < MAX_DEP_LAYERS; did++)
                    for (qid = 0; qid < MAX_QUALITY_LEVELS; qid++)
                        for (tid = 0; tid < MAX_TEMP_LEVELS; tid++)
                            if (hrdParams[did][qid][tid].TargetKbps || hrdParams[did][qid][tid].MaxKbps ||
                                hrdParams[did][qid][tid].BufferSizeInKB || hrdParams[did][qid][tid].InitialDelayInKB)
                            {
                                svcRC_out->Layer[i].DependencyId = did;
                                svcRC_out->Layer[i].QualityId = qid;
                                svcRC_out->Layer[i].TemporalId = svcinfo_out ? svcinfo_out->DependencyLayer[did].TemporalId[tid] : tid;
                                svcRC_out->Layer[i].CbrVbr.TargetKbps       = hrdParams[did][qid][tid].TargetKbps;
                                svcRC_out->Layer[i].CbrVbr.MaxKbps          = hrdParams[did][qid][tid].MaxKbps;
                                svcRC_out->Layer[i].CbrVbr.BufferSizeInKB   = hrdParams[did][qid][tid].BufferSizeInKB;
                                svcRC_out->Layer[i].CbrVbr.InitialDelayInKB = hrdParams[did][qid][tid].InitialDelayInKB;
                                svcRC_out->NumLayers ++;
                                i ++;
                            }
            }

        } else if (svcRC_out) {
            memset(svcRC_out, 0, sizeof(*svcRC_out));
        }

        mfxVideoInternalParam checked = *out;
        mfxStatus st;
        //checked.NumExtParam = 0; checked.ExtParam = 0; // to prevent modifications and exposure
        st = CheckProfileLevelLimits_H264enc(&checked, true);
        if (st == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
            out->mfx = checked.mfx;
            isCorrected ++;
        } else if (st != MFX_ERR_NONE)
            isInvalid ++;
        if (out->mfx.CodecProfile != MFX_PROFILE_UNKNOWN && out->mfx.CodecProfile != checked.mfx.CodecProfile) {
            out->mfx.CodecProfile = checked.mfx.CodecProfile;
            isCorrected ++;
        }
        if (out->mfx.CodecLevel != MFX_LEVEL_UNKNOWN && out->mfx.CodecLevel != checked.mfx.CodecLevel) {
            out->mfx.CodecLevel = checked.mfx.CodecLevel;
            isCorrected ++;
        }
        if (out->calcParam.TargetKbps != 0 && out->calcParam.TargetKbps != checked.calcParam.TargetKbps) {
            out->calcParam.TargetKbps = checked.calcParam.TargetKbps;
            isCorrected ++;
        }
        if (out->calcParam.MaxKbps != 0 && out->calcParam.MaxKbps != checked.calcParam.MaxKbps) {
            out->calcParam.MaxKbps = checked.calcParam.MaxKbps;
            isCorrected ++;
        }
        if (out->calcParam.BufferSizeInKB != 0 && out->calcParam.BufferSizeInKB != checked.calcParam.BufferSizeInKB) {
            out->calcParam.BufferSizeInKB = checked.calcParam.BufferSizeInKB;
            isCorrected ++;
        }
        if (   out->mfx.RateControlMethod == MFX_RATECONTROL_CBR 
            && out->calcParam.InitialDelayInKB != 0 
            && out->calcParam.InitialDelayInKB != checked.calcParam.InitialDelayInKB) {
            out->calcParam.InitialDelayInKB = checked.calcParam.InitialDelayInKB;
            isCorrected ++;
        }
        if (out->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN && out->mfx.FrameInfo.PicStruct != checked.mfx.FrameInfo.PicStruct) {
            out->mfx.FrameInfo.PicStruct = checked.mfx.FrameInfo.PicStruct;
            isCorrected ++;
        }
        if (opts_in && opts_out && opts_in->FramePicture != opts_out->FramePicture)
            isCorrected ++;

        *par_out = *out;
        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            out->GetCalcParams(par_out);
    }

    if (isInvalid)
        return MFX_ERR_UNSUPPORTED;
    if (isCorrected)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_NULL_PTR1(request)

    mfxStatus st;

    // check for valid IOPattern
    mfxU16 IOPatternIn = par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);
    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0) ||
        ((IOPatternIn != MFX_IOPATTERN_IN_VIDEO_MEMORY) && ((IOPatternIn != MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_OPAQUE_MEMORY))))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->Protected != 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxVideoInternalParam par_SPSPPS = mfxVideoInternalParam();
    mfxExtCodingOptionSPSPPS* optsSPSPPS = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( par->ExtParam,  par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    mfxExtAvcTemporalLayers* tempLayers = (mfxExtAvcTemporalLayers*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
    mfxExtSVCSeqDesc *svcinfo = (mfxExtSVCSeqDesc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );

    if (optsSPSPPS) {
        H264SeqParamSet seq_parms;
        H264PicParamSet pic_parms;

        // read and check attached SPS/PPS header
        st = LoadSPSPPS(par, seq_parms, pic_parms);
        if (!(st == MFX_ERR_NOT_FOUND && tempLayers)) // if temporal layers are specified SPSID and PPSID are enough
            MFX_CHECK_STS(st);

        st = ConvertVideoParamFromSPSPPS_H264enc(&par_SPSPPS, &seq_parms, &pic_parms);
        par_SPSPPS.mfx.FrameInfo.FourCC = par->mfx.FrameInfo.FourCC;
        MFX_CHECK_STS(st);
    }

    mfxU16 nFrames = par->mfx.GopRefDist; // 1 for current and GopRefDist - 1 for reordering

    if(svcinfo) {
        nFrames = 0;
        for (mfxI32 id = 0; id < MAX_DEP_LAYERS; id++) {
            if (svcinfo->DependencyLayer[id].Active && nFrames < svcinfo->DependencyLayer[id].GopRefDist)
                nFrames = svcinfo->DependencyLayer[id].GopRefDist;
        }
    }

    if( nFrames == 0 ){ //Aligned with ConvertVideoParam_H264enc and Query
        nFrames = 1; // 1 for current
        switch( par->mfx.TargetUsage ){
        case MFX_TARGETUSAGE_BEST_QUALITY:
        case 2:
        case 3:
        case MFX_TARGETUSAGE_BALANCED:
            nFrames += 1; // 1 for reordering
            break;
        case 5:
        case 6:
        case MFX_TARGETUSAGE_BEST_SPEED:
            nFrames += 0;
            break;
        default : // use MFX_TARGET_USAGE_BALANCED by default
            nFrames += 1; // 1 for reordering
            break;
        }
    }
    request->NumFrameMin = nFrames;
    request->NumFrameSuggested = IPP_MAX(nFrames,par->AsyncDepth);
#ifdef USE_PSYINFO
    //if ( (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) )
    request->NumFrameSuggested += 1;
    request->NumFrameMin += 1;
#endif // USE_PSYINFO
    // take FrameInfo from attached SPS/PPS headers (if any)
    request->Info = optsSPSPPS ? par_SPSPPS.mfx.FrameInfo : par->mfx.FrameInfo;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY){
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }else if(par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } else {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::GetEncodeStat(mfxEncodeStat *stat)
{
    if( !m_base.m_Initialized ) return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat)
    memset(stat, 0, sizeof(mfxEncodeStat));
    stat->NumCachedFrame = m_base.m_frameCountBufferedSync; //(mfxU32)m_inFrames.size();
    stat->NumBit = m_base.m_totalBits;
    stat->NumFrame = m_base.m_encodedFrames;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)
{
    mfxStatus st;
    UMC::VideoData m_data_in;
    UMC::MediaData m_data_out;
    UMC::MediaData m_data_out_ext, *p_data_out_ext = 0;
    mfxBitstream *bitstream = 0;
    mfxU8* dataPtr;
    mfxU32 initialDataLength = 0;
    mfxFrameSurface1 *surface = inputSurface;

    h264Layer *layer = &m_base;
//    bool lastDid = 0;

    // if input is svc layer - find proper encoder by Id
    if (m_svc_count > 0 && surface) {
        mfxU32 pos;
        mfxFrameInfo *fr = &surface->Info;
        for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
            if (!m_svc_layers[pos]) continue;
            struct SVCLayerParams *lsps = &m_svc_layers[pos]->enc->m_svc_layer;
            sNALUnitHeaderSVCExtension* hd = &lsps->svc_ext;
            if (fr->FrameId.DependencyId == hd->dependency_id)
            {
                layer = m_svc_layers[pos];
                if( !layer->m_Initialized ) return MFX_ERR_NOT_INITIALIZED;
//                hd->temporal_id = fr->FrameId.TemporalId;
//                if( fr->FrameId.TemporalId == layer->enc->TempIdMax )
//                    lastDid = true;
                break;
            }
        }
    }

    if (!surface && m_svc_count) {
        mfxU32 minpos = 0, pos;
        mfxI32 minpoc = 0x7fffffff;
        mfxU16 mindid = 0xffff;
        bool POC_in_progress = false;
        for ( pos = 0; pos < MAX_DEP_LAYERS; ++pos ) {
            h264Layer *lr = m_svc_layers[pos];
            if (!lr) continue;
            struct SVCLayerParams *lsps = &lr->enc->m_svc_layer;
            sNALUnitHeaderSVCExtension* hd = &lsps->svc_ext;
            if (lr->m_frameCountBuffered) {
                H264EncoderFrame_8u16s* frm =
                    H264EncoderFrameList_findOldestToEncode_8u16s(
                        &lr->enc->m_cpb,
                        &lr->enc->m_dpb,
                        0 /*noL1 ? 0 : lr->enc->m_l1_cnt_to_start_B*/, // TODO count L1 refs
                        lr->enc->m_info.treat_B_as_reference);
                if (!frm) // for KW, never happens
                    return MFX_ERR_NOT_FOUND;
                if (frm->m_PicOrderCnt[0] == m_selectedPOC) { // POC in progress, use only this
                    if (minpoc != m_selectedPOC || mindid > hd->dependency_id) { //other POC or smaller did
                        minpoc = m_selectedPOC;
                        mindid = hd->dependency_id;
                        minpos = pos;
                    }
                    POC_in_progress = true; // ignore not m_selectedPOC
                }
                else if (frm->m_PicOrderCnt[0] == minpoc && mindid > hd->dependency_id) { // smaller did with same POC
                    mindid = hd->dependency_id;
                    minpos = pos;
                }
                else if (frm->m_PicOrderCnt[0] < minpoc && !POC_in_progress) { // smaller POC preferred, the only for AVC
                    minpoc = frm->m_PicOrderCnt[0];
                    mindid = hd->dependency_id;
                    minpos = pos;
                }
            }
        }
        if (mindid == 0xffff) {
            // nothing buffered
            return MFX_ERR_NOT_FOUND; //MFX_ERR_MORE_DATA;
        }
        layer = m_svc_layers[minpos];
        m_selectedPOC = minpoc;
    }

    m_layer = layer;
    H264CoreEncoder_8u16s* cur_enc = layer->enc;
    mfxVideoParam *vp = &layer->m_mfxVideoParam;

    if( !m_base.m_Initialized ) return MFX_ERR_NOT_INITIALIZED;

    /* RD: not clear with HRD ??????????????????? */
    // bs could be NULL if input frame is buffered
    if (bs) {
        if( bs->MaxLength - (bs->DataOffset + bs->DataLength) < layer->enc->requiredBsBufferSize)
            return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    // field-based output: just copy encoded 2nd field from internal m_extBitstream
    if (m_fieldOutput) {
        if (pInternalParams == 0){
            return MFX_ERR_UNKNOWN;
        }
        mfxU8 fieldOutputState = (pInternalParams->InternalFlags & (FIELD_OUTPUT_FIRST_FIELD | FIELD_OUTPUT_SECOND_FIELD));
        if (fieldOutputState != FIELD_OUTPUT_SECOND_FIELD && fieldOutputState != FIELD_OUTPUT_FIRST_FIELD){
            return MFX_ERR_UNKNOWN;
        }
        if (fieldOutputState == FIELD_OUTPUT_SECOND_FIELD) {
            if (bs == 0){
                return MFX_ERR_UNKNOWN;
            }
            dataPtr = bs->Data + bs->DataOffset + bs->DataLength;
            // check bs for enough buffer size to place real encoded data
            if (bs->MaxLength - (bs->DataOffset + bs->DataLength) < m_extBitstream.DataLength)
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            bs->DataLength += m_extBitstream.DataLength;
            bs->FrameType = m_extBitstream.FrameType;
            bs->PicStruct = m_extBitstream.PicStruct;
            bs->TimeStamp = m_extBitstream.TimeStamp;
            MFX_INTERNAL_CPY(dataPtr, m_extBitstream.Data, m_extBitstream.DataLength);
            return MFX_ERR_NONE;
        }
    }

    Status res = UMC_OK;

    // inputSurface can be opaque. Get real surface from core
    surface = GetOriginalSurface(inputSurface);

    if (inputSurface != surface) { // input surface is opaque surface
        assert(surface);
        assert(inputSurface);
        surface->Data.FrameOrder = inputSurface->Data.FrameOrder;
        surface->Data.TimeStamp = inputSurface->Data.TimeStamp;
        surface->Data.Corrupted = inputSurface->Data.Corrupted;
        surface->Data.DataFlag = inputSurface->Data.DataFlag;
        surface->Info = inputSurface->Info;
    }

    mfxFrameSurface1 *initialSurface = surface;

    // bs could be NULL if input frame is buffered
    if (bs) {
        bitstream = bs;
        dataPtr = bitstream->Data + bitstream->DataOffset + bitstream->DataLength;
        initialDataLength = bitstream->DataLength;

        m_data_out.SetBufferPointer(dataPtr, bitstream->MaxLength - (bitstream->DataOffset + bitstream->DataLength));
        m_data_out.SetDataSize(0);

        // field-based output: prepare to place 2nd encoded field to m_extBitstream
        if (m_fieldOutput) {
            m_extBitstream.DataLength = 0;
            m_extBitstream.DataOffset = 0;
            m_data_out_ext.SetBufferPointer(m_extBitstream.Data, m_extBitstream.MaxLength);
            m_data_out_ext.SetDataSize(0);
            p_data_out_ext = &m_data_out_ext;
        }
    }

    if (surface)
    {
        // check PicStruct in surface
        mfxU16 picStruct;
        st = AnalyzePicStruct(layer, surface->Info.PicStruct, &picStruct);

#ifdef H264_INTRA_REFRESH
        bool isFieldEncoding = (m_base.enc->m_info.coding_type == CODINGTYPE_PICAFF || m_base.enc->m_info.coding_type == CODINGTYPE_INTERLACE) && !(picStruct & MFX_PICSTRUCT_PROGRESSIVE);
        if (isFieldEncoding && m_base.enc->m_refrType)
            m_base.enc->m_refrType = NO_REFRESH;
#endif // H264_INTRA_REFRESH

        if (st < MFX_ERR_NONE) // error occurred
            return st;

        UMC::ColorFormat cf = UMC::NONE;
        switch( surface->Info.FourCC ){
            case MFX_FOURCC_YV12:
                cf = UMC::YUV420;
                break;
            case MFX_FOURCC_RGB3:
                cf = UMC::RGB24;
                break;
            case MFX_FOURCC_NV12:
                cf = UMC::NV12;
                break;
            default:
                return MFX_ERR_NULL_PTR;
        }
//f        m_data_in.Init(surface->Info.Width, surface->Info.Height, cf, 8);
        m_data_in.Init(cur_enc->m_info.info.clip_info.width, cur_enc->m_info.info.clip_info.height, cf, 8);

        bool locked = false;
        if (layer->m_useAuxInput) { // copy from d3d to internal frame in system memory

            // Lock internal. FastCopy to use locked, to avoid additional lock/unlock
            st = m_core->LockFrame(layer->m_auxInput.Data.MemId, &layer->m_auxInput.Data);
            MFX_CHECK_STS(st);

            st = m_core->DoFastCopyWrapper(&layer->m_auxInput,
                                           MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                           surface,
                                           MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            MFX_CHECK_STS(st);

//            m_core->DecreaseReference(&(surface->Data)); // not here to keep related mfxEncodeCtrl

            layer->m_auxInput.Data.FrameOrder = surface->Data.FrameOrder;
            layer->m_auxInput.Data.TimeStamp = surface->Data.TimeStamp;
            surface = &layer->m_auxInput; // replace input pointer
        } else {
            if (surface->Data.Y == 0 && surface->Data.U == 0 &&
                surface->Data.V == 0 && surface->Data.A == 0)
            {
                st = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
                if (st == MFX_ERR_NONE)
                    locked = true;
                else
                    return st;
            }
        }

        if (!surface->Data.Y || surface->Data.Pitch > 0x7fff || surface->Data.Pitch < cur_enc->m_info.info.clip_info.width || !surface->Data.Pitch ||
            (surface->Info.FourCC == MFX_FOURCC_NV12 && !surface->Data.UV) ||
            (surface->Info.FourCC == MFX_FOURCC_YV12 && (!surface->Data.U || !surface->Data.V)) )
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        if( surface->Info.FourCC == MFX_FOURCC_YV12 ){
            m_data_in.SetPlanePointer(surface->Data.Y, 0);
            m_data_in.SetPlanePitch(surface->Data.Pitch, 0);
            m_data_in.SetPlanePointer(surface->Data.Cb, 1);
            m_data_in.SetPlanePitch(surface->Data.Pitch >> 1, 1);
            m_data_in.SetPlanePointer(surface->Data.Cr, 2);
            m_data_in.SetPlanePitch(surface->Data.Pitch >> 1, 2);
        }else if( surface->Info.FourCC == MFX_FOURCC_NV12 ){
            m_data_in.SetPlanePointer(surface->Data.Y, 0);
            m_data_in.SetPlanePitch(surface->Data.Pitch, 0);
            m_data_in.SetPlanePointer(surface->Data.UV, 1);
            m_data_in.SetPlanePitch(surface->Data.Pitch, 1);
        }

        m_data_in.SetTime(GetUmcTimeStamp(surface->Data.TimeStamp));

        if (vp->mfx.EncodedOrder)
            layer->m_frameCount = surface->Data.FrameOrder;

        // need to call always now
        res = Encode(cur_enc, &m_data_in, &m_data_out, p_data_out_ext, picStruct, ctrl, initialSurface);

        if ( layer->m_useAuxInput ) {
            m_core->UnlockFrame(layer->m_auxInput.Data.MemId, &layer->m_auxInput.Data);
        } else {
            if (locked)
                m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));
        }
        layer->m_frameCount ++;
    } else {
        res = Encode(cur_enc, 0, &m_data_out, p_data_out_ext, 0, 0, 0);
    }


    mfxStatus mfxRes = MFX_ERR_MORE_DATA;
    if (UMC_OK == res && cur_enc->m_pCurrentFrame) {
        //Copy timestamp to output.
        mfxFrameSurface1* surf = 0;
        bs->TimeStamp = GetMfxTimeStamp(m_data_out.GetTime());
        surf = (mfxFrameSurface1*)cur_enc->m_pCurrentFrame->m_pAux[1];
        if (surf) {
            bs->TimeStamp = surf->Data.TimeStamp;
            bs->DecodeTimeStamp = CalculateDTSFromPTS_H264enc(
                m_base.m_mfxVideoParam.mfx.FrameInfo,
                cur_enc->m_SEIData.PictureTiming.dpb_output_delay,
                bs->TimeStamp);            
            m_core->DecreaseReference(&(surf->Data));
            cur_enc->m_pCurrentFrame->m_pAux[0] = cur_enc->m_pCurrentFrame->m_pAux[1] = 0;
        }

        if (m_fieldOutput)
        {
            m_extBitstream.TimeStamp = bs->TimeStamp;
            m_extBitstream.DecodeTimeStamp = bs->DecodeTimeStamp;
        }

        // Set FrameType
        mfxU16 frame_type = 0, frame_type_ext = 0;
        switch (cur_enc->m_pCurrentFrame->m_PicCodType) { // set pic type for frame or 1st field
            case INTRAPIC: frame_type = MFX_FRAMETYPE_I; break;
            case PREDPIC:  frame_type = MFX_FRAMETYPE_P; break;
            case BPREDPIC: frame_type = MFX_FRAMETYPE_B; break;
            default: {return MFX_ERR_UNKNOWN;}
        }
        if (cur_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE) { // surface was encoded as 2 fields - set pic type for 2nd field
            if (m_fieldOutput == false) {
                switch (cur_enc->m_pCurrentFrame->m_PicCodTypeFields[1]) {
                    case INTRAPIC: frame_type |= MFX_FRAMETYPE_xI; break;
                    case PREDPIC:  frame_type |= MFX_FRAMETYPE_xP; break;
                    case BPREDPIC: frame_type |= MFX_FRAMETYPE_xB; break;
                    default: {return MFX_ERR_UNKNOWN;}
                }
            } else {
                switch (cur_enc->m_pCurrentFrame->m_PicCodTypeFields[1]) {
                    case INTRAPIC: frame_type_ext = MFX_FRAMETYPE_xI; break;
                    case PREDPIC:  frame_type_ext = MFX_FRAMETYPE_xP; break;
                    case BPREDPIC: frame_type_ext = MFX_FRAMETYPE_xB; break;
                    default: {return MFX_ERR_UNKNOWN;}
                }
            }
        }

        if( cur_enc->m_pCurrentFrame->m_bIsIDRPic ){
            frame_type |= MFX_FRAMETYPE_IDR;
        }
        if( cur_enc->m_pCurrentFrame->m_RefPic ){
            frame_type |= MFX_FRAMETYPE_REF;
            if (cur_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE) {
                if (m_fieldOutput == false)
                    frame_type |= MFX_FRAMETYPE_xREF;
                else
                    frame_type_ext |= MFX_FRAMETYPE_xREF;
            }
        }
        bs->FrameType = frame_type;
        if (m_fieldOutput)
            m_extBitstream.FrameType = frame_type_ext;

        //Set pic structure
        bs->PicStruct = cur_enc->m_pCurrentFrame->m_coding_type;
        if (m_fieldOutput)
            m_extBitstream.PicStruct = cur_enc->m_pCurrentFrame->m_coding_type;

        if((m_data_out.GetDataSize() + (m_fieldOutput ? m_data_out_ext.GetDataSize() : 0)) != 0 ) mfxRes = MFX_ERR_NONE;
    } else {
        mfxRes = h264enc_ConvertStatus(res);
        if (mfxRes == MFX_ERR_MORE_DATA)
            mfxRes = MFX_ERR_NONE;
    }

    // bs could be NULL if input frame is buffered
    if (bs) {
        mfxI32 bytes = (mfxU32) m_data_out.GetDataSize();
        bitstream->DataLength += bytes;

        if (m_fieldOutput) {
            bytes = (mfxU32) m_data_out_ext.GetDataSize();
            m_extBitstream.DataLength = bytes;
        }

        if( UMC_OK == res && layer->m_putEOSeq && cur_enc->m_pCurrentFrame && cur_enc->m_pCurrentFrame->m_putEndOfSeq) {
            if (m_fieldOutput == false)
                WriteEndOfSequence(bs);
            else
                WriteEndOfSequence(&m_extBitstream);
            cur_enc->m_pCurrentFrame->m_putEndOfSeq = false;
        }

        if( layer->m_putEOStream && surface == NULL){
            if (m_fieldOutput == false)
                WriteEndOfStream(bs);
            else
                WriteEndOfStream(&m_extBitstream);
        }

        if((bitstream->DataLength != initialDataLength) &&
            (m_fieldOutput ? m_extBitstream.DataLength > 0 : 1))
          layer->m_encodedFrames++;
        layer->m_totalBits += (bitstream->DataLength - initialDataLength) * 8;
        if (m_fieldOutput)
            layer->m_totalBits += m_extBitstream.DataLength * 8;
    }

    return mfxRes;
}

//mfxStatus MFXVideoENCODEH264::InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts)
//{
//    //Store value for further use
//    //Throw away all previous data
//    if( m_userData != NULL ) free( m_userData );
//    m_userData = (mfxU8*)malloc(len);
//    MFX_INTERNAL_CPY( m_userData, ud, len );
//    m_userDataLen = len;
//    m_userDataTime = ts;
//
//    return MFX_ERR_NONE;
//}

mfxStatus MFXVideoENCODEH264::GetVideoParam(mfxVideoParam *par)
{
    if( !m_base.m_Initialized ) return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par)
    CHECK_VERSION(par->Version);

    if (0 > CheckExtBuffers_H264enc( par->ExtParam, par->NumExtParam ))
        return MFX_ERR_UNSUPPORTED;
    mfxExtCodingOption* opts = GetExtCodingOptions( par->ExtParam, par->NumExtParam );
    mfxExtCodingOption2* opts2 = (mfxExtCodingOption2*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );
    mfxExtCodingOptionSPSPPS* optsSP = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    mfxExtAvcTemporalLayers* tempLayers = (mfxExtAvcTemporalLayers*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
    mfxExtSVCSeqDesc* pSvcLayers = (mfxExtSVCSeqDesc*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
    mfxExtSVCRateControl *rc = (mfxExtSVCRateControl*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );

    // copy structures and extCodingOption if exist in dst
    par->mfx = m_base.m_mfxVideoParam.mfx;
//  if (m_base.m_mfxVideoParam.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    m_base.m_mfxVideoParam.GetCalcParams(par);
    par->Protected = 0;
    par->AsyncDepth = m_base.m_mfxVideoParam.AsyncDepth;
    par->IOPattern = m_base.m_mfxVideoParam.IOPattern;
    if (opts != NULL) {
        m_base.m_extOption.EndOfSequence      = (mfxU16)( m_base.m_putEOSeq ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_base.m_extOption.ResetRefList       = (mfxU16)( m_base.m_resetRefList ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_base.m_extOption.EndOfStream        = (mfxU16)( m_base.m_putEOStream ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_base.m_extOption.PicTimingSEI       = (mfxU16)( m_base.m_putTimingSEI ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_base.m_extOption.FieldOutput        = (mfxU16)( m_fieldOutput ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_base.m_extOption.SingleSeiNalUnit   = (mfxU16)( m_separateSEI ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON );
        *opts = m_base.m_extOption;
    }

    if (opts2 != NULL) {
        opts2->IntRefType = m_base.enc->m_refrType;
        opts2->IntRefQPDelta = m_base.enc->m_QPDelta;
        if(m_base.enc->m_refrType) {
            mfxU16 refreshSizeInMBs =  m_base.enc->m_refrType == HORIZ_REFRESH ? par->mfx.FrameInfo.Height >> 4 : par->mfx.FrameInfo.Width >> 4;
            opts2->IntRefCycleSize = (refreshSizeInMBs + m_base.enc->m_stripeWidth - 1)/m_base.enc->m_stripeWidth;
        } else {
            opts2->IntRefCycleSize = 0;
        }
    }

    if (tempLayers) {
        mfxU8 i = 0;
        if (m_temporalLayers.NumLayers) {
            tempLayers->BaseLayerPID = m_temporalLayers.LayerPIDs[0];
            for ( i = 0; i < m_temporalLayers.NumLayers; i ++) {
                tempLayers->Layer[i].Scale = m_temporalLayers.LayerScales[i];
            }
        } else
            tempLayers->BaseLayerPID = 0;

        for (; i < 8; i ++) {
            tempLayers->Layer[i].Scale = 0;
        }
    }

    // get copy of SPS/PPS headers
    if (optsSP != 0) {
        optsSP->SPSId = m_base.enc->m_SeqParamSet->seq_parameter_set_id;
        optsSP->PPSId = m_base.enc->m_PicParamSet->pic_parameter_set_id;
        if (optsSP->SPSBuffer && optsSP->PPSBuffer) {
            MFX_CHECK(optsSP->SPSBufSize, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(optsSP->PPSBufSize, MFX_ERR_INVALID_VIDEO_PARAM);

            UMC::MediaData m_data_SPS, m_data_PPS;
            UMC::Status ps;
            H264BsReal_8u16s* bs;

            m_data_SPS.SetBufferPointer(optsSP->SPSBuffer, optsSP->SPSBufSize);
            m_data_PPS.SetBufferPointer(optsSP->PPSBuffer, optsSP->PPSBufSize);

            bs = m_base.enc->m_bs1; // temporary use main bitstream to encode SPS/PPS headers

            ps = H264CoreEncoder_encodeSPSPPS_8u16s( m_base.enc, bs, &m_data_SPS, &m_data_PPS);

            optsSP->SPSBufSize = (mfxU16)m_data_SPS.GetDataSize();
            optsSP->PPSBufSize = (mfxU16)m_data_PPS.GetDataSize();

            if (ps != UMC_OK)
                return h264enc_ConvertStatus(ps);
        }
    }

    if (pSvcLayers) {
        mfxU32 i0, i1;

        for ( i0 = 0; i0 < MAX_DEP_LAYERS; i0++ ) {
            if (!m_svc_layers[i0]) {
                pSvcLayers->DependencyLayer[i0].Active = 0;
                continue;
            }
            pSvcLayers->DependencyLayer[i0] = m_svc_layers[i0]->m_InterLayerParam;
            for (i1 = 0; i1 < MAX_TEMP_LEVELS; i1++)
                pSvcLayers->TemporalScale[i1] = 0;
            for (i1 = 0; i1 < pSvcLayers->DependencyLayer[i0].TemporalNum; i1++) {
                mfxU16 tid = pSvcLayers->DependencyLayer[i0].TemporalId[i1];
                pSvcLayers->TemporalScale[tid] = m_svc_layers[i0]->enc->TemporalScaleList[i1]; // no id gaps in enc
            }
            // overwrite AVC params from top layer
            par->mfx.BufferSizeInKB = (m_svc_layers[i0]->enc->requiredBsBufferSize + 999) / 1000;
        }
    }

    if (rc) {
        mfxU32 ii = 0, did, qid, tid;
        rc->RateControlMethod = par->mfx.RateControlMethod;

        for (did = 0; did < MAX_DEP_LAYERS; did++) {
            if (!m_svc_layers[did])
                continue;
            for (qid = 0; qid < m_svc_layers[did]->m_InterLayerParam.QualityNum; qid++) {
                for (tid = 0; tid < m_svc_layers[did]->m_InterLayerParam.TemporalNum; tid++) {
                    rc->Layer[ii].DependencyId = did;
                    rc->Layer[ii].QualityId = qid;
                    rc->Layer[ii].TemporalId = tid;
                    if (rc->RateControlMethod == MFX_RATECONTROL_CBR || rc->RateControlMethod == MFX_RATECONTROL_VBR) {
                        rc->Layer[ii].CbrVbr.BufferSizeInKB = m_HRDparams[did][qid][tid].BufferSizeInKB;
                        rc->Layer[ii].CbrVbr.TargetKbps = m_HRDparams[did][qid][tid].TargetKbps;
                        rc->Layer[ii].CbrVbr.InitialDelayInKB = m_HRDparams[did][qid][tid].InitialDelayInKB;
                        rc->Layer[ii].CbrVbr.MaxKbps = m_HRDparams[did][qid][tid].MaxKbps;
                    } else if (rc->RateControlMethod == MFX_RATECONTROL_CQP) {
                        rc->Layer[ii].Cqp.QPI = m_svc_layers[did]->m_cqp[qid][tid].QPI;
                        rc->Layer[ii].Cqp.QPP = m_svc_layers[did]->m_cqp[qid][tid].QPP;
                        rc->Layer[ii].Cqp.QPB = m_svc_layers[did]->m_cqp[qid][tid].QPB;
                    } 
                    //          else { AVBR - currently not supported }
                    ii++;
               }
            }
            // overwrite AVC params from top layer
            par->mfx.BufferSizeInKB = (m_svc_layers[did]->enc->requiredBsBufferSize + 999) / 1000;
        }
        rc->NumLayers = ii;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::GetFrameParam(mfxFrameParam * /*par*/)
{
    return MFX_ERR_UNSUPPORTED;
}
//
//mfxStatus MFXVideoENCODEH264::GetSliceParam(mfxSliceParam *par)
//{
//    MFX_CHECK_NULL_PTR1(par)
//    *par = m_mfxSliceParam;
//    return MFX_ERR_NONE;
//}



//locates center and splits
mfxU16 MFXVideoENCODEH264::SetBEncOrder (sH264EncoderFrame_8u16s *left, sH264EncoderFrame_8u16s *right, mfxU16 order, mfxI16 freerefs)
{
    h264Layer *layer = m_layer;
    H264CoreEncoder_8u16s* enc = layer->enc;
    mfxVideoParam *vp = &layer->m_mfxVideoParam;
    sH264EncoderFrame_8u16s *pCurr, *pBest = 0;
    Ipp32s lpoc = left->m_FrameDisp;
    Ipp32s rpoc = right ? right->m_FrameDisp : enc->m_pLastFrame->m_FrameDisp + enc->rateDivider;
    Ipp32s bpoc=0, bdist=0;

    if (rpoc - lpoc < 2) return order;
    for (pCurr = enc->m_cpb.m_pHead; pCurr; pCurr = pCurr->m_pFutureFrame) {
        Ipp32s cpoc = pCurr->m_FrameDisp;
        Ipp32s cdist = (cpoc-lpoc)*(rpoc-cpoc); // the greater the closer to center
        if (cpoc <= lpoc || cpoc >= rpoc) continue;
        if (rpoc - lpoc == 2) {
            pCurr->m_BEncOrder = order++;
            if (!right && !vp->mfx.EncodedOrder && !(vp->mfx.GopOptFlag & MFX_GOP_STRICT)) {
                //pCurr->m_RefPic = 1;
                pCurr->m_PicCodType = PREDPIC;
            }
            pCurr->m_BRefCount = 0;
            left->m_BRefCount++;
            if (right)
                right->m_BRefCount++;
            return order;
        }
        if (freerefs <= 0) { //no refs avail - make oldest B not ref
            if (!pBest || cpoc < bpoc) {
                pBest = pCurr;
                bpoc = cpoc;
                //bdist = cdist;
            }
        }
        // find lower tId and closer to center
        else if (!pBest || pCurr->m_temporalId < pBest->m_temporalId ||
            (pCurr->m_temporalId == pBest->m_temporalId &&
            (cdist > bdist || ((cdist == bdist) && cpoc < bpoc)) )) {
            pBest = pCurr;
            bpoc = cpoc;
            bdist = cdist;
        }
    }
    // mark found as reference, recursively call sides
    if (!pBest)
        return order; // impossible
    pBest->m_BEncOrder = order++;
    pBest->m_BRefCount = 0;
    left->m_BRefCount++;
    if (right)
        right->m_BRefCount++;
    if (freerefs < 1) {
        //pBest->m_RefPic = 0;
        if (rpoc - bpoc > 1)
            order = SetBEncOrder (pBest, right, order, 0);
        return order;
    }
    pBest->m_RefPic = 1;
    if (!right && !vp->mfx.EncodedOrder && !(vp->mfx.GopOptFlag & MFX_GOP_STRICT))
            pBest->m_PicCodType = PREDPIC;
    if (bpoc - lpoc > 1)
        order = SetBEncOrder ( left, pBest, order, freerefs - 1);
    if (rpoc - bpoc > 1)
        order = SetBEncOrder (pBest, right, order, freerefs - 1);
    return order;
}

// selects buffered B to become references after normal reference frame came
// number of ref B should be <= num_ref_frames - 2
void MFXVideoENCODEH264::MarkBRefs(mfxI32 lastIPdist)
{
    H264CoreEncoder_8u16s* enc = m_layer->enc;
    mfxVideoParam *vp = &m_layer->m_mfxVideoParam;
    if ( enc->m_info.treat_B_as_reference && lastIPdist > 2 && enc->m_SeqParamSet->num_ref_frames > 2) {
        sH264EncoderFrame_8u16s *pCurr;
        // search for previous ref frame
        for (pCurr = enc->m_dpb.m_pTail; pCurr; pCurr = pCurr->m_pPreviousFrame)
            if (pCurr->m_FrameDisp == m_layer->m_lastRefFrame) {
                if (!enc->m_pCurrentFrame && !vp->mfx.EncodedOrder && enc->m_pLastFrame->m_temporalId == 0) {
                    if (!(vp->mfx.GopOptFlag & MFX_GOP_STRICT))
                        enc->m_pLastFrame->m_PicCodType = PREDPIC; // change type of last frame in type to P
                    enc->m_pLastFrame->m_RefPic = 1;
                    SetBEncOrder(pCurr, enc->m_pLastFrame, 1, enc->m_SeqParamSet->num_ref_frames - 2);
                } else
                    SetBEncOrder(pCurr, enc->m_pCurrentFrame, 1,
                      enc->m_SeqParamSet->num_ref_frames - 2 + (enc->m_pCurrentFrame==0));
                break;
            }
    }
}

// select frame type on VPP analysis results
// or psy-analysis if ANALYSE_AHEAD set
// performed now on previous to latest, so, having next frame in cpb
// returns 1 if appeared B frame without L1 refs, 0 otherwise
// bNotLast for PsyAnalyze means have next frame, for !Psy - have current (new)
// when treat_B_as_reference order for the B-group is defined here
mfxI32 MFXVideoENCODEH264::SelectFrameType_forLast( bool bNotLast, mfxEncodeCtrl *ctrl )
{
    mfxExtVppAuxData* optsSA = 0;
    bool scene_change = false;
    bool forced_IDR = false;
    bool forced_I = false;
    bool close_gop = false;
    bool internal_detect = false;
    h264Layer *layer = m_layer;
    H264CoreEncoder_8u16s* enc = layer->enc;
    mfxI32 frameNum = /*enc->m_pCurrentFrame ? enc->m_pCurrentFrame->m_FrameDisp :*/ m_layer->m_frameCount; // to check
    mfxVideoParam *vp = &layer->m_mfxVideoParam;
    bool allowTypeChange = !vp->mfx.EncodedOrder && !(vp->mfx.GopOptFlag & MFX_GOP_STRICT);
    EnumPicCodType eOriginalType = INTRAPIC; // avoiding the warning

#ifdef USE_TEMPTEMP
    if (!enc->m_pCurrentFrame || !bNotLast) // more cases to be here
        m_usetemptemp = 0;
    // new approach, take all info from refctrl
    mfxI32 pos;
    if (m_usetemptemp &&
        !(enc->m_Analyse_ex & ANALYSE_PSY_AHEAD)) {
        if (enc->m_pCurrentFrame->m_bIsIDRPic) {
            pos = 0;
        } else {
            pos = enc->m_pCurrentFrame->m_FrameDisp*enc->rateDivider - *enc->temptempstart;
            if (pos >= enc->temptemplen || pos > 0 && !enc->refctrl[pos].codorder)
                pos -= enc->temptempwidth;
            assert(pos>=0);
            assert(pos < enc->temptemplen);
            assert(pos == 0 || enc->refctrl[pos].codorder > 0);
        }
        enc->m_pCurrentFrame->m_PicCodType = eOriginalType = enc->refctrl[pos].pcType;
        enc->m_pCurrentFrame->m_RefPic = enc->refctrl[pos].isRef;
        enc->m_pCurrentFrame->m_temporalId = enc->refctrl[pos].tid;
        enc->m_pCurrentFrame->m_bIsIDRPic = enc->refctrl[pos].pcType == INTRAPIC && enc->refctrl[pos].tid == 0 &&
            (Ipp32s)(frameNum - layer->m_lastIDRframe) >= enc->m_info.key_frame_controls.interval * (enc->m_info.key_frame_controls.idr_interval+1);
        if (enc->m_pCurrentFrame->m_bIsIDRPic)
            layer->m_lastIDRframe = frameNum;
        //return ; // need a proper value (num bwd refs)
    } else {

    }
#endif // USE_TEMPTEMP

    // TODO svc case
#ifdef USE_TEMPTEMP
    if (!m_usetemptemp)
#endif // USE_TEMPTEMP
    if (!bNotLast && !(enc->m_Analyse_ex & ANALYSE_PSY_AHEAD)) { // terminate GOP, no input
        if (enc->m_pLastFrame && enc->m_pLastFrame->m_PicCodType == BPREDPIC && !enc->m_pLastFrame->m_wasEncoded) {
            if (allowTypeChange) {
                enc->m_pLastFrame->m_PicCodType = PREDPIC;
                enc->m_pLastFrame->m_RefPic = 1;
            } else {
                enc->m_pLastFrame->m_noL1RefForB = true;
                if (enc->m_info.treat_B_as_reference && enc->m_info.B_frame_rate > 1)
                    enc->m_pLastFrame->m_RefPic = 1;
                else {
                    // encode all buffered B frames with no future references
                    for (H264EncoderFrame_8u16s *fr = enc->m_cpb.m_pHead; fr; fr = fr->m_pFutureFrame)
                        if (!fr->m_wasEncoded && fr->m_PicCodType == BPREDPIC)
                            fr->m_noL1RefForB = true;
                }
            }
        }
        if (enc->m_info.treat_B_as_reference && enc->m_info.B_frame_rate > 1) {
            if (enc->m_pLastFrame && enc->m_pLastFrame->m_BEncOrder==0) // if m_BEncOrder>0 => already done
                MarkBRefs(frameNum - layer->m_lastRefFrame);
        }
        return 0;
    }

#ifdef USE_TEMPTEMP
    if (!m_usetemptemp)
#endif // USE_TEMPTEMP
    {
        H264CoreEncoder_8u16s *topenc = m_svc_layers[m_maxDid] ? m_svc_layers[m_maxDid]->enc : enc;
        // currently I are possible only at 0th temporal layer
        if ( !frameNum ||
            (enc->m_pCurrentFrame && //enc->m_pCurrentFrame->m_temporalId == 0 &&
            (Ipp32s)(frameNum-layer->m_lastIframe)*enc->rateDivider >= topenc->m_info.key_frame_controls.interval) )
            eOriginalType = INTRAPIC;
        else if ((Ipp32s)(frameNum-layer->m_lastRefFrame)*enc->rateDivider > topenc->m_info.B_frame_rate)
            eOriginalType = PREDPIC;
        else
            eOriginalType = BPREDPIC;
    }

    if (!layer->m_mfxVideoParam.mfx.EncodedOrder && ctrl && ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN) {
        if (ctrl->FrameType & MFX_FRAMETYPE_IDR)
            forced_IDR = true;
        else
            forced_I = true;
#ifdef USE_TEMPTEMP
        m_usetemptemp = 0;
#endif // USE_TEMPTEMP
    } else if (allowTypeChange) {
        if (ctrl)
            optsSA = (mfxExtVppAuxData*)GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA );
        if (optsSA) // vpp_aux present and filled
#ifdef USE_TEMPTEMP
            m_usetemptemp = 0;
#endif // USE_TEMPTEMP
        if (optsSA) // vpp_aux present and filled
            scene_change = (optsSA->SceneChangeRate > 90
                /*|| optsSA->SpatialComplexity && optsSA->TemporalComplexity && optsSA->SpatialComplexity*4 <= 3*optsSA->TemporalComplexity*/ );
        if (!scene_change) {
            internal_detect = ((enc->m_Analyse & ANALYSE_FRAME_TYPE) != 0);
#ifdef H264_INTRA_REFRESH
            if (enc->m_refrType)
                internal_detect = false;
#endif // H264_INTRA_REFRESH
        }


        //if (optsSA) printf("\ts:%4d t:%4d chance:%3d%c%c ", optsSA->SpatialComplexity, optsSA->TemporalComplexity, optsSA->SceneChangeRate,
        //    optsSA->SpatialComplexity*4 <= 3*optsSA->TemporalComplexity ? 'I':' ',
        //    optsSA->SpatialComplexity*2 <= 3*optsSA->TemporalComplexity ? 'P':' ');
    } else
        optsSA = 0; // no need to detect

    // TODO svc case
    if (!bNotLast && eOriginalType == BPREDPIC && allowTypeChange ) {
        eOriginalType = PREDPIC; // avoid tail B (display order)
#ifdef USE_TEMPTEMP
        m_usetemptemp = 0;
#endif // USE_TEMPTEMP
    }

#ifdef USE_PSYINFO
    PsyInfo *stat = &enc->m_pCurrentFrame->psyStatic;
    PsyInfo *dyn = enc->m_pCurrentFrame->psyDyna;
    if (internal_detect && !optsSA && bNotLast && (enc->m_Analyse_ex & ANALYSE_PSY_SCENECHANGE)) {
        if (eOriginalType == INTRAPIC && dyn[0].dc > 4 && dyn[1].dc > dyn[0].dc * 3 && dyn[1].dc > dyn[2].dc * 2) {
            eOriginalType = PREDPIC; // most probably next is scene change
        }
        if (dyn[0].dc*2 > dyn[1].dc * 3 && dyn[0].dc > dyn[2].dc * 3 || dyn[0].dc > dyn[1].dc * 3 && dyn[0].dc*2 > dyn[2].dc * 3){
            scene_change = true;
            internal_detect = false;
            eOriginalType = INTRAPIC;
        }
    }
    if (internal_detect && eOriginalType == BPREDPIC && (enc->m_Analyse_ex & ANALYSE_PSY_TYPE_FR)) {
        Ipp32s cmplxty_s = stat->bgshift + stat->details;
        Ipp32s cmplxty_d0 = dyn[0].bgshift + dyn[0].details;
        Ipp32s cmplxty_d1 = dyn[1].bgshift + dyn[1].details;
        if (cmplxty_s > 0 && cmplxty_s*5 < cmplxty_d0*4 && (!bNotLast || cmplxty_s*5 < cmplxty_d1*4) ) {
            eOriginalType = PREDPIC;
            internal_detect = false;
        }
        if (cmplxty_s > 10*cmplxty_d0) {
            internal_detect = false;
        }
    }
#endif // USE_PSYINFO

#ifdef USE_TEMPTEMP
    if (!m_usetemptemp)
#endif // USE_TEMPTEMP
    {
        EnumPicCodType ePictureType;

        enc->m_pCurrentFrame->m_bIsIDRPic = 0; // TODO: decide on IDR for EncodedOrder
        if (vp->mfx.EncodedOrder) {
            ePictureType = enc->m_pCurrentFrame->m_PicCodType;
        }
        else if ( !frameNum || eOriginalType == INTRAPIC || scene_change || forced_IDR || forced_I) {
            ePictureType = INTRAPIC;
        } else if (eOriginalType == PREDPIC  || (optsSA && (optsSA->SpatialComplexity*2 <= 3*optsSA->TemporalComplexity) && optsSA->TemporalComplexity)) {
            ePictureType = PREDPIC;
        } else
            ePictureType = BPREDPIC; // B can be changed later
        enc->m_pCurrentFrame->m_PicCodType = ePictureType;
        enc->m_pCurrentFrame->m_RefPic = ePictureType != BPREDPIC;

        if (internal_detect && !(enc->m_Analyse_ex & ANALYSE_PSY_TYPE_FR)) { // can change B->P->I
            H264CoreEncoder_FrameTypeDetect_8u16s(enc);
            ePictureType = enc->m_pCurrentFrame->m_PicCodType;
            scene_change = internal_detect && (ePictureType == INTRAPIC) && (eOriginalType != INTRAPIC);
        }

        if (ePictureType == INTRAPIC && enc->m_pCurrentFrame->m_temporalId == 0) // IDR only tId==0
            if (!frameNum || (Ipp32s)(frameNum - layer->m_lastIDRframe) >= enc->m_info.key_frame_controls.interval * (enc->m_info.key_frame_controls.idr_interval)+1
                || forced_IDR || forced_I || scene_change) {
                    enc->m_pCurrentFrame->m_bIsIDRPic = 1;
            }

        // update m_EncodedFrameNum, specify non-reference frame type for highest temporal layer
        if (m_temporalLayers.NumLayers) {
            if (enc->m_pCurrentFrame->m_bIsIDRPic == false) {
                enc->m_pCurrentFrame->m_EncodedFrameNum = layer->m_frameCount - layer->m_lastIDRframe;
                if (m_temporalLayers.NumLayers > 1) {
                    mfxU16 layerId = GetTemporalLayer(enc->m_pCurrentFrame->m_EncodedFrameNum, m_temporalLayers.NumLayers);
                    if (layerId == m_temporalLayers.NumLayers - 1) // no reference pictures in highest temporal layer
                        enc->m_pCurrentFrame->m_RefPic = 0;
                }
            } else
                enc->m_pCurrentFrame->m_EncodedFrameNum = 0;
        }

        if (ePictureType == INTRAPIC || ePictureType == PREDPIC)
            if (enc->m_info.treat_B_as_reference && enc->m_info.B_frame_rate)
                MarkBRefs(frameNum - layer->m_lastRefFrame); // TODO check for temporal
        if (ePictureType == INTRAPIC) {
            //if (enc->m_pCurrentFrame->m_temporalId == 0) // true key only tId==0
                layer->m_lastIframe = frameNum;
            if (enc->m_pCurrentFrame->m_RefPic)
                layer->m_lastRefFrame = frameNum;
            if (enc->m_pCurrentFrame->m_bIsIDRPic)
                layer->m_lastIDRframe = frameNum;
        } else if (ePictureType == PREDPIC && enc->m_pCurrentFrame->m_RefPic)
            layer->m_lastRefFrame = frameNum;

        close_gop = ePictureType == INTRAPIC && enc->m_pCurrentFrame->m_temporalId == 0 &&
            (vp->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT)) == MFX_GOP_CLOSED;
    }
    //Close previous GOP in the end, on scene change, on IDR (need all I when CLOSED_GOP is set?)
    // if last input proposed as B, change it to P
#ifdef USE_TEMPTEMP
    if (!m_usetemptemp)
#endif // USE_TEMPTEMP
    if ( scene_change || enc->m_pCurrentFrame->m_bIsIDRPic || close_gop) {
        if( enc->m_pLastFrame && enc->m_pLastFrame->m_PicCodType == BPREDPIC && !enc->m_pLastFrame->m_wasEncoded) {
            if(allowTypeChange) {
                enc->m_pLastFrame->m_PicCodType = PREDPIC;
                enc->m_pLastFrame->m_RefPic = 1;
            }
        }
    }

    // control insertion of "End of sequence" NALU
    // if B frame buffer is used then choose frame for EOSeq insertion when current frame is IDR
    if (frameNum && enc->m_pCurrentFrame->m_bIsIDRPic && layer->m_frameCountBuffered) {
        H264EncoderFrame_8u16s *fr, *frEOSeq = 0;
        for (fr = enc->m_cpb.m_pHead; fr; fr = fr->m_pFutureFrame) {
            if (!fr->m_wasEncoded) {
                if (!frEOSeq || fr->m_PicCodType > frEOSeq->m_PicCodType ||
                    (fr->m_PicCodType == BPREDPIC && frEOSeq->m_PicCodType == BPREDPIC && ((frEOSeq->m_RefPic && !fr->m_RefPic) || (frEOSeq->m_RefPic == fr->m_RefPic && fr->m_PicOrderCnt[0] > frEOSeq->m_PicOrderCnt[0]))) ||
                    (fr->m_PicCodType != BPREDPIC && fr->m_PicCodType == frEOSeq->m_PicCodType && fr->m_PicOrderCnt[0] > frEOSeq->m_PicOrderCnt[0]))
                    frEOSeq = fr;
            }
        }
        if (frEOSeq)
            frEOSeq->m_putEndOfSeq = true;
    }

    // if B frame buffer isn't used then then check next frame for IDR to insert EOSeq
    if (!layer->m_frameCountBuffered && (Ipp32s)((frameNum + 1) - layer->m_lastIframe) >= enc->m_info.key_frame_controls.interval &&
        (Ipp32s)((frameNum + 1) - layer->m_lastIDRframe) >= enc->m_info.key_frame_controls.interval * (enc->m_info.key_frame_controls.idr_interval+1))
            enc->m_pCurrentFrame->m_putEndOfSeq = true;

    return 0;
}

#define PIXTYPE Ipp8u

Status MFXVideoENCODEH264::Encode(
    void* state,
    VideoData* src,
    MediaData* dst,
    MediaData* dst_ext,
    Ipp16u picStruct,
    mfxEncodeCtrl *ctrl,
    mfxFrameSurface1 *surface)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s*)state;
    Status ps = UMC_OK;
    core_enc->m_pCurrentFrame = 0;
    core_enc->m_field_index = 0;
    ////Ipp32s isRef = 0;
    h264Layer *layer = m_layer;
    mfxVideoParam *vp = &layer->m_mfxVideoParam;

    if (src) { // copy input frame to internal buffer
        core_enc->m_pCurrentFrame = H264EncoderFrameList_InsertFrame_8u16s(
            &core_enc->m_cpb,
            src,
            INTRAPIC/*ePictureType*/,
            0/*isRef*/,
            core_enc->m_info.num_slices * ((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1),
            core_enc->m_PaddedSize
#if defined ALPHA_BLENDING_H264
            , core_enc->m_SeqParamSet->aux_format_idc
#endif
            );

        if (core_enc->m_pCurrentFrame == 0)
            return UMC_ERR_ALLOC;

        if (vp->mfx.EncodedOrder) {
            ////core_enc->m_pCurrentFrame->m_FrameTag = surface->Data.FrameOrder; // mark UMC frame with it's mfx FrameOrder
            core_enc->m_pCurrentFrame->m_FrameDisp = surface->Data.FrameOrder; // mark UMC frame with it's mfx FrameOrder
        } else if (core_enc->m_pLastFrame) {
            ////core_enc->m_pCurrentFrame->m_FrameTag = core_enc->m_pLastFrame->m_FrameTag + core_enc->rateDivider;
            core_enc->m_pCurrentFrame->m_FrameDisp = core_enc->m_pLastFrame->m_FrameDisp + core_enc->rateDivider;
        } else {
            ////core_enc->m_pCurrentFrame->m_FrameTag = 0;
            core_enc->m_pCurrentFrame->m_FrameDisp = 0;
        }
        core_enc->m_pCurrentFrame->m_BRefCount = 0;

        ////layer->m_frameCount = core_enc->m_pCurrentFrame->m_FrameTag;

        // SVC:
        // create not provided frames for lower layers, start with them
        // ...
        for (h264Layer* prev_lr = layer->ref_layer; prev_lr; prev_lr = prev_lr->ref_layer) {
            sH264EncoderFrame_8u16s *src_from;
            ////src_from = H264EncoderFrameList_findSVCref_8u16s(
            ////    &prev_lr->enc->m_dpb,
            ////    core_enc->m_pCurrentFrame);
            src_from = prev_lr->enc->m_pLastFrame; // last input on prev layer
            if (src_from /*&& (src_from->m_wasEncoded || src_from->m_wasInterpolated)*/)
                break;
            UMC::VideoData vdata;
            vdata.Init(prev_lr->enc->m_info.info.clip_info.width, prev_lr->enc->m_info.info.clip_info.height, src->GetColorFormat(), 8);
            prev_lr->enc->m_pCurrentFrame = H264EncoderFrameList_InsertFrame_8u16s(
                &prev_lr->enc->m_cpb,
                &vdata,
                INTRAPIC/*ePictureType*/,
                0/*isRef*/,
                prev_lr->enc->m_info.num_slices * ((prev_lr->enc->m_info.coding_type == 1 || prev_lr->enc->m_info.coding_type == 3) + 1),
                prev_lr->enc->m_PaddedSize
                #if defined ALPHA_BLENDING_H264
                , prev_lr->enc->m_SeqParamSet->aux_format_idc
                #endif
                );
            if (!prev_lr->enc->m_pCurrentFrame)
                return MFX_ERR_ABORTED;
            if (!prev_lr->ref_layer)
                break;
        }


        while (layer->src_layer && !core_enc->m_pCurrentFrame->m_wasInterpolated){
            // go up while src_layer, then interpolate one down
            // ? no need if layer->ref_layer->src_layer != 0
            //H264EncoderFrameType *src;
            sH264EncoderFrame_8u16s *src_to = 0, *src_from = 0;
            h264Layer* lr;

            for(lr = layer, src_to = core_enc->m_pCurrentFrame;;src_to = src_from)
            {
                // find src for downsample
                lr = lr->src_layer;
                //src_from = H264EncoderFrameList_findSVCref_8u16s(
                //    &lr->enc->m_dpb,
                //    core_enc->m_pCurrentFrame);
                src_from = lr->enc->m_pLastFrame;
                if (!src_from)
                    return MFX_ERR_NOT_FOUND; // FAIL - no src for downsample - TODO: decide return code
                if (!lr->src_layer || src_from->m_wasInterpolated) {
                    // found topmost source, ready for downsample
                    break;
                }
                // find next src
            }

            // Now interpolate src_from => src_to ...
            DownscaleSurface(src_to, src_from, (Ipp32u*) lr->m_InterLayerParam.ScaledRefLayerOffsets);

        }

        core_enc->m_pCurrentFrame->m_temporalId = surface->Info.FrameId.TemporalId; // kta ??? // will be rewritten
        core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated = layer->m_frameCount * 2 * core_enc->rateDivider; // TODO: find Frame order for EncodedOrder
        // POC for fields is set assuming that encoding order and display order are equal (no reordering of fields)
        core_enc->m_pCurrentFrame->m_PicOrderCnt[0] = core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;
        core_enc->m_pCurrentFrame->m_PicOrderCnt[1] = core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated + 1;

        core_enc->m_pCurrentFrame->m_PQUANT[0] = core_enc->m_pCurrentFrame->m_PQUANT[1] = 0;

        // set per-frame const QP (if any)
        if (ctrl && core_enc->m_info.rate_controls.method == H264_RCM_QUANT)
            core_enc->m_pCurrentFrame->m_PQUANT[0] = core_enc->m_pCurrentFrame->m_PQUANT[1] = ctrl->QP;

#ifdef H264_INTRA_REFRESH
        if (core_enc->m_refrType) {
            mfxExtCodingOption2* pCO2 = 0;

            if (ctrl && ctrl->ExtParam && ctrl->NumExtParam)
                pCO2 = (mfxExtCodingOption2*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_CODING_OPTION2);

            if (pCO2)
                core_enc->m_pCurrentFrame->m_QPDelta = pCO2->IntRefQPDelta;
            else
                core_enc->m_pCurrentFrame->m_QPDelta = core_enc->m_QPDelta;
        }
#endif // H264_INTRA_REFRESH

        if (vp->mfx.EncodedOrder) { // set frame/fields types and per-frame QP in EncodedOrder

            Ipp16u firstFieldType, secondFieldType;
            if (!ctrl) // for EncodedOrder application should provide FrameType via ctrl
                return UMC_ERR_INVALID_PARAMS;
            firstFieldType = ctrl->FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B);
            secondFieldType = ctrl->FrameType & (MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xB);

            // initialize picture type for whole frame and for both fields
            core_enc->m_pCurrentFrame->m_PicCodType = core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] =
                core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = ConvertPicType_H264enc(firstFieldType);

            // only "IP" field pair is supported
            if (firstFieldType == MFX_FRAMETYPE_I && (secondFieldType == MFX_FRAMETYPE_xP || secondFieldType == 0))
                core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = PREDPIC;

#ifdef FRAME_QP_FROM_FILE
                char cft = frame_type.front();
                frame_type.pop_front();
                switch( cft ){
                case 'i':
                case 'I':
                    core_enc->m_pCurrentFrame->m_PicCodType = core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] =
                    core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = INTRAPIC;
                    core_enc->m_bMakeNextFrameIDR = true;
                    break;
                case 'p':
                case 'P':
                    core_enc->m_pCurrentFrame->m_PicCodType = core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] =
                    core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = PREDPIC;
                    break;
                case 'b':
                case 'B':
                    core_enc->m_pCurrentFrame->m_PicCodType = core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] =
                    core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = BPREDPIC;
                    break;
                default:
                    core_enc->m_pCurrentFrame->m_PicCodType = core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] =
                    core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = INTRAPIC;
                }
                //fprintf(stderr,"frame: %d %c ",core_enc->m_uFrames_Num,ft);
#endif // FRAME_QP_FROM_FILE
        }
    }

    // prepare parameters for input frame
    if (core_enc->m_pCurrentFrame){
        core_enc->m_pCurrentFrame->m_pAux[0] = ctrl;
        core_enc->m_pCurrentFrame->m_pAux[1] = surface;
//TODO check if FrameTag is set elsewhere (merging)
        if (surface->Data.FrameOrder == static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
            m_internalFrameOrders = true;
        // application doesn't provide FrameOrders, use own FrameOrders
        if (vp->mfx.EncodedOrder == 0 && surface->Data.FrameOrder == 0 && layer->m_frameCount > 0 &&
            core_enc->m_info.num_ref_frames > 1 && core_enc->m_dpb.m_pHead->m_FrameTag == 0)
            m_internalFrameOrders = true;
        if (m_internalFrameOrders == false)
            core_enc->m_pCurrentFrame->m_FrameTag = surface->Data.FrameOrder; // mark UMC frame with it's mfx FrameOrder
        else
            core_enc->m_pCurrentFrame->m_FrameTag = layer->m_frameCount;

        // !!! NEED to re-consider FrameTags for temporal since internal frame tags are used by AVC encoder
        // temporal needs unique FrameTag
        // Done, frameDisp is normal display order
//        if (core_enc->TempIdMax > 0 && core_enc->m_pLastFrame &&
        if (m_svc_count > 0 && core_enc->m_pLastFrame && // kta ???
            core_enc->m_pCurrentFrame->m_FrameDisp <= core_enc->m_pLastFrame->m_FrameDisp)
            core_enc->m_pCurrentFrame->m_FrameDisp = core_enc->m_pLastFrame->m_FrameDisp + core_enc->rateDivider;
        // compute temporalId from POC
        core_enc->m_pCurrentFrame->m_temporalId = 0;
        if (core_enc->TempIdMax > 0) {
            mfxU16 tid;
            for (tid = 0; tid<=core_enc->TempIdMax; tid++) {
                mfxU16 scale = core_enc->TemporalScaleList[core_enc->TempIdMax] / core_enc->TemporalScaleList[tid] * core_enc->rateDivider;
                if (!(core_enc->m_pCurrentFrame->m_FrameDisp % scale)) {
                    core_enc->m_pCurrentFrame->m_temporalId = tid;
                    break;
                }
            }
        }

#ifdef FRAME_QP_FROM_FILE
        int qp = frame_qp.front();
        frame_qp.pop_front();
        core_enc->m_pCurrentFrame->frame_qp = qp;
        //fprintf(stderr,"qp: %d\n",qp);
#endif
        core_enc->m_pCurrentFrame->m_coding_type = picStruct;
        mfxU16 basePicStruct = core_enc->m_pCurrentFrame->m_coding_type & 0x07;
        switch (core_enc->m_info.coding_type) {
            case 0:
                core_enc->m_pCurrentFrame->m_PictureStructureForDec = core_enc->m_pCurrentFrame->m_PictureStructureForRef = FRM_STRUCTURE;
                core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
                core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 0;
                break;
            case 1:
                core_enc->m_pCurrentFrame->m_PictureStructureForDec = core_enc->m_pCurrentFrame->m_PictureStructureForRef = FLD_STRUCTURE;
                if (basePicStruct == MFX_PICSTRUCT_FIELD_BFF) {
                    core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 1;
                    core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 0;
                } else {
                    core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
                    core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 1;
                }
                break;
            case 2: // MBAFF encoding isn't supported
                core_enc->m_pCurrentFrame->m_PictureStructureForDec = core_enc->m_pCurrentFrame->m_PictureStructureForRef = FRM_STRUCTURE; // TODO: change to AFRM_STRUCTURE when MBAFF is implemented
                core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
                core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 0;
                break;
            case 3:
                if (basePicStruct & MFX_PICSTRUCT_PROGRESSIVE) {
                    core_enc->m_pCurrentFrame->m_PictureStructureForDec = core_enc->m_pCurrentFrame->m_PictureStructureForRef = core_enc->m_pCurrentFrame->m_PlaneStructureForRef = FRM_STRUCTURE;
                    core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
                    core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 1;
                }
                else {
                    core_enc->m_pCurrentFrame->m_PictureStructureForDec = core_enc->m_pCurrentFrame->m_PictureStructureForRef = core_enc->m_pCurrentFrame->m_PlaneStructureForRef = FLD_STRUCTURE;
                    if (basePicStruct & MFX_PICSTRUCT_FIELD_BFF) {
                        core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 1;
                        core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 0;
                    }
                    else {
                        core_enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
                        core_enc->m_pCurrentFrame->m_bottom_field_flag[1] = 1;
                    }
                }
                break;
            default:
                return UMC_ERR_UNSUPPORTED;
        }
        ////            core_enc->m_PicOrderCnt++;
        if (core_enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag) {
            core_enc->m_pCurrentFrame->m_DeltaTfi = core_enc->m_SEIData.PictureTiming.DeltaTfiAccumulated;
            switch( core_enc->m_pCurrentFrame->m_coding_type ){
                case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED:
                    core_enc->m_SEIData.PictureTiming.DeltaTfiAccumulated += 1;
                    break;
                case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_REPEATED:
                    core_enc->m_SEIData.PictureTiming.DeltaTfiAccumulated += 1;
                    break;
                case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_DOUBLING:
                    core_enc->m_SEIData.PictureTiming.DeltaTfiAccumulated += 2;
                    break;
                case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_TRIPLING:
                    core_enc->m_SEIData.PictureTiming.DeltaTfiAccumulated += 4;
                    break;
                default:
                    break;
            }
        }
    }

    ///* */
    //// Scene analysis info from VPP is inside
    ///*ePictureType =*/
    //SelectFrameType_forLast( src, (mfxEncodeCtrl*)pInternalParams);
    //if (core_enc->m_pCurrentFrame)
    //    core_enc->m_pLastFrame = core_enc->m_pCurrentFrame;

    //if(core_enc->m_pCurrentFrame) {
    //    H264EncoderFrame_InitRefPicListResetCount_8u16s(core_enc->m_pCurrentFrame, 0);
    //    H264EncoderFrame_InitRefPicListResetCount_8u16s(core_enc->m_pCurrentFrame, 1);
    //    if (core_enc->m_pCurrentFrame->m_bIsIDRPic)
    //        H264EncoderFrameList_IncreaseRefPicListResetCount_8u16s(&core_enc->m_cpb, core_enc->m_pCurrentFrame);
    //}

    // pad frame to MB boundaries
    if(core_enc->m_pCurrentFrame)
    {

        Ipp32s padW = core_enc->m_PaddedSize.width - core_enc->m_pCurrentFrame->uWidth;
        if (padW > 0) {
            Ipp32s  i, j;
            PIXTYPE *py = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pCurrentFrame->uWidth;
            for (i = 0; i < (Ipp32s)core_enc->m_pCurrentFrame->uHeight; i ++) {
                for (j = 0; j < padW; j ++)
                    py[j] = py[-1];
                    // py[j] = 0;
                py += core_enc->m_pCurrentFrame->m_pitchPixels;
            }
            if (core_enc->m_info.chroma_format_idc != 0) {
                Ipp32s h = core_enc->m_pCurrentFrame->uHeight;
                if (core_enc->m_info.chroma_format_idc == 1)
                    h >>= 1;
                padW >>= 1;
                PIXTYPE *pu = core_enc->m_pCurrentFrame->m_pUPlane + (core_enc->m_pCurrentFrame->uWidth >> 1);
                PIXTYPE *pv = core_enc->m_pCurrentFrame->m_pVPlane + (core_enc->m_pCurrentFrame->uWidth >> 1);
                for (i = 0; i < h; i ++) {
                    for (j = 0; j < padW; j ++) {
                        pu[j] = pu[-1];
                        pv[j] = pv[-1];
                        // pu[j] = 0;
                        // pv[j] = 0;
                    }
                    pu += core_enc->m_pCurrentFrame->m_pitchPixels;
                    pv += core_enc->m_pCurrentFrame->m_pitchPixels;
                }
            }
        }
        Ipp32s padH = core_enc->m_PaddedSize.height - core_enc->m_pCurrentFrame->uHeight;
        if (padH > 0) {
            Ipp32s  i;
            PIXTYPE *pyD = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pCurrentFrame->uHeight * core_enc->m_pCurrentFrame->m_pitchPixels;
            PIXTYPE *pyS = pyD - core_enc->m_pCurrentFrame->m_pitchPixels;
            for (i = 0; i < padH; i ++) {
                MFX_INTERNAL_CPY(pyD, pyS, core_enc->m_PaddedSize.width * sizeof(PIXTYPE));
                //memset(pyD, 0, core_enc->m_PaddedSize.width * sizeof(PIXTYPE));
                pyD += core_enc->m_pCurrentFrame->m_pitchPixels;
            }
            if (core_enc->m_info.chroma_format_idc != 0) {
                Ipp32s h = core_enc->m_pCurrentFrame->uHeight;
                if (core_enc->m_info.chroma_format_idc == 1) {
                    h >>= 1;
                    padH >>= 1;
                }
                PIXTYPE *puD = core_enc->m_pCurrentFrame->m_pUPlane + h * core_enc->m_pCurrentFrame->m_pitchPixels;
                PIXTYPE *pvD = core_enc->m_pCurrentFrame->m_pVPlane + h * core_enc->m_pCurrentFrame->m_pitchPixels;
                PIXTYPE *puS = puD - core_enc->m_pCurrentFrame->m_pitchPixels;
                PIXTYPE *pvS = pvD - core_enc->m_pCurrentFrame->m_pitchPixels;
                for (i = 0; i < padH; i ++) {
                    MFX_INTERNAL_CPY(puD, puS, (core_enc->m_PaddedSize.width >> 1) * sizeof(PIXTYPE));
                    MFX_INTERNAL_CPY(pvD, pvS, (core_enc->m_PaddedSize.width >> 1) * sizeof(PIXTYPE));
                    //memset(puD, 0, (core_enc->m_PaddedSize.width >> 1) * sizeof(PIXTYPE));
                    //memset(pvD, 0, (core_enc->m_PaddedSize.width >> 1) * sizeof(PIXTYPE));
                    puD += core_enc->m_pCurrentFrame->m_pitchPixels;
                    pvD += core_enc->m_pCurrentFrame->m_pitchPixels;
                }
            }
        }
    }

    // switch to previous frame, which have info about its next frame
#ifdef USE_PSYINFO
    if ( (core_enc->m_Analyse_ex & ANALYSE_PSY) ) {
        if(core_enc->m_pCurrentFrame)
            H264CoreEncoder_FrameAnalysis_8u16s(core_enc);
        if ( (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ) {
            if (core_enc->m_pNextFrame)
                H264EncoderFrameList_insertAtCurrent_8u16s(&core_enc->m_cpb, core_enc->m_pNextFrame);
            if (core_enc->m_pCurrentFrame)
                H264EncoderFrameList_RemoveFrame_8u16s(&core_enc->m_cpb, core_enc->m_pCurrentFrame);
            UMC_H264_ENCODER::sH264EncoderFrame_8u16s* tmp = core_enc->m_pNextFrame;
            core_enc->m_pNextFrame = core_enc->m_pCurrentFrame;
            core_enc->m_pCurrentFrame = tmp;
            //core_enc->m_pCurrentFrame = H264CoreEncoder_PrevFrame_8u16s(core_enc, core_enc->m_pCurrentFrame);
            //frameNum --;
        }
    }
#endif // USE_PSYINFO

//    if (core_enc->m_pCurrentFrame && (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD)) {
//        Ipp32s uNumMBs = core_enc->m_HeightInMBs * core_enc->m_WidthInMBs;
//        uNumMBs = uNumMBs/256 + 1;
//        printf("\nfr:%3d  %d/%d \t%d/%d \t%d/%d \t%d/%d\t", core_enc->m_pCurrentFrame->m_FrameDisp,
//            core_enc->m_pCurrentFrame->psyStatic.dc/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].dc/uNumMBs,
//            core_enc->m_pCurrentFrame->psyStatic.bgshift/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].bgshift/uNumMBs,
//            core_enc->m_pCurrentFrame->psyStatic.details/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].details/uNumMBs,
//            core_enc->m_pCurrentFrame->psyStatic.interl/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].interl/uNumMBs);
//
//        static int head = 0;
//        FILE* csv = fopen("psy.csv", head?"at":"wt");
//        if (!head) {
//            fprintf (csv, "num, dc, sh, det, int, Ddc, Dsh, Ddet, Dint\n");
//            head=1;
//        }
//        fprintf (csv, "%d, %d, %d, %d, %d, %d, %d, %d, %d\n", core_enc->m_pCurrentFrame->m_FrameDisp,
//            core_enc->m_pCurrentFrame->psyStatic.dc/uNumMBs,
//            core_enc->m_pCurrentFrame->psyStatic.bgshift/uNumMBs,
//            core_enc->m_pCurrentFrame->psyStatic.details/uNumMBs,
//            core_enc->m_pCurrentFrame->psyStatic.interl/uNumMBs,
//            core_enc->m_pCurrentFrame->psyDyna[0].dc/uNumMBs,
//            core_enc->m_pCurrentFrame->psyDyna[0].bgshift/uNumMBs,
//            core_enc->m_pCurrentFrame->psyDyna[0].details/uNumMBs,
//            core_enc->m_pCurrentFrame->psyDyna[0].interl/uNumMBs
//        );
//        fclose(csv);
//    }

    mfxI32 noL1 = 0;
    if(core_enc->m_pCurrentFrame) {
        bool notLastFrame = (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ? (core_enc->m_pNextFrame != 0) : true;
        ctrl = (mfxEncodeCtrl*)core_enc->m_pCurrentFrame->m_pAux[0];
        // Scene analysis info from VPP is inside
        noL1 = SelectFrameType_forLast( notLastFrame, ctrl);

        if (core_enc->m_pCurrentFrame)
            core_enc->m_pLastFrame = core_enc->m_pCurrentFrame;

        H264EncoderFrame_InitRefPicListResetCount_8u16s(core_enc->m_pCurrentFrame, 0);
        H264EncoderFrame_InitRefPicListResetCount_8u16s(core_enc->m_pCurrentFrame, 1);
        if (core_enc->m_pCurrentFrame->m_bIsIDRPic)
            H264EncoderFrameList_IncreaseRefPicListResetCount_8u16s(&core_enc->m_cpb, core_enc->m_pCurrentFrame);
        core_enc->m_pCurrentFrame->m_FrameCount = core_enc->m_pCurrentFrame->m_FrameTag;
    } else if (!(core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD))
        noL1 = SelectFrameType_forLast( false, 0); // to terminate GOP

    //////////////////////////////////////////////////////////////////////////
    // After next call core_enc->m_pCurrentFrame points
    // not to the input frame, but
    // to the frame to be encoded
    //////////////////////////////////////////////////////////////////////////

    if (vp->mfx.EncodedOrder == 0) {
        Ipp32u noahead = (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ? 0 : 1;
        h264Layer *toplayer = m_svc_layers[m_maxDid] ? m_svc_layers[m_maxDid] : layer;
        if (layer->m_frameCount >= noahead && toplayer->m_frameCountBuffered <
            (mfxU32)toplayer->enc->m_info.B_frame_rate + 1 - noahead && core_enc->m_pCurrentFrame) {
            // these B_frame_rate frames go to delay (B-reorder) buffer
            layer->m_frameCountBuffered++;
            core_enc->m_pCurrentFrame = 0;
            return UMC_OK; // return OK from async part in case of buffering
        } else {
            if (core_enc->m_pCurrentFrame || layer->m_frameCountBuffered > 0) {
#ifdef USE_TEMPTEMP
                if (m_usetemptemp) {
                    // find minimal codorder
                    H264EncoderFrame_8u16s *fr;
                    mfxI32 minorder = 0x7fffffff;
                    core_enc->m_pCurrentFrame = 0;
                    for (fr = core_enc->m_cpb.m_pHead; fr; fr = fr->m_pFutureFrame) {
                        if (fr->m_wasEncoded) continue;
                        mfxI32 numtemp = fr->m_FrameDisp*core_enc->rateDivider - m_temptempstart;
                        mfxI32 added = 0;
                        if (numtemp && (numtemp>=m_temptemplen || !m_refctrl[numtemp].codorder)) {
                            numtemp -= m_temptempwidth;
                            added = m_temptempwidth;
                        }
                        if (m_refctrl[numtemp].codorder + added < minorder) {
                            core_enc->m_pCurrentFrame = fr;
                            minorder = m_refctrl[numtemp].codorder + added;
                        }
                    }
                } else
#endif // USE_TEMPTEMP
                if (m_selectedPOC == POC_UNSPECIFIED) {
                    core_enc->m_pCurrentFrame = H264EncoderFrameList_findOldestToEncode_8u16s(
                        &core_enc->m_cpb,
                        &core_enc->m_dpb,
                        noL1 ? 0 : core_enc->m_l1_cnt_to_start_B,
                        core_enc->m_info.treat_B_as_reference);
                    if (core_enc->m_pCurrentFrame)
                        m_selectedPOC = core_enc->m_pCurrentFrame->m_PicOrderCnt[0]; // upper dep layers have to code this POC
                } else { // POC is known - find the frame
                    sH264EncoderFrame_8u16s *pCurr;
                    for (pCurr = core_enc->m_cpb.m_pHead; pCurr; pCurr = pCurr->m_pFutureFrame)
                        if (pCurr->m_PicOrderCnt[0] == m_selectedPOC && !pCurr->m_wasEncoded) {
                            //assert(!pCurr->m_wasEncoded);
                            break;
                        }
                    core_enc->m_pCurrentFrame = pCurr;
                }
            }
            if (core_enc->m_pCurrentFrame) {
                // if frame is taken from buffer
                if (!src && layer->m_frameCountBuffered > 0)
                    layer->m_frameCountBuffered --;
                // set pic types for fields
                core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] = core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = core_enc->m_pCurrentFrame->m_PicCodType;
                if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC && core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE) // encode as "IP" field pair
                    core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = PREDPIC;
            }
        }
    }

//if (core_enc->m_pCurrentFrame)
//    printf("\t%4d  in:%3d T:%2d A:%3d  out:%3d T:%2d A:%3d\n", core_enc->m_pCurrentFrame->m_FrameDisp,
//        core_enc->m_pLastFrame ? core_enc->m_pLastFrame->m_PicOrderCnt[0]/2 : 999,
//        core_enc->m_pLastFrame ? core_enc->m_pLastFrame->m_PicCodType : 99,
//        core_enc->m_pLastFrame ? core_enc->m_pLastFrame->m_PicOrderCounterAccumulated/2 : 999,
//        core_enc->m_pCurrentFrame ? core_enc->m_pCurrentFrame->m_PicOrderCnt[0]/2 : 999,
//        core_enc->m_pCurrentFrame ? core_enc->m_pCurrentFrame->m_PicCodType : 99,
//        core_enc->m_pCurrentFrame ? core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated/2 : 999);

    if (core_enc->m_pCurrentFrame){

        // set const QP for frame or both fields
        if (core_enc->m_info.rate_controls.method == H264_RCM_QUANT) {
            ps = SetConstQP(vp->mfx.EncodedOrder, core_enc->m_pCurrentFrame, &core_enc->m_info);
            if (ps != UMC_OK)
                return ps;
        }

        core_enc->m_pCurrentFrame->m_noL1RefForB = false;

        if (core_enc->m_pCurrentFrame->m_bIsIDRPic)
            core_enc->m_uFrameCounter = 0;
        core_enc->m_pCurrentFrame->m_FrameNum = core_enc->m_uFrameCounter;

        if( core_enc->m_pCurrentFrame->m_RefPic )
            core_enc->m_uFrameCounter++;

        if (core_enc->m_pCurrentFrame->m_bIsIDRPic) {
            core_enc->m_PicOrderCnt_LastIDR = core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;//core_enc->m_uFrames_Num;
            //core_enc->m_PicOrderCnt = 0;//frameNum - core_enc->m_uFrames_Num;
            core_enc->m_bMakeNextFrameIDR = false;
        }

        H264CoreEncoder_MoveFromCPBToDPB_8u16s(state);

    } else {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    EnumPicCodType ePictureType = core_enc->m_pCurrentFrame->m_PicCodType;
    EnumPicClass    ePic_Class;
    switch (ePictureType) {
        case INTRAPIC:
            if (core_enc->m_pCurrentFrame->m_bIsIDRPic || core_enc->m_uFrames_Num == 0) {
                // Right now, only the first INTRAPIC in a sequence is an IDR Frame
                ePic_Class = IDR_PIC;
                core_enc->m_l1_cnt_to_start_B = core_enc->m_info.num_ref_to_start_code_B_slice;
            } else
                ePic_Class = core_enc->m_pCurrentFrame->m_RefPic ? REFERENCE_PIC : DISPOSABLE_PIC;
            layer->m_lastIframeEncCount = core_enc->m_info.numEncodedFrames;
#ifdef H264_INTRA_REFRESH
            if (core_enc->m_refrType) {
                m_base.enc->m_stopFramesCount = 0;
                core_enc->m_firstLineInStripe = -core_enc->m_refrSpeed;
            }
#endif // H264_INTRA_REFRESH
            break;

        case PREDPIC:
            ePic_Class = core_enc->m_pCurrentFrame->m_RefPic ? REFERENCE_PIC : DISPOSABLE_PIC;
            break;

        case BPREDPIC:
            ePic_Class = core_enc->m_info.treat_B_as_reference && core_enc->m_pCurrentFrame->m_RefPic ? REFERENCE_PIC : DISPOSABLE_PIC;
            break;

        default:
            VM_ASSERT(false);
            ePic_Class = IDR_PIC;
            break;
    }

    core_enc->m_pCurrentFrame->m_EncFrameCount = core_enc->m_info.numEncodedFrames;
    core_enc->m_pCurrentFrame->m_LastIframeEncCount = layer->m_lastIframeEncCount;
#ifdef H264_INTRA_REFRESH
    if (core_enc->m_refrType) {
        core_enc->m_pCurrentFrame->m_periodCount = core_enc->m_periodCount;
        core_enc->m_pCurrentFrame->m_firstLineInStripe = core_enc->m_firstLineInStripe;
        core_enc->m_pCurrentFrame->m_isRefreshed = false;
    }
#endif // H264_INTRA_REFRESH

    // ePictureType is a reference variable.  It is updated by CompressFrame if the frame is internally forced to a key frame.
    //ps = H264CoreEncoder_CompressFrame(state, ePictureType, ePic_Class, dst, dst_ext, (mfxEncodeCtrl*)core_enc->m_pCurrentFrame->m_pAux[0]);
    mfxI32 i;

    core_enc->m_svc_layer.svc_ext.priority_id = 0;
    core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag = 1;
//    core_enc->m_svc_layer.svc_ext.temporal_id = core_enc->m_pCurrentFrame->m_temporalId;
    core_enc->m_svc_layer.svc_ext.use_ref_base_pic_flag = 0;
    core_enc->m_svc_layer.svc_ext.discardable_flag = 0;
    core_enc->m_svc_layer.svc_ext.output_flag = 1;
    core_enc->m_svc_layer.svc_ext.reserved_three_2bits = 3; /* is needed for ref bit extractor */

    core_enc->m_svc_layer.svc_ext.store_ref_base_pic_flag = 0;
    core_enc->m_svc_layer.svc_ext.adaptive_ref_base_pic_marking_mode_flag = 0;

    //core_enc->m_scan_idx_start = 0;
    //core_enc->m_scan_idx_end = 15;

    ////while (layer->src_layer && !core_enc->m_pCurrentFrame->m_wasInterpolated){
    ////    // go up while src_layer, then interpolate one down
    ////    // ? no need if layer->ref_layer->src_layer != 0
    ////    //H264EncoderFrameType *src;
    ////    sH264EncoderFrame_8u16s *src_to = 0, *src_from = 0;
    ////    h264Layer* lr;

    ////    for(lr = layer, src_to = core_enc->m_pCurrentFrame;;src_to = src_from)
    ////    {
    ////        // find src for downsample
    ////        lr = lr->src_layer;
    ////        src_from = H264EncoderFrameList_findSVCref_8u16s(
    ////            &lr->enc->m_dpb,
    ////            core_enc->m_pCurrentFrame);
    ////        if (!src_from)
    ////            return MFX_ERR_NOT_FOUND; // FAIL - no src for downsample - TODO: decide return code
    ////        if (!lr->src_layer || src_from->m_wasInterpolated) {
    ////            // found topmost source, ready for downsample
    ////            break;
    ////        }
    ////        // find next src
    ////    }

    ////    // Now interpolate src_from => src_to ...
    ////    DownscaleSurface(src_to, src_from, (Ipp32u*) lr->m_InterLayerParam.scaled_ref_layer_offsets);

    ////}

    if(layer->ref_layer && core_enc->m_pCurrentFrame->m_temporalId <= layer->ref_layer->enc->TempIdMax) {
        // interpolate from reference layer
        // 1.find reference frame in ref_layer
        sH264EncoderFrame_8u16s* ref;
        ref = layer->ref_layer->enc->m_pCurrentFrame; // last encoded on prev layer
        if (ref) {
            core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag = 0;

            core_enc->m_Slices[0].m_disable_inter_layer_deblocking_filter_idc =
                (m_layer->m_InterLayerParam.DisableDeblockingFilter == MFX_CODINGOPTION_ON) ?
                DEBLOCK_FILTER_OFF : DEBLOCK_FILTER_ON;

            LoadMBInfoFromRefLayer(core_enc, layer->ref_layer->enc);
        }
    }


    for (i = 0; i < core_enc->QualityNum; i++) {
        core_enc->m_svc_layer.svc_ext.quality_id = i;
        if (i > 0)
            core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag = 0;

        core_enc->brc = layer->m_brc[i];

        if (core_enc->m_info.rate_controls.method == H264_RCM_QUANT) {
            mfxU16 qP = core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC ? layer->m_cqp[i][core_enc->m_pCurrentFrame->m_temporalId].QPI :
                (core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC ? layer->m_cqp[i][core_enc->m_pCurrentFrame->m_temporalId].QPP : layer->m_cqp[i][core_enc->m_pCurrentFrame->m_temporalId].QPB);
            if (qP > 0)
                core_enc->m_pCurrentFrame->m_PQUANT[0] = core_enc->m_pCurrentFrame->m_PQUANT[1] = qP;
        }

        ps = H264CoreEncoder_CompressFrame(state, ePictureType, ePic_Class, dst, dst_ext, (mfxEncodeCtrl*)core_enc->m_pCurrentFrame->m_pAux[0]);
        if (ps != UMC_OK)
            break;

        if (core_enc->m_svc_layer.svc_ext.dependency_id == 0 && i == 0 && core_enc->QualityNum > 1) {
            Ipp32s nMBCount =  core_enc->m_pCurrentFrame->totalMBs;
            MFX_INTERNAL_CPY(core_enc->m_mbinfo_saved.MV[0], core_enc->m_pCurrentFrame->m_mbinfo.MV[0], nMBCount*sizeof(H264MacroblockMVs));
            MFX_INTERNAL_CPY(core_enc->m_mbinfo_saved.MV[1], core_enc->m_pCurrentFrame->m_mbinfo.MV[1], nMBCount*sizeof(H264MacroblockMVs));
            MFX_INTERNAL_CPY(core_enc->m_mbinfo_saved.RefIdxs[0], core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0], nMBCount*sizeof(H264MacroblockRefIdxs));
            MFX_INTERNAL_CPY(core_enc->m_mbinfo_saved.RefIdxs[1], core_enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1], nMBCount*sizeof(H264MacroblockRefIdxs));
            MFX_INTERNAL_CPY(core_enc->m_mbinfo_saved.mbs, core_enc->m_pCurrentFrame->m_mbinfo.mbs, nMBCount*sizeof(H264MacroblockGlobalInfo));
        }
        if (core_enc->m_svc_layer.svc_ext.dependency_id == m_svc_count - 1 && i == core_enc->QualityNum - 1 && m_svc_layers[0]->enc->QualityNum > 1) {
            Ipp32s nMBCount =  m_svc_layers[0]->enc->m_pCurrentFrame->totalMBs;
            MFX_INTERNAL_CPY(m_svc_layers[0]->enc->m_pCurrentFrame->m_mbinfo.MV[0], m_svc_layers[0]->enc->m_mbinfo_saved.MV[0], nMBCount*sizeof(H264MacroblockMVs));
            MFX_INTERNAL_CPY(m_svc_layers[0]->enc->m_pCurrentFrame->m_mbinfo.MV[1],m_svc_layers[0]->enc->m_mbinfo_saved.MV[1], nMBCount*sizeof(H264MacroblockMVs));
            MFX_INTERNAL_CPY(m_svc_layers[0]->enc->m_pCurrentFrame->m_mbinfo.RefIdxs[0], m_svc_layers[0]->enc->m_mbinfo_saved.RefIdxs[0], nMBCount*sizeof(H264MacroblockRefIdxs));
            MFX_INTERNAL_CPY(m_svc_layers[0]->enc->m_pCurrentFrame->m_mbinfo.RefIdxs[1], m_svc_layers[0]->enc->m_mbinfo_saved.RefIdxs[1], nMBCount*sizeof(H264MacroblockRefIdxs));
            MFX_INTERNAL_CPY(m_svc_layers[0]->enc->m_pCurrentFrame->m_mbinfo.mbs, m_svc_layers[0]->enc->m_mbinfo_saved.mbs, nMBCount*sizeof(H264MacroblockGlobalInfo));
        }
    }

    dst->SetTime( core_enc->m_pCurrentFrame->pts_start, core_enc->m_pCurrentFrame->pts_end);

    if (ps == UMC_OK)
        core_enc->m_pCurrentFrame->m_wasEncoded = true;
    H264CoreEncoder_CleanDPB_8u16s(state);
    if (ps == UMC_OK) {
        core_enc->m_info.numEncodedFrames++;
        if (core_enc->m_svc_layer.svc_ext.dependency_id == m_maxDid)
            m_selectedPOC = POC_UNSPECIFIED;
    }

    ////if (core_enc->m_svc_layer.svc_ext.dependency_id == m_svc_count - 1 && 
    ////    core_enc->m_svc_layer.svc_ext.quality_id == core_enc->QualityNum - 1)
    ////    rd_frame_num ++;

    return ps;
}


mfxStatus MFXVideoENCODEH264::WriteEndOfSequence(mfxBitstream *bs)
{
   H264BsReal_8u16s rbs;
   rbs.m_pbsRBSPBase = rbs.m_base.m_pbs = 0;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H264BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_EOSEQ, s, 0);
   return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEH264::WriteEndOfStream(mfxBitstream *bs)
{
   H264BsReal_8u16s rbs;
   rbs.m_pbsRBSPBase = rbs.m_base.m_pbs = 0;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H264BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_EOSTREAM, s, 0);
   return MFX_ERR_NONE;
}

// reorder list for temporal scalability, so that higher temporal_id are excluded
// TODO: Need to fill more fields
Status MFXVideoENCODEH264::ReorderListSVC( H264CoreEncoder_8u16s* core_enc, EnumPicClass& ePic_Class)
{
    Ipp8u posin, posout, i;
    H264EncoderFrame_8u16s *pFrm;
    Ipp16u maxNumActiveRefsByStandard = core_enc->m_SliceHeader.field_pic_flag ? 32 : 16;
    Ipp16u maxNumActiveRefsInL0 = maxNumActiveRefsByStandard;
    Ipp16u maxNumActiveRefsInL1 = maxNumActiveRefsByStandard;
    Ipp8u fieldIdx = core_enc->m_field_index;
    // get default ref pic lists for current picture
    RefPicListInfo &refPicListInfoCommon = core_enc->m_pCurrentFrame->m_RefPicListInfo[fieldIdx];
    EncoderRefPicListStruct_8u16s *pRefListL0 = &core_enc->m_pCurrentFrame->m_refPicListCommon[fieldIdx].m_RefPicListL0;
    EncoderRefPicListStruct_8u16s *pRefListL1 = &core_enc->m_pCurrentFrame->m_refPicListCommon[fieldIdx].m_RefPicListL1;
    Ipp8u NumRefsInL0List = refPicListInfoCommon.NumRefsInL0List; // number of ref frames in ref pic list L0
    Ipp8u NumRefsInL1List = refPicListInfoCommon.NumRefsInL1List; // number of ref frames in ref pic list L1
    bool bUseIntraMBsOnly = false;
    bool bIsBFrame = core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC;

    if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC) {
        NumRefsInL0List = NumRefsInL1List = 0; // are invalid for intra frames
        bUseIntraMBsOnly = true;
    }

    // H264CoreEncoder_ModifyRefPicList functionality
    // prepare place for reordered ref pic lists L0, L1
    EncoderRefPicListStruct_8u16s reorderedRefListL0, reorderedRefListL1;
    memset(&reorderedRefListL0, 0, sizeof(EncoderRefPicListStruct_8u16s));
    memset(&reorderedRefListL1, 0, sizeof(EncoderRefPicListStruct_8u16s));

    // form ref pic list L0
    for (posin = 0, posout = 0; posin < NumRefsInL0List; posin++) {
        pFrm = pRefListL0->m_RefPicList[posin];
        if (!pFrm->m_wasEncoded || !pFrm->m_RefPic)
            continue; // error if here
        if ( pFrm->m_temporalId > core_enc->m_pCurrentFrame->m_temporalId)
            continue;
        reorderedRefListL0.m_RefPicList[posout++] = pFrm;
    }

    if (posout < NumRefsInL0List) {
        // improper refs are already removed
        maxNumActiveRefsInL0 = posout;
        if (maxNumActiveRefsInL0 == 0 && bIsBFrame == false) // all refs in L0 are rejected
            bUseIntraMBsOnly = true; // use only Intra MBs to encode the picture
    }

    // form ref pic list L1
    if (bIsBFrame) {
        for (posin = 0, posout = 0; posin < NumRefsInL1List; posin++) {
            pFrm = pRefListL1->m_RefPicList[posin];
            if (!pFrm->m_wasEncoded || !pFrm->m_RefPic)
                continue; // error if here
            if ( pFrm->m_temporalId > core_enc->m_pCurrentFrame->m_temporalId)
                continue;
            reorderedRefListL1.m_RefPicList[posout++] = pFrm;
        }

        if (posout < NumRefsInL1List) {
            // improper refs are already removed
            maxNumActiveRefsInL1 = posout;
            if (maxNumActiveRefsInL1 == 0 && maxNumActiveRefsInL0 == 0) // all refs in L0 and L1 are rejected
                bUseIntraMBsOnly = true; // use only Intra MBs to encode the picture
        }
    }

    if (core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC && core_enc->m_info.treat_B_as_reference/*core_enc->m_pCurrentFrame->m_RefPic*/) {
        if (reorderedRefListL0.m_RefPicList[0])
            reorderedRefListL0.m_RefPicList[0]->m_BRefCount--;
        if (reorderedRefListL1.m_RefPicList[0])
            reorderedRefListL1.m_RefPicList[0]->m_BRefCount--;
    }

    // prepare ref pic list modification syntax based on initial and reordered ref lists
    if (reorderedRefListL0.m_RefPicList[0]) { // check if reordered list L0 is constructed
        // build ref pic list modification syntax for list L0
        H264CoreEncoder_BuildRefPicListModSyntax(core_enc,
            pRefListL0,
            &reorderedRefListL0,
            &core_enc->m_ReorderInfoL0[fieldIdx],
            maxNumActiveRefsInL0);
        core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 1;

        // clear initial ref list L0
        memset(pRefListL0, 0, sizeof(EncoderRefPicListStruct_8u16s));
        // copy reordered ref list L0
        for (i = 0; i < NumRefsInL0List; i ++) {
            pRefListL0->m_RefPicList[i] = reorderedRefListL0.m_RefPicList[i];
            pRefListL0->m_Prediction[i] = reorderedRefListL0.m_Prediction[i];
        }
    }

    if (bIsBFrame && reorderedRefListL1.m_RefPicList[0]) {  // check if reordered list L1 is constructed
        // build ref pic list modification syntax for list L1
        H264CoreEncoder_BuildRefPicListModSyntax(core_enc,
            pRefListL1,
            &reorderedRefListL1,
            &core_enc->m_ReorderInfoL1[fieldIdx],
            maxNumActiveRefsInL1);
        core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l1 = 1;

        // clear initial ref list L1
        memset(pRefListL1, 0, sizeof(EncoderRefPicListStruct_8u16s));
        // copy reordered ref list L1
        for (i = 0; i < NumRefsInL1List; i ++) {
            pRefListL1->m_RefPicList[i] = reorderedRefListL1.m_RefPicList[i];
            pRefListL1->m_Prediction[i] = reorderedRefListL1.m_Prediction[i];
        }
    }

    core_enc->m_pCurrentFrame->m_MaxNumActiveRefsInL0[fieldIdx] = maxNumActiveRefsInL0;
    core_enc->m_pCurrentFrame->m_MaxNumActiveRefsInL1[fieldIdx] = maxNumActiveRefsInL1;
    core_enc->m_pCurrentFrame->m_use_intra_mbs_only[fieldIdx] =   bUseIntraMBsOnly;


    // Now build ref pic marking

    mfxExtAVCRefListCtrl framelist = {{MFX_EXTBUFF_AVC_REFLIST_CTRL, sizeof(mfxExtAVCRefListCtrl)}, };
    mfxExtBuffer* bufptr = &framelist.Header;
    mfxEncodeCtrl ctrl = {0,}, *pCtrl = 0;
    mfxU32 idx = 0, addedLT = 0;
    H264EncoderFrame_8u16s *pFrmLatest = 0;

    ctrl.NumExtParam = 1;
    ctrl.ExtParam = &bufptr;

    // latest ref frame to prevent fwd ref for B from removing
    if (core_enc->m_info.B_frame_rate > 0)
    for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
        if (pFrm->m_wasEncoded && pFrm->m_RefPic && core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC &&
            (!pFrmLatest || pFrmLatest->m_FrameDisp < pFrm->m_FrameDisp))
            pFrmLatest = pFrm;
    }

    if (core_enc->m_pCurrentFrame->m_RefPic && core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC /*&&
        core_enc->m_pCurrentFrame->m_temporalId < core_enc->TempIdMax*/) { // probably for all
        mfxI32 tid = core_enc->m_pCurrentFrame->m_temporalId;
        mfxI32 refdist = core_enc->m_info.B_frame_rate+1;
        mfxI32 req = core_enc->TemporalScaleList[core_enc->TempIdMax] / core_enc->TemporalScaleList[tid] / refdist;
        { // common multiple
            mfxI32 i1 = core_enc->TemporalScaleList[core_enc->TempIdMax] / core_enc->TemporalScaleList[tid];
            mfxI32 i2 = refdist;
            if (i1 > 1 && req * i2 != i1) {
                mfxI32 sarr[] = {2,3,5,7,11,13,17,19};
                mfxI32 dv;
                req = i1*i2;
                for (size_t isarr = 0; isarr < sizeof(sarr)/sizeof(sarr[0]); isarr++ ) {
                    dv = sarr[isarr];
                    if (dv * dv > req) break;
                    while ((i1/dv)*dv == i1 && (i2/dv)*dv == i2) {
                        req /= dv;
                        i1  /= dv;
                        i2  /= dv;
                    }
                }
            }
        }
        req += tid;
        if (core_enc->m_info.treat_B_as_reference) {
            for(mfxI32 d=2; d<refdist && d>0; d<<=1) req++;
        }
        // Check if should be LT
        if (core_enc->m_info.key_frame_controls.interval != 1)
        if (core_enc->m_info.num_ref_frames > tid+1) // reject LT, keep single ST
        if (req > core_enc->m_info.num_ref_frames) { // to mark LT
            framelist.LongTermRefList[0].FrameOrder = core_enc->m_pCurrentFrame->m_FrameTag;
            framelist.LongTermRefList[0].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            addedLT = 1;
            if (!core_enc->m_pCurrentFrame->m_bIsIDRPic)
            for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                if (pFrm->m_wasEncoded && (pFrm->m_isLongTermRef[0] || pFrm->m_isLongTermRef[1]) && pFrm != pFrmLatest ) {
                    if ( pFrm->m_temporalId == tid) {
                        // mark this LT as no more needed, 
                        assert (idx<16);
                        framelist.RejectedRefList[idx].FrameOrder = pFrm->m_FrameTag;
                        framelist.RejectedRefList[idx].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                        idx++;
                    }
                }
            }
        }
        // now find latest LT at lower layers, then remove LT from this layer if it is older
        if (tid > 0 && !addedLT) {
            H264EncoderFrame_8u16s *pLTlower = 0, *pLTthis = 0;
            for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                if (pFrm->m_wasEncoded && (pFrm->m_isLongTermRef[0] || pFrm->m_isLongTermRef[1]) && pFrm != pFrmLatest ) {
                    if ( pFrm->m_temporalId == tid)
                        pLTthis = pFrm;
                    else if ( pFrm->m_temporalId < tid && (!pLTlower || pLTlower->m_FrameDisp < pFrm->m_FrameDisp))
                        pLTlower = pFrm;
                }
            }
            if (pLTlower && pLTthis && pLTthis->m_FrameDisp < pLTlower->m_FrameDisp) {
                // mark this LT as no more needed, 
                assert (idx<16);
                framelist.RejectedRefList[idx].FrameOrder = pLTthis->m_FrameTag;
                framelist.RejectedRefList[idx].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                idx++;
            }
        }
    }

    if (idx != 0 || addedLT ) {
        for (;           idx<16;idx++) framelist.RejectedRefList [idx].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        for (idx=0;      idx<32;idx++) framelist.PreferredRefList[idx].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        for (idx=addedLT;idx<16;idx++) framelist.LongTermRefList [idx].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        pCtrl = &ctrl;
    }

    // integrated H264CoreEncoder_ModifyRefPicList( core_enc, pCtrl);
    return H264CoreEncoder_PrepareRefPicMarking(core_enc, pCtrl, ePic_Class, 0);
}

// reorder ref pic list in compliance with mfxExtAVCRefListCtrl structure
// preferred refs are placed in the beginning of the list in order present in PreferredList
// rejected refs are reordered to the end of the list
Status H264CoreEncoder_ReorderRefPicList(
    EncoderRefPicListStruct_8u16s       *pRefListIn,
    EncoderRefPicListStruct_8u16s       *pRefListOut,
    mfxExtAVCRefListCtrl                *pRefPicListCtrl,
    Ipp16u                              numRefInList,
    Ipp16u                              &numRefAllowedInList,
    bool                                isFieldRefPicList)
{
    Ipp32s i, j;
    Ipp32s lastRejected = numRefInList - 1, lastPreferred = 0;

    H264EncoderFrame_8u16s **pRefPicListIn = pRefListIn->m_RefPicList;
    H264EncoderFrame_8u16s **pRefPicListOut = pRefListOut->m_RefPicList;

    Ipp8s *pFieldsIn = pRefListIn->m_Prediction;
    Ipp8s *pFieldsOut = pRefListOut->m_Prediction;

    if (!isFieldRefPicList) { // progressive ref pic list

        memset(pRefListOut, 0, sizeof(EncoderRefPicListStruct_8u16s));
        for (i = 0; i < 16; i ++) { // go through PreferredRefList and RejectedList
            if (pRefPicListCtrl->PreferredRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) ||
                pRefPicListCtrl->RejectedRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN)) {
                // check PicStruct
                if (pRefPicListCtrl->PreferredRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) &&
                    pRefPicListCtrl->PreferredRefList[i].PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
                    pRefPicListCtrl->PreferredRefList[i].PicStruct != MFX_PICSTRUCT_UNKNOWN) {
                    return UMC_ERR_FAILED;
                }
                if (pRefPicListCtrl->RejectedRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) &&
                    pRefPicListCtrl->RejectedRefList[i].PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
                    pRefPicListCtrl->RejectedRefList[i].PicStruct != MFX_PICSTRUCT_UNKNOWN) {
                    return UMC_ERR_FAILED;
                }
                // reorder preferred refs to beginning of the list, rejected to the end of the list
                for (j = 0; j < numRefInList; j ++) {
                    if (pRefPicListIn[j]->m_FrameTag == pRefPicListCtrl->PreferredRefList[i].FrameOrder &&
                        IsRejected(pRefPicListIn[j]->m_FrameTag, pRefPicListCtrl) == false) {
                            pRefPicListOut[lastPreferred ++] = pRefPicListIn[j];
                    } else if (pRefPicListIn[j]->m_FrameTag == pRefPicListCtrl->RejectedRefList[i].FrameOrder)
                        pRefPicListOut[lastRejected --] = pRefPicListIn[j];
                }
            }
        }

        if (lastRejected == numRefInList - 1 && lastPreferred == 0) // no reordering performed
            return UMC_OK;

        // place the rest of frames between preferred and rejected
        for (j = 0; j < numRefInList; j ++) {
            if (IsRejected(pRefPicListIn[j]->m_FrameTag, pRefPicListCtrl) == false &&
                IsPreferred(pRefPicListIn[j]->m_FrameTag, pRefPicListCtrl) == false)
                pRefPicListOut[lastPreferred ++] = pRefPicListIn[j];
        }

        numRefAllowedInList = lastRejected + 1; // calculate and return number of not rejected refs
    } else { // interlaced ref pic list
        // only limited support for fields: only RejectedRefList, FrameOrder = (MFX_PICSTRUCT_FIELD_TFF & MFX_PICSTRUCT_FIELD_BFF)

        memset(pRefListOut, 0, sizeof(EncoderRefPicListStruct_8u16s));
        memset(pFieldsOut, 0, MAX_NUM_REF_FRAMES + 1);

        for (i = 0; i < 16; i ++) { // go through RejectedList
            if ( pRefPicListCtrl->RejectedRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN)) {
                if (pRefPicListCtrl->RejectedRefList[i].PicStruct != (MFX_PICSTRUCT_FIELD_TFF & MFX_PICSTRUCT_FIELD_BFF) &&
                    pRefPicListCtrl->RejectedRefList[i].PicStruct != MFX_PICSTRUCT_UNKNOWN)
                    return UMC_ERR_FAILED;
                // reorder rejected refs to the end of the list
                for (j = 0; j < numRefInList; j ++) {
                    if (pRefPicListIn[j]->m_FrameTag == pRefPicListCtrl->RejectedRefList[i].FrameOrder) {
                        pRefPicListOut[lastRejected] = pRefPicListIn[j];
                        pFieldsOut[lastRejected --] = pFieldsIn[j];
                    }
                }
            }
        }

        if (lastRejected == numRefInList - 1) // no reordering performed
            return UMC_OK;

        // place the rest of frames before rejected
        for (j = 0; j < numRefInList; j ++) {
            if (IsRejected(pRefPicListIn[j]->m_FrameTag, pRefPicListCtrl) == false) {
                pRefPicListOut[lastPreferred] = pRefPicListIn[j];
                pFieldsOut[lastPreferred ++] = pFieldsIn[j];
            }
        }

        numRefAllowedInList = lastRejected + 1; // calculate and return number of not rejected refs
    }

    // check that initial and reordered lists differ in active range
    bool bIsDiffer = false;
    for (i = 0; i < numRefAllowedInList; i ++)
        if (pRefPicListOut[i] != pRefPicListIn[i] || pFieldsOut[i] != pFieldsIn[i])
            bIsDiffer = true;

    // if no difference or no allowed refs in list then clear reordered list
    if (bIsDiffer == false) {
        memset(pRefListOut, 0, sizeof(EncoderRefPicListStruct_8u16s));
        memset(pFieldsOut, 0, MAX_NUM_REF_FRAMES + 1);
    }

    return UMC_OK;
}

// prepare ref pic list modification syntax based on default and reordered ref lists
Status H264CoreEncoder_BuildRefPicListModSyntax(
    H264CoreEncoder_8u16s*              core_enc,
    EncoderRefPicListStruct_8u16s       *pRefListDefault,
    EncoderRefPicListStruct_8u16s       *pRefListReordered,
    RefPicListReorderInfo               *pReorderInfo,
    Ipp16u                              numRefActiveInList)
{
    H264ENC_UNREFERENCED_PARAMETER(pRefListDefault);

    pReorderInfo->num_entries = 0;

    Ipp32s currPicNum = core_enc->m_pCurrentFrame->m_PicNum[core_enc->m_field_index], i;
    Ipp32s picNumL0PredWrap = currPicNum; // wrapped prediction for reordered picNum

    // TODO: add support for fields

    H264EncoderFrame_8u16s **pRefPicListReordered = pRefListReordered->m_RefPicList;

    if (core_enc->m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE) {
        for (i = 0; i < numRefActiveInList; i ++) {
            if (!pRefPicListReordered[i]) continue;
            if (H264EncoderFrame_isShortTermRef1_8u16s(pRefPicListReordered[i], 0)) {
                if (pRefPicListReordered[i]->m_PicNum[0] <= picNumL0PredWrap) {
                    pReorderInfo->reordering_of_pic_nums_idc[i] = 0; // reordering_of_pic_nums_idc
                    pReorderInfo->reorder_value[i] = picNumL0PredWrap - pRefPicListReordered[i]->m_PicNum[0]; // abs_diff_pic_num
                    pReorderInfo->num_entries ++;
                } else {
                    pReorderInfo->reordering_of_pic_nums_idc[i] = 1; // reordering_of_pic_nums_idc
                    pReorderInfo->reorder_value[i] = pRefPicListReordered[i]->m_PicNum[0] - picNumL0PredWrap; // abs_diff_pic_num
                    pReorderInfo->num_entries ++;
                }
                picNumL0PredWrap = pRefPicListReordered[i]->m_PicNum[0];
            } else if (H264EncoderFrame_isLongTermRef1_8u16s(pRefPicListReordered[i], 0)) {
                pReorderInfo->reordering_of_pic_nums_idc[i] = 2; // reordering_of_pic_nums_idc
                pReorderInfo->reorder_value[i] = pRefPicListReordered[i]->m_LongTermPicNum[0]; // abs_diff_pic_num
                pReorderInfo->num_entries ++;
            }
        }
    } else {
        Ipp8s *pFieldsReordered = pRefListReordered->m_Prediction;
        for (i = 0; i < numRefActiveInList; i ++) {
            if (!pRefPicListReordered[i]) continue;
            Ipp32s fieldIdx = H264EncoderFrame_GetNumberByParity_8u16s(pRefPicListReordered[i], pFieldsReordered[i]);
            if (H264EncoderFrame_isShortTermRef1_8u16s(pRefPicListReordered[i], fieldIdx)) {
                if (pRefPicListReordered[i]->m_PicNum[fieldIdx] <= picNumL0PredWrap) {
                    pReorderInfo->reordering_of_pic_nums_idc[i] = 0; // reordering_of_pic_nums_idc
                    pReorderInfo->reorder_value[i] = picNumL0PredWrap - pRefPicListReordered[i]->m_PicNum[fieldIdx]; // abs_diff_pic_num
                    pReorderInfo->num_entries ++;
                } else {
                    pReorderInfo->reordering_of_pic_nums_idc[i] = 1; // reordering_of_pic_nums_idc
                    pReorderInfo->reorder_value[i] = pRefPicListReordered[i]->m_PicNum[fieldIdx] - picNumL0PredWrap; // abs_diff_pic_num
                    pReorderInfo->num_entries ++;
                }
                picNumL0PredWrap = pRefPicListReordered[i]->m_PicNum[fieldIdx];
            } else if (H264EncoderFrame_isLongTermRef1_8u16s(pRefPicListReordered[i], fieldIdx)) {
                pReorderInfo->reordering_of_pic_nums_idc[i] = 2; // reordering_of_pic_nums_idc
                pReorderInfo->reorder_value[i] = pRefPicListReordered[i]->m_LongTermPicNum[fieldIdx]; // abs_diff_pic_num
                pReorderInfo->num_entries ++;
            }
        }
    }

    return UMC_OK;
}

// Perform modification of default ref pic list
Status MFXVideoENCODEH264::H264CoreEncoder_ModifyRefPicList( H264CoreEncoder_8u16s* core_enc, mfxEncodeCtrl *ctrl)
{
    Ipp32s i, j;
    Ipp16u maxNumActiveRefsInL0, maxNumActiveRefsInL1;
    Ipp16u maxNumActiveRefsByStandard = core_enc->m_SliceHeader.field_pic_flag ? 32 : 16;
    Ipp8u fieldIdx = core_enc->m_field_index;
    bool bUseIntraMBsOnly = false;
    // check if cur picture is part of I/P field pair
    bool bIsIP = core_enc->m_pCurrentFrame->m_bIsIDRPic == false &&
        core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] == INTRAPIC && core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] == PREDPIC;
    // check if need to remove from ref lists references from previous GOP
    bool bClearPrevRefs = m_layer->m_mfxVideoParam.mfx.GopOptFlag & MFX_GOP_CLOSED || core_enc->m_pCurrentFrame->m_FrameCount > m_layer->m_lastIframe;
    bool bIsBFrame = core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC;

    if ((core_enc->m_pCurrentFrame->m_PicCodType != PREDPIC && core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC &&
        core_enc->m_pCurrentFrame->m_PictureStructureForDec == FRM_STRUCTURE) ||
        (bIsIP && fieldIdx == 0))
        return UMC_OK;

    // reset reordering info
    RefPicListInfo &refPicListInfoCommon = core_enc->m_pCurrentFrame->m_RefPicListInfo[fieldIdx];
    RefPicListInfo refPicListInfoL0, refPicListInfoL1;
    refPicListInfoL0 = refPicListInfoL1 = refPicListInfoCommon;
    maxNumActiveRefsInL0 = maxNumActiveRefsInL1 = maxNumActiveRefsByStandard;
    core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 0;
    core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l1 = 0;
    core_enc->m_ReorderInfoL0[fieldIdx].num_entries = 0;
    core_enc->m_ReorderInfoL1[fieldIdx].num_entries = 0;

    // get default ref pic lists for current picture
    EncoderRefPicListStruct_8u16s *pRefListL0 = &core_enc->m_pCurrentFrame->m_refPicListCommon[fieldIdx].m_RefPicListL0;
    EncoderRefPicListStruct_8u16s *pRefListL1 = &core_enc->m_pCurrentFrame->m_refPicListCommon[fieldIdx].m_RefPicListL1;
    Ipp8u NumRefsInL0List = refPicListInfoCommon.NumRefsInL0List; // number of ref frames in ref pic list L0
    Ipp8u NumRefsInL1List = refPicListInfoCommon.NumRefsInL1List; // number of ref frames in ref pic list L1

    // prepare place for reordered ref pic lists L0, L1
    EncoderRefPicListStruct_8u16s reorderedRefListL0, reorderedRefListL1;
    memset(&reorderedRefListL0, 0, sizeof(EncoderRefPicListStruct_8u16s));
    memset(&reorderedRefListL1, 0, sizeof(EncoderRefPicListStruct_8u16s));

    mfxExtAVCRefListCtrl refPicListCtrlL0, *pRefPicListCtrlL0 = &refPicListCtrlL0;
    mfxExtAVCRefListCtrl refPicListCtrlL1, *pRefPicListCtrlL1 = &refPicListCtrlL1;;

    // get ref pic list control info
    mfxExtAVCRefListCtrl *pRefPicListCtrl = 0;
    if (core_enc->m_SliceHeader.field_pic_flag == 0 &&
        (core_enc->m_info.B_frame_rate == 0 || core_enc->TempIdMax) && // both types of temporal scalability
        ctrl && ctrl->ExtParam && ctrl->NumExtParam)
        pRefPicListCtrl = (mfxExtAVCRefListCtrl*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);

    if (pRefPicListCtrl)
        refPicListCtrlL0 = refPicListCtrlL1 = *pRefPicListCtrl;
    else {
        ClearRefListCtrl(&refPicListCtrlL0);
        ClearRefListCtrl(&refPicListCtrlL1);
    }

    if (m_temporalLayers.NumLayers) {
        mfxU16 refFrameNum = GetRefFrameNum(core_enc->m_pCurrentFrame->m_EncodedFrameNum, m_temporalLayers.NumLayers);
        for (i = 0; i < NumRefsInL0List; i ++) {
            H264EncoderFrame_8u16s *fr = pRefListL0->m_RefPicList[i];
            if (fr->m_EncodedFrameNum != refFrameNum && IsRejected(fr->m_FrameTag, &refPicListCtrlL0) == false) {
                j = 0;
                while (refPicListCtrlL0.RejectedRefList[j].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN)) j++;
                refPicListCtrlL0.RejectedRefList[j].FrameOrder = fr->m_FrameTag;
            }
        }
    } else {
        if (bClearPrevRefs || (bIsIP && fieldIdx == 1)) {
            // reject ref pics from previous GOP
            RejectPrevRefsEncOrder(&refPicListCtrlL0, pRefListL0, &refPicListInfoL0, 0, core_enc->m_pCurrentFrame);
            if (bIsBFrame && bClearPrevRefs)
                RejectPrevRefsEncOrder(&refPicListCtrlL1, pRefListL1, &refPicListInfoL1, 1, core_enc->m_pCurrentFrame);
        }

        if (bIsBFrame) {
            if (refPicListInfoL0.NumForwardSTRefs || refPicListInfoL0.NumRefsInLTList)
                RejectFutureRefs( &refPicListCtrlL0, pRefListL0, NumRefsInL0List, core_enc->m_pCurrentFrame);
            else
                refPicListCtrlL0.NumRefIdxL0Active = 1; // if only future refs then use 1st of them

            if (refPicListInfoL1.NumBackwardSTRefs || refPicListInfoL1.NumRefsInLTList)
                RejectPastRefs( &refPicListCtrlL1, pRefListL1, NumRefsInL1List, core_enc->m_pCurrentFrame);
            else
                refPicListCtrlL1.NumRefIdxL1Active = 1; // if only past refs then use 1st of them
        }
    }

    // disable ref pic lists reordering if it's not required
    if (pRefPicListCtrl == 0 && refPicListCtrlL0.RejectedRefList[0].FrameOrder == static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
        pRefPicListCtrlL0 = 0;
    if (pRefPicListCtrl == 0 && refPicListCtrlL1.RejectedRefList[0].FrameOrder == static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
        pRefPicListCtrlL1 = 0;

    if (pRefPicListCtrlL0) {
        // reorder ref list l0
        H264CoreEncoder_ReorderRefPicList(pRefListL0,
                                          &reorderedRefListL0,
                                          pRefPicListCtrlL0,
                                          NumRefsInL0List,
                                          maxNumActiveRefsInL0,
                                          core_enc->m_SliceHeader.field_pic_flag == 1);
        if (maxNumActiveRefsInL0 == 0 && bIsBFrame == false) // all refs in L0 are rejected
            bUseIntraMBsOnly = true; // use only Intra MBs to encode the picture
        else if (pRefPicListCtrlL0->NumRefIdxL0Active != 0)
            maxNumActiveRefsInL0 = IPP_MIN(pRefPicListCtrlL0->NumRefIdxL0Active, maxNumActiveRefsInL0);
    }

    if (bIsBFrame && pRefPicListCtrlL1) {
        // for B-picture reorder ref list L1
        H264CoreEncoder_ReorderRefPicList(pRefListL1,
                                          &reorderedRefListL1,
                                          pRefPicListCtrlL1,
                                          NumRefsInL1List,
                                          maxNumActiveRefsInL1,
                                          core_enc->m_SliceHeader.field_pic_flag == 1);
        if (maxNumActiveRefsInL1 == 0 && maxNumActiveRefsInL0 == 0) // all refs in both L0 and L1 are rejected
            bUseIntraMBsOnly = true; // use only Intra MBs to encode the picture
        else if (pRefPicListCtrlL1->NumRefIdxL1Active != 0)
            maxNumActiveRefsInL1 = IPP_MIN(pRefPicListCtrlL1->NumRefIdxL1Active, maxNumActiveRefsInL1);
    }

    if (core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC && core_enc->m_info.treat_B_as_reference/*core_enc->m_pCurrentFrame->m_RefPic*/) {
        if (reorderedRefListL0.m_RefPicList[0])
            reorderedRefListL0.m_RefPicList[0]->m_BRefCount--;
        if (reorderedRefListL1.m_RefPicList[0])
            reorderedRefListL1.m_RefPicList[0]->m_BRefCount--;
    }

    // prepare ref pic list modification syntax based on initial and reordered ref lists
    if (reorderedRefListL0.m_RefPicList[0]) { // check if reordered list L0 is constructed
        // build ref pic list modification syntax for list L0
        H264CoreEncoder_BuildRefPicListModSyntax(core_enc,
                                                 pRefListL0,
                                                 &reorderedRefListL0,
                                                 &core_enc->m_ReorderInfoL0[fieldIdx],
                                                 maxNumActiveRefsInL0);
        core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 1;

        // clear initial ref list L0
        memset(pRefListL0, 0, sizeof(EncoderRefPicListStruct_8u16s));
        // copy reordered ref list L0
        for (i = 0; i < NumRefsInL0List; i ++) {
            pRefListL0->m_RefPicList[i] = reorderedRefListL0.m_RefPicList[i];
            pRefListL0->m_Prediction[i] = reorderedRefListL0.m_Prediction[i];
        }
    }

    if (bIsBFrame && reorderedRefListL1.m_RefPicList[0]) {  // check if reordered list L1 is constructed
        // build ref pic list modification syntax for list L1
        H264CoreEncoder_BuildRefPicListModSyntax(core_enc,
                                                 pRefListL1,
                                                 &reorderedRefListL1,
                                                 &core_enc->m_ReorderInfoL1[fieldIdx],
                                                 maxNumActiveRefsInL1);
        core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l1 = 1;

        // clear initial ref list L1
        memset(pRefListL1, 0, sizeof(EncoderRefPicListStruct_8u16s));
        // copy reordered ref list L1
        for (i = 0; i < NumRefsInL1List; i ++) {
            pRefListL1->m_RefPicList[i] = reorderedRefListL1.m_RefPicList[i];
            pRefListL1->m_Prediction[i] = reorderedRefListL1.m_Prediction[i];
        }
    }

    core_enc->m_pCurrentFrame->m_MaxNumActiveRefsInL0[fieldIdx] = maxNumActiveRefsInL0;
    core_enc->m_pCurrentFrame->m_MaxNumActiveRefsInL1[fieldIdx] = maxNumActiveRefsInL1;
    core_enc->m_pCurrentFrame->m_use_intra_mbs_only[fieldIdx] =   bUseIntraMBsOnly;

    return UMC_OK;
}

// fill UMC structures for adaptive ref pic marking process
Status H264CoreEncoder_PrepareRefPicMarking(H264CoreEncoder_8u16s* core_enc, mfxEncodeCtrl *ctrl, EnumPicClass& ePic_Class, AVCTemporalLayers *tempLayers)
{

    if ((ePic_Class == DISPOSABLE_PIC && core_enc->m_pCurrentFrame->m_RefPic == 0) ||
        core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        return UMC_OK;

    H264EncoderFrame_8u16s *pFrm;
    H264EncoderFrame_8u16s *pHead = core_enc->m_dpb.m_pHead;
    Ipp8u fieldIdx = (Ipp8u)core_enc->m_field_index;
    Ipp32u currEntry;

    // try to get ref pic list control info
    mfxExtAVCRefListCtrl *pRefPicListCtrl = 0;
    if (core_enc->m_SliceHeader.field_pic_flag == 0 && core_enc->m_info.B_frame_rate == 0 && tempLayers && tempLayers->NumLayers == 0 &&
        ctrl && ctrl->ExtParam && ctrl->NumExtParam)
        pRefPicListCtrl = (mfxExtAVCRefListCtrl*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);
    else if (core_enc->TempIdMax > 0 && ctrl && ctrl->ExtParam && ctrl->NumExtParam)
        pRefPicListCtrl = (mfxExtAVCRefListCtrl*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);

    core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries = 0;
    core_enc->m_DecRefPicMarkingInfo[fieldIdx].long_term_reference_flag = 0;
    core_enc->m_DecRefPicMarkingInfo[fieldIdx].no_output_of_prior_pics_flag = 0;
    core_enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 0;

#ifdef USE_TEMPTEMP
    if (*core_enc->usetemptemp) {
        Ipp32s numtemp = core_enc->m_pCurrentFrame->m_FrameDisp*core_enc->rateDivider - *core_enc->temptempstart;
        assert(numtemp < core_enc->temptemplen);
        //numtemp = core_enc->refctrl[numtemp];
        //if (numtemp)
        if (core_enc->refctrl[numtemp].isLT) {
            core_enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 1;
            currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;

            // count current ref count
            mfxI32 refcnt = 0;
            for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                if (pFrm->m_wasEncoded && pFrm->m_RefPic)
                    refcnt++;
            }

            // mark current frame as LT reference
            if (core_enc->m_pCurrentFrame->m_bIsIDRPic == true ||
                refcnt < core_enc->m_info.num_ref_frames) { // IDR
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].long_term_reference_flag = 1;
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].no_output_of_prior_pics_flag = 0;
                // mark LongTermFrameIdx = 0 as occupied
                if (core_enc->m_pCurrentFrame->m_bIsIDRPic == true)
                    H264CoreEncoder_ClearAllLTIdxs_8u16s(core_enc); // need mmco to clear?
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 6;
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                //H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);
            } else {
                // LT frame to replace
                for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                    Ipp32s numtref = pFrm->m_FrameDisp*core_enc->rateDivider - *core_enc->temptempstart;
                    assert(numtref < core_enc->temptemplen);
                    if (numtref != numtemp &&
                        core_enc->refctrl[core_enc->refctrl[numtref].deadline].codorder <=
                        core_enc->refctrl[numtemp].codorder) {
                    //if (core_enc->refctrl[numtref].deadline <= numtemp) {
                        if (core_enc->refctrl[numtref].isLT) {
                            core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 6;
                            core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = pFrm->m_LongTermFrameIdx;
                            H264CoreEncoder_ClearLTIdx_8u16s(core_enc, pFrm->m_LongTermFrameIdx);
                        } else {
                            // ref will be replaced by sliding window
                            core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 6;
                            core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);
                            //core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 3;
                            //core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[0] - (pFrm->m_PicNum[0] + 1);
                            //core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry*2+1] = H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);
                        }
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                        currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
                    }
                }
            }

        }

        /*if (core_enc->TempIdMax > 0)*/ {
            for(pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                Ipp32s numtref = pFrm->m_FrameDisp*core_enc->rateDivider - *core_enc->temptempstart;
                assert(numtref < core_enc->temptemplen);
                if (pFrm->m_wasEncoded && pFrm->m_RefPic &&
                    core_enc->refctrl[core_enc->refctrl[numtref].lastuse].codorder <
                    core_enc->refctrl[numtemp].codorder &&
                    //core_enc->refctrl[numtref].lastuse < numtemp &&
                    pFrm->m_temporalId >= core_enc->m_pCurrentFrame->m_temporalId) {
                    // delete ref frame which was used for the last time
                    core_enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 1;
                    currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
                    if (H264EncoderFrame_isShortTermRef1_8u16s(pFrm, 0)) {
                        // memory_management_control_operation equal to 1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 1;
                        // difference_of_pic_nums_minus1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[0] - (pFrm->m_PicNum[0] + 1);
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                    }
                }
            }
        }
        // tmp
        return UMC_OK;
    }
#endif // USE_TEMPTEMP

    // else - not m_usetemptemp
    if (/*core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC &&*/ core_enc->m_info.treat_B_as_reference/*core_enc->m_pCurrentFrame->m_RefPic*/) {
        // locate B refs which are completely used to remove from ref list
        for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
            if (pFrm->m_PicCodType == BPREDPIC && pFrm->m_wasEncoded && pFrm->m_RefPic && pFrm->m_BRefCount <= 0 &&
                pFrm->m_temporalId == core_enc->m_pCurrentFrame->m_temporalId) {
                core_enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 1;
                currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
                if (pFrm->m_PictureStructureForRef >= FRM_STRUCTURE) { // frame
                    if (H264EncoderFrame_isShortTermRef1_8u16s(pFrm, 0)) {
                        // memory_management_control_operation equal to 1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 1;
                        // difference_of_pic_nums_minus1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[0] - (pFrm->m_PicNum[0] + 1);
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                    }
                } else { // field
                    if (H264EncoderFrame_isShortTermRef1_8u16s(pFrm, 0)) {
                        // memory_management_control_operation equal to 1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 1;
                        // difference_of_pic_nums_minus1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[0] - (pFrm->m_PicNum[0] + 1);
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                    }
                    currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
                    if (H264EncoderFrame_isShortTermRef1_8u16s(pFrm, 1)) {
                        // memory_management_control_operation equal to 1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 1;
                        // difference_of_pic_nums_minus1
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[1] - (pFrm->m_PicNum[1] + 1);
                        core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                    }
                }
            }
        }
    }
    H264EncoderFrame_8u16s* LTFramesToRemove[16] = {0,};
    H264EncoderFrame_8u16s* STFramesToRemove[16] = {0,};
    H264EncoderFrame_8u16s* LTFramesToMark[16] = {0,};
    Ipp8u numLTtoRemove = 0;
    Ipp8u numSTtoRemove = 0;
    Ipp8u numToMark = 0;
    Ipp8u i;
    Ipp32u numRefFramesAfterMarking = 0;

    if (pRefPicListCtrl && !core_enc->m_field_index) { // apply external ref pic marking (if any)
        // TODO (fields): if both of fields of current frame are marked as LT then
        //                dec ref pic marking should be separated between 1st and 2nd fields
        Ipp32u LTFrameOrder;
        Ipp32u FrameOrderToRemove;

        // parse external ref pic marking structure and find frames to mark as LT or remove from DPB
        // TODO (fields): for fields numToMark and numLTtoRemove should represent number of frames but not fields
        // TODO (fields): also for fields info about field structure should be gathered
        for (i = 0; i < 16; i ++) {
            // collect frames to mark as LT
            LTFrameOrder = pRefPicListCtrl->LongTermRefList[i].FrameOrder;
            if (LTFrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && (IsRejected(LTFrameOrder, pRefPicListCtrl) == false)) {
                for (pFrm = pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                    if (pFrm->m_FrameTag == LTFrameOrder &&
                        (H264EncoderFrame_isShortTermRef0_8u16s(pFrm) || pFrm == core_enc->m_pCurrentFrame)) {
                            LTFramesToMark[numToMark ++] = pFrm;
                    }
                }
            }
            // collect frames to mark as "unused for reference"
            // TODO can be ignored with IDR
            FrameOrderToRemove = pRefPicListCtrl->RejectedRefList[i].FrameOrder;
            if (FrameOrderToRemove != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN)) {
                for (pFrm = pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                    if (pFrm->m_FrameTag == FrameOrderToRemove) {
                        if (H264EncoderFrame_isShortTermRef0_8u16s(pFrm))
                            STFramesToRemove[numSTtoRemove++] = pFrm;
                        else if (H264EncoderFrame_isLongTermRef0_8u16s(pFrm))
                            LTFramesToRemove[numLTtoRemove++] = pFrm;
                    }
                }
            }
        }
    }

    // check for enough space in DPB
    // calculate number of ref frames in DPB after marking process
    numRefFramesAfterMarking = core_enc->m_NumShortTermRefFrames + core_enc->m_NumLongTermRefFrames - numLTtoRemove - numSTtoRemove + 1;
    if (pRefPicListCtrl && numRefFramesAfterMarking > (Ipp32u)core_enc->m_SeqParamSet->num_ref_frames) {
        // not enough space in DPB for current frame
        // need to clear space
        if (core_enc->m_NumShortTermRefFrames > numToMark) {
            // after LT marking we have ST refs in DPB. Mark oldest of them as "unused for reference"
            H264EncoderFrame_8u16s *pCurr = core_enc->m_dpb.m_pHead;
            H264EncoderFrame_8u16s *pOldest = 0;
            Ipp32s  SmallestFrameNumWrap = 0x0fffffff;

            while (pCurr)
            {
                if (H264EncoderFrame_isShortTermRef0_8u16s(pCurr) &&
                    (pCurr->m_FrameNumWrap < SmallestFrameNumWrap))
                {
                    for (i = 0; i < numToMark && pCurr != LTFramesToMark[i]; i ++);
                    if (i == numToMark) {
                        pOldest = pCurr;
                        SmallestFrameNumWrap = pCurr->m_FrameNumWrap;
                    }
                }
                pCurr = pCurr->m_pFutureFrame;
            }

            if (pOldest)
                STFramesToRemove[numSTtoRemove++] = pOldest;
        } else {
            return UMC_ERR_FAILED;
        }

    }

    VM_ASSERT(numToMark <= core_enc->m_NumShortTermRefFrames);
    VM_ASSERT(numLTtoRemove <= core_enc->m_NumLongTermRefFrames);
    VM_ASSERT(numSTtoRemove <= core_enc->m_NumShortTermRefFrames);

    // here numLTtoRemove, numSTtoRemove and numToMark represent numbers in terms of frames
    // TODO (fields): implement use of info about field structure to mark/unmark particular field or both fields
    if (numSTtoRemove || numLTtoRemove || numToMark) {
        core_enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 1;

        // remove LT refs from DPB and free LT frame indexes
        for (i = 0; i < numLTtoRemove; i ++) {
            currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
            // memory_management_control_operation equal to 2
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 2;
            // LongTermPicNum
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = LTFramesToRemove[i]->m_LongTermPicNum[0];
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
            H264CoreEncoder_ClearLTIdx_8u16s(core_enc, LTFramesToRemove[i]->m_LongTermFrameIdx);
        }

        // remove ST refs from DPB
        for (i = 0; i < numSTtoRemove; i ++) {
            currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
            // memory_management_control_operation equal to 1
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 1;
            // difference_of_pic_nums_minus1
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[0] - (STFramesToRemove[i]->m_PicNum[0] + 1);
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
        }

        if (core_enc->TempIdMax > 0 && !core_enc->m_pCurrentFrame->m_bIsIDRPic && numToMark > numLTtoRemove) {
            // (re)set maximum long-term frame index on IDR
            currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
            // memory_management_control_operation equal to 4
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 4;
            // maximum long-term frame index
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->TempIdMax + 1; //core_enc->m_info.num_ref_frames;
            core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
        }

        // mark current frame or ST refs as LT
        for (i = 0; i < numToMark; i ++) {
            currEntry = core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries;
            if (LTFramesToMark[i] == core_enc->m_pCurrentFrame) {
                // mark current frame as LT reference
                if (core_enc->m_pCurrentFrame->m_bIsIDRPic == true) { // IDR
                    core_enc->m_DecRefPicMarkingInfo[fieldIdx].long_term_reference_flag = 1;
                    core_enc->m_DecRefPicMarkingInfo[fieldIdx].no_output_of_prior_pics_flag = 0;
                    // mark LongTermFrameIdx = 0 as occupied
                    H264CoreEncoder_ClearAllLTIdxs_8u16s(core_enc);
                    H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);
                } else { // non IDR
                    // memory_management_control_operation equal to 6
                    core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 6;
                    // LongTermFrameIdx
                    core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);
                    core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
                }
            } else { // mark ST ref from DPB as LT reference
                // memory_management_control_operation equal to 3
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].mmco[currEntry] = 3;
                // LongTermFrameIdx
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2] = core_enc->m_pCurrentFrame->m_PicNum[0] - (LTFramesToMark[i]->m_PicNum[0] + 1);
                core_enc->m_DecRefPicMarkingInfo[fieldIdx].value[currEntry * 2 + 1] = H264CoreEncoder_GetFreeLTIdx_8u16s(core_enc);

                core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries ++;
            }
        }
    }

    // Prepare data for dec ref pic marking repetition SEI if dec_ref_pic_marking() syntax is encoded for current picture
    if (core_enc->m_DecRefPicMarkingInfo[fieldIdx].num_entries > 0 || ePic_Class == IDR_PIC) {
        core_enc->m_SEIData.DecRefPicMrkRep.isCoded = true;
        core_enc->m_SEIData.DecRefPicMrkRep.original_idr_flag = (ePic_Class == IDR_PIC);
        core_enc->m_SEIData.DecRefPicMrkRep.original_frame_num = core_enc->m_SliceHeader.frame_num;
        core_enc->m_SEIData.DecRefPicMrkRep.original_field_pic_flag = core_enc->m_SliceHeader.field_pic_flag;
        core_enc->m_SEIData.DecRefPicMrkRep.original_bottom_field_flag = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index];
        // keep dec_ref_pic_marking() syntax from current picture for dec ref pic marking repetition SEI
        H264BsReal_8u16s bs;
        memset(&bs, 0, sizeof(H264BsReal_8u16s));
        bs.m_base.m_pbs = core_enc->m_SEIData.DecRefPicMrkRep.dec_ref_pic_marking;
        bs.m_base.m_bitOffset = 0;
        bs.m_base.m_pbsBase = bs.m_base.m_pbs;
        bs.m_base.m_maxBsSize = 0xff;
        H264BsReal_PutDecRefPicMarking_8u16s(&bs, core_enc->m_SliceHeader, ePic_Class, core_enc->m_DecRefPicMarkingInfo[core_enc->m_field_index]);
        core_enc->m_SEIData.DecRefPicMrkRep.sizeInBytes = bs.m_base.m_pbs - bs.m_base.m_pbsBase;
        core_enc->m_SEIData.DecRefPicMrkRep.bitOffset = bs.m_base.m_bitOffset;
    }

    return UMC_OK;
}

Status MFXVideoENCODEH264::H264CoreEncoder_CompressFrame(
    void* state,
    EnumPicCodType& ePictureType,
    EnumPicClass& ePic_Class,
    MediaData* dst_main,
    MediaData* dst_ext,
    mfxEncodeCtrl *ctrl)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status status = UMC_OK;
    Ipp32s slice;
    bool bufferOverflowFlag = false;
    bool buffersNotFull = true;
    Ipp32s bitsPerFrame = 0;
    Ipp32s brcRecode = 0; // nonzero if brc decided to recode picture
    bool recodePicture = false;
    bool recodePCM = false;
    Ipp32s payloadBits = 0;
    Ipp32s brc_data_size;
    H264EncoderFrame_8u16s tempRefFrame;
    Ipp32s picStartDataSize = 0;
    MediaData* dst = dst_main;
    Ipp8u  nal_header_ext[3] = {0,};
    sNALUnitHeaderSVCExtension svc_header = {0, };
    Ipp32s min_qp = 0;// min slice QP before buffer overflow
    Ipp32s small_frame_qp = 0;

    H264ENC_UNREFERENCED_PARAMETER(ePictureType);


#ifdef SLICE_CHECK_LIMIT
    Ipp32s numSliceMB;
#endif // SLICE_CHECK_LIMIT

    Ipp32s data_size = (Ipp32s)dst_main->GetDataSize();
    //   fprintf(stderr,"frame=%d\n", core_enc->m_uFrames_Num);
    if (core_enc->m_svc_layer.svc_ext.quality_id == 0)
    {
        if (core_enc->m_Analyse & ANALYSE_RECODE_FRAME /*&& core_enc->m_pReconstructFrame == NULL //could be size change*/){ //make init reconstruct buffer
            core_enc->m_pReconstructFrame = core_enc->m_pReconstructFrame_allocated;
            core_enc->m_pReconstructFrame->m_bottom_field_flag[0] = core_enc->m_pCurrentFrame->m_bottom_field_flag[0];
            core_enc->m_pReconstructFrame->m_bottom_field_flag[1] = core_enc->m_pCurrentFrame->m_bottom_field_flag[1];
        }
    }
    core_enc->m_is_cur_pic_afrm = (Ipp32s)(core_enc->m_pCurrentFrame->m_PictureStructureForDec==AFRM_STRUCTURE);

#ifdef USE_PSYINFO
#ifndef FRAME_QP_FROM_FILE
    if (core_enc->m_Analyse_ex & ANALYSE_PSY_FRAME) {
        if (core_enc->m_pCurrentFrame->psyStatic.dc > 0 && core_enc->m_info.rate_controls.method != H264_RCM_QUANT) {
            Ipp32s qp;
            if (core_enc->brc)
                qp = core_enc->brc->GetQP(ePictureType == INTRAPIC ? I_PICTURE : (ePictureType == PREDPIC ? P_PICTURE : B_PICTURE));
            else
                qp = H264_AVBR_GetQP(&core_enc->avbr, ePictureType);

            if (core_enc->m_Analyse_ex & ANALYSE_PSY_STAT_FRAME) {
                PsyInfo* stat = &core_enc->m_pCurrentFrame->psyStatic;
                mfxI32 cmplxty_s = stat->bgshift + stat->details + (core_enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE ? stat->interl/8 : stat->interl);
                if (cmplxty_s > 4000)
                //    qp += 2;
                //else if (cmplxty_s > 2000)
                    qp ++;
                else if (cmplxty_s < 400 && ePictureType == INTRAPIC)
                    qp --;
            }

            if (core_enc->m_Analyse_ex & ANALYSE_PSY_DYN_FRAME) {
                PsyInfo* dyna = &core_enc->m_pCurrentFrame->psyDyna[0];
                if (dyna->dc > 0) {
                    mfxI32 cmplxty_t = dyna->dc + dyna->bgshift + dyna->details + (core_enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE ? dyna->interl/8 : dyna->interl);
                    if (cmplxty_t > 5000)
                        qp ++;
                    else if (cmplxty_t < 500 && ePictureType != BPREDPIC)
                        qp --;
                }
                dyna = &core_enc->m_pCurrentFrame->psyDyna[1];
                if (dyna->dc > 0) {
                    mfxI32 cmplxty_t = dyna->dc + dyna->bgshift + dyna->details + (core_enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE ? dyna->interl/8 : dyna->interl);
                    if (cmplxty_t > 5000)
                        qp ++;
                    else if (cmplxty_t < 500 && ePictureType != BPREDPIC)
                        qp --;
                }
            }

            if( qp > 51 ) qp = 51;
            else if (qp < 1) qp = 1;

            if (core_enc->brc)
                core_enc->brc->SetQP(qp, ePictureType == INTRAPIC ? I_PICTURE : (ePictureType == PREDPIC ? P_PICTURE : B_PICTURE));
            else
                H264_AVBR_SetQP(&core_enc->avbr, ePictureType, /*++*/ qp);
        }
    }
#endif // !FRAME_QP_FROM_FILE
#endif // USE_PSYINFO

    // reencode loop
    for (core_enc->m_field_index=0;core_enc->m_field_index<=(Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE);core_enc->m_field_index++) {

#ifdef H264_RECODE_PCM
        if(core_enc->useMBT && !recodePCM)
        {
            core_enc->m_nPCM = 0;
            memset(core_enc->m_mbPCM, 0, core_enc->m_HeightInMBs * core_enc->m_WidthInMBs * 2 * sizeof(Ipp32s));
        }
#endif //H264_RECODE_PCM

        if (core_enc->m_svc_layer.isActive &&
            (core_enc->m_SeqParamSet->seq_tcoeff_level_prediction_flag || core_enc->m_SeqParamSet->adaptive_tcoeff_level_prediction_flag))
        {
            Ipp32s nMBCount = core_enc->m_WidthInMBs * core_enc->m_HeightInMBs * ((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1);
            if (brcRecode) {
                // restore Tcoeffs from reserved
                ippsCopy_16s(core_enc->m_ReservedMacroblockTCoeffs, core_enc->m_SavedMacroblockTCoeffs, COEFFICIENTS_BUFFER_SIZE*nMBCount);
            } else {
                // reserve Tcoeffs
                ippsCopy_16s(core_enc->m_SavedMacroblockTCoeffs, core_enc->m_ReservedMacroblockTCoeffs, COEFFICIENTS_BUFFER_SIZE*nMBCount);
            }
        }

        if (dst_ext && core_enc->m_field_index)
            dst = dst_ext;
        core_enc->m_PicType = core_enc->m_pCurrentFrame->m_PicCodTypeFields[core_enc->m_field_index];
        brc_data_size = (Ipp32s)dst->GetDataSize();
        bool startPicture = true;
        core_enc->m_NeedToCheckMBSliceEdges = core_enc->m_info.num_slices > 1 || core_enc->m_field_index >0;
        EnumSliceType default_slice_type = INTRASLICE;
        if (core_enc->brc) {
            FrameType picType = (core_enc->m_PicType == INTRAPIC ? I_PICTURE : (core_enc->m_PicType == PREDPIC ? P_PICTURE : B_PICTURE));
            Ipp32s picture_structure = core_enc->m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE ? PS_FRAME :
            (core_enc->m_pCurrentFrame->m_PictureStructureForDec == BOTTOM_FLD_STRUCTURE ? PS_BOTTOM_FIELD : PS_TOP_FIELD);
            core_enc->brc->SetPictureFlags(picType, picture_structure);
            if (!brcRecode)
                core_enc->brc->PreEncFrame(picType, 0, core_enc->m_pCurrentFrame->m_temporalId);
        }
#ifdef SLICE_CHECK_LIMIT
        numSliceMB = 0;
#endif // SLICE_CHECK_LIMIT
#if defined ALPHA_BLENDING_H264
        bool alpha = true; // Is changed to the opposite at the beginning.
        do { // First iteration primary picture, second -- alpha (when present)
        alpha = !alpha;
        if(!alpha) {
#endif // ALPHA_BLENDING_H264
        if (ePic_Class == IDR_PIC) {
            if (core_enc->m_field_index)
                ePic_Class = REFERENCE_PIC;
            else {
                if (core_enc->m_svc_layer.svc_ext.quality_id == 0 &&
                    core_enc->m_svc_layer.svc_ext.dependency_id == 0)
                {
                    //if (core_enc->m_uFrames_Num == 0) //temporarily disabled

                    //for( struct h264Layer* layer = m_layer; layer; layer = layer->src_layer) {
                    //    core_enc = layer->enc;
                    //    H264CoreEncoder_SetSequenceParameters_8u16s(core_enc);
                    //    H264CoreEncoder_SetPictureParameters_8u16s(core_enc);
                    //}
                    H264CoreEncoder_SetSequenceParameters_8u16s(core_enc);
                    H264CoreEncoder_SetPictureParameters_8u16s(core_enc);
                    for( Ipp32u ll = 1; ll < m_svc_count; ll++) { // put for other spatial layers
                        core_enc = m_svc_layers[ll]->enc;
                        H264CoreEncoder_SetSequenceParameters_8u16s(core_enc);
                        H264CoreEncoder_SetPictureParameters_8u16s(core_enc);
                    }
                    core_enc = (H264CoreEncoder_8u16s *)state; // restore core_enc
                }

                if (core_enc->m_svc_layer.svc_ext.quality_id == 0)
                {
                    // Toggle the idr_pic_id on and off so that adjacent IDRs will have different values
                    // This is done here because it is done per frame and not per slice.
//                         core_enc->m_SliceHeader.idr_pic_id ^= 0x1;
                    core_enc->m_SliceHeader.idr_pic_id++;
                    core_enc->m_SliceHeader.idr_pic_id &= 0xff; //Restrict to 255 to reduce number of bits(max value 65535 in standard)
                }
            }
        }
        default_slice_type = (core_enc->m_PicType == PREDPIC) ? PREDSLICE : (core_enc->m_PicType == INTRAPIC) ? INTRASLICE : BPREDSLICE;
        core_enc->m_PicParamSet->chroma_format_idc = core_enc->m_SeqParamSet->chroma_format_idc;
        core_enc->m_PicParamSet->bit_depth_luma = core_enc->m_SeqParamSet->bit_depth_luma;
#if defined ALPHA_BLENDING_H264
        } else {
            H264EncoderFrameList_switchToAuxiliary_8u16s(&core_enc->m_cpb);
            H264EncoderFrameList_switchToAuxiliary_8u16s(&core_enc->m_dpb);
            core_enc->m_PicParamSet->chroma_format_idc = 0;
            core_enc->m_PicParamSet->bit_depth_luma = core_enc->m_SeqParamSet->bit_depth_aux;
        }
#endif // ALPHA_BLENDING_H264
        // TODO: check how affects SVC IL predictions, stored in recon. frame
        if (core_enc->m_svc_layer.svc_ext.quality_id == 0)
            if (!(core_enc->m_Analyse & ANALYSE_RECODE_FRAME)){
                core_enc->m_pReconstructFrame = core_enc->m_pCurrentFrame;
            }

        /* Fill ext header */
        if (core_enc->m_svc_layer.isActive) {
            core_enc->m_svc_layer.svc_ext.idr_flag = ePic_Class == IDR_PIC;
        }

        // reset bitstream object before begin compression
        Ipp32s i;
        for (i = 0; i < core_enc->m_info.num_slices*((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1); i++)
            H264BsReal_Reset_8u16s(core_enc->m_pbitstreams[i]);
#if defined ALPHA_BLENDING_H264
        if (!alpha)
#endif // ALPHA_BLENDING_H264
        {
            H264CoreEncoder_SetSliceHeaderCommon_8u16s(state, core_enc->m_pCurrentFrame);
            if( default_slice_type == BPREDSLICE ){
                if( core_enc->m_Analyse & ANALYSE_ME_AUTO_DIRECT){
                    if( core_enc->m_SliceHeader.direct_spatial_mv_pred_flag )
                        core_enc->m_SliceHeader.direct_spatial_mv_pred_flag = core_enc->m_DirectTypeStat[0] > ((545*core_enc->m_DirectTypeStat[1])>>9) ? 0:1;
                    else
                        core_enc->m_SliceHeader.direct_spatial_mv_pred_flag = core_enc->m_DirectTypeStat[1] > ((545*core_enc->m_DirectTypeStat[0])>>9) ? 1:0;
                    core_enc->m_DirectTypeStat[0]=core_enc->m_DirectTypeStat[1]=0;
                } else
                    core_enc->m_SliceHeader.direct_spatial_mv_pred_flag = core_enc->m_info.direct_pred_mode & 0x1;
            }
            if (recodePicture == false) { // write SPS, PPS, SEI in 1st iteration of re-encoding loop
                payloadBits = (Ipp32s)dst->GetDataSize();
                status = H264CoreEncoder_encodeFrameHeader(state, core_enc->m_bs1, dst, (ePic_Class == IDR_PIC), startPicture);
                if (status != UMC_OK) goto done;

                // KL: commented out, because base spatial layer is extracted improperly with it
                ////if (core_enc->m_svc_layer.isActive && ePic_Class == IDR_PIC
                ////    && core_enc->m_svc_layer.svc_ext.quality_id == 0
                ////    && (core_enc->m_svc_layer.svc_ext.dependency_id > 0 /* ??? */
                ////        || core_enc->QualityNum > 1 || core_enc->TempIdMax > 0)) {
                ////    status = SVC_encodeBufferingPeriodSEI_inScalableNestingSEI(
                ////        state,
                ////        core_enc->m_bs1,
                ////        dst,
                ////        startPicture);
                ////}


/* kta ???
                if (m_svc_count == 0 ||
                    (/ * core_enc->m_pCurrentFrame->m_temporalId == 0 && * / // ??? kta
                    core_enc->m_svc_layer.svc_ext.dependency_id == 0 && core_enc->m_svc_layer.svc_ext.quality_id)) // ??? kta
*/
                    status = H264CoreEncoder_encodeSEI(state, core_enc->m_bs1, dst, (ePic_Class == IDR_PIC), startPicture, ctrl, (Ipp8u)brcRecode);
                if (status != UMC_OK) goto done;

                if (core_enc->m_svc_layer.isActive) {
                    sNALUnitHeaderSVCExtension *svc_header = &core_enc->m_svc_layer.svc_ext;
                    Ipp8u TID = m_svc_layers[svc_header->dependency_id]->m_InterLayerParam.TemporalId[core_enc->m_pCurrentFrame->m_temporalId];
                    nal_header_ext[0] = 0x80 | (svc_header->idr_flag << 6) | svc_header->priority_id;
                    nal_header_ext[1] = (svc_header->no_inter_layer_pred_flag << 7) | (svc_header->dependency_id << 4) | svc_header->quality_id;
                    nal_header_ext[2] = (TID << 5) | (svc_header->use_ref_base_pic_flag << 4) |
                        (svc_header->discardable_flag << 3) | (svc_header->output_flag << 2) | svc_header->reserved_three_2bits;
                } else {
                    nal_header_ext[2] = nal_header_ext[1] = nal_header_ext[0] = 0;
                }
                /* Base layer */
                if (core_enc->m_svc_layer.isActive) {
                    if ((core_enc->m_svc_layer.svc_ext.dependency_id == 0) &&
                        (core_enc->m_svc_layer.svc_ext.quality_id == 0)) {
                            size_t oldsize = dst->GetDataSize();

                            H264BsReal_Reset_8u16s(core_enc->m_bs1);

                            if (ePic_Class != DISPOSABLE_PIC) {
                                // Write the prefix_nal_unit_svc
                                status = H264BsReal_PutSVCPrefix_8u16s(core_enc->m_bs1, &core_enc->m_svc_layer.svc_ext);
                                if (status != UMC_OK) goto done;

                                H264BsBase_WriteTrailingBits(&core_enc->m_bs1->m_base);
                            }

                            //Write to output bitstream
                            dst->SetDataSize(oldsize +
                                H264BsReal_EndOfNAL_8u16s(core_enc->m_bs1,
                                (Ipp8u*)dst->GetDataPointer() + oldsize,
                                (ePic_Class != DISPOSABLE_PIC),
                                NAL_UT_PREFIX, startPicture,
                                nal_header_ext));
                    }
                }
                payloadBits = (Ipp32s)((dst->GetDataSize() - payloadBits) << 3);

                if (m_temporalLayers.NumLayers) { // write Prefix NAL unit to precede 1st coded slice NALU
                    size_t oldsize = dst->GetDataSize();

                    // prepare nal_unit_header_svc_extension
                    mfxU16 layerId = GetTemporalLayer( core_enc->m_pCurrentFrame->m_EncodedFrameNum, m_temporalLayers.NumLayers);
                    // write svc_extension_flag = 1, idr_flag, priority_id
                    nal_header_ext[0] = 0x80 | (core_enc->m_pCurrentFrame->m_bIsIDRPic << 6) | m_temporalLayers.LayerPIDs[layerId];
                    // write no_inter_layer_pred_flag = 1, dependency_id = 0, quality_id = 0
                    nal_header_ext[1] = 0x80;
                    // temporal_id, write use_ref_base_pic_flag = 0, discardable_flag = 1, output_flag = 1, reserved_three_2bits = 3
                    nal_header_ext[2] = (layerId << 5) | 0x0f;

                    // write Prefix NAL unit
                    if (ePic_Class != DISPOSABLE_PIC) { // fill prefix_nal_unit_svc syntax for slice of reference picture
                        svc_header.store_ref_base_pic_flag = 0;
                        svc_header.use_ref_base_pic_flag = 0;

                        status = H264BsReal_PutSVCPrefix_8u16s(core_enc->m_bs1, &svc_header);
                        if (status != UMC_OK) goto done;

                        H264BsBase_WriteTrailingBits(&core_enc->m_bs1->m_base);
                    }

                    bool tmp = startPicture;

                    dst->SetDataSize(oldsize +
                            H264BsReal_EndOfNAL_8u16s(core_enc->m_bs1,
                                                    (Ipp8u*)dst->GetDataPointer() + oldsize,
                                                    (ePic_Class != DISPOSABLE_PIC),
                                                    NAL_UT_PREFIX,
                                                    tmp,
                                                    nal_header_ext));
                }

                picStartDataSize = (Ipp32s)dst->GetDataSize();
            } else {
                dst->SetDataSize(picStartDataSize); // use previously written SPS, PPS, SEI in subsequent iterations of re-encoding loop
                startPicture = false;
            }
        }
        status = H264CoreEncoder_Start_Picture_8u16s(state, &ePic_Class, core_enc->m_PicType);
        if (status != UMC_OK)
            goto done;
        Ipp32s slice_qp_delta_default = core_enc->m_Slices[0].m_slice_qp_delta;
        // update dpb for prediction of current picture, construct default ref pic list
        if (core_enc->m_svc_layer.svc_ext.quality_id == 0) /* first quality layer */
        {
#ifdef USE_TEMPTEMP
            if (m_usetemptemp && core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC /*&& core_enc->m_pCurrentFrame->m_temporalId == 0*/) {
                m_temptempstart = core_enc->m_pCurrentFrame->m_FrameDisp * core_enc->rateDivider;
            }
#endif // USE_TEMPTEMP

            H264CoreEncoder_UpdateRefPicListCommon_8u16s(state);
            if (!core_enc->TempIdMax /*core_enc->m_pCurrentFrame->m_temporalId == core_enc->TempIdMax*/) {
                // specify ref pic list modification
                H264CoreEncoder_ModifyRefPicList(core_enc, ctrl);
                // prepare all ref pic marking here
                H264CoreEncoder_PrepareRefPicMarking(core_enc, ctrl, ePic_Class, &m_temporalLayers);
            } else {
                // Modifies lists, creates ExtBuff and calls Marking. TODO: to try with temporalLayers
                ReorderListSVC(core_enc, ePic_Class);
            }
        }
        if (core_enc->m_svc_layer.svc_ext.quality_id) {
            core_enc->m_spatial_resolution_change = false;
            core_enc->m_restricted_spatial_resolution_change = true;
        }

#if defined _OPENMP
        vm_thread_priority mainTreadPriority = vm_get_current_thread_priority();
#ifndef MB_THREADING
#pragma omp parallel for private(slice)
#endif // MB_THREADING
#endif // _OPENMP

#ifdef H264_NEW_THREADING
        if (!core_enc->useMBT)
        {
            m_taskParams.thread_number = 0;
            m_taskParams.parallel_executing_threads = 0;
            vm_event_reset(&m_taskParams.parallel_region_done);

            for (i = 0; i < core_enc->m_info.numThreads; i++)
            {
                m_taskParams.threads_data[i].core_enc = core_enc;
                m_taskParams.threads_data[i].state = state;
                // Slice number is computed in the tasks themself
                m_taskParams.threads_data[i].slice_qp_delta_default = slice_qp_delta_default;
                m_taskParams.threads_data[i].default_slice_type = default_slice_type;
            }
            // Start a new loop for slices in parallel
            m_taskParams.slice_counter = core_enc->m_info.num_slices * core_enc->m_field_index;
            m_taskParams.slice_number_limit = core_enc->m_info.num_slices * (core_enc->m_field_index + 1);

            // Start parallel region
            m_taskParams.single_thread_done = true;
            // Signal scheduler that all busy threads should be unleashed
            m_core->INeedMoreThreadsInside(this);

            mfxI32 slice_number;
            Status umc_status = UMC_OK;
            do
            {
                umc_status = EncodeNextSliceTask(0, &slice_number);
            }
            while (UMC_OK == umc_status && slice_number < m_taskParams.slice_number_limit);

            if (UMC_OK != umc_status)
                status = umc_status;

            // Disable parallel task execution
            m_taskParams.single_thread_done = false;

            // Wait for other threads to finish encoding their last slice
            while(m_taskParams.parallel_executing_threads > 0)
                vm_event_wait(&m_taskParams.parallel_region_done);
        }
        else
        {
            m_taskParams.threads_data[0].core_enc = core_enc;
            m_taskParams.threads_data[0].state = state;
            m_taskParams.threads_data[0].slice_qp_delta_default = slice_qp_delta_default;
            m_taskParams.threads_data[0].default_slice_type = default_slice_type;

            for(slice = core_enc->m_info.num_slices * core_enc->m_field_index;
                slice < core_enc->m_info.num_slices * (core_enc->m_field_index + 1); slice++)
            {
                m_taskParams.thread_number = 0;
                m_taskParams.parallel_executing_threads = 0;
                vm_event_reset(&m_taskParams.parallel_region_done);

                m_taskParams.threads_data[0].slice = slice;

                // All threading stuff is done inside of H264CoreEncoder_Compress_Slice_MBT
                Status umc_status = ThreadedSliceCompress(0);
                if (UMC_OK != umc_status)
                {
                    status = umc_status;
                    break;
                }
            }
        }
#elif defined VM_SLICE_THREADING_H264

        Ipp32s realThreadsAllocated = threadsAllocated;
        if (core_enc->useMBT)
            // Workaround for MB threading which cannot encode many slices in parallel yet
            threadsAllocated = 1;

        slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index;
        Ipp32s nSlice = core_enc->m_info.num_slices / threadsAllocated;

        while(slice < (core_enc->m_info.num_slices*core_enc->m_field_index + nSlice*threadsAllocated)) {
            for (i = 0; i < threadsAllocated; i++) {
                threadSpec[i].core_enc = core_enc;
                threadSpec[i].state = state;
                threadSpec[i].slice = slice+i;
                threadSpec[i].slice_qp_delta_default = slice_qp_delta_default;
                threadSpec[i].default_slice_type = default_slice_type;
            }
            if (threadsAllocated > 1) {
                // start additional thread(s)
                for (i = 0; i < threadsAllocated - 1; i++) {
                    vm_event_signal(&threads[i]->start_event);
                }
            }

            ThreadedSliceCompress(0);

            if (threadsAllocated > 1) {
                // wait additional thread(s)
                for (i = 0; i < threadsAllocated - 1; i++) {
                    vm_event_wait(&threads[i]->stop_event);
                }
            }
            slice += threadsAllocated;
        }
        // to check in the above:
        // 1. new iteration waits ALL threads
        // 2. why not to add tail processing there
        // 3. Looks like slices are disordered in output

        nSlice = core_enc->m_info.num_slices - nSlice*threadsAllocated;
        if (nSlice > 0) {
            for (i = 0; i < nSlice; i++) {
                threadSpec[i].core_enc = core_enc;
                threadSpec[i].state = state;
                threadSpec[i].slice = slice+i;
                threadSpec[i].slice_qp_delta_default = slice_qp_delta_default;
                threadSpec[i].default_slice_type = default_slice_type;
            }
            if (nSlice > 1) {
                // start additional thread(s)
                for (i = 0; i < nSlice - 1; i++) {
                    vm_event_signal(&threads[i]->start_event);
                }
            }

            ThreadedSliceCompress(0);

            if (nSlice > 1) {
                // wait additional thread(s)
                for (i = 0; i < nSlice - 1; i++) {
                    vm_event_wait(&threads[i]->stop_event);
                }
            }
            slice += nSlice;
        }
        threadsAllocated = realThreadsAllocated;

#else // VM_SLICE_THREADING_H264
        for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++){

#if defined _OPENMP
            vm_set_current_thread_priority(mainTreadPriority);
#endif // _OPENMP
            core_enc->m_Slices[slice].m_slice_qp_delta = (Ipp8s)slice_qp_delta_default;
            core_enc->m_Slices[slice].m_slice_number = slice;
            core_enc->m_Slices[slice].m_slice_type = default_slice_type; // Pass to core encoder
            H264CoreEncoder_UpdateRefPicList_8u16s(state, core_enc->m_Slices + slice, &core_enc->m_pCurrentFrame->m_pRefPicList[slice], core_enc->m_SliceHeader, &core_enc->m_ReorderInfoL0, &core_enc->m_ReorderInfoL1);
            //if (core_enc->m_SliceHeader.MbaffFrameFlag)
            //    H264ENC_MAKE_NAME(H264CoreEncoder_UpdateRefPicList)(state, &core_enc->m_pRefPicList[slice], core_enc->m_SliceHeader, &core_enc->m_ReorderInfoL0, &core_enc->m_ReorderInfoL1);
#ifdef SLICE_CHECK_LIMIT
re_encode_slice:
#endif // SLICE_CHECK_LIMIT
            Ipp32s slice_bits = H264BsBase_GetBsOffset(core_enc->m_Slices[slice].m_pbitstream);
            EnumPicCodType pic_type = INTRAPIC;
            if (core_enc->m_info.rate_controls.method == H264_RCM_VBR_SLICE || core_enc->m_info.rate_controls.method == H264_RCM_CBR_SLICE){
                pic_type = (core_enc->m_Slices[slice].m_slice_type == INTRASLICE) ? INTRAPIC : (core_enc->m_Slices[slice].m_slice_type == PREDSLICE) ? PREDPIC : BPREDPIC;
                core_enc->m_Slices[slice].m_slice_qp_delta = (Ipp8s)(H264_AVBR_GetQP(&core_enc->avbr, pic_type) - core_enc->m_PicParamSet->pic_init_qp);
                core_enc->m_Slices[slice].m_iLastXmittedQP = core_enc->m_PicParamSet->pic_init_qp + core_enc->m_Slices[slice].m_slice_qp_delta;
            }
            // Compress one slice
            if (core_enc->m_is_cur_pic_afrm)
                core_enc->m_Slices[slice].status = H264CoreEncoder_Compress_Slice_MBAFF_8u16s(state, core_enc->m_Slices + slice);
            else {
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
                if (core_enc->useMBT)
                    core_enc->m_Slices[slice].status = H264CoreEncoder_Compress_Slice_MBT_8u16s(state, core_enc->m_Slices + slice);
                else
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT
                core_enc->m_Slices[slice].status = H264CoreEncoder_Compress_Slice_8u16s(state, core_enc->m_Slices + slice, core_enc->m_Slices[slice].m_slice_number == core_enc->m_info.num_slices*core_enc->m_field_index);
#ifdef SLICE_CHECK_LIMIT
                if( core_enc->m_MaxSliceSize){
                    Ipp32s numMBs = core_enc->m_HeightInMBs*core_enc->m_WidthInMBs;
                    numSliceMB += core_enc->m_Slices[slice].m_MB_Counter;
                    dst->SetDataSize(dst->GetDataSize() + H264BsReal_EndOfNAL_8u16s(core_enc->m_Slices[slice].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize(), (ePic_Class != DISPOSABLE_PIC),
#if defined ALPHA_BLENDING_H264
                        (alpha) ? NAL_UT_LAYERNOPART :
#endif // ALPHA_BLENDING_H264
                        ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE), startPicture, 0));
                    if (numSliceMB != numMBs) {
                        core_enc->m_NeedToCheckMBSliceEdges = true;
                        H264BsReal_Reset_8u16s(core_enc->m_Slices[slice].m_pbitstream);
                        core_enc->m_Slices[slice].m_slice_number++;
                        goto re_encode_slice;
                    } else
                        core_enc->m_Slices[slice].m_slice_number = slice;
                }
#endif // SLICE_CHECK_LIMIT
            }
            if (core_enc->m_info.rate_controls.method == H264_RCM_VBR_SLICE || core_enc->m_info.rate_controls.method == H264_RCM_CBR_SLICE)
                H264_AVBR_PostFrame(&core_enc->avbr, pic_type, (Ipp32s)slice_bits);
            slice_bits = H264BsBase_GetBsOffset(core_enc->m_Slices[slice].m_pbitstream) - slice_bits;
#ifdef H264_STAT
            core_enc->hstats.slice_sizes.push_back( slice_bits );
#endif
        }
#endif // #ifdef VM_SLICE_THREADING_H264

#ifdef SLICE_THREADING_LOAD_BALANCING
        if (core_enc->slice_load_balancing_enabled)
        {
            Ipp64u ticks_total = 0;
            for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++)
                ticks_total += core_enc->m_Slices[slice].m_ticks_per_slice;
            if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC) // TODO: use field type instead of frame type if SLB will be enabled for fields
            {
                core_enc->m_B_ticks_data_available = 0;
                core_enc->m_P_ticks_data_available = 0;
                core_enc->m_P_ticks_per_frame = ticks_total;
                core_enc->m_B_ticks_per_frame = ticks_total;
            }
            else if (core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC) // TODO: use field type instead of frame type if SLB will be enabled for fields
            {
                core_enc->m_P_ticks_data_available = 1;
                core_enc->m_P_ticks_per_frame = ticks_total;
            }
            else
            {
                core_enc->m_B_ticks_data_available = 1;
                core_enc->m_B_ticks_per_frame = ticks_total;
            }
        }
#endif // SLICE_THREADING_LOAD_BALANCING

        // reset re-encode indicator before making re-encode decision
        recodePicture = false;

        //Write slice to the stream in order, copy Slice RBSP to the end of the output buffer after adding start codes and SC emulation prevention.
#ifdef SLICE_CHECK_LIMIT
        if( !core_enc->m_MaxSliceSize ){
#endif // SLICE_CHECK_LIMIT
            
#ifdef H264_INSERT_CABAC_ZERO_WORDS
        Ipp64u AvgBinCountPerSlice = 0;
        if (core_enc->m_info.entropy_coding_mode)
        {
            for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++)
                AvgBinCountPerSlice += core_enc->m_Slices[slice].m_pbitstream->m_numBins;
            AvgBinCountPerSlice = (AvgBinCountPerSlice + core_enc->m_info.num_slices - 1) / core_enc->m_info.num_slices;
        }
#endif //H264_INSERT_CABAC_ZERO_WORDS

        for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++){
            size_t oldsize = dst->GetDataSize();

            if (m_temporalLayers.NumLayers && slice != core_enc->m_info.num_slices*core_enc->m_field_index) {
                // write Prefix NAL unit to precede coded slice NALU
                if (ePic_Class != DISPOSABLE_PIC) { // fill prefix_nal_unit_svc syntax for slice of reference picture
                    svc_header.store_ref_base_pic_flag = 0;
                    svc_header.use_ref_base_pic_flag = 0;

                    status = H264BsReal_PutSVCPrefix_8u16s(core_enc->m_bs1, &svc_header);
                    if (status != UMC_OK) goto done;

                    H264BsBase_WriteTrailingBits(&core_enc->m_bs1->m_base);
                }

                dst->SetDataSize(oldsize +
                        H264BsReal_EndOfNAL_8u16s(core_enc->m_bs1,
                                                (Ipp8u*)dst->GetDataPointer() + oldsize,
                                                (ePic_Class != DISPOSABLE_PIC),
                                                NAL_UT_PREFIX,
                                                startPicture,
                                                nal_header_ext));
                oldsize = dst->GetDataSize();
            }

            size_t bufsize = dst->GetBufferSize();
            if(oldsize + (Ipp32s)H264BsBase_GetBsSize(core_enc->m_Slices[slice].m_pbitstream) + 5 /* possible extra bytes */ > bufsize) {
                bufferOverflowFlag = true;
            } else {
                NAL_Unit_Type uUnitType;

                if (core_enc->m_svc_layer.isActive) {
                    if ((core_enc->m_svc_layer.svc_ext.dependency_id != 0) ||
                        (core_enc->m_svc_layer.svc_ext.quality_id != 0))
                        uUnitType = NAL_UT_CODED_SLICE_EXTENSION;
                    else
                        uUnitType = ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE);
                } else {
                    uUnitType = ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE);
                }

                //Write to output bitstream
                dst->SetDataSize(oldsize + H264BsReal_EndOfNAL_8u16s(core_enc->m_Slices[slice].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + oldsize, (ePic_Class != DISPOSABLE_PIC),
#if defined ALPHA_BLENDING_H264
                    (alpha) ? NAL_UT_LAYERNOPART :
#endif // ALPHA_BLENDING_H264
                    uUnitType, startPicture,
                    nal_header_ext));
                
#ifdef H264_INSERT_CABAC_ZERO_WORDS
                if (core_enc->m_info.entropy_coding_mode)
                {
                    Ipp8u* data = (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize();
                    Ipp64u RawMbBits = 3072;//256 * BitDepthY + 2 * MbWidthC * MbHeightC * BitDepthC
                    Ipp64u PicSizeInMbs = core_enc->m_WidthInMBs * core_enc->m_HeightInMBs;
                    Ipp64u nBytes = (dst->GetDataSize() - oldsize);
                    Ipp64u nBytesAvailable = (dst->GetBufferSize() - dst->GetDataSize());

                    while( AvgBinCountPerSlice > (((nBytes - 3 - !slice) * 32 / 3) + (RawMbBits*PicSizeInMbs) / 32 / core_enc->m_info.num_slices) 
                        && nBytesAvailable > 3)
                    {
                        //cabac_zero_word
                        data[0] = 0; 
                        data[1] = 0; 
                        data[2] = 3; 
                        data += 3;
                        nBytes += 3;
                        nBytesAvailable -= 3;
                    }
                    dst->SetDataSize(oldsize + nBytes);
                }
#endif //H264_INSERT_CABAC_ZERO_WORDS
            }
            buffersNotFull = buffersNotFull && H264BsBase_CheckBsLimit(core_enc->m_Slices[slice].m_pbitstream);
        }

        if ((bufferOverflowFlag || !buffersNotFull) && core_enc->m_info.rate_controls.method == H264_RCM_QUANT ) {
            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            goto done;
        }

        if (bufferOverflowFlag){
            if(core_enc->m_Analyse & ANALYSE_RECODE_FRAME) { //Output buffer overflow
                goto recode_check;
            }else{
                status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                goto done;
            }
        }

        Ipp32s nMBCount = 0;

        // check for buffer overrun on some of slices
        if (!buffersNotFull ){
            if( core_enc->m_Analyse & ANALYSE_RECODE_FRAME ){
                goto recode_check;
            }else{
                dst->SetDataSize(picStartDataSize);
                status = H264CoreEncoder_EncodeDummyPicture_8u16s(state);
                if (status == UMC_OK)
                {
                    size_t oldsize = dst->GetDataSize();
                    dst->SetDataSize(oldsize + H264BsReal_EndOfNAL_8u16s(core_enc->m_Slices[core_enc->m_info.num_slices*core_enc->m_field_index].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + oldsize, (core_enc->m_PicClass != DISPOSABLE_PIC),
                        ((core_enc->m_PicClass == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE), startPicture, 0));
                    goto end_of_frame;
                }
                else
                {
                    status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                    goto done;
                }
            }
        }

#ifdef H264_RECODE_PCM
        if(core_enc->m_nPCM > 0 && !core_enc->brc)
        {
            core_enc->m_nPCM = 0;
            dst->SetDataSize(brc_data_size);
            brcRecode = UMC::BRC_RECODE_QP;
            core_enc->recodeFlag = UMC::BRC_RECODE_QP;
            if (ePic_Class == IDR_PIC && (!core_enc->m_field_index)) {
                if (core_enc->m_svc_layer.svc_ext.quality_id == 0) /* first quality layer */
                {
                    core_enc->m_SliceHeader.idr_pic_id--;
                    core_enc->m_SliceHeader.idr_pic_id &= 0xff; //Restrict to 255 to reduce number of bits(max value 65535 in standard)
                }
            }
            core_enc->m_field_index--;
            recodePicture = true;
            recodePCM = true;
            core_enc->m_HeightInMBs <<= (Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); //Do we need it here?
            goto brc_recode;
        }
#endif //H264_RECODE_PCM

#define FRAME_SIZE_RATIO_THRESH_MAX 16.0
#define FRAME_SIZE_RATIO_THRESH_MIN 8.0
#define TOTAL_SIZE_RATIO_THRESH 1.5
        if (!core_enc->brc) // tmp!!!
        if (core_enc->m_info.rate_controls.method == H264_RCM_VBR || core_enc->m_info.rate_controls.method == H264_RCM_CBR || core_enc->m_info.rate_controls.method == H264_RCM_AVBR) {
            bitsPerFrame = (Ipp32s)((dst->GetDataSize() - brc_data_size) << 3);
            /*
            if (core_enc->m_Analyse & ANALYSE_RECODE_FRAME) {
                Ipp64s totEncoded, totTarget;
                totEncoded = core_enc->avbr.mBitsEncodedTotal + bitsPerFrame;
                totTarget = core_enc->avbr.mBitsDesiredTotal + core_enc->avbr.mBitsDesiredFrame;
                Ipp64f thratio, thratio_tot = ((Ipp64f)totEncoded - totTarget) / totTarget;
                thratio = FRAME_SIZE_RATIO_THRESH_MAX * (1.0 - thratio_tot);
                h264_Clip(thratio, FRAME_SIZE_RATIO_THRESH_MIN, FRAME_SIZE_RATIO_THRESH_MAX);
                if (bitsPerFrame > core_enc->avbr.mBitsDesiredFrame * thratio) {
                  Ipp32s qp = H264_AVBR_GetQP(&core_enc->avbr, core_enc->m_PicType);
                  if (qp < 51) {
                    Ipp32s qp_new = (Ipp32s)(qp * sqrt((Ipp64f)bitsPerFrame / (thratio * core_enc->avbr.mBitsDesiredFrame)));
                    if (qp_new <= qp)
                      qp_new++;
                    h264_Clip(qp_new, 1, 51);
                    H264_AVBR_SetQP(&core_enc->avbr, core_enc->m_PicType, qp_new);
                    brcRecode = 1;
                    goto recode_check;
                  }
                }
            }
            */
            H264_AVBR_PostFrame(&core_enc->avbr, core_enc->m_PicType, bitsPerFrame);
        }
        if (core_enc->brc) {
            FrameType picType = (core_enc->m_PicType == INTRAPIC ? I_PICTURE : (core_enc->m_PicType == PREDPIC ? P_PICTURE : B_PICTURE));
            BRCStatus hrdSts;
            Ipp32s curr_qp = core_enc->brc->GetQP(picType);
            bitsPerFrame = (Ipp32s)((dst->GetDataSize() - brc_data_size) << 3);
            hrdSts = core_enc->brc->PostPackFrame(picType, bitsPerFrame, payloadBits, brcRecode, core_enc->m_pCurrentFrame->m_PicOrderCnt[core_enc->m_field_index]);
            if (m_ConstQP)
            {
                core_enc->brc->SetQP(m_ConstQP, I_PICTURE);
                core_enc->brc->SetQP(m_ConstQP, P_PICTURE);
                core_enc->brc->SetQP(m_ConstQP, B_PICTURE);
                hrdSts = BRC_OK;
            }
            
#ifdef H264_RECODE_PCM
            if(core_enc->m_nPCM > 0)
            {
                core_enc->m_nPCM = 0;
                if(hrdSts == BRC_OK)
                {
                    hrdSts = BRC_ERR_BIG_FRAME;
                    recodePCM = true;
                }
            }
#endif //H264_RECODE_PCM

            if (BRC_OK != hrdSts && (core_enc->m_Analyse & ANALYSE_RECODE_FRAME)) {
                Ipp32s qp = core_enc->brc->GetQP(picType);
                bool norecode = false;

                if (!(hrdSts & BRC_NOT_ENOUGH_BUFFER))
                {
                    // to avoid infinite loop: --QP -> BIG_FRAME -> ++QP -> SMALL_FRAME etc.
                    if (hrdSts & BRC_ERR_SMALL_FRAME)
                    {
                        norecode = (small_frame_qp == curr_qp);
                        small_frame_qp = small_frame_qp ? IPP_MIN(small_frame_qp, curr_qp) : curr_qp;
                    } 
                    else if (small_frame_qp && small_frame_qp < qp && (hrdSts & BRC_ERR_BIG_FRAME))
                    {
                        qp = small_frame_qp - (small_frame_qp - curr_qp) / 2;
                        core_enc->brc->SetQP(qp, picType);
                    }
                }

                if ((hrdSts & BRC_ERR_SMALL_FRAME) && qp < min_qp) { // min_qp set in recode_check or = 0
                    // to avoid infinite loop: brc_recode(--QP) -> overflow -> recode_check(++QP) etc.
                    hrdSts |= BRC_NOT_ENOUGH_BUFFER; 
                }
                min_qp = 0;

                if (!(hrdSts & BRC_NOT_ENOUGH_BUFFER) && !norecode) {
                    dst->SetDataSize(brc_data_size);
                    brcRecode = UMC::BRC_RECODE_QP;
                    core_enc->recodeFlag = UMC::BRC_RECODE_QP;
                    if (ePic_Class == IDR_PIC && (!core_enc->m_field_index)) {
                        if (core_enc->m_svc_layer.svc_ext.quality_id == 0) /* first quality layer */
                        {
                            core_enc->m_SliceHeader.idr_pic_id--;
                            core_enc->m_SliceHeader.idr_pic_id &= 0xff; //Restrict to 255 to reduce number of bits(max value 65535 in standard)
                        }
                    }
                    core_enc->m_field_index--;
                    recodePicture = true;
                    core_enc->m_HeightInMBs <<= (Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); //Do we need it here?
                    goto brc_recode;
                } else {
                    if (hrdSts & BRC_ERR_SMALL_FRAME) {
                        Ipp32s maxSize, minSize, bitsize = bitsPerFrame;
                        size_t oldsize = dst->GetDataSize();
                        Ipp8u *p = (Ipp8u*)dst->GetDataPointer() + oldsize;
                        core_enc->brc->GetMinMaxFrameSize(&minSize, &maxSize);
                        if (minSize >  ((Ipp32s)dst->GetBufferSize() << 3)) {
                            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                            goto done;
                        }
                        while (bitsize < minSize - 32) {
                            *(Ipp32u*)p = 0;
                            p += 4;
                            bitsize += 32;
                        }
                        while (bitsize < minSize) {
                            *p = 0;
                            p++;
                            bitsize += 8;
                        }
                        core_enc->brc->PostPackFrame(picType, bitsize, payloadBits + bitsize - bitsPerFrame, 1, core_enc->m_pCurrentFrame->m_PicOrderCnt[core_enc->m_field_index]);
                        dst->SetDataSize(brc_data_size + (bitsize >> 3));
                        brcRecode = 0;
                        core_enc->recodeFlag = 0;
//                           status = UMC::UMC_OK;
                    } else {
                        if (core_enc->m_svc_layer.isActive) {
                            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                            goto done;
                        }
                        dst->SetDataSize(picStartDataSize);
                        status = H264CoreEncoder_EncodeDummyPicture_8u16s(state);
                        if (status != UMC_OK) {
                            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                            goto done;
                        } else {
                            size_t oldsize = dst->GetDataSize();
                            dst->SetDataSize(oldsize + H264BsReal_EndOfNAL_8u16s(core_enc->m_Slices[core_enc->m_info.num_slices*core_enc->m_field_index].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + oldsize, (core_enc->m_PicClass != DISPOSABLE_PIC),
                              ((core_enc->m_PicClass == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE), startPicture, 0));
                        }
                        bitsPerFrame = (Ipp32s)((dst->GetDataSize() - brc_data_size) << 3);
                        hrdSts = core_enc->brc->PostPackFrame(picType, bitsPerFrame, payloadBits, 2, core_enc->m_pCurrentFrame->m_PicOrderCnt[core_enc->m_field_index]);
                        if (hrdSts == BRC_OK)
                        {
                            brcRecode = 0;
                            core_enc->recodeFlag = 0;
                            goto end_of_frame;
                        }
                        else
                        {
                            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                            goto done;
                        }
                    }
                }
            } else {
              brcRecode = 0;
              core_enc->recodeFlag = 0;
            }
        }

        // No brcRecode further, update recode-preserved data
        nMBCount = core_enc->m_WidthInMBs * core_enc->m_HeightInMBs * ((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1);
        for (Ipp32s mb = 0; mb < nMBCount; mb++) {
            H264MacroblockLocalInfo& mbli = core_enc->m_mbinfo.mbs[mb];
//            mbli.lumaQP_ref = getLumaQP(mbli.lumaQP_ref_new, core_enc->m_PicParamSet->bit_depth_luma);
//            mbli.chromaQP_ref = getChromaQP(mbli.lumaQP_ref_new, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
            mbli.lumaQP_ref = mbli.lumaQP_ref_new;
            mbli.chromaQP_ref = mbli.chromaQP_ref_new;
        }


#ifdef SLICE_CHECK_LIMIT
        }
#endif // SLICE_CHECK_LIMIT
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
//        if (!core_enc->useMBT)
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT
        //Deblocking all frame/field
#ifdef DEBLOCK_THREADING
        if( ePic_Class != DISPOSABLE_PIC ){
            //Current we assume the same slice type for all slices
            H264CoreEncoder_DeblockingFunction pDeblocking = NULL;
            switch (default_slice_type){
                case INTRASLICE:
                    pDeblocking = &H264CoreEncoder_DeblockMacroblockISlice_8u16s;
                    break;
                case PREDSLICE:
                    pDeblocking = &H264CoreEncoder_DeblockMacroblockPSlice_8u16s;
                    break;
                case BPREDSLICE:
                    pDeblocking = &H264CoreEncoder_DeblockMacroblockBSlice_8u16s;
                    break;
                default:
                    pDeblocking = NULL;
                    break;
            }

            Ipp32s mbstr;
            //Reset table
#pragma omp parallel for private(mbstr)
            for( mbstr=0; mbstr<core_enc->m_HeightInMBs; mbstr++ ){
                memset( (void*)(core_enc->mbs_deblock_ready + mbstr*core_enc->m_WidthInMBs), 0, sizeof(Ipp8u)*core_enc->m_WidthInMBs);
            }
#pragma omp parallel for private(mbstr) schedule(static,1)
            for( mbstr=0; mbstr<core_enc->m_HeightInMBs; mbstr++ ){
                //Lock string
                Ipp32s first_mb = mbstr*core_enc->m_WidthInMBs;
                Ipp32s last_mb = first_mb + core_enc->m_WidthInMBs;
                for( Ipp32s mb=first_mb; mb<last_mb; mb++ ){
                    //Check up mb is ready
                    Ipp32s upmb = mb - core_enc->m_WidthInMBs + 1;
                    if( mb == last_mb-1 ) upmb = mb - core_enc->m_WidthInMBs;
                    if( upmb >= 0 )
                        while( *(core_enc->mbs_deblock_ready + upmb) == 0 );
                     (*pDeblocking)(state, mb);
                     //Unlock ready MB
                     *(core_enc->mbs_deblock_ready + mb) = 1;
                }
            }
        }
#else
        if (core_enc->m_svc_layer.svc_ext.quality_id == core_enc->QualityNum-1) {
            if (core_enc->m_svc_layer.isActive) {
                H264EncoderFrame_8u16s *srcframe = core_enc->m_pReconstructFrame;
                H264EncoderFrame_8u16s *dstframe = core_enc->m_pCurrentFrame;


                // save not deblocked surface for future IL-prediction
                assert(srcframe->m_pitchBytes == dstframe->m_pitchBytes);
                assert(srcframe->uHeight == dstframe->uHeight);
                MFX_INTERNAL_CPY(dstframe->m_pYPlane, srcframe->m_pYPlane, dstframe->uHeight*dstframe->m_pitchBytes);
                MFX_INTERNAL_CPY(dstframe->m_pUPlane, srcframe->m_pUPlane, dstframe->uHeight/2*dstframe->m_pitchBytes);
            }

            core_enc->m_deblocking_IL = 0;
            for (core_enc->m_deblocking_stage2 = 0; core_enc->m_deblocking_stage2 < 2; core_enc->m_deblocking_stage2++) {
                for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++){
                    if (core_enc->m_Slices[slice].status != UMC_OK){
                        // It is unreachable in the current implementation, so there is no problem!!!
                        core_enc->m_bMakeNextFrameKey = true;
                        VM_ASSERT(0);// goto done;
#ifdef SVC_DUMP_RECON
                    // deblock B frames when dumping to match decoder
                    }else if( !(core_enc->m_Analyse_ex & ANALYSE_DEBLOCKING_OFF)){
#else
                    }else if(ePic_Class != DISPOSABLE_PIC && !(core_enc->m_Analyse_ex & ANALYSE_DEBLOCKING_OFF)){
#endif
                        H264CoreEncoder_DeblockSlice_8u16s(state, core_enc->m_Slices + slice, core_enc->m_Slices[slice].m_first_mb_in_slice + core_enc->m_WidthInMBs*core_enc->m_HeightInMBs*core_enc->m_field_index, core_enc->m_Slices[slice].m_MB_Counter);
                    }
                }
                slice--;
                Ipp32s deblocking_idc = core_enc->m_Slices[slice].m_disable_deblocking_filter_idc;
                if (deblocking_idc != DEBLOCK_FILTER_ON_2_PASS &&
                    deblocking_idc != DEBLOCK_FILTER_ON_2_PASS_NO_CHROMA)
                    break;
            }
        }
#endif
end_of_frame:
        if (ePic_Class != DISPOSABLE_PIC)
            H264CoreEncoder_End_Picture_8u16s(state);
        core_enc->m_HeightInMBs <<= (Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); //Do we need it here?
#if defined ALPHA_BLENDING_H264
        } while(!alpha && core_enc->m_SeqParamSet->aux_format_idc );
        if(alpha){
//                core_enc->m_pCurrentFrame->usePrimary();
            H264EncoderFrameList_switchToPrimary_8u16s(&core_enc->m_cpb);
            H264EncoderFrameList_switchToPrimary_8u16s(&core_enc->m_dpb);
        }
#endif // ALPHA_BLENDING_H264
        //for(slice = 0; slice < core_enc->m_info.num_slices*((core_enc->m_info.coding_type == 1) + 1); slice++)  //TODO fix for PicAFF/AFRM
        //    bitsPerFrame += core_enc->m_pbitstreams[slice]->GetBsOffset();
        if (core_enc->m_svc_layer.svc_ext.quality_id == core_enc->QualityNum-1) /* last quality layer */
        {

            if (ePic_Class != DISPOSABLE_PIC) {
                H264CoreEncoder_UpdateRefPicMarking_8u16s(state);

                if (core_enc->m_info.coding_type == 1 || (core_enc->m_info.coding_type == 3 && core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE))
                {
                    if (core_enc->m_field_index == 0)
                    {
                        // reconstructed first field can be used as reference for the second
                        // to avoid copy replace m_pCurrentFrame in dpb with it's copy pointing to reconstructed planes
                        tempRefFrame = *core_enc->m_pCurrentFrame;

                        tempRefFrame.m_pYPlane = core_enc->m_pReconstructFrame->m_pYPlane;
                        tempRefFrame.m_pUPlane = core_enc->m_pReconstructFrame->m_pUPlane;
                        tempRefFrame.m_pVPlane = core_enc->m_pReconstructFrame->m_pVPlane;

                        H264EncoderFrameList_RemoveFrame_8u16s(&core_enc->m_dpb, core_enc->m_pCurrentFrame);
                        H264EncoderFrameList_insertAtCurrent_8u16s(&core_enc->m_dpb, &tempRefFrame);
                    }
                    else
                    {
                        // return m_pCurrentFrame to it's place in dpb
                        H264EncoderFrameList_RemoveFrame_8u16s(&core_enc->m_dpb, &tempRefFrame);
                        H264EncoderFrameList_insertAtCurrent_8u16s(&core_enc->m_dpb, core_enc->m_pCurrentFrame);
                    }
                }
            }
        }

#if 0
    fprintf(stderr, "%c\t%d\t%d\n", (ePictureType == INTRAPIC) ? 'I' : (ePictureType == PREDPIC) ? 'P' : 'B', (Ipp32s)core_enc->m_total_bits_encoded, H264_AVBR_GetQP(&core_enc->avbr, core_enc->m_PicType));
#endif
recode_check:
        if(core_enc->m_Analyse & ANALYSE_RECODE_FRAME && (bufferOverflowFlag || !buffersNotFull)) { //check frame size to be less than output buffer otherwise change qp and recode frame
            Ipp32s qp;
            if (core_enc->brc)
                qp = core_enc->brc->GetQP(core_enc->m_PicType == INTRAPIC ? I_PICTURE : (core_enc->m_PicType == PREDPIC ? P_PICTURE : B_PICTURE));
            else
                qp = H264_AVBR_GetQP(&core_enc->avbr, core_enc->m_PicType);
            qp++;
            min_qp = qp; // set min frame QP value before buffer overflow
            if( qp == 51 ){
                status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                goto done;
            }
            if (core_enc->brc)
                core_enc->brc->SetQP(qp, core_enc->m_PicType == INTRAPIC ? I_PICTURE : (core_enc->m_PicType == PREDPIC ? P_PICTURE : B_PICTURE));
            else
                H264_AVBR_SetQP(&core_enc->avbr, core_enc->m_PicType, ++qp);
            bufferOverflowFlag = false;
            buffersNotFull = true;
            //            if (ePic_Class == IDR_PIC && (!core_enc->m_field_index)) {
            if (ePic_Class == IDR_PIC && (!core_enc->m_field_index)) {
                core_enc->m_SliceHeader.idr_pic_id--;
                core_enc->m_SliceHeader.idr_pic_id &= 0xff; //Restrict to 255 to reduce number of bits(max value 65535 in standard)
            }
            core_enc->m_field_index--;
            recodePicture = true;
            recodePCM     = false;
            core_enc->m_HeightInMBs <<= (Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); //Do we need it here?
            dst->SetDataSize(brc_data_size);
        }
brc_recode: ;
    } // end of field loop

    bitsPerFrame = (Ipp32s)((dst_main->GetDataSize() - data_size) << 3);
    if (dst_ext)
        bitsPerFrame += (Ipp32s)dst_ext->GetDataSize() << 3;
    core_enc->m_total_bits_encoded += bitsPerFrame;


    if (m_svc_count > 0 && core_enc->m_SeqParamSet->svc_vui_parameters_present_flag && core_enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag) {
        Ipp32u did = core_enc->m_svc_layer.svc_ext.dependency_id;
        Ipp32u qid = core_enc->m_svc_layer.svc_ext.quality_id;
        Ipp32u tid = core_enc->m_pCurrentFrame->m_temporalId;
        Ipp32s frbytes = bitsPerFrame >> 3;
        Ipp32u i, j, k;

        if ((did | qid) == 0) {
            for (k = 0; k < m_svc_count; k++)
                for (j = 0; j < m_svc_layers[k]->enc->QualityNum; j++)
                    m_svc_layers[k]->m_representationFrameSize[j][tid] = frbytes;
        } else {
            for (j = qid; j < core_enc->QualityNum; j++)
                m_svc_layers[did]->m_representationFrameSize[j][tid] += frbytes;
            for (k = did + 1; k < m_svc_count; k++)
                for (j = 0; j < m_svc_layers[k]->enc->QualityNum; j++)
                    m_svc_layers[k]->m_representationFrameSize[j][tid] += frbytes;
        }

        for (i = tid; i < (Ipp32u)(core_enc->TempIdMax + 1); i++) {
            SVCVUIParams *vui = &core_enc->m_SeqParamSet->svc_vui_parameters[i+qid*(core_enc->TempIdMax+1)];
            if (vui->vui_ext_nal_hrd_parameters_present_flag) {
                Ipp32u maxBitrate = (vui->nal_hrd_parameters.bit_rate_value_minus1[0] + 1) << (vui->nal_hrd_parameters.bit_rate_scale + 3); // 3 not 6 cause bits->bytes
                Ipp32u hrdBufferSize = (vui->nal_hrd_parameters.cpb_size_value_minus1[0] + 1) << (vui->nal_hrd_parameters.cpb_size_scale + 1);
                m_svc_layers[did]->m_representationBF[qid][i] += (Ipp64s)maxBitrate * vui->vui_ext_num_units_in_tick;
                m_svc_layers[did]->m_representationBF[qid][i] -= (Ipp64s)m_svc_layers[did]->m_representationFrameSize[qid][tid] * (vui->vui_ext_time_scale >> 1);

                if ((!vui->nal_hrd_parameters.cbr_flag[0]) && (m_svc_layers[did]->m_representationBF[qid][tid] > (Ipp64s)hrdBufferSize * (vui->vui_ext_time_scale >> 1)))
                    m_svc_layers[did]->m_representationBF[qid][i] = (Ipp64s)hrdBufferSize * (vui->vui_ext_time_scale >> 1);
            }
        }


/*
        for (i = tid; i <= core_enc->TempIdMax; i++) {
            for (j = qid; j < core_enc->QualityNum; j++) {
                m_svc_layers[did]->m_representationFrameSize[j][i] += frbytes;
            }
        }

        for (k = did + 1; k < m_svc_count; k++) {
            for (i = tid; i <= m_svc_layers[k]->enc->TempIdMax; i++) {
                for (j = 0; j < m_svc_layers[k]->enc->QualityNum; j++) {
                    m_svc_layers[k]->m_representationFrameSize[j][i] += frbytes;
                }
            }
        }

        SVCVUIParams *vui = &core_enc->m_SeqParamSet->svc_vui_parameters[tid+qid*(core_enc->TempIdMax+1)];
        if (vui->vui_ext_nal_hrd_parameters_present_flag) {
            Ipp32u maxBitrate = (vui->nal_hrd_parameters.bit_rate_value_minus1[0] + 1) << (vui->nal_hrd_parameters.bit_rate_scale + 3);
            Ipp32u hrdBufferSize = (vui->nal_hrd_parameters.cpb_size_value_minus1[0] + 1) << (vui->nal_hrd_parameters.cpb_size_scale + 1);
            m_svc_layers[did]->m_representationBF[qid][tid] += (Ipp64s)maxBitrate * vui->vui_ext_num_units_in_tick;
            m_svc_layers[did]->m_representationBF[qid][tid] -= (Ipp64s)m_svc_layers[did]->m_representationFrameSize[qid][tid] * (vui->vui_ext_time_scale >> 1);

            if ((!vui->nal_hrd_parameters.cbr_flag[0]) && (m_svc_layers[did]->m_representationBF[qid][tid] > (Ipp64s)hrdBufferSize * (vui->vui_ext_time_scale >> 1)))
                m_svc_layers[did]->m_representationBF[qid][tid] = (Ipp64s)hrdBufferSize * (vui->vui_ext_time_scale >> 1);
        }
*/
    }

#ifdef SVC_DUMP_RECON
    if (core_enc->m_svc_layer.svc_ext.quality_id == core_enc->QualityNum-1) /* last quality layer */
    {
        char fname[128];
        mfxU8 buf[2048];
        FILE *f;
        Ipp32s W = core_enc->m_WidthInMBs*16;
        Ipp32s H = core_enc->m_HeightInMBs*16;
        mfxU8* fbuf = 0;
        sprintf(fname, "rec%dx%d_l%d_q%d.yuv", W, H, core_enc->m_svc_layer.svc_ext.dependency_id, core_enc->m_svc_layer.svc_ext.quality_id);
        Ipp32s numlater = 0; // number of dumped frames with later POC
        if (core_enc->m_PicType == BPREDPIC) {
            H264EncoderFrame_8u16s *pFrm;
            for (pFrm = core_enc->m_dpb.m_pHead; pFrm; pFrm = pFrm->m_pFutureFrame) {
                if (pFrm->m_wasEncoded && core_enc->m_pCurrentFrame->m_FrameDisp < pFrm->m_FrameDisp)
                    numlater++;
            }
        }
        if ( numlater ) { // simple reorder for B: read last ref, replacing with B, write ref
            fbuf = new mfxU8[numlater*W*H*3/2];
            f = fopen(fname,"r+b");
            fseek(f, -numlater*W*H*3/2, SEEK_END);
            fread(fbuf, 1, numlater*W*H*3/2, f);
            //fclose(f);
            //f = fopen(fname,"r+b");
            fseek(f, -numlater*W*H*3/2, SEEK_END);
        } else
            f = fopen(fname, core_enc->m_uFrames_Num ? "a+b" : "wb");

        int i,j;
        mfxU8 *p = core_enc->m_pReconstructFrame->m_pYPlane;
        for (i = 0; i < H; i++) {
            fwrite(p, 1, W, f);
            p += core_enc->m_pReconstructFrame->m_pitchBytes;
        }
        p = core_enc->m_pReconstructFrame->m_pUPlane;
        for (i = 0; i < H >> 1; i++) {
            for (j = 0; j < W >> 1; j++) {
                buf[j] = p[2*j];
            }
            fwrite(buf, 1, W >> 1, f);
            p += core_enc->m_pReconstructFrame->m_pitchBytes;
        }
        p = core_enc->m_pReconstructFrame->m_pVPlane;
        for (i = 0; i < H >> 1; i++) {
            for (j = 0; j < W >> 1; j++) {
                buf[j] = p[2*j];
            }
            fwrite(buf, 1, W >> 1, f);
            p += core_enc->m_pReconstructFrame->m_pitchBytes;
        }
        if (fbuf) {
            fwrite(fbuf, 1, numlater*W*H*3/2, f);
            delete[] fbuf;
        }
        fclose(f);
    }
#endif

    if (core_enc->m_svc_layer.svc_ext.quality_id == core_enc->QualityNum-1)
    {
        if( core_enc->m_Analyse & ANALYSE_RECODE_FRAME ){
            H264EncoderFrame_exchangeFrameYUVPointers_8u16s(core_enc->m_pReconstructFrame, core_enc->m_pCurrentFrame);
            core_enc->m_pReconstructFrame = NULL;
        }
    }
    if (dst->GetDataSize() == 0) {
        core_enc->m_bMakeNextFrameKey = true;
        status = UMC_ERR_FAILED;
        goto done;
    }
#ifdef H264_STAT
    core_enc->hstats.addFrame( ePictureType, dst->GetDataSize());
#endif
    if (core_enc->m_svc_layer.svc_ext.quality_id == core_enc->QualityNum-1)
        core_enc->m_uFrames_Num++;
#ifdef H264_INTRA_REFRESH
    if (core_enc->m_refrType) {
        Ipp32s lastMBLinePlus1 = core_enc->m_refrType == HORIZ_REFRESH ? core_enc->m_HeightInMBs : core_enc->m_WidthInMBs;
        if ((core_enc->m_firstLineInStripe + core_enc->m_stripeWidth) >= lastMBLinePlus1) {
            core_enc->m_firstLineInStripe = -core_enc->m_refrSpeed;
            m_base.enc->m_periodCount ++;
            m_base.enc->m_stopFramesCount = 0;
            core_enc->m_pCurrentFrame->m_isRefreshed = true;
        }
        if (m_base.enc->m_stopFramesCount ++ >= core_enc->m_stopRefreshFrames)
            core_enc->m_firstLineInStripe = (core_enc->m_firstLineInStripe + core_enc->m_refrSpeed) % lastMBLinePlus1;
    }
#endif // H264_INTRA_REFRESH
done:
#ifdef H264_HRD_CPB_MODEL
    core_enc->m_SEIData.m_recentFrameEncodedSize = bitsPerFrame;
#endif // H264_HRD_CPB_MODEL
    H264CoreEncoder_updateSEIData( core_enc );
    return status;
}

#define PUT_SEI_MESSAGES_TO_NALU \
    H264BsBase_WriteTrailingBits(&bs->m_base);\
    oldsize = dst->GetDataSize();\
    dst->SetDataSize( oldsize + H264BsReal_EndOfNAL_8u16s( bs, (Ipp8u*)dst->GetDataPointer() + oldsize, 0, NAL_UT_SEI, startPicture, 0));


Status MFXVideoENCODEH264::SVC_encodeBufferingPeriodSEI_inScalableNestingSEI(
    void* state,
    H264BsReal_8u16s* bs,
    MediaData* dst,
    bool& startPicture)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status ps = UMC_OK;
    Ipp32s i, j;
    Ipp32u maxBitrate, hrdBufferSize;
    Ipp32u initial_cpb_removal_delay;

    Ipp8u dependency_id = core_enc->m_svc_layer.svc_ext.dependency_id;
    for (j = 0; j <= core_enc->TempIdMax; j++) {
        for (i = 0; i < core_enc->QualityNum; i++) {
            SVCVUIParams *vui = &core_enc->m_SeqParamSet->svc_vui_parameters[j+i*(core_enc->TempIdMax+1)];
            if (vui->vui_ext_nal_hrd_parameters_present_flag) {
                if (m_layer->m_brc[i]) {
//                    VideoBrcParams brcParams;
//                    m_layer->m_brc[i][j]->GetParams(&brcParams);
//                    maxBitrate = brcParams.maxBitrate;
//                    hrdBufferSize = brcParams.HRDBufferSizeBytes;
                    maxBitrate = (vui->nal_hrd_parameters.bit_rate_value_minus1[0] + 1) << (vui->nal_hrd_parameters.bit_rate_scale + 3);
                    hrdBufferSize = (vui->nal_hrd_parameters.cpb_size_value_minus1[0] + 1) << (vui->nal_hrd_parameters.cpb_size_scale + 1);
                    if (hrdBufferSize == 0 || maxBitrate == 0)
                        continue;
                    core_enc->m_SEIData.BufferingPeriod.seq_parameter_set_id = core_enc->m_SeqParamSet->seq_parameter_set_id;

                    Ipp64u temp1_u64, temp2_u64;
                    temp1_u64 = (Ipp64u)m_svc_layers[dependency_id]->m_representationBF[i][j] * 90000;
                    temp2_u64 = (Ipp64u)maxBitrate * (vui->vui_ext_time_scale >> 1);
                    initial_cpb_removal_delay = (Ipp32u)(temp1_u64/temp2_u64);

//                    m_layer->m_brc[i][j]->GetInitialCPBRemovalDelay(&initial_cpb_removal_delay, recode);

                    core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] = initial_cpb_removal_delay;
                    core_enc->m_SEIData.BufferingPeriod.vcl_initial_cpb_removal_delay[0] = initial_cpb_removal_delay; // ???
                    core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay_offset[0] =
                    core_enc->m_SEIData.BufferingPeriod.vcl_initial_cpb_removal_delay_offset[0] = // ???
                        (Ipp32u)((Ipp64f)(hrdBufferSize) / maxBitrate * 90000 - core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0]);


                    Ipp8u quality_id = (Ipp8u)i;
                    Ipp8u temporal_id = (Ipp8u)j;
                    Status sts;


                    sts = H264BsReal_PutScalableNestingSEI_BufferingPeriod_8u16s(bs,
                                                                                 *core_enc->m_SeqParamSet,
                                                                                 core_enc->m_SEIData.BufferingPeriod,
                                                                                 vui,
                                                                                 0,
                                                                                 1,
                                                                                 &dependency_id,
                                                                                 &quality_id,
                                                                                 temporal_id);
                    if (sts == UMC_OK) {
                        H264BsBase_WriteTrailingBits(&bs->m_base);
                        size_t oldsize = dst->GetDataSize();

                        dst->SetDataSize(oldsize +
                            H264BsReal_EndOfNAL_8u16s(bs, (Ipp8u*)dst->GetDataPointer() + oldsize, 0, NAL_UT_SEI, startPicture, 0));
                    }
                }
            }
        }
    }

//     MFX_INTERNAL_CPY(core_enc->m_SEIData.m_insertedSEI, need_insert, 36 * sizeof(Ipp8u));

    return ps;
}



Status MFXVideoENCODEH264::H264CoreEncoder_encodeSEI(
    void* state,
    H264BsReal_8u16s* bs,
    MediaData* dst,
    bool bIDR_Pic,
    bool& startPicture,
    mfxEncodeCtrl *ctrl,
    Ipp8u recode)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status ps = UMC_OK;

    Ipp8u NalHrdBpPresentFlag, VclHrdBpPresentFlag, CpbDpbDelaysPresentFlag, pic_struct_present_flag;
    Ipp8u sei_inserted = 0, need_insert_external_payload = 0;
    Ipp8u need_insert[36] = {};
    Ipp8u is_cur_pic_field = core_enc->m_info.coding_type == 1 || (core_enc->m_info.coding_type == 3 && core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE);
    Ipp16s GOP_structure_map = -1, Closed_caption = -1;

    size_t oldsize = 0;

#ifdef H264_INTRA_REFRESH
    if (core_enc->m_refrType)
        need_insert[SEI_TYPE_RECOVERY_POINT] = (core_enc->m_firstLineInStripe == 0) && m_base.m_putRecoveryPoint;
    else
#endif // H264_INTRA_REFRESH
    need_insert[SEI_TYPE_RECOVERY_POINT] = 0;
    need_insert[SEI_TYPE_PAN_SCAN_RECT] = 0;
    need_insert[SEI_TYPE_FILLER_PAYLOAD] = 0;
    need_insert[SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35] = 0;

    if (core_enc->m_SeqParamSet->svc_vui_parameters_present_flag || core_enc->m_SeqParamSet->vui_parameters_present_flag)
    {
        if (m_svc_count > 0 && core_enc->m_SeqParamSet->svc_vui_parameters_present_flag) {
            Ipp32u qid = core_enc->m_svc_layer.svc_ext.quality_id;
            Ipp32u tid = core_enc->m_pCurrentFrame->m_temporalId;
            SVCVUIParams *vui = &core_enc->m_SeqParamSet->svc_vui_parameters[tid+qid*(core_enc->TempIdMax+1)];
            NalHrdBpPresentFlag = vui->vui_ext_nal_hrd_parameters_present_flag;
            VclHrdBpPresentFlag = vui->vui_ext_vcl_hrd_parameters_present_flag;
            pic_struct_present_flag = vui->vui_ext_pic_struct_present_flag;
        } else {
            NalHrdBpPresentFlag = core_enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag;
            VclHrdBpPresentFlag = core_enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag;
            pic_struct_present_flag = core_enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag;
        }
    }
    else
    {
        NalHrdBpPresentFlag = 0;
        VclHrdBpPresentFlag = 0;
        pic_struct_present_flag = 0;
    }

    need_insert_external_payload = ctrl && ctrl->Payload && ctrl->NumPayload > 0;

    // if external payload contains Recovery point SEI then encoder should write Buffering period SEI in current AU
    if( need_insert_external_payload ) {
        for( mfxU16 i = core_enc->m_field_index; i<ctrl->NumPayload; i+=is_cur_pic_field ? 2:1 ) {// odd payloads for the 1st field, even for the 2nd
            if (ctrl->Payload[i]->Type == SEI_TYPE_RECOVERY_POINT)
                need_insert[SEI_TYPE_BUFFERING_PERIOD] = 1;
            // check for GOP_structure_map() and cc_data()
            if (ctrl->Payload[i]->Type == SEI_TYPE_USER_DATA_UNREGISTERED) {
                // skip sei_message() payloadType last byte
                Ipp8u *pData = ctrl->Payload[i]->Data + 1;
                // skip payloadSize and uuid_iso_iec_11578
                while (*pData == 0xff)
                    pData ++;
                pData += 17;
                // if sei_message() contains GOP_structure_map()
                if (pData[0] == 0x47 && pData[1] == 0x53 && pData[2] == 0x4d && pData[3] == 0x50)
                    GOP_structure_map = i;
                // if sei_message() contains cc_data()
                if (pData[0] == 0x47 && pData[1] == 0x41 && pData[2] == 0x39 && pData[3] == 0x34)
                    Closed_caption = i;
            }
        }
    }

    CpbDpbDelaysPresentFlag = NalHrdBpPresentFlag || VclHrdBpPresentFlag;

    if (core_enc->m_svc_layer.svc_ext.dependency_id > 0 /* ??? */ || core_enc->QualityNum > 1 || core_enc->TempIdMax > 0) // BP SEI is written in scalable nesting SEI
        need_insert[SEI_TYPE_BUFFERING_PERIOD] = 0;
    else {
        need_insert[SEI_TYPE_BUFFERING_PERIOD] = (need_insert[SEI_TYPE_BUFFERING_PERIOD] || bIDR_Pic 
#ifdef H264_INTRA_REFRESH
            || need_insert[SEI_TYPE_RECOVERY_POINT]
#endif // H264_INTRA_REFRESH
        );
        need_insert[SEI_TYPE_BUFFERING_PERIOD] = (need_insert[SEI_TYPE_BUFFERING_PERIOD] && (CpbDpbDelaysPresentFlag || pic_struct_present_flag));
    }

    if (need_insert[SEI_TYPE_BUFFERING_PERIOD]) {
        if (m_svc_count > 0) { // svc, but only base layer (see above)
            SVCVUIParams *vui = &core_enc->m_SeqParamSet->svc_vui_parameters[0];
            if (CpbDpbDelaysPresentFlag) {
                Ipp32u maxBitrate, hrdBufferSize;
                Ipp32u initial_cpb_removal_delay;
                maxBitrate = (vui->nal_hrd_parameters.bit_rate_value_minus1[0] + 1) << (vui->nal_hrd_parameters.bit_rate_scale + 6);
                hrdBufferSize = (vui->nal_hrd_parameters.cpb_size_value_minus1[0] + 1) << (vui->nal_hrd_parameters.cpb_size_scale + 1);
                if (hrdBufferSize > 0 || maxBitrate > 0) {
                    core_enc->m_SEIData.BufferingPeriod.seq_parameter_set_id = core_enc->m_SeqParamSet->seq_parameter_set_id;

                    Ipp64u temp1_u64, temp2_u64;
                    temp1_u64 = (Ipp64u)m_svc_layers[0]->m_representationBF[0][0] * 90000;
                    temp2_u64 = (Ipp64u)maxBitrate * (vui->vui_ext_time_scale >> 1);
                    initial_cpb_removal_delay = (Ipp32u)(temp1_u64/temp2_u64);


                    core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] = initial_cpb_removal_delay;
                    core_enc->m_SEIData.BufferingPeriod.vcl_initial_cpb_removal_delay[0] = initial_cpb_removal_delay; // ???
                    core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay_offset[0] =
                        core_enc->m_SEIData.BufferingPeriod.vcl_initial_cpb_removal_delay_offset[0] = // ???
                        (Ipp32u)((Ipp64f)(hrdBufferSize << 3) / maxBitrate * 90000 - core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0]);
                } else
                    ps = UMC_ERR_NOT_ENOUGH_DATA;

                if (ps != UMC_OK)
                    return ps;
            }
        } else {
            // put Buffering period SEI
            if (CpbDpbDelaysPresentFlag) {
                VideoBrcParams  brcParams;

                if (core_enc->brc)
                {
                    core_enc->brc->GetParams(&brcParams);

                    core_enc->m_SEIData.BufferingPeriod.seq_parameter_set_id = core_enc->m_SeqParamSet->seq_parameter_set_id;

#ifdef H264_HRD_CPB_MODEL
                    Ipp64f bufFullness;
                    core_enc->brc->GetHRDBufferFullness(&bufFullness, recode);
                    if ( core_enc->m_uFrames_Num == 0 )
                    {
                        if( brcParams.maxBitrate != 0 )
                        {
                            core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] = (Ipp32u)(bufFullness / brcParams.maxBitrate * 90000);
                            core_enc->m_SEIData.tai_n = 0;
                            core_enc->m_SEIData.trn_n = (Ipp64f)core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0]/90000;
                            core_enc->m_SEIData.trn_nb = core_enc->m_SEIData.trn_n;
                            core_enc->m_SEIData.taf_n_minus_1 = 0;
                        }
                        else
                            ps = UMC_ERR_NOT_ENOUGH_DATA;
                    }
                    else
                    {
                        core_enc->m_SEIData.trn_nb = core_enc->m_SEIData.trn_n;
                        //if (core_enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0])
                            core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] = 90000 * (core_enc->m_SEIData.trn_n - core_enc->m_SEIData.taf_n_minus_1);
                        //else
                        //core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] = (90000 * (core_enc->m_SEIData.trn_n - core_enc->m_SEIData.taf_n_minus_1) - 10000) > 10000 ? (90000 * (core_enc->m_SEIData.trn_n - core_enc->m_SEIData.taf_n_minus_1) - 10000) : 90000 * (core_enc->m_SEIData.trn_n - core_enc->m_SEIData.taf_n_minus_1;
                    }
#else // H264_HRD_CPB_MODEL
                    if( brcParams.maxBitrate != 0 ) {
                        Ipp32u initial_cpb_removal_delay;
                        core_enc->brc->GetInitialCPBRemovalDelay(&initial_cpb_removal_delay, recode);
                        core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] =
                        core_enc->m_SEIData.BufferingPeriod.vcl_initial_cpb_removal_delay[0] = initial_cpb_removal_delay;
                    } else
                        ps = UMC_ERR_NOT_ENOUGH_DATA;
#endif // H264_HRD_CPB_MODEL

                    if (brcParams.HRDBufferSizeBytes != 0 &&  brcParams.maxBitrate != 0 )
                        core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay_offset[0] =
                        core_enc->m_SEIData.BufferingPeriod.vcl_initial_cpb_removal_delay_offset[0] =
                        (Ipp32u)((Ipp64f)(brcParams.HRDBufferSizeBytes << 3) / brcParams.maxBitrate * 90000 - core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0]);
                    else
                        ps = UMC_ERR_NOT_ENOUGH_DATA;
                }
                else
                    ps = UMC_ERR_NOT_ENOUGH_DATA;

                if (ps != UMC_OK)
                    return ps;
            }
        }
        ps = H264BsReal_PutSEI_BufferingPeriod_8u16s(
            bs,
            *core_enc->m_SeqParamSet,
            NalHrdBpPresentFlag,
            VclHrdBpPresentFlag,
            core_enc->m_SEIData.BufferingPeriod);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    // insert GOP_structure_map()
    // ignore GOP_structure_map() if it is effective for non I picture or for 2nd field
    if( GOP_structure_map >= 0 && core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC && !core_enc->m_field_index){
        mfxPayload* pl = ctrl->Payload[GOP_structure_map];
        mfxU32 data_bytes = pl->NumBit>>3;
        //Copy data
        for( mfxU32 k=0; k<data_bytes; k++ )
            H264BsReal_PutBits_8u16s(bs, pl->Data[k], 8);
        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    // insert cc_data()
    if( Closed_caption >= 0 ){
        mfxPayload* pl = ctrl->Payload[Closed_caption];
        mfxU32 data_bytes = pl->NumBit>>3;
        //Copy data
        for( mfxU32 k=0; k<data_bytes; k++ )
            H264BsReal_PutBits_8u16s(bs, pl->Data[k], 8);
        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    // put Pic timing SEI

    need_insert[SEI_TYPE_PIC_TIMING] = (CpbDpbDelaysPresentFlag || pic_struct_present_flag);
    if (!need_insert[SEI_TYPE_BUFFERING_PERIOD] && bIDR_Pic)
        if (core_enc->m_svc_layer.svc_ext.dependency_id > 0) // KL for a while
            need_insert[SEI_TYPE_PIC_TIMING] = 0;

    // calculate cpb_removal_delay and dpb_output_delay
    if (is_cur_pic_field && core_enc->m_field_index) {
        if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
        {
            core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated += (m_layer->m_frameCount ? core_enc->m_SEIData.PictureTiming.cpb_removal_delay : 0);
            core_enc->m_SEIData.PictureTiming.cpb_removal_delay = 1;
        }
        else
            core_enc->m_SEIData.PictureTiming.cpb_removal_delay ++;
    }

    // Delay for DBP output time of 1st and subsequent AUs should be used in some cases:
    // 1) no delay for encoding with only I and P frames
    // 1) delay = 2 for encoding with I, P and non-reference B frames
    // 2) delay = 4 for encoding with I, P and reference B frames
    if (need_insert[SEI_TYPE_BUFFERING_PERIOD])
        core_enc->m_SEIData.PictureTiming.initial_dpb_output_delay = 2 * GetNumReorderFrames(core_enc->m_info.B_frame_rate, !!core_enc->m_info.treat_B_as_reference);

    core_enc->m_SEIData.PictureTiming.dpb_output_delay = core_enc->m_SEIData.PictureTiming.initial_dpb_output_delay +
        /*core_enc->m_PicOrderCnt_Accu*2 +*/ core_enc->m_pCurrentFrame->m_PicOrderCnt[core_enc->m_field_index] -
        (core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated + ((m_layer->m_frameCount || (is_cur_pic_field && core_enc->m_field_index)) ? core_enc->m_SEIData.PictureTiming.cpb_removal_delay : 0));
    if (core_enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag)
        core_enc->m_SEIData.PictureTiming.dpb_output_delay += core_enc->m_pCurrentFrame->m_DeltaTfi;

    if (need_insert[SEI_TYPE_PIC_TIMING])
    {
#if 0 // setting dpb_output_delay for constant GOP structure. Replaced by algo that works for dynamic frame type detection
        if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC)
        {
            if (core_enc->m_uFrames_Num == 0)
                core_enc->m_SEIData.PictureTiming.dpb_output_delay = 2;
            else
            {
                // for not first I-frame dpb_output_delay depends on number of previous B-frames in display order
                // that will follow this I-frame in dec order
                Ipp32s numBFramesBeforeI;
                // count number of such B-frames
                if (core_enc->m_info.key_frame_controls.interval % (core_enc->m_info.B_frame_rate + 1))
                    numBFramesBeforeI = core_enc->m_info.key_frame_controls.interval % (core_enc->m_info.B_frame_rate + 1) - 1;
                else
                    numBFramesBeforeI = core_enc->m_info.B_frame_rate;
                core_enc->m_SEIData.PictureTiming.dpb_output_delay = 2 + 2 * numBFramesBeforeI;
            }
        }
        else if (core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC)
            core_enc->m_SEIData.PictureTiming.dpb_output_delay = core_enc->m_info.B_frame_rate * 2 + 2;
        else
            core_enc->m_SEIData.PictureTiming.dpb_output_delay = 0;
#endif // 0
        mfxU32 ctType = 0; // progressive
        mfxU32 numClockTS = 1;
        if( core_enc->m_SliceHeader.field_pic_flag ){
            if( core_enc->m_SliceHeader.bottom_field_flag ){
                core_enc->m_SEIData.PictureTiming.pic_struct = 2; // bottom field
                ctType = 1; // interlace
            }else{
                core_enc->m_SEIData.PictureTiming.pic_struct = 1; // top field
                ctType = 1; // interlace
            }
        }else
            switch( core_enc->m_pCurrentFrame->m_coding_type ){
            case MFX_PICSTRUCT_PROGRESSIVE:
                core_enc->m_SEIData.PictureTiming.pic_struct = 0;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF: // progressive frame, displayed as field TFF
            case MFX_PICSTRUCT_FIELD_TFF: // interlaced frame (coded as MBAFF), displayed as field TFF
                core_enc->m_SEIData.PictureTiming.pic_struct = 3;
                numClockTS = 2;
                ctType = 1; // interlace
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF: // progressive frame, displayed as field BFF
            case MFX_PICSTRUCT_FIELD_BFF: // interlaced frame (coded as MBAFF), displayed as field BFF
                core_enc->m_SEIData.PictureTiming.pic_struct = 4;
                numClockTS = 2;
                ctType = 1; // interlace
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED:
                core_enc->m_SEIData.PictureTiming.pic_struct = 5;
                numClockTS = 3;
                ctType = 1; // interlace
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_REPEATED:
                core_enc->m_SEIData.PictureTiming.pic_struct = 6;
                numClockTS = 3;
                ctType = 1; // interlace
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_DOUBLING:
                core_enc->m_SEIData.PictureTiming.pic_struct = 7;
                numClockTS = 2;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_TRIPLING:
                core_enc->m_SEIData.PictureTiming.pic_struct = 8;
                numClockTS = 3;
                break;
            default:
                core_enc->m_SEIData.PictureTiming.pic_struct = 0;
                break;
        }

        H264SEIData_PictureTiming curPicTimingData = core_enc->m_SEIData.PictureTiming;

        mfxExtPictureTimingSEI* picTimingSei = 0;

        if (ctrl)
            picTimingSei = (mfxExtPictureTimingSEI*)GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );

        if (picTimingSei) {
            if (curPicTimingData.pic_struct != 1 && curPicTimingData.pic_struct != 2) { // progressive encoding
                for (mfxU32 i = 0; i < numClockTS; i++)
                {
                    curPicTimingData.clock_timestamp_flag[i] = picTimingSei->TimeStamp[i].ClockTimestampFlag;
                    if (curPicTimingData.clock_timestamp_flag[i]) {
                        curPicTimingData.ct_type[i] = picTimingSei->TimeStamp[i].CtType;
                        curPicTimingData.nuit_field_based_flag[i] = picTimingSei->TimeStamp[i].NuitFieldBasedFlag;
                        curPicTimingData.counting_type[i] = picTimingSei->TimeStamp[i].CountingType;
                        curPicTimingData.full_timestamp_flag[i] = picTimingSei->TimeStamp[i].FullTimestampFlag;
                        curPicTimingData.discontinuity_flag[i] = picTimingSei->TimeStamp[i].DiscontinuityFlag;
                        curPicTimingData.cnt_dropped_flag[i] = picTimingSei->TimeStamp[i].CntDroppedFlag;
                        curPicTimingData.n_frames[i] = picTimingSei->TimeStamp[i].NFrames;
                        curPicTimingData.seconds_flag[i] = picTimingSei->TimeStamp[i].SecondsFlag;
                        curPicTimingData.seconds_value[i] = picTimingSei->TimeStamp[i].SecondsValue;
                        curPicTimingData.minutes_flag[i] = picTimingSei->TimeStamp[i].MinutesFlag;
                        curPicTimingData.minutes_value[i] = picTimingSei->TimeStamp[i].MinutesValue;
                        curPicTimingData.hours_flag[i] = picTimingSei->TimeStamp[i].HoursFlag;
                        curPicTimingData.hours_value[i] = picTimingSei->TimeStamp[i].HoursValue;
                        curPicTimingData.time_offset[i] = picTimingSei->TimeStamp[i].TimeOffset;
                    }
                }
            } else { // interlaced encoding
                curPicTimingData.clock_timestamp_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].ClockTimestampFlag;
                if (curPicTimingData.clock_timestamp_flag[0]) {
                    curPicTimingData.ct_type[0] = picTimingSei->TimeStamp[core_enc->m_field_index].CtType;
                    curPicTimingData.nuit_field_based_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].NuitFieldBasedFlag;
                    curPicTimingData.counting_type[0] = picTimingSei->TimeStamp[core_enc->m_field_index].CountingType;
                    curPicTimingData.full_timestamp_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].FullTimestampFlag;
                    curPicTimingData.discontinuity_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].DiscontinuityFlag;
                    curPicTimingData.cnt_dropped_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].CntDroppedFlag;
                    curPicTimingData.n_frames[0] = picTimingSei->TimeStamp[core_enc->m_field_index].NFrames;
                    curPicTimingData.seconds_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].SecondsFlag;
                    curPicTimingData.seconds_value[0] = picTimingSei->TimeStamp[core_enc->m_field_index].SecondsValue;
                    curPicTimingData.minutes_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].MinutesFlag;
                    curPicTimingData.minutes_value[0] = picTimingSei->TimeStamp[core_enc->m_field_index].MinutesValue;
                    curPicTimingData.hours_flag[0] = picTimingSei->TimeStamp[core_enc->m_field_index].HoursFlag;
                    curPicTimingData.hours_value[0] = picTimingSei->TimeStamp[core_enc->m_field_index].HoursValue;
                    curPicTimingData.time_offset[0] = picTimingSei->TimeStamp[core_enc->m_field_index].TimeOffset;
                }
            }
        } else if (m_base.m_putClockTimestamp) {
            for (mfxU32 i = 0; i < numClockTS; i++)
                if (curPicTimingData.clock_timestamp_flag[i] && curPicTimingData.ct_type[i] == 0xffff)
                    curPicTimingData.ct_type[i] = ctType;
        }

        ps = H264BsReal_PutSEI_PictureTiming_8u16s(
            bs,
            *core_enc->m_SeqParamSet,
            CpbDpbDelaysPresentFlag,
            pic_struct_present_flag,
            curPicTimingData);

        if (ps != UMC_OK)
            return ps;

        if (is_cur_pic_field && core_enc->m_field_index) {
            if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
                core_enc->m_SEIData.PictureTiming.cpb_removal_delay = 0; // set to 0 for further incrementing by 2
            else
                core_enc->m_SEIData.PictureTiming.cpb_removal_delay --; // reset cpb_removal_delay for re-encode loop
        }

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    // write external payloads to bit stream
    if( need_insert_external_payload ){
        for( mfxI32 i = core_enc->m_field_index; i<ctrl->NumPayload; i+=is_cur_pic_field ? 2:1 ){ // odd payloads for the 1st field, even for the 2nd
            if (i != GOP_structure_map && i != Closed_caption) {
                mfxPayload* pl = ctrl->Payload[i];
                mfxU32 data_bytes = pl->NumBit>>3;
                //Copy data
                for( mfxU32 k=0; k<data_bytes; k++ )
                    H264BsReal_PutBits_8u16s(bs, pl->Data[k], 8);
                if (m_separateSEI == true) {
                    PUT_SEI_MESSAGES_TO_NALU
                }
            }
        }
        sei_inserted = 1;
    }


    // put Dec ref pic marking repetition SEI

    need_insert[SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION] = m_layer->m_putDecRefPicMarkinfRepSEI && core_enc->m_SEIData.DecRefPicMrkRep.isCoded;

    if (need_insert[SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION]){

        ps = H264BsReal_PutSEI_DecRefPicMrkRep_8u16s(bs, *core_enc->m_SeqParamSet, core_enc->m_SEIData.DecRefPicMrkRep);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }

        core_enc->m_SEIData.DecRefPicMrkRep.isCoded = false;
    }



    if (need_insert[SEI_TYPE_PAN_SCAN_RECT])
    {
        Ipp32u pan_scan_rect_id = 255;
        Ipp8u pan_scan_rect_cancel_flag = 0;
        Ipp8u pan_scan_cnt_minus1 = 0;
        Ipp32s pan_scan_rect_left_offset[] = {core_enc->m_SeqParamSet->frame_crop_left_offset, 0, 0};
        Ipp32s pan_scan_rect_right_offset[] = {core_enc->m_SeqParamSet->frame_crop_right_offset, 0, 0};
        Ipp32s pan_scan_rect_top_offset[] = {core_enc->m_SeqParamSet->frame_crop_top_offset, 0, 0};
        Ipp32s pan_scan_rect_bottom_offset[] = {core_enc->m_SeqParamSet->frame_crop_bottom_offset, 0, 0};
        Ipp32s pan_scan_rect_repetition_period = 1;

        ps = H264BsReal_PutSEI_PanScanRectangle_8u16s(
            bs,
            pan_scan_rect_id,
            pan_scan_rect_cancel_flag,
            pan_scan_cnt_minus1,
            pan_scan_rect_left_offset,
            pan_scan_rect_right_offset,
            pan_scan_rect_top_offset,
            pan_scan_rect_bottom_offset,
            pan_scan_rect_repetition_period);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    if (need_insert[SEI_TYPE_FILLER_PAYLOAD])
    {
        ps = H264BsReal_PutSEI_FillerPayload_8u16s(bs,37);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    if (need_insert[SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35])
    {
        Ipp8u itu_t_t35_country_code = 0xb8; // Russian Federation
        Ipp8u itu_t_t35_country_extension_byte = 0;
        Ipp8u payload = 0xfe;

        ps = H264BsReal_PutSEI_UserDataRegistred_ITU_T_T35_8u16s(
            bs,
            itu_t_t35_country_code,
            itu_t_t35_country_extension_byte,
            &payload,
            1);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

    if (need_insert[SEI_TYPE_RECOVERY_POINT])
    {
#ifdef H264_INTRA_REFRESH
        if(core_enc->m_refrSpeed){
            Ipp16u refrPeriod = core_enc->m_refrType == HORIZ_REFRESH ? core_enc->m_HeightInMBs : core_enc->m_WidthInMBs;
            refrPeriod = (refrPeriod + core_enc->m_refrSpeed - 1) / core_enc->m_refrSpeed;
            ps = H264BsReal_PutSEI_RecoveryPoint_8u16s(bs,refrPeriod - 1,1,0,0);
        } else
#else // H264_INTRA_REFRESH
        ps = H264BsReal_PutSEI_RecoveryPoint_8u16s(bs,4,1,1,2);
#endif // H264_INTRA_REFRESH

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

#if 0 // not to write copyright and preset in bitstream by default
#include <ippversion.h>
    if( core_enc->m_uFrames_Num == 0 ){
        char buf[1024];
        H264EncoderParams* par = &core_enc->m_info;
        Ipp32s len;
#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
#define snprintf _snprintf
#endif
        len = snprintf( buf, 1023, "INTEL H.264 ENCODER: IPP %d.%d.%d : GOP=%d IDR=%d B=%d Bref=%d Nref=%d Ns=%d RC=%d Brate=%d ME=%d Split=%d MEx=%d MEy=%d DirType=%d Q/S=%d OptQ=%d",
            IPP_VERSION_MAJOR, IPP_VERSION_MINOR, IPP_VERSION_BUILD,
            par->key_frame_controls.interval, par->key_frame_controls.idr_interval,
            par->B_frame_rate, par->treat_B_as_reference,
            par->num_ref_frames, par->num_slices,
            par->rate_controls.method, (int)par->info.bitrate,
            par->mv_search_method, par->me_split_mode, par->me_search_x, par->me_search_y,
            par->direct_pred_mode,
            par->m_QualitySpeed, (int)par->quant_opt_level
            );
        ps = H264BsReal_PutSEI_UserDataUnregistred_8u16s( bs, buf, len );

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
        if (m_separateSEI == true) {
            PUT_SEI_MESSAGES_TO_NALU
        }
    }

#endif // no copyright and preset

    if (sei_inserted && m_separateSEI == false)
    {
        H264BsBase_WriteTrailingBits(&bs->m_base);
        size_t oldsize = dst->GetDataSize();

        dst->SetDataSize( oldsize +
            H264BsReal_EndOfNAL_8u16s( bs, (Ipp8u*)dst->GetDataPointer() + oldsize, 0, NAL_UT_SEI, startPicture, 0));
    }

    MFX_INTERNAL_CPY(core_enc->m_SEIData.m_insertedSEI, need_insert, 36 * sizeof(Ipp8u));

    return ps;
}

Status MFXVideoENCODEH264::H264CoreEncoder_updateSEIData( void* state )
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status ps = UMC_OK;

    if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
    {
        core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated += (m_layer->m_frameCount ? core_enc->m_SEIData.PictureTiming.cpb_removal_delay : 0);
        core_enc->m_SEIData.PictureTiming.cpb_removal_delay = 2;
    }
    else
        core_enc->m_SEIData.PictureTiming.cpb_removal_delay += 2;

#ifdef H264_HRD_CPB_MODEL
        if (core_enc->m_SeqParamSet->vui_parameters_present_flag)
        {
            if (core_enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag || core_enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag)
            {
                VideoBrcParams  brcParams;

                if (core_enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0])
                    core_enc->m_SEIData.tai_n = core_enc->m_SEIData.taf_n_minus_1;
                else if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
                {
                    core_enc->m_SEIData.tai_earliest = core_enc->m_SEIData.trn_n -
                        (Ipp64f)core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] / 90000;
                    core_enc->m_SEIData.tai_n = IPP_MAX(core_enc->m_SEIData.taf_n_minus_1,core_enc->m_SEIData.tai_earliest);
                }
                else
                {
                    core_enc->m_SEIData.tai_earliest = core_enc->m_SEIData.trn_n -
                        (Ipp64f)(core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] + core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay_offset[0]) / 90000;
                    core_enc->m_SEIData.tai_n = IPP_MAX(core_enc->m_SEIData.taf_n_minus_1,core_enc->m_SEIData.tai_earliest);
                }

                if (core_enc->brc)
                {
                    core_enc->brc->GetParams(&brcParams);
                    if( brcParams.maxBitrate != 0 )
                        core_enc->m_SEIData.taf_n = core_enc->m_SEIData.tai_n + (Ipp64f)(core_enc->m_SEIData.m_recentFrameEncodedSize) / brcParams.maxBitrate;
                    else
                        ps = UMC_ERR_NOT_ENOUGH_DATA;
                }
                else
                    ps = UMC_ERR_NOT_ENOUGH_DATA;

                core_enc->m_SEIData.taf_n_minus_1 = core_enc->m_SEIData.taf_n;

                core_enc->m_SEIData.trn_n = core_enc->m_SEIData.trn_nb + core_enc->m_SEIData.m_tickDuration * core_enc->m_SEIData.PictureTiming.cpb_removal_delay;
            }
        }
#endif // H264_HRD_CPB_MODEL

    return ps;
}

//

static Ipp32s shiftTable[] =
{
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
};

static inline Ipp32s countShift(Ipp32s src)
{
    Ipp32s shift;

    if (src == 0)
        return 31;

    shift = 0;
    src--;

    if (src >= 32768)
    {
        src >>= 16;
        shift = 16;
    }

    if (src >= 256)
    {
        src >>= 8;
        shift += 8;
    }

    if (src >= 16)
    {
        src >>= 4;
        shift += 4;
    }

    return (31 - (shift + shiftTable[src]));
}

//

static void MFXVideoENCODEH264_CalculateUpsamplingPars(
    Ipp32u refW,
    Ipp32u refH,
    Ipp32u scaledW,
    Ipp32u scaledH,
    Ipp32u phaseX,
    Ipp32u phaseY,
    Ipp32u level_idc,
    IppiUpsampleSVCParams *pars
    ) {
    if (level_idc <= 30) {
        pars->shiftX = pars->shiftY = 16;
    } else {
        pars->shiftX = countShift(refW);
        pars->shiftY = countShift(refH);
    }
    pars->scaleX = ((refW  << pars->shiftX)+(scaledW>>1)) / scaledW;
    pars->scaleY = ((refH << pars->shiftY)+(scaledH>>1)) / scaledH;
    pars->addX = (((refW * (2 + phaseX)) << (pars->shiftX - 2)) + (scaledW >> 1)) / scaledW +
        (1 << (pars->shiftX - 5));
    pars->addY = (((refH * (2 + phaseY)) << (pars->shiftY - 2)) + (scaledH >> 1)) / scaledH +
        (1 << (pars->shiftY - 5));
}

mfxStatus MFXVideoENCODEH264::InitSVCLayer(const mfxExtSVCRateControl* rc, mfxU16 did, h264Layer *layer)
{
    mfxStatus st, QueryStatus;
    UMC::Status status;
    UMC::VideoBrcParams brcParams;
    H264EncoderParams videoParams;
    mfxVideoInternalParam* par = &layer->m_mfxVideoParam;

    if( layer == NULL ) return MFX_ERR_NULL_PTR;
    if (layer->m_Initialized)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    //par->ExtParam = 0;
    //par->NumExtParam = 0;

    mfxExtCodingOption* opts = &layer->m_extOption;
    //mfxExtCodingOption* opts = GetExtCodingOptions( par->ExtParam, par->NumExtParam );
    //if(GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS ))
    //    return MFX_ERR_UNSUPPORTED;
    //if(GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
    //    return MFX_ERR_UNSUPPORTED;
    //if(GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DDI ))
    //    return MFX_ERR_UNSUPPORTED;
    //if(GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC ))
    //    return MFX_ERR_UNSUPPORTED;

    svcLayerInfo* pInterParams = &layer->m_InterLayerParam;
    //if (pInterParams) {
    //    layer->m_InterLayerParam = *pInterParams;
    //} else {
    //    memset(&layer->m_InterLayerParam, 0, sizeof(layer->m_InterLayerParam));
    //}
    par->mfx.FrameInfo.Width  = layer->m_InterLayerParam.Width;
    par->mfx.FrameInfo.Height = layer->m_InterLayerParam.Height;
    par->mfx.FrameInfo.CropX = layer->m_InterLayerParam.CropX;
    par->mfx.FrameInfo.CropY = layer->m_InterLayerParam.CropY;
    par->mfx.FrameInfo.CropW = layer->m_InterLayerParam.CropW;
    par->mfx.FrameInfo.CropH = layer->m_InterLayerParam.CropH;

    if (layer->m_InterLayerParam.GopPicSize)
        par->mfx.GopPicSize = layer->m_InterLayerParam.GopPicSize;
    else if (par->mfx.GopPicSize) {
        par->mfx.GopPicSize /= layer->enc->rateDivider;
        if (par->mfx.GopPicSize == 0)
            par->mfx.GopPicSize = 1; // restore if got zero from nonzero
    }
    if (layer->m_InterLayerParam.GopRefDist)
        par->mfx.GopRefDist = layer->m_InterLayerParam.GopRefDist;
    else if (par->mfx.GopRefDist) {
        par->mfx.GopRefDist /= layer->enc->rateDivider;
        if (par->mfx.GopRefDist == 0)
            par->mfx.GopRefDist = 1; // restore if got zero from nonzero
    }
    par->mfx.GopOptFlag = layer->m_InterLayerParam.GopOptFlag;
    par->mfx.IdrInterval = layer->m_InterLayerParam.IdrInterval;

    if (rc){
        mfxU16 topqid, toptid;
        par->mfx.RateControlMethod = rc->RateControlMethod;
        toptid = pInterParams->TemporalId[pInterParams->TemporalNum - 1];
        topqid = pInterParams->QualityNum - 1;
        for(size_t ii=0; ii<sizeof(rc->Layer)/sizeof(rc->Layer[0]); ii++) {
            if (rc->Layer[ii].DependencyId == did && rc->Layer[ii].QualityId == topqid && rc->Layer[ii].TemporalId == toptid) {
                switch (rc->RateControlMethod) {
                    case MFX_RATECONTROL_CBR:
                    case MFX_RATECONTROL_VBR:
                        par->mfx.TargetKbps       = rc->Layer[ii].CbrVbr.TargetKbps;
                        par->mfx.InitialDelayInKB = rc->Layer[ii].CbrVbr.InitialDelayInKB;
                        par->mfx.BufferSizeInKB   = rc->Layer[ii].CbrVbr.BufferSizeInKB;
                        par->mfx.MaxKbps          = rc->Layer[ii].CbrVbr.MaxKbps;
                        break;
                    case MFX_RATECONTROL_CQP:
                        par->mfx.QPI = rc->Layer[ii].Cqp.QPI;
                        par->mfx.QPP = rc->Layer[ii].Cqp.QPP;
                        par->mfx.QPB = rc->Layer[ii].Cqp.QPB;
                        break;
                    case MFX_RATECONTROL_AVBR:
                        par->mfx.TargetKbps  = rc->Layer[ii].Avbr.TargetKbps;
                        par->mfx.Convergence = rc->Layer[ii].Avbr.Convergence;
                        par->mfx.Accuracy    = rc->Layer[ii].Avbr.Accuracy;
                        break;
                    default:
                        break;
                }
                break;
            }
        } // for in Layer[]
    }
    par->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;

    mfxVideoInternalParam checked, checkedSetByTU = mfxVideoInternalParam();
    mfxExtCodingOption checked_ext;
    mfxExtBuffer *ptr_checked_ext[2] = {0,};
    mfxU16 ext_counter = 0;
    checked = *par;
    //if (opts) {
        checked_ext = *opts;
        ptr_checked_ext[ext_counter++] = &checked_ext.Header;
    //} else {
    //    memset(&checked_ext, 0, sizeof(checked_ext));
    //    checked_ext.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    //    checked_ext.Header.BufferSz = sizeof(checked_ext);
    //}
    checked.ExtParam = ptr_checked_ext;
    checked.NumExtParam = ext_counter;

    st = Query(par, &checked);

    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && st != MFX_WRN_PARTIAL_ACCELERATION) {
        if (st == MFX_ERR_UNSUPPORTED)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
            return st;
    }
    // tmp workaround - to be removed when Query is fixed  // kta
    if (st == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        st = MFX_ERR_NONE;

    QueryStatus = st;

    // workaround for default entropy_coding_mode_flag with opts == 0
    // need to set correct profile for TU 5
    //if (!opts) {
    //    ptr_checked_ext[ext_counter++] = &checked_ext.Header;
    //    checked.NumExtParam = ext_counter;
    //}

    par = &checked; // from now work with fixed copy of input!
    opts = &checked_ext;

    par->SetCalcParams(&checked);
    //optsSP = &checked_extSP;

    // lots of checking may be unneeded, because structures were copied from m_base

    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    memset(&layer->m_auxInput, 0, sizeof(layer->m_auxInput));
    layer->m_useAuxInput = 0;
    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        memset(&layer->m_response, 0, sizeof(layer->m_response));
        request.Info              = par->mfx.FrameInfo;
        request.NumFrameMin       = 1;
        request.NumFrameSuggested = 1;
        request.Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;

        st = m_core->AllocFrames(&request, &layer->m_response);
        MFX_CHECK_STS(st);
        layer->m_useAuxInput = true;

        if (layer->m_response.NumFrameActual < request.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        layer->m_auxInput.Data.MemId = layer->m_response.mids[0];
        layer->m_auxInput.Info = request.Info;
    }

    if( opts != NULL ){
        if( opts->MaxDecFrameBuffering > 0 && opts->MaxDecFrameBuffering < par->mfx.NumRefFrame ){
            par->mfx.NumRefFrame = opts->MaxDecFrameBuffering;
        }
    }

    layer->enc->free_slice_mode_enabled = (par->mfx.NumSlice == 0);
    videoParams.m_Analyse_on = 0;
    videoParams.m_Analyse_restrict = 0;
    st = ConvertVideoParam_H264enc(par, &videoParams); // converts all with preference rules
    MFX_CHECK_STS(st);

    if (par->mfx.NumRefFrame == 0) { // num refs in the case of SVC temporal scalability
        videoParams.num_ref_frames = layer->enc->TempIdMax+1 +
            (videoParams.B_frame_rate>0);
        // add B refs count
        if (videoParams.treat_B_as_reference) {
            for(mfxI32 d=2; d<=videoParams.B_frame_rate && d>0; d<<=1) videoParams.num_ref_frames++;
        }
        // if topmost contains only B - decrease
        if (layer->enc->TempIdMax > 0 &&
            layer->enc->TemporalScaleList[layer->enc->TempIdMax] / layer->enc->TemporalScaleList[layer->enc->TempIdMax - 1] <= 1+videoParams.B_frame_rate)
            videoParams.num_ref_frames --;
    }

    if (videoParams.info.clip_info.width == 0 || videoParams.info.clip_info.height == 0 ||
        par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 ||
        (videoParams.rate_controls.method != H264_RCM_QUANT && videoParams.info.bitrate == 0) || videoParams.info.framerate == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // to set profile/level and related vars
    // first copy fields which could have been changed from TU
    memset(&checkedSetByTU, 0, sizeof(mfxVideoParam));
    if (par->mfx.CodecProfile == MFX_PROFILE_UNKNOWN && videoParams.transform_8x8_mode_flag)
        par->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    if (par->mfx.GopRefDist == 0) {
        checkedSetByTU.mfx.GopRefDist = 1;
        par->mfx.GopRefDist = (mfxU16)(videoParams.B_frame_rate + 1);
    }
    if (par->mfx.NumRefFrame == 0) {
        checkedSetByTU.mfx.NumRefFrame = 1;
        par->mfx.NumRefFrame = (mfxU16)videoParams.num_ref_frames;
    }
    if (checked_ext.CAVLC == MFX_CODINGOPTION_UNKNOWN)
        checked_ext.CAVLC = (mfxU16)(videoParams.entropy_coding_mode ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);

    st = CheckProfileLevelLimits_H264enc(par, false, &checkedSetByTU);
    if (st != MFX_ERR_NONE)
        QueryStatus = st;

    if (par->mfx.CodecProfile > MFX_PROFILE_AVC_BASELINE && par->mfx.CodecProfile != MFX_PROFILE_AVC_SCALABLE_BASELINE &&
        par->mfx.CodecLevel >= MFX_LEVEL_AVC_3)
        videoParams.use_direct_inference = 1;
    // get back mfx params to umc
    videoParams.profile_idc = (UMC::H264_PROFILE_IDC)ConvertCUCProfileToUMC( par->mfx.CodecProfile );
    videoParams.level_idc = ConvertCUCLevelToUMC( par->mfx.CodecLevel );
    videoParams.num_ref_frames = par->mfx.NumRefFrame;
    if (opts && opts->VuiNalHrdParameters == MFX_CODINGOPTION_OFF)
        videoParams.info.bitrate = par->calcParam.TargetKbps * 1000;
    else
        videoParams.info.bitrate = par->calcParam.MaxKbps * 1000;
    if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || checked_ext.FramePicture == MFX_CODINGOPTION_ON)
        videoParams.coding_type = 0;

    // init brc (if not const QP)
    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        mfxU8 qid, tid;
//        mfxU32 ii;
        mfxU32 i;
        UMC::VideoBrcParams tempBrcParams[MAX_TEMP_LEVELS];

        if (layer->enc->TempIdMax > 0) {

            for (qid = 0; qid < layer->enc->QualityNum; qid++) {
                for (tid = 0; tid <= layer->enc->TempIdMax; tid++) {

                    struct LayerHRDParams *lowerHRDParams = 0;
                    if (qid > 0)
                        lowerHRDParams = &m_HRDparams[did][qid-1][tid];
                    else if (did > 0) {
                        if (tid <= m_svc_layers[did-1]->enc->TempIdMax)
                            lowerHRDParams =  &m_HRDparams[did-1][m_svc_layers[did-1]->enc->QualityNum-1][tid];
                    }

                    if (lowerHRDParams) {
                        memset(&par->calcParam, 0, sizeof(par->calcParam));

                        if (m_HRDparams[did][qid][tid].TargetKbps)
                            par->calcParam.TargetKbps       = m_HRDparams[did][qid][tid].TargetKbps - lowerHRDParams->TargetKbps;
                        if (m_HRDparams[did][qid][tid].BufferSizeInKB)
                            par->calcParam.BufferSizeInKB   = m_HRDparams[did][qid][tid].BufferSizeInKB - lowerHRDParams->BufferSizeInKB;
                        if (m_HRDparams[did][qid][tid].InitialDelayInKB)
                            par->calcParam.InitialDelayInKB = m_HRDparams[did][qid][tid].InitialDelayInKB - lowerHRDParams->InitialDelayInKB;
                        if (m_HRDparams[did][qid][tid].MaxKbps)
                            par->calcParam.MaxKbps          = m_HRDparams[did][qid][tid].MaxKbps - lowerHRDParams->MaxKbps;

                    } else {
                        par->calcParam.TargetKbps       = m_HRDparams[did][qid][tid].TargetKbps;
                        par->calcParam.InitialDelayInKB = m_HRDparams[did][qid][tid].InitialDelayInKB;
                        par->calcParam.BufferSizeInKB   = m_HRDparams[did][qid][tid].BufferSizeInKB;
                        par->calcParam.MaxKbps          = m_HRDparams[did][qid][tid].MaxKbps;
                    }

                    par->GetCalcParams(par);

                    st = ConvertVideoParam_Brc(par, &tempBrcParams[tid]);
                    MFX_CHECK_STS(st);

                    // ??? kta
                    tempBrcParams[tid].GOPPicSize = videoParams.key_frame_controls.interval;
                    tempBrcParams[tid].GOPRefDist = videoParams.B_frame_rate + 1;
                    tempBrcParams[tid].profile = videoParams.profile_idc;
                    tempBrcParams[tid].level = videoParams.level_idc;

                    if (tid > 0) {
                        // need frameRateExtN_1 ??? kta
                        mfxU16 scale = layer->enc->TemporalScaleList[tid];
                        mfxU16 scale_1 = layer->enc->TemporalScaleList[tid-1];
                        tempBrcParams[tid].frameRateExtN_1 = tempBrcParams[tid].frameRateExtN * scale_1;
                        tempBrcParams[tid].frameRateExtN *= scale;
                    }
                }
                if (!layer->m_brc[qid])
                    layer->m_brc[qid] = new UMC::SVCBRC;


                status = layer->m_brc[qid]->Init(tempBrcParams, layer->enc->TempIdMax+1);

                st = h264enc_ConvertStatus(status);
                MFX_CHECK_STS(st);

                for (i = 0; i <= layer->enc->TempIdMax; i++)
                    layer->m_representationFrameSize[qid][i] = 0;
            }

        } else {

            for (qid = 0; qid < layer->enc->QualityNum; qid++) {
                
                struct LayerHRDParams *lowerHRDParams = 0;
                if (qid > 0)
                    lowerHRDParams = &m_HRDparams[did][qid-1][0];
                else if (did > 0)
                    lowerHRDParams =  &m_HRDparams[did-1][m_svc_layers[did-1]->enc->QualityNum-1][0];

                if (lowerHRDParams) {
                    memset(&par->calcParam, 0, sizeof(par->calcParam));

                    if (m_HRDparams[did][qid][0].TargetKbps)
                        par->calcParam.TargetKbps       = m_HRDparams[did][qid][0].TargetKbps - lowerHRDParams->TargetKbps;
                    if (m_HRDparams[did][qid][0].BufferSizeInKB)
                        par->calcParam.BufferSizeInKB       = m_HRDparams[did][qid][0].BufferSizeInKB - lowerHRDParams->BufferSizeInKB;
                    if (m_HRDparams[did][qid][0].InitialDelayInKB)
                        par->calcParam.InitialDelayInKB       = m_HRDparams[did][qid][0].InitialDelayInKB - lowerHRDParams->InitialDelayInKB;
                    if (m_HRDparams[did][qid][0].MaxKbps)
                        par->calcParam.MaxKbps       = m_HRDparams[did][qid][0].MaxKbps - lowerHRDParams->MaxKbps;

                } else {
                    par->calcParam.TargetKbps       = m_HRDparams[did][qid][0].TargetKbps;
                    par->calcParam.InitialDelayInKB = m_HRDparams[did][qid][0].InitialDelayInKB;
                    par->calcParam.BufferSizeInKB   = m_HRDparams[did][qid][0].BufferSizeInKB;
                    par->calcParam.MaxKbps          = m_HRDparams[did][qid][0].MaxKbps;
                }

                par->GetCalcParams(par);
                st = ConvertVideoParam_Brc(par, &brcParams);
                MFX_CHECK_STS(st);

                brcParams.GOPPicSize = videoParams.key_frame_controls.interval;
                brcParams.GOPRefDist = videoParams.B_frame_rate + 1;
                brcParams.profile = videoParams.profile_idc;
                brcParams.level = videoParams.level_idc;

                if (!layer->m_brc[qid])
                    layer->m_brc[qid] = new UMC::H264BRC;

                status = layer->m_brc[qid]->Init(&brcParams);
                layer->m_representationFrameSize[qid][0] = 0;

                st = h264enc_ConvertStatus(status);
                MFX_CHECK_STS(st);

            }
        }

        Ipp32u bufSize = 0;
        tid = layer->enc->TempIdMax;
        for (qid = 0; qid < layer->enc->QualityNum; qid++) {
            if (layer->m_brc[qid]) {
                layer->m_brc[qid]->GetParams(&brcParams, tid);
                if ((brcParams.BRCMode == BRC_CBR || brcParams.BRCMode == BRC_VBR) && brcParams.HRDBufferSizeBytes > 0  /* ??? > some reasonable threshold ? */) 
                    bufSize += brcParams.HRDBufferSizeBytes;
                else
                    bufSize += videoParams.info.clip_info.width * videoParams.info.clip_info.height * 3/2;
            } else
                bufSize += videoParams.info.clip_info.width * videoParams.info.clip_info.height * 3/2;
        }
        layer->enc->requiredBsBufferSize = bufSize;
        layer->enc->brc = layer->m_brc[0]; // just to avoid the internal brc initialization in H264CoreEncoder_Init_8u16s() // kta
    }
    else { // const QP
        mfxU8 i;
        mfxU8 qid, tid;
        mfxU32 ii;

        for (ii = 0; ii < rc->NumLayers; ii++) {
            if (rc->Layer[ii].DependencyId == did) {
                qid = rc->Layer[ii].QualityId;
                tid = rc->Layer[ii].TemporalId;
                layer->m_cqp[qid][tid].QPI = rc->Layer[ii].Cqp.QPI;
                layer->m_cqp[qid][tid].QPP = rc->Layer[ii].Cqp.QPP;
                layer->m_cqp[qid][tid].QPB = rc->Layer[ii].Cqp.QPB;

                if (layer->m_cqp[qid][tid].QPI > 51) layer->m_cqp[qid][tid].QPI = 0; // not set / invalid - use mfx.QPI ??? 
                if (layer->m_cqp[qid][tid].QPP > 51) layer->m_cqp[qid][tid].QPP = 0; // not set / invalid - use mfx.QPI ??? 
                if (layer->m_cqp[qid][tid].QPB > 51) layer->m_cqp[qid][tid].QPB = 0; // not set / invalid - use mfx.QPI ??? 
            }
        }

        // should be set to 0 at layer init ???
        for (i = 0; i < layer->enc->QualityNum; i++)
            layer->m_brc[i] = NULL;
        layer->enc->requiredBsBufferSize = layer->enc->QualityNum * videoParams.info.clip_info.width * videoParams.info.clip_info.height * 3/2;
        layer->enc->brc = NULL;
    }
    layer->enc->recodeFlag = 0;


    // Set low initial QP values to prevent BRC panic mode if superfast mode
/*
    {
        mfxU8 i, j;

        for (i = 0; i < layer->enc->QualityNum; i++) {
            for (j = 0; j <= layer->enc->TempIdMax; j++) {
                if(par->mfx.TargetUsage==MFX_TARGETUSAGE_BEST_SPEED && layer->m_brc[i][j]) {
                    if(layer->m_brc[i][j]->GetQP(I_PICTURE) < 30) {
                        layer->m_brc[i][j]->SetQP(30, I_PICTURE); layer->m_brc[i][j]->SetQP(30, B_PICTURE);
                    }
                }
            }
        }
    }
*/
    //Reset frame count
    layer->m_frameCountSync = 0;
    layer->m_frameCountBufferedSync = 0;
    layer->m_frameCount = 0;
    layer->m_frameCountBuffered = 0;
    layer->m_lastIDRframe = 0;
    layer->m_lastIframe = 0;
    layer->m_lastRefFrame = 0;

    if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        videoParams.coding_type = 0; // frames only
    else {
        if( opts && opts->FramePicture== MFX_CODINGOPTION_ON )
            videoParams.coding_type = 2; // MBAFF
        else
            videoParams.coding_type = 3; // PicAFF
    }

    if (videoParams.m_Analyse_on){
        if (videoParams.transform_8x8_mode_flag == 0)
            videoParams.m_Analyse_on &= ~ANALYSE_I_8x8;
        if (videoParams.entropy_coding_mode == 0)
            videoParams.m_Analyse_on &= ~(ANALYSE_RD_OPT | ANALYSE_RD_MODE | ANALYSE_B_RD_OPT);
        if (videoParams.coding_type && videoParams.coding_type != 2) // TODO: remove coding_type != 2 when MBAFF is implemented
            videoParams.m_Analyse_on &= ~ANALYSE_ME_AUTO_DIRECT;
        //if (par->mfx.GopOptFlag & MFX_GOP_STRICT) frame type analysis is always turned off for svc
        videoParams.m_Analyse_on &= ~ANALYSE_FRAME_TYPE;
    }

    layer->m_mfxVideoParam.mfx = par->mfx;
    layer->m_mfxVideoParam.calcParam = par->calcParam;
    layer->m_mfxVideoParam.IOPattern = par->IOPattern;
    layer->m_mfxVideoParam.Protected = 0;
    layer->m_mfxVideoParam.AsyncDepth = par->AsyncDepth;

    // set like in par-files
    if( par->mfx.RateControlMethod == MFX_RATECONTROL_CBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_CBR;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_CQP )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_QUANT;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_AVBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_AVBR;
    else
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_VBR;

    status = H264CoreEncoder_Init_8u16s(layer->enc, &videoParams, m_allocator);

    if (layer->enc->free_slice_mode_enabled)
        layer->m_mfxVideoParam.mfx.NumSlice = 0;

    //Set specific parameters
    layer->enc->m_info.write_access_unit_delimiters = 1;
    if( opts != NULL ){
        if( opts->MaxDecFrameBuffering != MFX_CODINGOPTION_UNKNOWN && opts->MaxDecFrameBuffering <= par->mfx.NumRefFrame){
            layer->enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag = true;
            layer->enc->m_SeqParamSet->vui_parameters.motion_vectors_over_pic_boundaries_flag = 1;
            layer->enc->m_SeqParamSet->vui_parameters.max_bytes_per_pic_denom = 0;
            layer->enc->m_SeqParamSet->vui_parameters.max_bits_per_mb_denom = 0;
            layer->enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_horizontal = 11;
            layer->enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_vertical = 11;
            layer->enc->m_SeqParamSet->vui_parameters.num_reorder_frames = 0;
            layer->enc->m_SeqParamSet->vui_parameters.max_dec_frame_buffering = opts->MaxDecFrameBuffering;
        }
        if( opts->AUDelimiter == MFX_CODINGOPTION_OFF ){
            layer->enc->m_info.write_access_unit_delimiters = 0;
        }

        layer->m_putEOSeq = ( opts->EndOfSequence == MFX_CODINGOPTION_ON );
        layer->m_putEOStream = ( opts->EndOfStream == MFX_CODINGOPTION_ON );
        layer->m_putTimingSEI = ( opts->PicTimingSEI == MFX_CODINGOPTION_ON );
        layer->m_resetRefList = ( opts->ResetRefList == MFX_CODINGOPTION_ON );
    }

    layer->enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
    if( par->mfx.FrameInfo.AspectRatioW != 0 && par->mfx.FrameInfo.AspectRatioH != 0 ){
        layer->enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = (Ipp8u)ConvertSARtoIDC_H264enc(par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);
        if (layer->enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 0) {
            layer->enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = 255; //Extended SAR
            layer->enc->m_SeqParamSet->vui_parameters.sar_width = par->mfx.FrameInfo.AspectRatioW;
            layer->enc->m_SeqParamSet->vui_parameters.sar_height = par->mfx.FrameInfo.AspectRatioH;
        }
    }

    if (par->mfx.FrameInfo.FrameRateExtD && par->mfx.FrameInfo.FrameRateExtN) {
        layer->enc->m_SeqParamSet->vui_parameters.timing_info_present_flag = 1;
        layer->enc->m_SeqParamSet->vui_parameters.num_units_in_tick = par->mfx.FrameInfo.FrameRateExtD;
        layer->enc->m_SeqParamSet->vui_parameters.time_scale = par->mfx.FrameInfo.FrameRateExtN * 2;
        layer->enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = 1;
    }
    else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    layer->enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = layer->enc->TempIdMax > 0; //0;
    layer->enc->m_SeqParamSet->vui_parameters.chroma_loc_info_present_flag = 0;

    if (opts != NULL)
    {
        if( opts->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN || opts->VuiNalHrdParameters == MFX_CODINGOPTION_ON)
        {
            layer->enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
            layer->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
                layer->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 1;
            else {
                layer->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
                if (opts->PicTimingSEI != MFX_CODINGOPTION_ON)
                    layer->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 0;
            }
        }else {
            layer->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
            if (opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                layer->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
        }
    }

    layer->enc->m_FieldStruct = par->mfx.FrameInfo.PicStruct;

    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {

        if (rc && (layer->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag |
            layer->enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag)) {

            mfxU32 qid, tid;

            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_cnt_minus1 = 0;
            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale = 0;
            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale = 3;
            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0] = par->mfx.RateControlMethod == MFX_RATECONTROL_CBR ? 1 : 0;

            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = 23; // default
            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = 23; // default
            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1 = 23; // default
            layer->enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length = 24; // default

            // see G.14.2, G.8.8.1
            layer->enc->m_SeqParamSet->svc_vui_parameters_present_flag = 1;
            layer->enc->m_SeqParamSet->vui_ext_num_entries = layer->enc->QualityNum * (layer->enc->TempIdMax+1);
            if (layer->enc->m_SeqParamSet->vui_ext_num_entries == 1 && did == 0) { // already have (0,0,0) vui in the "regular" AVC VUI
                layer->enc->m_SeqParamSet->svc_vui_parameters_present_flag = 0;
                // Note, vui is not written, but vui_ext_num_entries remains 1, because is used in BRC
            }

            if (layer->enc->m_SeqParamSet->vui_ext_num_entries)
                layer->enc->m_SeqParamSet->svc_vui_parameters = new SVCVUIParams[layer->enc->m_SeqParamSet->vui_ext_num_entries];

            for (tid = 0; tid <= layer->enc->TempIdMax; tid++) {
                // bit_rate_scale and cpb_size_scale are the same for all layers
                Ipp32u  cpb_size = 0;
                Ipp32u  bit_rate = 0;
                Ipp32u  init_delay = 0;
                SVCVUIParams *lower_vui;
                if (did > 0) {
                    H264CoreEncoder_8u16s *lower_enc =  m_svc_layers[did - 1]->enc;
                    Ipp32s lower_tid = (Ipp32s)tid;
                    if (lower_tid > (Ipp32s)lower_enc->TempIdMax)
                        lower_tid = (Ipp32s)lower_enc->TempIdMax;
                    lower_vui = &m_svc_layers[did - 1]->enc->m_SeqParamSet->svc_vui_parameters[(lower_enc->QualityNum-1)*(lower_enc->TempIdMax+1) + lower_tid];
                    cpb_size = lower_vui->nal_hrd_parameters.cpb_size_value_minus1[0] + 1;
                    bit_rate = lower_vui->nal_hrd_parameters.bit_rate_value_minus1[0] + 1;
                    init_delay = lower_vui->initial_delay_bytes;
                }
                for (qid = 0; qid < layer->enc->QualityNum; qid++) {
                    Ipp32u ind = qid * (layer->enc->TempIdMax+1) + tid;
                    SVCVUIParams *vui = &layer->enc->m_SeqParamSet->svc_vui_parameters[ind];
                    vui->vui_ext_dependency_id = did;
                    vui->vui_ext_quality_id = qid;
                    vui->vui_ext_temporal_id = tid;

                    vui->vui_ext_nal_hrd_parameters_present_flag = layer->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag;
                    vui->vui_ext_vcl_hrd_parameters_present_flag = layer->enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag;
                    vui->vui_ext_low_delay_hrd_flag = layer->enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag;
                    vui->vui_ext_pic_struct_present_flag = layer->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag;

                    if (vui->vui_ext_nal_hrd_parameters_present_flag) {
                        vui->nal_hrd_parameters.cpb_cnt_minus1 = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_cnt_minus1;
                        vui->nal_hrd_parameters.bit_rate_scale = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale;
                        vui->nal_hrd_parameters.cpb_size_scale = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale;

/*
                        if (tid > 0) {
                            lower_vui = &layer->enc->m_SeqParamSet->svc_vui_parameters[ind-1];
                            cpb_size += lower_vui->nal_hrd_parameters.cpb_size_value_minus1[0] + 1;
                            bit_rate += lower_vui->nal_hrd_parameters.bit_rate_value_minus1[0] + 1;
                            init_delay +=  lower_vui->initial_delay_bytes;
*/
                        VideoBrcParams  brcParams;
                        layer->m_brc[qid]->GetParams(&brcParams, tid);
                        bit_rate += brcParams.maxBitrate >> (6 + vui->nal_hrd_parameters.bit_rate_scale);
                        cpb_size += brcParams.HRDBufferSizeBytes >> (1 + vui->nal_hrd_parameters.cpb_size_scale);
                        init_delay += brcParams.HRDInitialDelayBytes;

                        if (bit_rate > 0 && cpb_size > 0) {
                            vui->nal_hrd_parameters.bit_rate_value_minus1[0] = bit_rate - 1;
                            vui->nal_hrd_parameters.cpb_size_value_minus1[0] = cpb_size - 1;
                            vui->initial_delay_bytes = init_delay;
                        } else {
                            vui->vui_ext_nal_hrd_parameters_present_flag = vui->vui_ext_vcl_hrd_parameters_present_flag = 0;
                        }


                        if ((did|qid|tid) == 0) {
                            if (bit_rate > 0 && cpb_size > 0) {
                                layer->enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_value_minus1[0] = bit_rate - 1;
                                layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_value_minus1[0] = cpb_size - 1;
                            } else {
                                layer->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 
                                    layer->enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag = 0;
                            }
                        }

                        vui->nal_hrd_parameters.cbr_flag[0] = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0];
                        vui->nal_hrd_parameters.initial_cpb_removal_delay_length_minus1 = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1;
                        vui->nal_hrd_parameters.cpb_removal_delay_length_minus1 = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1;
                        vui->nal_hrd_parameters.dpb_output_delay_length_minus1 = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1;
                        vui->nal_hrd_parameters.time_offset_length = layer->enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length;
                    }

                    if (vui->vui_ext_vcl_hrd_parameters_present_flag) {
                        vui->vcl_hrd_parameters = vui->nal_hrd_parameters;
                        if ((did|qid|tid) == 0) {
                            layer->enc->m_SeqParamSet->vui_parameters.vcl_hrd_params.bit_rate_value_minus1[0] = vui->vcl_hrd_parameters.bit_rate_value_minus1[0];
                            layer->enc->m_SeqParamSet->vui_parameters.vcl_hrd_params.cpb_size_value_minus1[0] = vui->vcl_hrd_parameters.cpb_size_value_minus1[0];
                        }
                    }

                    Ipp16u scale = layer->enc->TemporalScaleList[tid];
                    vui->vui_ext_timing_info_present_flag = layer->enc->m_SeqParamSet->vui_parameters.timing_info_present_flag;
                    vui->vui_ext_num_units_in_tick = par->mfx.FrameInfo.FrameRateExtD;
                    vui->vui_ext_time_scale = par->mfx.FrameInfo.FrameRateExtN * 2 * scale;
                    vui->vui_ext_fixed_frame_rate_flag = layer->enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag;

                    layer->m_representationBF[qid][tid] = (Ipp64s)vui->initial_delay_bytes * (vui->vui_ext_time_scale >> 1);
//                    layer->enc->m_representationBF = (Ipp64s)vui->initial_delay_bytes * (vui->vui_ext_time_scale >> 1);
                }
            }
        }
    }

#ifdef H264_HRD_CPB_MODEL
    layer->enc->m_SEIData.m_tickDuration = ((Ipp64f)layer->enc->m_SeqParamSet->vui_parameters.num_units_in_tick) / layer->enc->m_SeqParamSet->vui_parameters.time_scale;
#endif // H264_HRD_CPB_MODEL

    // Init extCodingOption first
    //if (opts != NULL)
        layer->m_extOption = *opts;
    //else {
    //    memset(&layer->m_extOption, 0, sizeof(layer->m_extOption));
    //    layer->m_extOption.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    //    layer->m_extOption.Header.BufferSz = sizeof(layer->m_extOption);
    //}
    layer->m_pExtBuffer = &layer->m_extOption.Header;
    layer->m_mfxVideoParam.ExtParam = &layer->m_pExtBuffer;
    layer->m_mfxVideoParam.NumExtParam = 1;

    st = ConvertVideoParamBack_H264enc(&layer->m_mfxVideoParam, layer->enc);
    MFX_CHECK_STS(st);

    // allocate empty frames to cpb
    UMC::Status ps = H264EncoderFrameList_Extend_8u16s(
        &layer->enc->m_cpb,
        layer->m_mfxVideoParam.mfx.NumRefFrame + (layer->m_mfxVideoParam.mfx.EncodedOrder ? 1 : layer->m_mfxVideoParam.mfx.GopRefDist),
        layer->enc->m_info.info.clip_info.width,
        layer->enc->m_info.info.clip_info.height,
        layer->enc->m_PaddedSize,
        UMC::NV12,
        layer->enc->m_info.num_slices * ((layer->enc->m_info.coding_type == 1 || layer->enc->m_info.coding_type == 3) + 1)
#if defined ALPHA_BLENDING_H264
        , layer->enc->m_SeqParamSet->aux_format_idc
#endif
        );
    if (ps != UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;


    // allocate frame for recode
    // do it always to allow switching  // if (enc->m_Analyse & ANALYSE_RECODE_FRAME /*&& core_enc->m_pReconstructFrame == NULL //could be size change*/){ //make init reconstruct buffer
    layer->enc->m_pReconstructFrame_allocated = (H264EncoderFrame_8u16s *)H264_Malloc(sizeof(H264EncoderFrame_8u16s));
    if (!layer->enc->m_pReconstructFrame_allocated)
        return MFX_ERR_MEMORY_ALLOC;
    H264EncoderFrame_Create_8u16s(layer->enc->m_pReconstructFrame_allocated, &layer->enc->m_cpb.m_pHead->m_data, layer->enc->memAlloc,
#if defined ALPHA_BLENDING_H264
        layer->enc->m_SeqParamSet->aux_format_idc,
#endif // ALPHA_BLENDING_H264
        0);
    if (H264EncoderFrame_allocate_8u16s(layer->enc->m_pReconstructFrame_allocated, layer->enc->m_PaddedSize,
        layer->enc->m_info.num_slices * ((layer->enc->m_info.coding_type == 1 || layer->enc->m_info.coding_type == 3) + 1)))
        return MFX_ERR_MEMORY_ALLOC;

    {
        Ipp32s pitchPixels = layer->enc->m_pReconstructFrame_allocated->m_pitchPixels;
        Ipp32s lumaSize = pitchPixels * (((layer->enc->m_PaddedSize.height + 15) >> 4) << 4);

        layer->enc->m_pYInputRefPlane = NULL;
        layer->enc->m_pYInputRefPlane = (PIXTYPE *)H264_Malloc(lumaSize + (lumaSize >> 1));

        if (layer->enc->m_pYInputRefPlane == NULL)
            return MFX_ERR_MEMORY_ALLOC;
        layer->enc->m_pUInputRefPlane = layer->enc->m_pYInputRefPlane + lumaSize;

#ifdef USE_NV12
        layer->enc->m_pVInputRefPlane = layer->enc->m_pUInputRefPlane + 1;
#else
        layer->enc->m_pVInputRefPlane = layer->enc->m_pUInputRefPlane + (pitchPixels >> 1);
#endif
    }

    layer->enc->m_spatial_resolution_change = false;
    layer->enc->m_restricted_spatial_resolution_change = true;

    // Init resize params, when needed
    memset(&layer->enc->m_svc_layer.resize, 0, sizeof(UMC::H264LayerResizeParameter));
    layer->enc->m_constrained_intra_pred_flag = 1;
    if (pInterParams && layer->ref_layer) {
        UMC::H264LayerResizeParameter *rsz = &layer->enc->m_svc_layer.resize;
        H264SeqParamSet* sps = layer->enc->m_SeqParamSet;
        Ipp32s pos, lastMB;

        layer->ref_layer->enc->m_constrained_intra_pred_flag = 1; // must be in reference layer
//        layer->enc->m_constrained_intra_pred_flag = 1; // temp - to try

        // resize info in SPS extension
        rsz->extended_spatial_scalability = 0;
        rsz->leftOffset = pInterParams->ScaledRefLayerOffsets[0];
        rsz->topOffset = pInterParams->ScaledRefLayerOffsets[1];
        rsz->rightOffset = pInterParams->ScaledRefLayerOffsets[2];
        rsz->bottomOffset = pInterParams->ScaledRefLayerOffsets[3];
        if (rsz->leftOffset | rsz->topOffset | rsz->rightOffset | rsz->bottomOffset)
            rsz->extended_spatial_scalability = 1;

        rsz->ref_layer_width = layer->ref_layer->enc->m_info.info.clip_info.width;
        rsz->ref_layer_height = layer->ref_layer->enc->m_info.info.clip_info.height;
        rsz->scaled_ref_layer_width = layer->enc->m_info.info.clip_info.width - rsz->leftOffset - rsz->rightOffset;
        rsz->scaled_ref_layer_height = layer->enc->m_info.info.clip_info.height - rsz->topOffset - rsz->bottomOffset;
        rsz->phaseX = layer->enc->m_SeqParamSet->chroma_phase_x_plus1 - 1;
        rsz->phaseY = layer->enc->m_SeqParamSet->chroma_phase_y_plus1 - 1;
        rsz->refPhaseX = layer->enc->m_SeqParamSet->seq_ref_layer_chroma_phase_x_plus1 - 1; // ? or from layer->ref_layer?
        rsz->refPhaseY = layer->enc->m_SeqParamSet->seq_ref_layer_chroma_phase_y_plus1 - 1;
        // copy values to sps (duplicated in setSPS...)
        sps->extended_spatial_scalability = rsz->extended_spatial_scalability;
        sps->seq_scaled_ref_layer_left_offset = rsz->leftOffset;
        sps->seq_scaled_ref_layer_top_offset = rsz->topOffset;
        sps->seq_scaled_ref_layer_right_offset = rsz->rightOffset;
        sps->seq_scaled_ref_layer_bottom_offset = rsz->bottomOffset;

        {
            Ipp32s CroppingChangeFlag;
            if (sps->extended_spatial_scalability == 2)
                CroppingChangeFlag = 1;
            else
                CroppingChangeFlag = 0;

            rsz->m_spatial_resolution_change = 1;
            rsz->m_restricted_spatial_resolution_change = 0;

            if ((CroppingChangeFlag == 0) &&
                (rsz->ref_layer_width == rsz->scaled_ref_layer_width) &&
                (rsz->ref_layer_height == rsz->scaled_ref_layer_height) &&
                ((rsz->leftOffset & 0xF) == 0) &&
                ((rsz->topOffset % (16 * (1 + sps->frame_mbs_only_flag))) == 0) &&
                //(curSliceHeader->MbaffFrameFlag == refLayer->is_MbAff) &&
                (rsz->refPhaseX == rsz->phaseX) &&
                (rsz->refPhaseX == rsz->phaseX))
            {
                rsz->m_spatial_resolution_change = 0;
                rsz->m_restricted_spatial_resolution_change = 1;
            }

            if ((CroppingChangeFlag == 0) &&
                ((rsz->ref_layer_width == rsz->scaled_ref_layer_width) || (2 * rsz->ref_layer_width == rsz->scaled_ref_layer_width)) &&
                ((rsz->ref_layer_height == rsz->scaled_ref_layer_height) || (2 * rsz->ref_layer_height == rsz->scaled_ref_layer_height)) &&
                ((rsz->leftOffset & 0xF) == 0) &&
                ((rsz->topOffset % (16 * (1 + sps->frame_mbs_only_flag))) == 0) //&&
                //(curSliceHeader->MbaffFrameFlag == refLayer->is_MbAff)
                )
            {
                rsz->m_restricted_spatial_resolution_change = 1;
            }
        
            // Initialize IPP upsampling
            // Luma
            IppiUpsampleSVCParams *upsampleSVCParams;
            
            upsampleSVCParams = &(rsz->upsampleSVCParamsLuma);
            upsampleSVCParams->deltaX = 8;
            upsampleSVCParams->deltaY = 8;
            upsampleSVCParams->leftOffset = rsz->leftOffset;
            upsampleSVCParams->topOffset = rsz->topOffset;
            upsampleSVCParams->cropWidth = rsz->scaled_ref_layer_width;
            upsampleSVCParams->cropHeight = rsz->scaled_ref_layer_height;
            MFXVideoENCODEH264_CalculateUpsamplingPars(rsz->ref_layer_width,
                rsz->ref_layer_height,
                rsz->scaled_ref_layer_width,
                rsz->scaled_ref_layer_height,
                0,
                0,
                layer->enc->m_SeqParamSet->level_idc,
                upsampleSVCParams);

            rsz->mbRegionFull.x = (upsampleSVCParams->leftOffset + 15) >> 4;
            rsz->mbRegionFull.y = (upsampleSVCParams->topOffset + 15) >> 4;
            rsz->mbRegionFull.width = ((upsampleSVCParams->leftOffset +
                upsampleSVCParams->cropWidth) >> 4) - rsz->mbRegionFull.x;
            rsz->mbRegionFull.height = ((upsampleSVCParams->topOffset +
                upsampleSVCParams->cropHeight) >> 4) - rsz->mbRegionFull.y;

            rsz->shiftX = upsampleSVCParams->shiftX;
            rsz->shiftY = upsampleSVCParams->shiftY;
            rsz->scaleX = upsampleSVCParams->scaleX;
            rsz->scaleY = upsampleSVCParams->scaleY;
            rsz->addX = (1 << (rsz->shiftX - 1)) - rsz->leftOffset * rsz->scaleX; // different addX
            rsz->addY = (1 << (rsz->shiftY - 1)) - rsz->topOffset * rsz->scaleY; // different addY

            // Chroma
            Ipp32s chromaShiftW = (3 > sps->chroma_format_idc) ? 1 : 0;
            Ipp32s chromaShiftH = (2 > sps->chroma_format_idc) ? 1 : 0;

            upsampleSVCParams = &(rsz->upsampleSVCParamsChroma);
            upsampleSVCParams->deltaX = 4 * (2 + rsz->refPhaseX);
            upsampleSVCParams->deltaY = 4 * (2 + rsz->refPhaseY);
            upsampleSVCParams->leftOffset = rsz->leftOffset >> chromaShiftW;
            upsampleSVCParams->topOffset = rsz->topOffset >> chromaShiftH;
            upsampleSVCParams->cropWidth = rsz->scaled_ref_layer_width >> chromaShiftW;
            upsampleSVCParams->cropHeight = rsz->scaled_ref_layer_height >> chromaShiftH;
            MFXVideoENCODEH264_CalculateUpsamplingPars(rsz->ref_layer_width >> chromaShiftW,
                rsz->ref_layer_height >> chromaShiftH,
                rsz->scaled_ref_layer_width >> chromaShiftW,
                rsz->scaled_ref_layer_height >> chromaShiftH,
                rsz->phaseX,
                rsz->phaseY,
                layer->enc->m_SeqParamSet->level_idc,
                upsampleSVCParams);

            IppiSize mbRegionSize, refSizeLuma, refSizeChroma;
            mbRegionSize.width = rsz->mbRegionFull.width;
            mbRegionSize.height = rsz->mbRegionFull.height;
            refSizeLuma.width = rsz->ref_layer_width;
            refSizeLuma.height = rsz->ref_layer_height;
            refSizeChroma.width =  refSizeLuma.width >> chromaShiftW;
            refSizeChroma.height =  refSizeLuma.height >> chromaShiftW;

            Ipp32u specIntraLumaSize, specIntraChromaSize, specResidLumaSize, specResidChromaSize;
            Ipp32u tmpSize, bufferSize, totalSize;
            IppStatus sts;

            sts = ippiUpsampleIntraLumaGetSize_SVC_8u_C1R(mbRegionSize, &(rsz->upsampleSVCParamsLuma),
                &specIntraLumaSize, &bufferSize);
            if (sts != ippStsNoErr){return MFX_ERR_UNKNOWN;}

            sts = ippiUpsampleIntraChromaGetSize_SVC_8u_C2R(mbRegionSize, &(rsz->upsampleSVCParamsChroma),
                &specIntraChromaSize, &tmpSize);
            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;
            if (bufferSize < tmpSize)
                bufferSize = tmpSize;

            sts = ippiUpsampleResidualsLumaGetSize_SVC_16s_C1R(mbRegionSize, &(rsz->upsampleSVCParamsLuma),
                &specResidLumaSize, &tmpSize);
            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;
            if (bufferSize < tmpSize)
                bufferSize = tmpSize;

            sts = ippiUpsampleResidualsChromaGetSize_SVC_16s_C1R(mbRegionSize, &(rsz->upsampleSVCParamsChroma),
                &specResidChromaSize, &tmpSize);
            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;
            if (bufferSize < tmpSize)
                bufferSize = tmpSize;

            totalSize = specIntraLumaSize + specIntraChromaSize + specResidLumaSize + specResidChromaSize + bufferSize;
            layer->enc->svc_UpsampleMem = (Ipp8u*)H264_Malloc(totalSize);
            if (!layer->enc->svc_UpsampleMem)
                return MFX_ERR_MEMORY_ALLOC;

            Ipp8u *ptr = layer->enc->svc_UpsampleMem;

            layer->enc->svc_UpsampleIntraLumaSpec = (IppiUpsampleSVCSpec *)ptr;
            ptr += specIntraLumaSize;
            layer->enc->svc_UpsampleIntraChromaSpec = (IppiUpsampleSVCSpec *)ptr;
            ptr += specIntraChromaSize;
            layer->enc->svc_UpsampleResidLumaSpec = (IppiUpsampleSVCSpec *)ptr;
            ptr += specResidLumaSize;
            layer->enc->svc_UpsampleResidChromaSpec = (IppiUpsampleSVCSpec *)ptr;
            ptr += specResidChromaSize;
            layer->enc->svc_UpsampleBuf = ptr;

            sts = ippiUpsampleIntraLumaInit_SVC_8u_C1R(&(rsz->upsampleSVCParamsLuma),
                layer->enc->svc_UpsampleIntraLumaSpec);
            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;

            sts = ippiUpsampleIntraChromaInit_SVC_8u_C2R(&(rsz->upsampleSVCParamsChroma),
                layer->enc->svc_UpsampleIntraChromaSpec);
            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;

            sts = ippiUpsampleResidualsLumaInit_SVC_16s_C1R(refSizeLuma,
                &(rsz->upsampleSVCParamsLuma),
                layer->enc->svc_UpsampleResidLumaSpec);
//            bug in IPP
//            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;

            sts = ippiUpsampleResidualsChromaInit_SVC_16s_C1R(refSizeChroma,
                &(rsz->upsampleSVCParamsChroma),
                layer->enc->svc_UpsampleResidChromaSpec);
//            bug in IPP
//            if (sts != ippStsNoErr) return MFX_ERR_UNKNOWN;
        }

        // now make a table for inter/intra bounds (ref borders mapped to current)
        layer->enc->svc_ResizeMap[0] = (SVCResizeMBPos *)H264_Malloc(sizeof(SVCResizeMBPos) * (layer->enc->m_WidthInMBs+layer->enc->m_HeightInMBs));
        if (!layer->enc->svc_ResizeMap[0])
            return MFX_ERR_MEMORY_ALLOC;
        layer->enc->svc_ResizeMap[1] = layer->enc->svc_ResizeMap[0] + layer->enc-> m_WidthInMBs;
        for (pos=0; pos<layer->enc->m_WidthInMBs+layer->enc->m_HeightInMBs; pos++) {
            layer->enc->svc_ResizeMap[0][pos].refMB[0] = -1;
            layer->enc->svc_ResizeMap[0][pos].refMB[1] = -1;
            layer->enc->svc_ResizeMap[0][pos].border = 0;
        }
        for(pos=rsz->leftOffset, lastMB=-1; pos < layer->enc->m_info.info.clip_info.width - rsz->rightOffset; pos++) {
            Ipp32s ref = ((pos/*-rsz->leftOffset*/) * rsz->scaleX + rsz->addX) >> (rsz->shiftX);
            if (ref >= rsz->ref_layer_width - 1)
                ref = rsz->ref_layer_width - 1;
            ref >>= 4;
            if (!(pos & 15)) {
                layer->enc->svc_ResizeMap[0][pos>>4].refMB[0] = lastMB = ref;
                layer->enc->svc_ResizeMap[0][pos>>4].border = 16;
            } else if (lastMB != ref) {
                layer->enc->svc_ResizeMap[0][pos>>4].refMB[1] = ref;
                layer->enc->svc_ResizeMap[0][pos>>4].border = pos & 15;
                pos |= 15; // to next mb
            }
        }
        for(pos=rsz->topOffset, lastMB=-1; pos < layer->enc->m_info.info.clip_info.height - rsz->bottomOffset; pos++) {
            Ipp32s ref = ((pos/*-rsz->topOffset*/) * rsz->scaleY + rsz->addY) >> (rsz->shiftY);
            if (ref >= rsz->ref_layer_height - 1)
                ref = rsz->ref_layer_height - 1;
            ref >>= 4;
            if (!(pos & 15)) {
                layer->enc->svc_ResizeMap[1][pos>>4].refMB[0] = lastMB = ref;
                layer->enc->svc_ResizeMap[1][pos>>4].border = 16;
            } else if (lastMB != ref) {
                layer->enc->svc_ResizeMap[1][pos>>4].refMB[1] = ref;
                layer->enc->svc_ResizeMap[1][pos>>4].border = pos & 15;
                pos |= 15; // to next mb
            }
        }
    }

    // Set SPS TCoeff flags;
    if (pInterParams) {
        H264SeqParamSet* sps = layer->enc->m_SeqParamSet;
        Ipp32s qid, noTCpred = 0;
        sps->seq_tcoeff_level_prediction_flag = 0;
        sps->adaptive_tcoeff_level_prediction_flag = 0;
        for (qid = 0; qid < pInterParams->QualityNum; qid++) {
            if(pInterParams->QualityLayer[qid].TcoeffPredictionFlag == MFX_CODINGOPTION_ON) 
                sps->seq_tcoeff_level_prediction_flag = 1;
            else
                noTCpred ++;
        }
        if (noTCpred && sps->seq_tcoeff_level_prediction_flag)
            sps->adaptive_tcoeff_level_prediction_flag = 1;
    }

#ifdef USE_TEMPTEMP
    layer->enc->refctrl = m_refctrl; // For temporal; contains index in GOP of the last frame, which refers to it (display order)
    layer->enc->temptempstart = &m_temptempstart; // FrameTag for first frame in temporal template
    layer->enc->temptemplen = m_temptemplen; // length of the temporal template
    layer->enc->temptempwidth = m_temptempwidth; // step back when went to the end of template
    layer->enc->usetemptemp = &m_usetemptemp; //allow use template
    layer->enc->rateDivider = m_maxTempScale / layer->enc->TemporalScaleList[layer->enc->TempIdMax];
#endif // USE_TEMPTEMP

#ifdef H264_NEW_THREADING
#else
#ifdef VM_SLICE_THREADING_H264
#endif // VM_SLICE_THREADING_H264
#endif // H264_NEW_THREADING
    st = h264enc_ConvertStatus(status);
    if (st == MFX_ERR_NONE)
    {
        layer->m_Initialized = true;
        layer->enc->m_svc_layer.isActive = 1;
        return (QueryStatus != MFX_ERR_NONE) ? QueryStatus : (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
    }
    else
        return st;
}

mfxStatus MFXVideoENCODEH264::CloseSVCLayer(h264Layer* layer)
{
    if( !layer || !layer->m_Initialized ) return MFX_ERR_NOT_INITIALIZED;
    H264CoreEncoder_8u16s* enc = layer->enc;

    ReleaseBufferedFrames(layer);
    if (layer->enc->svc_ResizeMap[0]) {
        H264_Free(layer->enc->svc_UpsampleMem);
        H264_Free(layer->enc->svc_ResizeMap[0]);
        layer->enc->svc_ResizeMap[0] = layer->enc->svc_ResizeMap[1] = 0;
    }

    if (layer->enc->m_pYInputRefPlane) {
        H264_Free(layer->enc->m_pYInputRefPlane);
        layer->enc->m_pYInputRefPlane = 0;
    }

    H264CoreEncoder_Close_8u16s(enc);
    H264EncoderFrameList_clearFrameList_8u16s(&enc->m_dpb);
    H264EncoderFrameList_clearFrameList_8u16s(&enc->m_cpb);
    if (enc->m_pReconstructFrame_allocated) {
        H264EncoderFrame_Destroy_8u16s(enc->m_pReconstructFrame_allocated);
        H264_Free(enc->m_pReconstructFrame_allocated);
        enc->m_pReconstructFrame_allocated = 0;
    }

    {
        mfxU8 i;
        for (i = 0; i < layer->enc->QualityNum; i++) {
            if (layer->m_brc[i]) {
                layer->m_brc[i]->Close();
                delete layer->m_brc[i];
                layer->m_brc[i] = 0;
            }
        }
    }

    if (layer->enc->m_SeqParamSet->svc_vui_parameters) {
        delete[] layer->enc->m_SeqParamSet->svc_vui_parameters;
        layer->enc->m_SeqParamSet->svc_vui_parameters = 0;
    }

    if (layer->m_useAuxInput) {
        m_core->FreeFrames(&layer->m_response);
        layer->m_useAuxInput = false;
    }

#ifdef USE_TEMPTEMP
    if (layer->enc->refctrl) {
        //H264_Free(layer->enc->refctrl);
        layer->enc->refctrl = 0;
        layer->enc->temptempstart = 0;
        layer->enc->temptemplen = 0;
        layer->enc->rateDivider = 1;
        layer->enc->usetemptemp = 0;
    }
#endif // USE_TEMPTEMP

    return MFX_ERR_NONE;
}

/* RD  Need to add */
mfxStatus MFXVideoENCODEH264::SetScalabilityInfoSE()
{
    mfxU8 i0, i1, i2;
    mfxU32 layer;

    layer = 0;

    for (i0 = 0; i0 < MAX_DEP_LAYERS; i0++) {
        if (!m_svc_layers[i0]) continue;

        for (i1 = 0; i1 < MAX_TEMP_LEVELS; i1++) {
            if (m_svc_layers[i0]->enc->TemporalScaleList[i1] == 0)
                continue;

            for (i2 = 0; i2 < m_svc_layers[i0]->enc->QualityNum; i2++) {
                m_svc_ScalabilityInfoSEI.layer_id[layer] = layer;
                m_svc_ScalabilityInfoSEI.dependency_id[layer] = m_svc_layers[i0]->enc->m_svc_layer.svc_ext.dependency_id;
                m_svc_ScalabilityInfoSEI.quality_id[layer] = i2;
                m_svc_ScalabilityInfoSEI.temporal_id[layer] = m_svc_layers[i0]->m_InterLayerParam.TemporalId[i1];

                m_svc_ScalabilityInfoSEI.layer_output_flag[layer] = 1;

                m_svc_ScalabilityInfoSEI.frm_size_info_present_flag[layer] = 1;
                m_svc_ScalabilityInfoSEI.frm_width_in_mbs_minus1[layer] = m_svc_layers[i0]->enc->m_WidthInMBs - 1;
                m_svc_ScalabilityInfoSEI.frm_height_in_mbs_minus1[layer] = m_svc_layers[i0]->enc->m_HeightInMBs - 1;

                layer++;
            }
        }
    }

    m_svc_ScalabilityInfoSEI.num_layers_minus1 = layer - 1;

    return MFX_ERR_NONE;
}

/*************************************************************
*  Name:         encodeFrameHeader
*  Description:  Write out the frame header to the bit stream.
* For SVC - set of SP[sub]S, then set of PPS
************************************************************/
Status MFXVideoENCODEH264::H264CoreEncoder_encodeFrameHeader(
    void* state,
    H264BsReal_8u16s* bs,
    MediaData* dst,
    bool bIDR_Pic,
    bool& startPicture )
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status ps = UMC_OK;

    // First, write a access unit delimiter for the frame.
    if (core_enc->m_info.write_access_unit_delimiters)
    {
        ps = H264BsReal_PutPicDelimiter_8u16s(bs, core_enc->m_PicType);

        H264BsBase_WriteTrailingBits(&bs->m_base);

        // Copy PicDelimiter RBSP to the end of the output buffer after
        // Adding start codes and SC emulation prevention.
        size_t oldsize = dst->GetDataSize();
        dst->SetDataSize(
            oldsize +
                H264BsReal_EndOfNAL_8u16s(
                    bs,
                    (Ipp8u*)dst->GetDataPointer() + oldsize,
                    0,
                    NAL_UT_AUD,
                    startPicture,
                    0));
    }

    // If this is an IDR picture, write the seq and pic parameter sets
    bool putSet = (m_svc_count == 0) ||
        ((core_enc->m_svc_layer.svc_ext.dependency_id == 0) &&
         (core_enc->m_svc_layer.svc_ext.quality_id == 0));

    if (bIDR_Pic)
    {
        if(core_enc->m_info.m_is_mvc_profile) {
            // If MVC && dependent view - skip SPS/PPS output, done at base view
            if(!core_enc->m_info.m_is_base_view) return ps;

            H264SeqParamSet   *sps_views = core_enc->m_SeqParamSet;
            H264PicParamSet   *pps_views = core_enc->m_PicParamSet;
            size_t             oldsize;
            do {
                // Write the seq_parameter_set_rbsp
                ps = H264BsReal_PutSeqParms_8u16s(bs, *sps_views);

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Sequence Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                oldsize = dst->GetDataSize();
                if(sps_views->profile_idc==H264_MULTIVIEWHIGH_PROFILE || sps_views->profile_idc==H264_STEREOHIGH_PROFILE) {
                    dst->SetDataSize(
                        oldsize +
                            H264BsReal_EndOfNAL_8u16s(
                                bs,
                                (Ipp8u*)dst->GetDataPointer() + oldsize,
                                1,
                                NAL_UT_SUBSET_SPS,
                                startPicture,
                                0));
                } else {
                    dst->SetDataSize(
                        oldsize +
                        H264BsReal_EndOfNAL_8u16s(
                        bs,
                        (Ipp8u*)dst->GetDataPointer() + oldsize,
                        1,
                        NAL_UT_SPS,
                        startPicture, 0));
                }

                if(sps_views->pack_sequence_extension) {
                    // Write the seq_parameter_set_extension_rbsp when needed.
                    ps = H264BsReal_PutSeqExParms_8u16s(bs, *sps_views);

                    H264BsBase_WriteTrailingBits(&bs->m_base);

                    // Copy Sequence Parms RBSP to the end of the output buffer after
                    // Adding start codes and SC emulation prevention.
                    oldsize = dst->GetDataSize();
                    dst->SetDataSize(
                        oldsize +
                            H264BsReal_EndOfNAL_8u16s(
                                bs,
                                (Ipp8u*)dst->GetDataPointer() + oldsize,
                                1,
                                NAL_UT_SEQEXT,
                                startPicture,
                                0));
                }
                sps_views++;
            } while (sps_views->profile_idc);

            sps_views = core_enc->m_SeqParamSet;
            do {
                ps = H264BsReal_PutPicParms_8u16s(
                    bs,
                    *pps_views,
                    *(sps_views+pps_views->seq_parameter_set_id));

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Picture Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                oldsize = dst->GetDataSize();
                dst->SetDataSize(
                    oldsize +
                        H264BsReal_EndOfNAL_8u16s(
                            bs,
                            (Ipp8u*)dst->GetDataPointer() + oldsize,
                            1,
                            NAL_UT_PPS,
                            startPicture,
                            0));
                pps_views++;
            } while (pps_views->seq_parameter_set_id);

        } else if (putSet) {
            if (m_svc_count > 0) {
                // Write the SEI scalable
                ps = H264BsReal_PutSVCScalabilityInfoSEI_8u16s(bs, core_enc->svc_ScalabilityInfoSEI);

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Sequence Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                dst->SetDataSize(
                    dst->GetDataSize() +
                    H264BsReal_EndOfNAL_8u16s(
                        bs,
                        (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize(),
                        0,
                        NAL_UT_SEI,
                        startPicture, 0));
            }

            bool two_sps = core_enc->QualityNum > 1 || core_enc->TempIdMax > 0;
            bool AVC_sps = two_sps || (core_enc->QualityNum == 1 && core_enc->TempIdMax == 0);

            H264_PROFILE_IDC saved_profile = core_enc->m_SeqParamSet->profile_idc;
            //for( struct h264Layer* layer = m_layer; layer; layer = layer->src_layer) {
            //    core_enc = layer->enc;
            for( Ipp32u ll = 0; ; ll++) { // put for single AVC or all spatial layers
                NAL_Unit_Type NalType = NAL_UT_SPS;
                if (ll > 0) {
                    if (ll < m_svc_count)
                        core_enc = m_svc_layers[ll]->enc;
                    else
                        break;
                }
again:
                if (AVC_sps) { // to encode normal (AVC) SPS first
                    core_enc->m_SeqParamSet->profile_idc = core_enc->m_info.profile_idc;
                    if (core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_BASELINE_PROFILE)
                        core_enc->m_SeqParamSet->profile_idc = H264_HIGH_PROFILE; //H264_MAIN_PROFILE; // TODO check why
                    else if(core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_HIGH_PROFILE)
                        core_enc->m_SeqParamSet->profile_idc = H264_HIGH_PROFILE;
                }
                else if (core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_HIGH_PROFILE ||
                         core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_BASELINE_PROFILE)
                    NalType = NAL_UT_SUBSET_SPS;

                // Write the seq_parameter_set_rbsp
                ps = H264BsReal_PutSeqParms_8u16s(bs, *core_enc->m_SeqParamSet);

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Sequence Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                size_t oldsize = dst->GetDataSize();
                dst->SetDataSize(
                    oldsize +
                    H264BsReal_EndOfNAL_8u16s(
                    bs,
                    (Ipp8u*)dst->GetDataPointer() + oldsize,
                    1,
                    NalType,
                    startPicture, 0));

                if(core_enc->m_SeqParamSet->pack_sequence_extension) {
                    // Write the seq_parameter_set_extension_rbsp when needed.
                    ps = H264BsReal_PutSeqExParms_8u16s(bs, *core_enc->m_SeqParamSet);

                    H264BsBase_WriteTrailingBits(&bs->m_base);

                    // Copy Sequence Parms RBSP to the end of the output buffer after
                    // Adding start codes and SC emulation prevention.
                    oldsize = dst->GetDataSize();
                    dst->SetDataSize(
                        oldsize +
                        H264BsReal_EndOfNAL_8u16s(
                        bs,
                        (Ipp8u*)dst->GetDataPointer() + oldsize,
                        1,
                        NAL_UT_SEQEXT,
                        startPicture, 0));
                }
                AVC_sps = false; // others, if any, to be subset_sps
                if (two_sps) {
                    // was encoded normal (AVC) SPS instead of subset, now need subset
                    core_enc->m_SeqParamSet->profile_idc = saved_profile;
                    two_sps = false; // second subset_sps only once
                    goto again;
                }
            }
            core_enc = (H264CoreEncoder_8u16s *)state; // restore core_enc

            //for( struct h264Layer* layer = m_layer; layer; layer = layer->src_layer) {
            //    core_enc = layer->enc;
            for( Ipp32u ll = 0; ; ll++) { // put for single AVC or all spatial layers
                if (ll > 0) {
                    if (ll < m_svc_count)
                        core_enc = m_svc_layers[ll]->enc;
                    else
                        break;
                }
                ps = H264BsReal_PutPicParms_8u16s(
                    bs,
                    *core_enc->m_PicParamSet,
                    *core_enc->m_SeqParamSet);

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Picture Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                size_t oldsize = dst->GetDataSize();
                dst->SetDataSize(
                    oldsize +
                    H264BsReal_EndOfNAL_8u16s(
                    bs,
                    (Ipp8u*)dst->GetDataPointer() + oldsize,
                    1,
                    NAL_UT_PPS,
                    startPicture, 0));
            }
            core_enc = (H264CoreEncoder_8u16s *)state; // restore core_enc
        }
    }

    return ps;
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODE
