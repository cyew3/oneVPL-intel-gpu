//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"


#if defined (UMC_ENABLE_MVC_VIDEO_ENCODER)
#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "mfx_common.h" // this is mfx file. mfx defs are of importance!!!
#if defined (MFX_ENABLE_MVC_VIDEO_ENCODE)

// #include "talx.h"

#include "umc_defs.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_to_ipp.h"
#include "umc_h264_bme.h"
#include "mfxdefs.h"
#include "mfx_h264_enc_common.h"
#include "mfx_mvc_encode.h"
#include "mfx_task.h"
#include "mfx_brc_common.h"
#include "mfx_session.h"
#include "mfx_tools.h"
#include "umc_h264_brc.h"
#include "vm_thread.h"
#include "vm_interlocked.h"
#include "mfx_ext_buffers.h"
#include <new>
#include <cmath>

#ifdef VM_SLICE_THREADING_H264
#include "vm_event.h"
#include "vm_sys_info.h"
#endif // VM_SLICE_THREADING_H264

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

Status MFXVideoENCODEMVC::UpdateRefPicList(
    void* state,
    H264Slice_8u16s *curr_slice,
    EncoderRefPicList_8u16s * ref_pic_list_in,
    H264SliceHeader &SHdr,
    RefPicListReorderInfo *pReorderInfo_L0,
    RefPicListReorderInfo *pReorderInfo_L1)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    EnumSliceType slice_type = curr_slice->m_slice_type;
    EncoderRefPicList_8u16s * ref_pic_list = 0;
    Status ps = UMC_OK;
    H264SeqParamSet *sps;
    Ipp32u uMaxFrameNum;
    Ipp32u uMaxPicNum;

    Ipp16u vo, voidx_ref, voidx = m_oidxmap[core_enc->m_view_id];
    Ipp16u ext_l0_refnum, ext_l1_refnum;
    Ipp16u *ext_l0_ref, *ext_l1_ref;

    VM_ASSERT(core_enc->m_pCurrentFrame);

    if (slice_type == INTRASLICE || slice_type == S_INTRASLICE)
        return UMC_OK;

    ref_pic_list = &core_enc->m_pCurrentFrame->m_pRefPicList[curr_slice->m_slice_number];
    *ref_pic_list = *ref_pic_list_in;

    H264EncoderFrame_8u16s **pRefPicList0 = ref_pic_list->m_RefPicListL0.m_RefPicList;
    H264EncoderFrame_8u16s **pRefPicList1 = ref_pic_list->m_RefPicListL1.m_RefPicList;
    Ipp8s *pFields0 = ref_pic_list->m_RefPicListL0.m_Prediction;
    Ipp8s *pFields1 = ref_pic_list->m_RefPicListL1.m_Prediction;
    Ipp32s * pPOC0 = ref_pic_list->m_RefPicListL0.m_POC;
    Ipp32s * pPOC1 = ref_pic_list->m_RefPicListL1.m_POC;

    RefPicListInfo &refPicListInfo = core_enc->m_pCurrentFrame->m_RefPicListInfo[core_enc->m_field_index];

    curr_slice->m_NumRefsInL0List   = refPicListInfo.NumRefsInL0List;
    curr_slice->m_NumRefsInL1List   = refPicListInfo.NumRefsInL1List;
    curr_slice->m_NumForwardSTRefs  = refPicListInfo.NumForwardSTRefs;
    curr_slice->m_NumBackwardSTRefs = refPicListInfo.NumBackwardSTRefs;
    curr_slice->m_NumRefsInLTList   = refPicListInfo.NumRefsInLTList;

    if(voidx) {
        ext_l0_refnum = (core_enc->m_pCurrentFrame->m_bIsIDRPic && !core_enc->m_field_index) ? m_views[voidx].m_dependency.NumAnchorRefsL0 : m_views[voidx].m_dependency.NumNonAnchorRefsL0;
        ext_l1_refnum = (core_enc->m_pCurrentFrame->m_bIsIDRPic && !core_enc->m_field_index) ? m_views[voidx].m_dependency.NumAnchorRefsL1 : m_views[voidx].m_dependency.NumNonAnchorRefsL1;
        ext_l0_ref = (core_enc->m_pCurrentFrame->m_bIsIDRPic && !core_enc->m_field_index) ? m_views[voidx].m_dependency.AnchorRefL0 : m_views[voidx].m_dependency.NonAnchorRefL0;
        ext_l1_ref = (core_enc->m_pCurrentFrame->m_bIsIDRPic && !core_enc->m_field_index) ? m_views[voidx].m_dependency.AnchorRefL1 : m_views[voidx].m_dependency.NonAnchorRefL1;
    } else {
        ext_l0_refnum = ext_l1_refnum = 0;
        ext_l0_ref = ext_l1_ref =0;
    }

    // Spec reference: 8.2.4, "Decoding process for reference picture lists
    // construction"

    // get pointers to the picture and sequence parameter sets currently in use
    //pps = &core_enc->m_PicParamSet;
    sps = core_enc->m_SeqParamSet;
    uMaxFrameNum = (1<<sps->log2_max_frame_num);
    uMaxPicNum = (SHdr.field_pic_flag == 0) ? uMaxFrameNum : uMaxFrameNum<<1;

    if ((core_enc->m_NumShortTermRefFrames + core_enc->m_NumLongTermRefFrames + ext_l0_refnum + ext_l1_refnum) == 0)
        VM_ASSERT(0);

    if(voidx) {
        if ((slice_type == PREDSLICE) || (slice_type == S_PREDSLICE)) {
            for(vo = 0; vo < ext_l0_refnum; vo++) {
                if(curr_slice->m_NumRefsInL0List<32) {
                    voidx_ref = m_oidxmap[ext_l0_ref[vo]];
                    pRefPicList0[curr_slice->m_NumRefsInL0List] = m_views[voidx_ref].enc->m_pCurrentFrame;
                    if (SHdr.field_pic_flag) {
                        pFields0[curr_slice->m_NumRefsInL0List] = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index];
                    }
                    curr_slice->m_NumRefsInL0List++;
                }
            }
        } else {
            for(vo = 0; vo < ext_l0_refnum; vo++) {
                if(curr_slice->m_NumRefsInL0List<32) {
                    voidx_ref = m_oidxmap[ext_l0_ref[vo]];
                    pRefPicList0[curr_slice->m_NumRefsInL0List] = m_views[voidx_ref].enc->m_pCurrentFrame;
                    if (SHdr.field_pic_flag) {
                        pFields0[curr_slice->m_NumRefsInL0List] = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index];
                    }
                    curr_slice->m_NumRefsInL0List++;
                }
            }
            for(vo = 0; vo < ext_l1_refnum; vo++) {
                if(curr_slice->m_NumRefsInL1List<32) {
                    voidx_ref = m_oidxmap[ext_l1_ref[vo]];
                    pRefPicList1[curr_slice->m_NumRefsInL1List] = m_views[voidx_ref].enc->m_pCurrentFrame;
                    if (SHdr.field_pic_flag) {
                        pFields1[curr_slice->m_NumRefsInL1List] = core_enc->m_pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index];
                    }
                    curr_slice->m_NumRefsInL1List++;
                }
            }
        }
    }
    if (SHdr.field_pic_flag) {
        curr_slice->m_NumRefsInL0List = MIN(32,curr_slice->m_NumRefsInL0List);
        curr_slice->m_NumRefsInL1List = MIN(32,curr_slice->m_NumRefsInL1List);
    } else {
        curr_slice->m_NumRefsInL0List = MIN(16,curr_slice->m_NumRefsInL0List);
        curr_slice->m_NumRefsInL1List = MIN(16,curr_slice->m_NumRefsInL1List);
    }

    // Default ref pic lists are ready. Perform their modification by following rules:
    // 1) If modification info for current slice is specified explicitly (curr_slice->b_PerformModificationOfLX == true) then
    // construct ref pic list in full compliance with specified modification info.
    // 2) If modification info isn't specified then nothing to do.
    // Note that NumRefsInLXList is number of ref pics that will be actually used for ME process

    // Modification of L0
    if (curr_slice->b_PerformModificationOfL0) {
        curr_slice->num_ref_idx_l0_active = MAX(MIN(curr_slice->m_MaxNumActiveRefsInL0, curr_slice->m_NumRefsInL0List), 1);
        if (pReorderInfo_L0->num_entries > 0 && SHdr.ref_pic_list_reordering_flag_l0)
            // reorder ref pic list L0 in compliance with core_enc->m_ReorderInfoL0 and num_ref_idx_l0_active
            H264CoreEncoder_ReOrderRefPicList_8u16s(state, SHdr.field_pic_flag != 0,pRefPicList0, pFields0,pReorderInfo_L0, uMaxPicNum, curr_slice->num_ref_idx_l0_active);
        curr_slice->m_NumRefsInL0List = curr_slice->num_ref_idx_l0_active;
    } else {
        curr_slice->num_ref_idx_l0_active = curr_slice->m_NumRefsInL0List;
    }

    // Modification of L1
    if (BPREDSLICE == slice_type) {
        if (curr_slice->b_PerformModificationOfL1) {
            curr_slice->num_ref_idx_l1_active = MAX(MIN(curr_slice->m_MaxNumActiveRefsInL1, curr_slice->m_NumRefsInL1List), 1);
            if (pReorderInfo_L1->num_entries > 0 && SHdr.ref_pic_list_reordering_flag_l1)
                // reorder ref pic list L1 in compliance with core_enc->m_ReorderInfoL1 and curr_slice->m_MaxNumActiveRefsInL1
                H264CoreEncoder_ReOrderRefPicList_8u16s(state, SHdr.field_pic_flag != 0,pRefPicList1, pFields1,pReorderInfo_L1, uMaxPicNum, curr_slice->num_ref_idx_l1_active);
            curr_slice->m_NumRefsInL1List = curr_slice->num_ref_idx_l1_active;
        } else {
            curr_slice->num_ref_idx_l1_active = curr_slice->m_NumRefsInL1List;
        }
    }

    curr_slice->num_ref_idx_active_override_flag = ((curr_slice->num_ref_idx_l0_active != core_enc->m_PicParamSet->num_ref_idx_l0_active)
                                                 || ((BPREDSLICE == slice_type) && (curr_slice->num_ref_idx_l1_active != core_enc->m_PicParamSet->num_ref_idx_l1_active)));

    // Construction of ref pic lists is completed. Perform some additional initialization.

    if (SHdr.field_pic_flag){
        Ipp32s i;
        for (i = 0; i < curr_slice->m_NumRefsInL0List; i++)
            pPOC0[i] = H264EncoderFrame_PicOrderCnt_8u16s(pRefPicList0[i], pFields0[i], 1);
        for (i = 0; i < curr_slice->m_NumRefsInL1List; i++)
            pPOC1[i] = H264EncoderFrame_PicOrderCnt_8u16s(pRefPicList1[i], pFields1[i], 1);
    } else {
        Ipp32s i;
        for (i = 0; i < curr_slice->m_NumRefsInL0List; i++)
            pPOC0[i] = H264EncoderFrame_PicOrderCnt_8u16s(pRefPicList0[i], 0, 3);
        for (i = 0; i < curr_slice->m_NumRefsInL1List; i++)
            pPOC1[i] = H264EncoderFrame_PicOrderCnt_8u16s(pRefPicList1[i], 0, 3);
    }

    // If B slice, init scaling factors array
    if ((BPREDSLICE == slice_type) && (pRefPicList1[0] != NULL)){
        H264CoreEncoder_InitDistScaleFactor_8u16s(state, curr_slice, curr_slice->num_ref_idx_l0_active, curr_slice->num_ref_idx_l1_active, pRefPicList0, pRefPicList1, pFields0,pFields1);
        H264CoreEncoder_InitMapColMBToList0_8u16s(curr_slice, curr_slice->num_ref_idx_l0_active, pRefPicList0, pRefPicList1 );
//                InitMVScale(curr_slice, curr_slice->num_ref_idx_l0_active, curr_slice->num_ref_idx_l1_active, pRefPicList0, pRefPicList1, pFields0,pFields1);
    }
    //update temporal refpiclists
    Ipp32s i;
    switch(core_enc->m_pCurrentFrame->m_PictureStructureForDec){
        case FLD_STRUCTURE:
        case FRM_STRUCTURE:
            for (i=0;i<MAX_NUM_REF_FRAMES;i++){
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[i]= pRefPicList0[i];
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_Prediction[i]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_Prediction[i]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[i]= pFields0[i];
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_POC[i]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_POC[i]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[i]= pPOC0[i];
            }
            for (i=0;i<MAX_NUM_REF_FRAMES;i++){
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_RefPicList[i]= pRefPicList1[i];
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_Prediction[i]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_Prediction[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_Prediction[i]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_Prediction[i]= pFields1[i];
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_POC[i]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_POC[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_POC[i]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_POC[i]= pPOC1[i];
        }
        break;
        case AFRM_STRUCTURE:
            for (i=0;i<MAX_NUM_REF_FRAMES / 2;i++){
                //frame part
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_RefPicList[i]= pRefPicList0[i];
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_Prediction[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_Prediction[i]= 0;
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_POC[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_POC[i]= pPOC0[i];
                //field part
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[2*i+0]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[2*i+0]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[2*i+1]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[2*i+1]= pRefPicList0[i];
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[2*i+0]=0;
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[2*i+1]=1;
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[2*i+0]=1;
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[2*i+1]=0;

                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[2*i+0]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[2*i+0]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[2*i+1]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[2*i+1]= pPOC0[i];
            }
            for (i=0;i<MAX_NUM_REF_FRAMES / 2;i++) {
            //frame part
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_RefPicList[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_RefPicList[i]= pRefPicList1[i];
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_Prediction[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_Prediction[i]= 0;
                curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_POC[i]=
                curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_POC[i]= pPOC1[i];
                //field part
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_RefPicList[2*i+0]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_RefPicList[2*i+0]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_RefPicList[2*i+1]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_RefPicList[2*i+1]= pRefPicList1[i];
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_Prediction[2*i+0]=0;
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_Prediction[2*i+1]=1;
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_Prediction[2*i+0]=1;
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_Prediction[2*i+1]=0;

                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_POC[2*i+0]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_POC[2*i+0]=
                curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_POC[2*i+1]=
                curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_POC[2*i+1]= pPOC1[i];
            }
            break;
        }

    return ps;
}    // UpdateRefPicList


#ifdef VM_SLICE_THREADING_H264
Status MFXVideoENCODEMVC::ThreadedSliceCompress(Ipp32s numTh){
#ifdef H264_NEW_THREADING
    H264CoreEncoder_8u16s* core_enc = m_cview->enc;
    void* state = m_cview->enc;
    EnumSliceType default_slice_type = m_taskParams.threads_data[numTh].default_slice_type;
    Ipp32s slice_qp_delta_default = m_taskParams.threads_data[numTh].slice_qp_delta_default;
    Ipp32s slice = m_taskParams.threads_data[numTh].slice;
#else
    H264CoreEncoder_8u16s* core_enc = threadSpec[numTh].core_enc;
    void* state = threadSpec[numTh].state;
    EnumSliceType default_slice_type = threadSpec[numTh].default_slice_type;
    Ipp32s slice_qp_delta_default = threadSpec[numTh].slice_qp_delta_default;
    Ipp32s slice = threadSpec[numTh].slice;
#endif // H264_NEW_THREADING

    core_enc->m_Slices[slice].m_slice_qp_delta = (Ipp8s)slice_qp_delta_default;
    core_enc->m_Slices[slice].m_slice_number = slice;
    core_enc->m_Slices[slice].m_slice_type = default_slice_type; // Pass to core encoder
    UpdateRefPicList(state, core_enc->m_Slices + slice, &core_enc->m_pCurrentFrame->m_refPicListCommon[core_enc->m_field_index], core_enc->m_SliceHeader, &core_enc->m_ReorderInfoL0[core_enc->m_field_index], &core_enc->m_ReorderInfoL1[core_enc->m_field_index]);
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

    // these values could be different for SVC. For AVC/MVC should be 0 and 15
    core_enc->m_Slices[slice].m_scan_idx_start = 0;
    core_enc->m_Slices[slice].m_scan_idx_end = 15;

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
                (alpha) ? NAL_UT_MFXVideoENCODEMVCNOPART :
#endif // ALPHA_BLENDING_H264
                ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE), startPicture), 0);
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
        H264_AVBR_PostFrame(&core_enc->avbr, pic_type, (Ipp32s)slice_bits); // is it thread safe???
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

Status MFXVideoENCODEMVC::ThreadCallBackVM_MBT(threadSpecificDataH264 &tsd)
{
    ThreadDataVM_MBT_8u16s *td = &tsd.mbt_data;
    H264CoreEncoder_8u16s *core_enc = td->core_enc;
    void *state = core_enc;
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
    Ipp32s pitchPixels = core_enc->m_pReconstructFrame->m_pitchPixels;
#ifdef FRAME_INTERPOLATION
    Ipp32s planeSize = core_enc->m_pReconstructFrame->m_PlaneSize;
    Ipp32s bitDepth = core_enc->m_PicParamSet->bit_depth_luma;
#endif
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
                    while (core_enc->mbReady_MBT[tRow] < tCol);
                    cur_mb.uMB = uMB;
                    cur_mb.uMBx = static_cast<Ipp16u>(tCol);
                    cur_mb.uMBy = static_cast<Ipp16u>(tRow - MBYAdjust);
                    cur_mb.lambda = lambda_sq[curr_slice->m_iLastXmittedQP];
                    cur_mb.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
                    cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                    cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << curr_slice->m_is_cur_mb_field;
                    H264CoreEncoder_UpdateCurrentMBInfo_8u16s(state, curr_slice);
                    cur_mb.cabac_data = &core_enc->gd_MBT[uMB].bc;
                    cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                    cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                    cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
                    sts = H264CoreEncoder_Put_MB_Real_8u16s(state, curr_slice);
                    if (sts != UMC_OK) {
                        status = sts;
                        break;
                    }
                    H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(pBitstream, (uMB == uFirstMB + uNumMBs - 1) || (core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB + 1].slice_id != curr_slice->m_slice_number));
                    H264CoreEncoder_ReconstuctCBP_8u16s(&cur_mb);
                }
            }
        } else {
            tRow = *td->incRow;
            H264Slice_8u16s *cur_s = &core_enc->m_Slices_MBT[tNum];
            cur_s->m_MB_Counter = 0;
            H264CurrentMacroblockDescriptor_8u16s &cur_mb = cur_s->m_cur_mb;
            while (tRow <= last_slice_row) {
                vm_mutex_lock(&core_enc->mutexIncRow);
                tRow = *td->incRow;
                (*td->incRow) ++;
#ifdef MB_THREADING_TW
                if (tRow < last_slice_row)
                    for (tCol = 0; tCol < core_enc->m_WidthInMBs; tCol ++)
                        vm_mutex_lock(core_enc->mMutexMT + tRow * core_enc->m_WidthInMBs + tCol);
#endif // MB_THREADING_TW
                vm_mutex_unlock(&core_enc->mutexIncRow);
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
                        cur_mb.lambda = lambda_sq[cur_s->m_iLastXmittedQP];
                        cur_mb.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
                        cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field];
                        cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << cur_s->m_is_cur_mb_field;
#ifdef MB_THREADING_TW
                        if ((tRow > first_slice_row) && (tCol < (core_enc->m_WidthInMBs - 1))) {
                            vm_mutex_lock(core_enc->mMutexMT + (tRow - 1) * core_enc->m_WidthInMBs + tCol + 1);
                            vm_mutex_unlock(core_enc->mMutexMT + (tRow - 1) * core_enc->m_WidthInMBs + tCol + 1);
                        }
#else
                        if ((tRow > first_slice_row) && (tCol < (core_enc->m_WidthInMBs - 1)))
                            while (core_enc->mbReady_MBT[tRow-1] <= tCol);
#endif // MB_THREADING_TW
                        H264CoreEncoder_LoadPredictedMBInfo_8u16s(state, cur_s);
                        H264CoreEncoder_UpdateCurrentMBInfo_8u16s(state, cur_s);
                        cur_mb.cabac_data = &core_enc->gd_MBT[uMB].bc;
                        cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                        cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                        cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
                        pSetMB8x8TSFlag(cur_mb.GlobalMacroblockInfo, 0);

                        H264CoreEncoder_MB_Decision_8u16s(state, cur_s);
                        cur_mb.LocalMacroblockInfo->cbp_bits = 0;
                        cur_mb.LocalMacroblockInfo->cbp_bits_chroma = 0;
                        H264CoreEncoder_CEncAndRecMB_8u16s(state, cur_s);
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
                                    H264CoreEncoder_DeblockSlice_8u16s(core_enc, cur_s, uMB - 1 * core_enc->m_WidthInMBs - 1, 1);
                                if (tCol == core_enc->m_WidthInMBs - 1)
                                    H264CoreEncoder_DeblockSlice_8u16s(core_enc, cur_s, uMB - 1 * core_enc->m_WidthInMBs, 1);
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

Status MFXVideoENCODEMVC::H264CoreEncoder_Compress_Slice_MBT(void* state, H264Slice_8u16s *curr_slice)
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
        core_enc->m_Slices_MBT[i].m_scan_idx_start = curr_slice->m_scan_idx_start;
        core_enc->m_Slices_MBT[i].m_scan_idx_end = curr_slice->m_scan_idx_end;
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
    H264BsReal_PutSliceHeader_8u16s(pBitstream,
        core_enc->m_SliceHeader,
        *core_enc->m_PicParamSet,
        *core_enc->m_SeqParamSet,
        core_enc->m_PicClass,
        curr_slice,
        core_enc->m_DecRefPicMarkingInfo[core_enc->m_field_index],
        core_enc->m_ReorderInfoL0[core_enc->m_field_index],
        core_enc->m_ReorderInfoL1[core_enc->m_field_index],
        &core_enc->m_svc_layer.svc_ext); // This field in not initialized in MVC ???

    if (core_enc->m_info.entropy_coding_mode)
    {
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
            MFX_INTERNAL_CPY(core_enc->m_Slices_MBT[i].m_pbitstream->context_array, curr_slice->m_pbitstream->context_array, 460 * sizeof(CABAC_CONTEXT));
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
    Ipp32s pitchPixels = core_enc->m_pReconstructFrame->m_pitchPixels;
#ifdef FRAME_INTERPOLATION
    Ipp32s planeSize = core_enc->m_pReconstructFrame->m_PlaneSize;
    Ipp32s bitDepth = core_enc->m_PicParamSet->bit_depth_luma;
#endif
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
                cur_mb.lambda = lambda_sq[curr_slice->m_iLastXmittedQP];
                cur_mb.chroma_format_idc = core_enc->m_PicParamSet->chroma_format_idc;
                cur_mb.mbPtr = core_enc->m_pCurrentFrame->m_pYPlane + core_enc->m_pMBOffsets[uMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
                cur_mb.mbPitchPixels = core_enc->m_pCurrentFrame->m_pitchPixels << curr_slice->m_is_cur_mb_field;
                H264CoreEncoder_UpdateCurrentMBInfo_8u16s(state, curr_slice);
                cur_mb.cabac_data = &core_enc->gd_MBT[uMB].bc;
                cur_mb.lumaQP = getLumaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                cur_mb.lumaQP51 = getLumaQP51(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->bit_depth_luma);
                cur_mb.chromaQP = getChromaQP(cur_mb.LocalMacroblockInfo->QP, core_enc->m_PicParamSet->chroma_qp_index_offset, core_enc->m_SeqParamSet->bit_depth_chroma);
                sts = H264CoreEncoder_Put_MB_Real_8u16s(state, curr_slice);
                if (sts != UMC_OK) {
                    status = sts;
                    break;
                }
                H264BsReal_EncodeFinalSingleBin_CABAC_8u16s(pBitstream, (uMB == m_first_mb_in_slice + (Ipp32s)curr_slice->m_MB_Counter - 1) || (core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB + 1].slice_id != curr_slice->m_slice_number));
                H264CoreEncoder_ReconstuctCBP_8u16s(&cur_mb);
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
                    PadMB_Luma_8u16s(core_enc->m_pReconstructFrame->m_pYPlane + core_enc->m_pMBOffsets[pMB].uLumaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], pitchPixels, padFlag, LUMA_PADDING);
                    if (core_enc->m_PicParamSet->chroma_format_idc)
#ifdef USE_NV12
                        PadMB_Chroma_8u16s(core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], pitchPixels, ch_X, ch_Y, padFlag, ch_P);
#else // USE_NV12
                        H264ENC_MAKE_NAME(PadMB_Chroma)(core_enc->m_pReconstructFrame->m_pUPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], core_enc->m_pReconstructFrame->m_pVPlane + core_enc->m_pMBOffsets[pMB].uChromaOffset[core_enc->m_is_cur_pic_afrm][cur_s->m_is_cur_mb_field], pitchPixels, ch_X, ch_Y, padFlag, ch_P);
#endif // USE_NV12
                }
            }
        }
#ifdef FRAME_INTERPOLATION
        if ((core_enc->m_Analyse & ANALYSE_ME_SUBPEL) && (core_enc->m_info.num_slices - 1 == curr_slice->m_slice_number))
        {
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

MFXVideoENCODEMVC::MFXVideoENCODEMVC(VideoCORE *core, mfxStatus *stat)
: VideoENCODE(),
  m_taskParams(),
  m_core(core),
  m_allocator(NULL),
  m_initValues(),
  m_au_count(0),
  m_au_view_count(0),
  m_vo_current_view(0),
  m_bf_data(0),
  m_ConstQP(0)
{
    ippStaticInit();
    m_isinitialized = false;
    m_isOpaque = false;
#ifdef VM_SLICE_THREADING_H264
    threadsAllocated = 0;
    threadSpec = 0;
    threads = 0;
#endif // VM_SLICE_THREADING_H264
    m_allocator_id = NULL;
    m_numviews = 0; m_views = m_cview = NULL;
    m_mvcext_data = NULL; m_mvcext_sz = 0;
    for(int cnt = 0; cnt<1024; cnt++) m_oidxmap[cnt] = -1;

    m_sps_views = 0; m_num_sps = 0; m_pps_views = 0; m_num_pps = 0;
    memset(&m_mvcdesc, 0, sizeof(m_mvcdesc));
    *stat = MFX_ERR_NONE;
}

MFXVideoENCODEMVC::~MFXVideoENCODEMVC()
{
    Close();
}

#ifdef H264_NEW_THREADING
mfxStatus MFXVideoENCODEMVC::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 ** reordered_surface, mfxEncodeInternalParams * /*pInternalParams*/, MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus           status = MFX_ERR_NONE;
    mfxFrameSurface1   *orig_surface;
    mfxU32              minBufferSize;
    mfxI16              voidx;
    mvcMVCEncJobDesc    job;
    bool                is_buffering = false;
    bool                is_runasync = false;

    if( !m_isinitialized ) return MFX_ERR_NOT_INITIALIZED;

    if (bs == 0) return MFX_ERR_NULL_PTR;

    //Check for enough bitstream buffer size
    if( bs->MaxLength < (bs->DataOffset + bs->DataLength))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    {
        // always check bitstream buffer size
        // even if task will not be submitted
        minBufferSize = 0;
        if(m_views[0].enc->m_info.m_viewOutput) {
            if(surface) {
                voidx = m_oidxmap[surface->Info.FrameId.ViewId];
            } else {
                voidx = (m_vo_current_view + 1) % m_numviews;
                if(m_views[voidx].m_frameCountBufferedSync == 0)
                    voidx = -1;
            }
            if(voidx != -1) {
                if (m_views[voidx].enc->brc && m_views[voidx].enc->m_SeqParamSet->vui_parameters_present_flag &&
                    m_views[voidx].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag) {
                    VideoBrcParams  curBRCParams;
                    m_views[voidx].enc->brc->GetParams(&curBRCParams);
                    minBufferSize = curBRCParams.HRDBufferSizeBytes;
                } else if (m_views[voidx].enc->m_info.rate_controls.method != H264_RCM_QUANT) {
                    minBufferSize = (mfxU32)(m_views[voidx].enc->m_info.info.bitrate / m_views[voidx].enc->m_info.info.framerate * 2 / 8);
                }
            }
        } else {
            for(voidx = 0; voidx <(mfxI16)m_numviews; voidx++) {
                if (m_views[voidx].enc->brc && m_views[voidx].enc->m_SeqParamSet->vui_parameters_present_flag &&
                    m_views[voidx].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag) {
                    VideoBrcParams  curBRCParams;
                    m_views[voidx].enc->brc->GetParams(&curBRCParams);
                    minBufferSize += curBRCParams.HRDBufferSizeBytes;
                } else if (m_views[voidx].enc->m_info.rate_controls.method != H264_RCM_QUANT) {
                    minBufferSize += (mfxU32)(m_views[voidx].enc->m_info.info.bitrate / m_views[voidx].enc->m_info.info.framerate * 2 / 8);
                }
            }
        }

        if( bs->MaxLength - bs->DataOffset - bs->DataLength < minBufferSize || bs->MaxLength - bs->DataOffset - bs->DataLength == 0)
            return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    if (bs->Data == 0)
        return MFX_ERR_NULL_PTR;

    pEntryPoint->pRoutine = TaskRoutine;
    pEntryPoint->pCompleteProc = TaskCompleteProc;
    pEntryPoint->pState = this;
    pEntryPoint->requiredNumThreads = m_views[0].enc->m_info.numThreads;
    pEntryPoint->pRoutineName = "EncodeMVC";

    orig_surface = GetOriginalSurface(surface);

    if (surface && !orig_surface)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(orig_surface) {
        orig_surface->Info.FrameId = surface->Info.FrameId;
        status = EncodeFrameCtrlCheck(ctrl, orig_surface);
        if (MFX_ERR_NONE == status || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status)
        {
            m_vo_current_view = voidx = m_oidxmap[orig_surface->Info.FrameId.ViewId];
            if(m_views[voidx].m_frameCountSync != m_au_count) return MFX_ERR_UNDEFINED_BEHAVIOR; // Incorrect view order in stream
            m_views[voidx].m_frameCountSync++; m_au_view_count++;
            m_core->IncreaseReference(&(orig_surface->Data));
            job.surface = surface;
            job.ctrl = ctrl;
            job.bs = bs;
            m_mvcjobs[voidx].push(job);

            if(!m_views[voidx].m_mfxVideoParam.mfx.EncodedOrder ) {
                Ipp32u noahead = (m_views[voidx].enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ? 0 : 1;
                if (m_views[voidx].m_frameCountSync > noahead && m_views[voidx].m_frameCountBufferedSync < (mfxU32)m_views[voidx].enc->m_info.B_frame_rate + 1 - noahead ) {
                    m_views[voidx].m_frameCountBufferedSync++;
                    is_buffering = true;
                }
            }

            if(m_au_view_count==m_numviews) {
                is_runasync = true; m_au_view_count = 0; m_au_count++;
            }
            if (m_views[0].enc->m_info.m_viewOutput)
                is_runasync = true;
        }
    } else {
        if (m_views[0].enc->m_info.m_viewOutput)
        {
            m_vo_current_view = (m_vo_current_view + 1) % m_numviews;
            if(m_views[m_vo_current_view].m_frameCountBufferedSync == 0)
                return MFX_ERR_MORE_DATA;

            m_views[m_vo_current_view].m_frameCountBufferedSync --;
            job.surface = 0;
            job.ctrl = 0;
            job.bs = bs;
            m_mvcjobs[m_vo_current_view].push(job);
        }
        else
        {
            if(m_views[0].m_frameCountBufferedSync == 0)
                return MFX_ERR_MORE_DATA;

            for(voidx = 0; voidx<(mfxI16)m_numviews; voidx++) {
                m_views[voidx].m_frameCountBufferedSync --;
                job.surface = 0;
                job.ctrl = 0;
                job.bs = bs;
                m_mvcjobs[voidx].push(job);
            }
        }
        is_runasync = true;
    }

    if (is_runasync) {
        MVCEncodeTaskInputParams *m_pTaskInputParams = (MVCEncodeTaskInputParams*)H264_Malloc(sizeof(MVCEncodeTaskInputParams));
        // MFX_ERR_MORE_DATA_SUBMIT_TASK means that frame will be buffered and will be encoded later. Output bitstream isn't required for this task
        m_pTaskInputParams->buffered = is_buffering;
        m_pTaskInputParams->jobs = new std::vector<mvcMVCEncJobDesc>(m_numviews);

        if (m_views[0].enc->m_info.m_viewOutput)
        {
            (*m_pTaskInputParams->jobs)[0] = m_mvcjobs[m_vo_current_view].front();
            m_mvcjobs[m_vo_current_view].pop();
        }
        else
        {
            for(mfxI16 voidx = 0; voidx <(mfxI16)m_numviews; voidx++) {
                (*m_pTaskInputParams->jobs)[voidx] = m_mvcjobs[voidx].front();
                m_mvcjobs[voidx].pop();
            }
        }
        pEntryPoint->pParam = m_pTaskInputParams;
        status = (is_buffering) ? (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK : status;

        if (m_views[0].enc->m_info.m_viewOutput && m_vo_current_view == 0 && (MFX_ERR_NONE == status || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status))
            status = MFX_ERR_MORE_BITSTREAM;
    } else {
        if (MFX_ERR_NONE == status || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == status) {
            pEntryPoint->pParam = 0;
            pEntryPoint->requiredNumThreads = 1;
            status = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;
        }
    }

    *reordered_surface = surface;

    return status;
}

Status MFXVideoENCODEMVC::EncodeNextSliceTask(mfxI32 thread_number, mfxI32 *slice_number)
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

mfxStatus MFXVideoENCODEMVC::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    H264ENC_UNREFERENCED_PARAMETER(threadNumber);
    H264ENC_UNREFERENCED_PARAMETER(callNumber);
//    TAL_SCOPED_TASK();

    if (pState == NULL) return MFX_ERR_NULL_PTR;
    if (pParam==NULL) return MFX_ERR_NONE;

    MFXVideoENCODEMVC *th = static_cast<MFXVideoENCODEMVC *>(pState);

    MVCEncodeTaskInputParams *pTaskParams = (MVCEncodeTaskInputParams*)pParam;

    if (th->m_taskParams.parallel_encoding_finished)
        return MFX_TASK_DONE;

    mfxI32 single_thread_selected = vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.single_thread_selected), 1, 0);
    if (!single_thread_selected)
    {
        // This task performs all single threaded regions
        mfxStatus   status = MFX_ERR_NONE;
        mfxI16      voidx;
        UMC::MediaData m_data_out;

        th->m_bf_stream.Reset();

        if (th->m_views[0].enc->m_info.m_viewOutput)
        {
            if(pTaskParams->buffered)
                (*pTaskParams->jobs)[0].bs->FrameType = 0;
            status = th->EncodeFrame((*pTaskParams->jobs)[0].ctrl, 0, (*pTaskParams->jobs)[0].surface, (*pTaskParams->jobs)[0].bs);
            th->m_taskParams.parallel_encoding_finished = true;

            if (status == MFX_ERR_NONE && th->m_bf_stream.GetDataSize() && (*pTaskParams->jobs)[0].bs)
            {
                mfxBitstream *bs = (*pTaskParams->jobs)[0].bs;
                m_data_out.SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);
                m_data_out.SetDataSize(bs->DataLength);
                th->m_bf_stream.MoveDataTo(&m_data_out);
                bs->DataLength = (mfxU32)m_data_out.GetDataSize();
            }
        }
        else
        {
            for(voidx = 0; voidx <(mfxI16)th->m_numviews; voidx++)
            {
                if(pTaskParams->buffered)
                    (*pTaskParams->jobs)[voidx].bs->FrameType = 0;
                status = th->EncodeFrame((*pTaskParams->jobs)[voidx].ctrl, 0, (*pTaskParams->jobs)[voidx].surface,
                    (*pTaskParams->jobs)[th->m_numviews - 1].bs);
                if(status != MFX_ERR_NONE)
                    break;
            }
            th->m_taskParams.parallel_encoding_finished = true;

            mfxBitstream *bs = (*pTaskParams->jobs)[th->m_numviews - 1].bs;
            if(th->m_bf_stream.GetDataSize() && bs)
            {
                m_data_out.SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);
                m_data_out.SetDataSize(bs->DataLength);
                th->m_bf_stream.MoveDataTo(&m_data_out);
                bs->DataLength = (mfxU32)m_data_out.GetDataSize();
            }
        }

        delete pTaskParams->jobs;
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
            if (thread_number >= th->m_cview->enc->m_info.numThreads)
                return MFX_TASK_BUSY;

            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&th->m_taskParams.parallel_executing_threads));
            Status umc_status = UMC_OK;

            if (!th->m_cview->enc->useMBT)
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

mfxStatus MFXVideoENCODEMVC::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)
{
    H264ENC_UNREFERENCED_PARAMETER(pParam);
    H264ENC_UNREFERENCED_PARAMETER(taskRes);
    if (pState == NULL)
        return MFX_ERR_NULL_PTR;

    MFXVideoENCODEMVC *th = static_cast<MFXVideoENCODEMVC *>(pState);

    th->m_taskParams.single_thread_selected = 0;
    th->m_taskParams.thread_number = 0;
    th->m_taskParams.parallel_encoding_finished = false;
    if(th->m_taskParams.single_thread_done) {
        for (mfxI32 i = 0; i < th->m_cview->enc->m_info.numThreads; i++)
        {
            th->m_taskParams.threads_data[i].core_enc = NULL;
            th->m_taskParams.threads_data[i].state = NULL;
            th->m_taskParams.threads_data[i].slice = 0;
            th->m_taskParams.threads_data[i].slice_qp_delta_default = 0;
            th->m_taskParams.threads_data[i].default_slice_type = INTRASLICE;
        }
        th->m_taskParams.single_thread_done = false;
    }

    return MFX_TASK_DONE;
}
#else
#ifdef VM_SLICE_THREADING_H264
Ipp32u VM_THREAD_CALLCONVENTION MFXVideoENCODEMVC::ThreadWorkingRoutine(void* ptr)
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

    if (th->m_lpOwner->enc->useMBT)
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

mfxFrameSurface1* MFXVideoENCODEMVC::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_isOpaque)
        return m_core->GetNativeSurface(surface);
    else
        return surface;
}

// function MFXVideoENCODEMVC::AnalyzePicStruct checks combination of (init PicStruct, runtime PicStruct) and converts this combination to internal format.
// Returns: parameter
//                      picStructForEncoding - internal format that used for encoding, pointer can be NULL
//          statuses
//                      MFX_ERR_NONE                       combination is applicable for encoding
//                      MFX_WRN_INCOMPATIBLE_VIDEO_PARAM   warning, combination isn't applicable, default PicStruct is used for encoding
//                      MFX_ERR_UNDEFINED_BEHAVIOR         error, combination isn't applicable, default PicStruct for encoding can't be choosen
//                      MFX_ERR_INVALID_VIDEO_PARAM        error, PicStruct was initialized incorrectly
mfxStatus MFXVideoENCODEMVC::AnalyzePicStruct(mfxMVCView *view, const mfxU16 picStructInSurface, mfxU16 *picStructForEncoding)
{
    H264CoreEncoder_8u16s  *cur_enc = view->enc;
    mfxVideoParam          *vp = &view->m_mfxVideoParam;
    mfxStatus               st = MFX_ERR_NONE;
    mfxU16                  picStruct = MFX_PICSTRUCT_UNKNOWN;

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
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
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
                break;
            case MFX_PICSTRUCT_UNKNOWN:
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

mfxStatus MFXVideoENCODEMVC::EncodeFrameCtrlCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface)
{
    mfxU16      internalPicStruct = MFX_PICSTRUCT_UNKNOWN;
    mfxU16      isFieldEncoding = 0;
    mfxStatus   st = MFX_ERR_NONE;
    mfxMVCView *checked_view;

    if(m_oidxmap[surface->Info.FrameId.ViewId]!=-1)
        checked_view=m_views+m_oidxmap[surface->Info.FrameId.ViewId];
    else
        return MFX_ERR_INVALID_HANDLE;

    if (surface->Info.Width < checked_view->enc->m_info.info.clip_info.width ||
        surface->Info.Height < checked_view->enc->m_info.info.clip_info.height ||
        //surface->Info.CropW > 0 && surface->Info.CropW != cur_enc->m_info.info.clip_info.width ||
        //surface->Info.CropH > 0 && surface->Info.CropH != cur_enc->m_info.info.clip_info.height ||
        surface->Info.ChromaFormat != checked_view->m_mfxVideoParam.mfx.FrameInfo.ChromaFormat)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // check PicStruct in surface
    st = AnalyzePicStruct(checked_view, surface->Info.PicStruct, &internalPicStruct);
    if (st < MFX_ERR_NONE) return st;

    /*if (surface->Info.Width != checked_view->enc->m_info.info.clip_info.width ||
        surface->Info.Height != checked_view->enc->m_info.info.clip_info.height)
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;*/

    isFieldEncoding = (checked_view->enc->m_info.coding_type == CODINGTYPE_PICAFF || checked_view->enc->m_info.coding_type == CODINGTYPE_INTERLACE) && !(internalPicStruct & MFX_PICSTRUCT_PROGRESSIVE);

    if (surface->Data.Y) {
        if (((surface->Info.FourCC == MFX_FOURCC_YV12) && (!surface->Data.U || !surface->Data.V)) ||
            ((surface->Info.FourCC == MFX_FOURCC_NV12) && (!surface->Data.UV)))
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        if (surface->Data.Pitch >= 0x8000 || !surface->Data.Pitch)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    } else {
        if (((surface->Info.FourCC == MFX_FOURCC_YV12) && (surface->Data.U || surface->Data.V)) ||
            ((surface->Info.FourCC == MFX_FOURCC_NV12) && surface->Data.UV))
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

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
                ctrl->Payload[i]->Type > SEI_TYPE_TONE_MAPPING_INFO)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    // check ref pic list control info
    if (ctrl && ctrl->ExtParam && ctrl->NumExtParam > 0) {
        mfxExtAVCRefListCtrl *pRefPicListCtrl = 0;
        // try to get ref pic list control info
        pRefPicListCtrl = (mfxExtAVCRefListCtrl*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);
        // ref pic list control info is ignored for B-frames
        if (pRefPicListCtrl)
            if (isFieldEncoding || (!checked_view->m_mfxVideoParam.mfx.EncodedOrder && checked_view->enc->m_info.B_frame_rate) || // field encoding or B-frames in GOP pattern
                (checked_view->m_mfxVideoParam.mfx.EncodedOrder && ((ctrl->FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)) == MFX_FRAMETYPE_B))) // B-frame in EncodedOrder
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if( checked_view->m_mfxVideoParam.mfx.EncodedOrder ) {
        if (!ctrl) // for EncodedOrder application should provide FrameType via ctrl
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        if (!checked_view->m_frameCountSync != !surface->Data.FrameOrder)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        if (ctrl) {
            mfxU16 firstFieldType = ctrl->FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B);
            mfxU16 secondFieldType = ctrl->FrameType & (MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xB);
            if ((firstFieldType != MFX_FRAMETYPE_I && firstFieldType != MFX_FRAMETYPE_P && firstFieldType != MFX_FRAMETYPE_B) ||
                (secondFieldType != 0 && secondFieldType != MFX_FRAMETYPE_xI && secondFieldType != MFX_FRAMETYPE_xP && secondFieldType != MFX_FRAMETYPE_xB))
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            else if (isFieldEncoding && secondFieldType != (firstFieldType << 8) && (firstFieldType != MFX_FRAMETYPE_I || secondFieldType != MFX_FRAMETYPE_xP)) {
                return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            if (!checked_view->m_frameCountSync) {
                switch(firstFieldType) {
                    case MFX_FRAMETYPE_I:
                        break;
                    case MFX_FRAMETYPE_P:
                        if(m_views[m_oidxmap[surface->Info.FrameId.ViewId]].m_dependency.NumAnchorRefsL0 == 0)
                            return MFX_ERR_UNDEFINED_BEHAVIOR;
                        break;
                    case MFX_FRAMETYPE_B:
                        if(m_views[m_oidxmap[surface->Info.FrameId.ViewId]].m_dependency.NumAnchorRefsL0 == 0 &&
                           m_views[m_oidxmap[surface->Info.FrameId.ViewId]].m_dependency.NumAnchorRefsL1 == 0)
                            return MFX_ERR_UNDEFINED_BEHAVIOR;
                        break;
                    default:
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
        }
    }

    return st;
}


mfxStatus MFXVideoENCODEMVC::InitMVCView(mfxVideoParam *par_in, mfxExtCodingOption *opts, mfxI16 viewoid)
{
    UMC::VideoBrcParams brcParams;

    H264EncoderParams   videoParams;
    mfxVideoInternalParam       checkedSetByTU = mfxVideoInternalParam();
    mfxStatus           QueryStatus, st;
    Status              status;
    mfxU32              cnt;
    mfxMVCView         *view = m_views + viewoid;
    mfxI16              d_voidx;

    mfxVideoInternalParam inInt = *par_in;
    mfxVideoInternalParam *par = &inInt;

    mfxExtMVCSeqDesc   *depinfo = (mfxExtMVCSeqDesc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
    mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );

    view->m_maxdepvoid = 0;
    view->m_is_ref4anchor = view->m_is_ref4nonanchor = false;
    view->m_dependency.ViewId = depinfo->View[viewoid].ViewId;
    view->m_dependency.NumAnchorRefsL0 = depinfo->View[viewoid].NumAnchorRefsL0;
    view->m_dependency.NumAnchorRefsL1 = depinfo->View[viewoid].NumAnchorRefsL1;
    view->m_dependency.NumNonAnchorRefsL0 = depinfo->View[viewoid].NumNonAnchorRefsL0;
    view->m_dependency.NumNonAnchorRefsL1 = depinfo->View[viewoid].NumNonAnchorRefsL1;
    for(cnt=0; cnt<view->m_dependency.NumAnchorRefsL0; cnt++) {
        d_voidx = m_oidxmap[depinfo->View[viewoid].AnchorRefL0[cnt]];
        if(d_voidx!=-1 && d_voidx < viewoid) {
            view->m_maxdepvoid = MAX(d_voidx,view->m_maxdepvoid);
            view->m_dependency.AnchorRefL0[cnt] = depinfo->View[viewoid].AnchorRefL0[cnt];
            m_views[d_voidx].m_is_ref4anchor = true;
        } else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    for(cnt=0; cnt<view->m_dependency.NumAnchorRefsL1; cnt++) {
        d_voidx = m_oidxmap[depinfo->View[viewoid].AnchorRefL1[cnt]];
        if(d_voidx!=-1 && d_voidx < viewoid) {
            view->m_maxdepvoid = MAX(d_voidx,view->m_maxdepvoid);
            view->m_dependency.AnchorRefL1[cnt] = depinfo->View[viewoid].AnchorRefL1[cnt];
            m_views[d_voidx].m_is_ref4anchor = true;
        } else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    for(cnt=0; cnt<view->m_dependency.NumNonAnchorRefsL0; cnt++) {
        d_voidx = m_oidxmap[depinfo->View[viewoid].NonAnchorRefL0[cnt]];
        if(d_voidx!=-1 && d_voidx < viewoid) {
            view->m_maxdepvoid = MAX(d_voidx,view->m_maxdepvoid);
            view->m_dependency.NonAnchorRefL0[cnt] = depinfo->View[viewoid].NonAnchorRefL0[cnt];
            m_views[d_voidx].m_is_ref4nonanchor = true;
        } else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    for(cnt=0; cnt<view->m_dependency.NumNonAnchorRefsL1; cnt++) {
        d_voidx = m_oidxmap[depinfo->View[viewoid].NonAnchorRefL1[cnt]];
        if(d_voidx!=-1 && d_voidx < viewoid) {
            view->m_maxdepvoid = MAX(d_voidx,view->m_maxdepvoid);
            view->m_dependency.NonAnchorRefL1[cnt] = depinfo->View[viewoid].NonAnchorRefL1[cnt];
            m_views[d_voidx].m_is_ref4nonanchor = true;
        } else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    if(viewoid && view->m_maxdepvoid>=viewoid) return MFX_ERR_INVALID_VIDEO_PARAM;

    /* Skip fake dependencies for base view */
    if(viewoid) {
        m_mvcext_sz += 5 + view->m_dependency.NumNonAnchorRefsL1 + view->m_dependency.NumNonAnchorRefsL0
                         + view->m_dependency.NumAnchorRefsL1 + view->m_dependency.NumAnchorRefsL0;
    } else {
        m_mvcext_sz += 1;
    }

    memset(&view->m_auxInput, 0, sizeof(mfxFrameSurface1));
    memset(&view->m_response, 0, sizeof(view->m_response));
    memset(&view->m_response_alien, 0, sizeof(view->m_response_alien));

    view->enc->free_slice_mode_enabled = (par->mfx.NumSlice == 0);
    videoParams.m_Analyse_on = 0;
    videoParams.m_Analyse_restrict = 0;
    st = ConvertVideoParam_H264enc(par, &videoParams); // converts all with preference rules
    MFX_CHECK_STS(st);

    if (m_isOpaque && opaqAllocReq->In.NumSurface < (videoParams.B_frame_rate + 1) * (m_numviews + 1))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    view->m_useAuxInput = false;
    view->m_useSysOpaq = false;
    view->m_useVideoOpaq = false;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_isOpaque) {
        bool bOpaqVideoMem = m_isOpaque && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
        bool bNeedAuxInput = (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) || bOpaqVideoMem;

        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));

        request.Info = par->mfx.FrameInfo;

        // try to allocate opaque surfaces in video memory for another component in transcoding chain
        if (bOpaqVideoMem) {
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin =request.NumFrameSuggested = (mfxU16)opaqAllocReq->In.NumSurface;

            st = m_core->AllocFrames(&request,
                                     &view->m_response_alien,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);

            if (MFX_ERR_NONE != st &&
                MFX_ERR_UNSUPPORTED != st) // unsupported means that current Core couldn;t allocate the surfaces
                return st;

            if (view->m_response_alien.NumFrameActual < request.NumFrameMin)
                return MFX_ERR_MEMORY_ALLOC;

            if (st != MFX_ERR_UNSUPPORTED)
                view->m_useVideoOpaq = true;
        }

        // allocate all we need in system memory
        request.Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
        if (bNeedAuxInput) {
            // allocate additional surface in system memory for FastCopy from video memory
            request.Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
            request.NumFrameMin       = 1;
            request.NumFrameSuggested = 1;
            st = m_core->AllocFrames(&request, &view->m_response);
            MFX_CHECK_STS(st);
        } else {
            // allocate opaque surfaces in system memory
            request.Type =  (mfxU16)opaqAllocReq->In.Type;
            request.NumFrameMin       = opaqAllocReq->In.NumSurface;
            request.NumFrameSuggested = opaqAllocReq->In.NumSurface;
            st = m_core->AllocFrames(&request,
                                     &view->m_response,
                                     opaqAllocReq->In.Surfaces,
                                     opaqAllocReq->In.NumSurface);
            MFX_CHECK_STS(st);
        }

        if (view->m_response.NumFrameActual < request.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        if (bNeedAuxInput) {
            view->m_useAuxInput = true;
            view->m_auxInput.Data.MemId = view->m_response.mids[0];
            view->m_auxInput.Info = request.Info;
        } else
            view->m_useSysOpaq = true;
    }

    videoParams.m_is_mvc_profile = true;
    videoParams.m_is_base_view = viewoid?false:true;

    if (videoParams.info.clip_info.width == 0 || videoParams.info.clip_info.height == 0 ||
        par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 ||
        (videoParams.rate_controls.method != H264_RCM_QUANT && videoParams.info.bitrate == 0) || videoParams.info.framerate == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // to set profile/level and related vars
    // first copy fields which could have been changed from TU

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
    if (opts && opts->CAVLC == MFX_CODINGOPTION_UNKNOWN)
        opts->CAVLC = (mfxU16)(videoParams.entropy_coding_mode ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);

    st = CheckProfileLevelLimits_H264enc(par, false, &checkedSetByTU);
    if (st != MFX_ERR_NONE) QueryStatus = st;

    if (par->mfx.CodecProfile > MFX_PROFILE_AVC_BASELINE && par->mfx.CodecLevel >= MFX_LEVEL_AVC_3)
        videoParams.use_direct_inference = 1;
    // get back mfx params to umc
    videoParams.profile_idc = (UMC::H264_PROFILE_IDC)ConvertCUCProfileToUMC( par->mfx.CodecProfile );
    videoParams.level_idc = ConvertCUCLevelToUMC( par->mfx.CodecLevel );
    videoParams.num_ref_frames = par->mfx.NumRefFrame;
    if (opts && opts->VuiNalHrdParameters == MFX_CODINGOPTION_OFF)
        videoParams.info.bitrate = par->calcParam.TargetKbps * 1000;
    else
        videoParams.info.bitrate = par->calcParam.MaxKbps * 1000;
    if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || (opts && opts->FramePicture == MFX_CODINGOPTION_ON))
        videoParams.coding_type = 0;

    // init brc (if not const QP)
    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        mfxVideoParam tmp = *par;
        par->GetCalcParams(&tmp);
        st = ConvertVideoParam_Brc(&tmp, &brcParams);
        MFX_CHECK_STS(st);
        brcParams.GOPPicSize = videoParams.key_frame_controls.interval;
        brcParams.GOPRefDist = videoParams.B_frame_rate + 1;
        brcParams.profile = videoParams.profile_idc;
        brcParams.level = videoParams.level_idc;

        view->m_brc = new UMC::H264BRC;
        status = view->m_brc->Init(&brcParams);

        if (m_ConstQP)
        {
            view->m_brc->SetQP(m_ConstQP, I_PICTURE);
            view->m_brc->SetQP(m_ConstQP, P_PICTURE);
            view->m_brc->SetQP(m_ConstQP, B_PICTURE);
        }

        st = h264enc_ConvertStatus(status);
        MFX_CHECK_STS(st);
        view->enc->brc = view->m_brc;
        
        view->m_brc->GetParams(&brcParams);
        if (brcParams.BRCMode != BRC_AVBR)
            view->enc->requiredBsBufferSize = brcParams.HRDBufferSizeBytes;
        else
            view->enc->requiredBsBufferSize = videoParams.info.clip_info.width * videoParams.info.clip_info.height * 3 / 2;
    }
    else { // const QP
        view->m_brc = NULL;
        view->enc->brc = NULL;
    }

    // Set low initial QP values to prevent BRC panic mode if superfast mode
    if(par->mfx.TargetUsage==MFX_TARGETUSAGE_BEST_SPEED && view->m_brc) {
        if(view->m_brc->GetQP(I_PICTURE) < 30) {
            view->m_brc->SetQP(30, I_PICTURE); view->m_brc->SetQP(30, B_PICTURE);
        }
    }

    //Reset frame count
    view->m_frameCountSync = 0;
    view->m_frameCountBufferedSync = 0;
    view->m_frameCount = 0;
    view->m_frameCountBuffered = 0;
    view->m_lastIDRframe = 0;
    view->m_lastIframe = 0;
    view->m_lastRefFrame = 0;

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

    view->m_mfxVideoParam.mfx = par->mfx;
    view->m_mfxVideoParam.calcParam = par->calcParam;
    view->m_mfxVideoParam.IOPattern = par->IOPattern;
    view->m_mfxVideoParam.Protected = 0;
    view->m_mfxVideoParam.AsyncDepth = par->AsyncDepth;

    // set like in par-files
    if( par->mfx.RateControlMethod == MFX_RATECONTROL_CBR )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_CBR;
    else if( par->mfx.RateControlMethod == MFX_RATECONTROL_CQP )
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_QUANT;
    else if (par->mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_AVBR;
    else
        videoParams.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_VBR;

    status = H264CoreEncoder_Init_8u16s(view->enc, &videoParams, m_allocator);

    if (view->enc->free_slice_mode_enabled)
        view->m_mfxVideoParam.mfx.NumSlice = 0;

    //Set specific parameters
    view->enc->m_info.write_access_unit_delimiters = 1;
    if(opts && opts->MaxDecFrameBuffering != MFX_CODINGOPTION_UNKNOWN){
        if ( opts->MaxDecFrameBuffering < view->enc->m_info.num_ref_frames)
            opts->MaxDecFrameBuffering = view->enc->m_info.num_ref_frames;
        view->enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag = true;
        view->enc->m_SeqParamSet->vui_parameters.motion_vectors_over_pic_boundaries_flag = 1;
        view->enc->m_SeqParamSet->vui_parameters.max_bytes_per_pic_denom = 2;
        view->enc->m_SeqParamSet->vui_parameters.max_bits_per_mb_denom = 1;
        view->enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_horizontal = 11;
        view->enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_vertical = 11;
        if (view->enc->m_info.B_frame_rate > 1 && view->enc->m_info.treat_B_as_reference)
            view->enc->m_SeqParamSet->vui_parameters.num_reorder_frames = 2;
        else if (view->enc->m_info.B_frame_rate)
            view->enc->m_SeqParamSet->vui_parameters.num_reorder_frames = 1;
        else
            view->enc->m_SeqParamSet->vui_parameters.num_reorder_frames = 0;
        view->enc->m_SeqParamSet->vui_parameters.max_dec_frame_buffering = opts->MaxDecFrameBuffering;
    }
    if(opts && opts->AUDelimiter == MFX_CODINGOPTION_OFF){
        view->enc->m_info.write_access_unit_delimiters = 0;
    //}else{
        //    enc->m_info.write_access_unit_delimiters = 0;
    }

    if(opts && opts->EndOfSequence == MFX_CODINGOPTION_ON)
        view->m_putEOSeq = true;
    else
        view->m_putEOSeq = false;

    if(opts && opts->EndOfStream == MFX_CODINGOPTION_ON)
        view->m_putEOStream = true;
    else
        view->m_putEOStream = false;

    if(opts && opts->PicTimingSEI == MFX_CODINGOPTION_ON)
        view->m_putTimingSEI = true;
    else
        view->m_putTimingSEI = false;

    if(opts && opts->ResetRefList == MFX_CODINGOPTION_ON)
        view->m_resetRefList = true;
    else
        view->m_resetRefList = false;

    view->enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
    if( par->mfx.FrameInfo.AspectRatioW != 0 && par->mfx.FrameInfo.AspectRatioH != 0 ){
        view->enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = (Ipp8u)ConvertSARtoIDC_H264enc(par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);
        if (view->enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 0) {
            view->enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = 255; //Extended SAR
            view->enc->m_SeqParamSet->vui_parameters.sar_width = par->mfx.FrameInfo.AspectRatioW;
            view->enc->m_SeqParamSet->vui_parameters.sar_height = par->mfx.FrameInfo.AspectRatioH;
        }
    }

    // set VUI video signal info
    if (videoSignalInfo != NULL) {
        view->enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = 1;
        view->enc->m_SeqParamSet->vui_parameters.video_format = videoSignalInfo->VideoFormat;
        view->enc->m_SeqParamSet->vui_parameters.video_full_range_flag = videoSignalInfo->VideoFullRange ? 1 : 0;
        view->enc->m_SeqParamSet->vui_parameters.colour_description_present_flag = videoSignalInfo->ColourDescriptionPresent ? 1 : 0;
        if (view->enc->m_SeqParamSet->vui_parameters.colour_description_present_flag) {
            view->enc->m_SeqParamSet->vui_parameters.colour_primaries = videoSignalInfo->ColourPrimaries;
            view->enc->m_SeqParamSet->vui_parameters.transfer_characteristics = videoSignalInfo->TransferCharacteristics;
            view->enc->m_SeqParamSet->vui_parameters.matrix_coefficients = videoSignalInfo->MatrixCoefficients;
        }
    }

    if (par->mfx.FrameInfo.FrameRateExtD && par->mfx.FrameInfo.FrameRateExtN) {
        view->enc->m_SeqParamSet->vui_parameters.timing_info_present_flag = 1;
        view->enc->m_SeqParamSet->vui_parameters.num_units_in_tick = par->mfx.FrameInfo.FrameRateExtD;
        view->enc->m_SeqParamSet->vui_parameters.time_scale = par->mfx.FrameInfo.FrameRateExtN * 2;
        view->enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = 1;
    }
    else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    view->enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = 0;
    view->enc->m_SeqParamSet->vui_parameters.chroma_loc_info_present_flag = 0;

    if (opts != NULL)
    {
        if( opts->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN || opts->VuiNalHrdParameters == MFX_CODINGOPTION_ON)
        {
            view->enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
            view->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP && par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
                view->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 1;
            else {
                view->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
                if (opts->PicTimingSEI != MFX_CODINGOPTION_ON)
                    view->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 0;
            }
        }else {
            view->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
            if (opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                view->enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
        }
    }

    view->enc->m_FieldStruct = par->mfx.FrameInfo.PicStruct;

    if (view->enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag == 1)
    {
        VideoBrcParams  curBRCParams;

        view->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_cnt_minus1 = 0;
        view->enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale = 0;
        view->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale = 3;

        if (view->enc->brc)
        {
            view->enc->brc->GetParams(&curBRCParams);

            if (curBRCParams.maxBitrate)
                view->enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_value_minus1[0] =
                (curBRCParams.maxBitrate >> (6 + view->enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale)) - 1;
            else
                status = UMC_ERR_FAILED;

            if (curBRCParams.HRDBufferSizeBytes)
                view->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_value_minus1[0] =
                (curBRCParams.HRDBufferSizeBytes  >> (1 + view->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale)) - 1;
            else
                status = UMC_ERR_FAILED;

            view->enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0] = (curBRCParams.BRCMode == BRC_CBR) ? 1 : 0;
        }
        else
            status = UMC_ERR_NOT_ENOUGH_DATA;

        view->enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = 23; // default
        view->enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = 23; // default
        view->enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1 = 23; // default
        view->enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length = 24; // default
    }

#ifdef H264_HRD_CPB_MODEL
    enc->m_SEIData.m_tickDuration = ((Ipp64f)enc->m_SeqParamSet->vui_parameters.num_units_in_tick) / enc->m_SeqParamSet->vui_parameters.time_scale;
#endif // H264_HRD_CPB_MODEL

    if (opts)
    {
        view->m_extOption = *opts;
        view->m_pExtBuffer = &view->m_extOption.Header;
        view->m_mfxVideoParam.ExtParam = &view->m_pExtBuffer;
        view->m_mfxVideoParam.NumExtParam = 1;
    }

    st = ConvertVideoParamBack_H264enc(&view->m_mfxVideoParam, view->enc);
    MFX_CHECK_STS(st);

    // allocate empty frames to cpb
    UMC::Status ps = H264EncoderFrameList_Extend_8u16s(
        &view->enc->m_cpb,
        view->m_mfxVideoParam.mfx.NumRefFrame + (view->m_mfxVideoParam.mfx.EncodedOrder ? 1 : view->m_mfxVideoParam.mfx.GopRefDist),
        view->enc->m_info.info.clip_info.width,
        view->enc->m_info.info.clip_info.height,
        view->enc->m_PaddedSize,
        UMC::NV12,
        view->enc->m_info.num_slices * ((view->enc->m_info.coding_type == 1 || view->enc->m_info.coding_type == 3) + 1)
#if defined ALPHA_BLENDING_H264
        , view->enc->m_SeqParamSet->aux_format_idc
#endif
    );
    if (ps != UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;


    // allocate frame for recode
    // do it always to allow switching
    view->enc->m_pReconstructFrame_allocated = (H264EncoderFrame_8u16s *)H264_Malloc(sizeof(H264EncoderFrame_8u16s));
    if (!view->enc->m_pReconstructFrame_allocated)
        return MFX_ERR_MEMORY_ALLOC;
    H264EncoderFrame_Create_8u16s(view->enc->m_pReconstructFrame_allocated, &view->enc->m_cpb.m_pHead->m_data, view->enc->memAlloc,
#if defined ALPHA_BLENDING_H264
        view->enc->m_SeqParamSet->aux_format_idc,
#endif // ALPHA_BLENDING_H264
        0);
    if (H264EncoderFrame_allocate_8u16s(view->enc->m_pReconstructFrame_allocated, view->enc->m_PaddedSize, view->enc->m_info.num_slices * ((view->enc->m_info.coding_type == 1 || view->enc->m_info.coding_type == 3) + 1)))
        return MFX_ERR_MEMORY_ALLOC;

    st = h264enc_ConvertStatus(status);

    view->enc->m_view_id = view->m_dependency.ViewId;
    view->enc->m_is_mvc_profile = true;
    m_oidxmap[view->m_dependency.ViewId] = viewoid;

    return st;
}

mfxStatus MFXVideoENCODEMVC::InitAllocMVCExtension()
{
    mfxU8   num_applicable_ops[256];
    mfxU32  levels_signaled, i, j, k, pos;

    memset(num_applicable_ops, 0, 256); levels_signaled = 0; pos = 0;

    for(i=0; i<m_mvcdesc.NumOP; i++) {
        for(j=0; j<m_mvcdesc.OP[i].NumTargetViews; j++) {
            if(m_views[m_oidxmap[m_mvcdesc.OP[i].TargetViewId[j]]].enc->m_SeqParamSet->level_idc > m_mvcdesc.OP[i].LevelIdc)
                m_mvcdesc.OP[i].LevelIdc = m_views[m_oidxmap[m_mvcdesc.OP[i].TargetViewId[j]]].enc->m_SeqParamSet->level_idc;
        }
    }

    for(i=0; i<m_mvcdesc.NumOP; i++) {
        num_applicable_ops[ConvertCUCLevelToUMC(m_mvcdesc.OP[i].LevelIdc)]++;
        m_mvcext_sz+=m_mvcdesc.OP[i].NumTargetViews;
    }
    for(i=0; i<256; i++) if(num_applicable_ops[i]>0) levels_signaled++;

    m_mvcext_sz += 1 + 2*levels_signaled; // num_level_values_signaled_minus1
    m_mvcext_sz += 3*m_mvcdesc.NumOP;

    m_mvcext_data = new mfxU32[m_mvcext_sz];
    if(m_mvcext_data==NULL) return MFX_ERR_MEMORY_ALLOC;

    m_mvcext_data[pos++] = m_numviews - 1;

    for(i=0;i<m_numviews;i++)
        m_mvcext_data[pos++] = m_views[i].m_dependency.ViewId;

    for(i=1;i<m_numviews;i++) {
        m_mvcext_data[pos++] = m_views[i].m_dependency.NumAnchorRefsL0;
        for(j=0;j<m_views[i].m_dependency.NumAnchorRefsL0;j++)
            m_mvcext_data[pos++] = m_views[i].m_dependency.AnchorRefL0[j];
        m_mvcext_data[pos++] = m_views[i].m_dependency.NumAnchorRefsL1;
        for(j=0;j<m_views[i].m_dependency.NumAnchorRefsL1;j++)
            m_mvcext_data[pos++] = m_views[i].m_dependency.AnchorRefL1[j];
    }

    for(i=1;i<m_numviews;i++) {
        m_mvcext_data[pos++] = m_views[i].m_dependency.NumNonAnchorRefsL0;
        for(j=0;j<m_views[i].m_dependency.NumNonAnchorRefsL0;j++)
            m_mvcext_data[pos++] = m_views[i].m_dependency.NonAnchorRefL0[j];
        m_mvcext_data[pos++] = m_views[i].m_dependency.NumNonAnchorRefsL1;
        for(j=0;j<m_views[i].m_dependency.NumNonAnchorRefsL1;j++)
            m_mvcext_data[pos++] = m_views[i].m_dependency.NonAnchorRefL1[j];
    }

    m_mvcext_data[pos++] = levels_signaled - 1;
    for(i=0;i<256;i++) {
        if(num_applicable_ops[i]==0) continue;
        m_mvcext_data[pos++] = ConvertUMCLevelToCUC(i) | 8 << 24; // use high byte to store number of valid bits for pak
        m_mvcext_data[pos++] = num_applicable_ops[i] - 1;
        for(j=0;j<m_mvcdesc.NumOP;j++) {
            if(ConvertCUCLevelToUMC(m_mvcdesc.OP[j].LevelIdc)!=i) continue;
            m_mvcext_data[pos++] = m_mvcdesc.OP[j].TemporalId | 3 << 24; // use high byte to store number of valid bits for pak
            m_mvcext_data[pos++] = m_mvcdesc.OP[j].NumTargetViews - 1;
            for(k=0; k<m_mvcdesc.OP[j].NumTargetViews; k++) m_mvcext_data[pos++] = m_mvcdesc.OP[j].TargetViewId[k];
            m_mvcext_data[pos++] = m_mvcdesc.OP[j].NumViews - 1;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::InitAllocMVCSeqDesc(mfxExtMVCSeqDesc *depinfo)
{
    mfxU16      i;

    m_mvcdesc.NumView = depinfo->NumView;

    if(depinfo->View && depinfo->NumViewAlloc < depinfo->NumView) return MFX_ERR_INVALID_VIDEO_PARAM;
    if(depinfo->ViewId && depinfo->NumViewIdAlloc < depinfo->NumViewId) return MFX_ERR_INVALID_VIDEO_PARAM;
    if(depinfo->OP && depinfo->NumOPAlloc < depinfo->NumOP) return MFX_ERR_INVALID_VIDEO_PARAM;
    if(depinfo->View && depinfo->ViewId && depinfo->OP == NULL) return MFX_ERR_INVALID_VIDEO_PARAM;

    m_mvcdesc.Header.BufferId = depinfo->Header.BufferId;
    m_mvcdesc.Header.BufferSz = depinfo->Header.BufferSz;

    m_mvcdesc.View = new mfxMVCViewDependency[m_mvcdesc.NumView];
    if(m_mvcdesc.View==NULL) return MFX_ERR_MEMORY_ALLOC;
    m_mvcdesc.NumViewAlloc = m_mvcdesc.NumView;

    if(depinfo->View==NULL) {
        // Initialization case 3, only NumView defined
        memset(m_mvcdesc.View, 0, m_mvcdesc.NumView * sizeof(mfxMVCViewDependency));
        for(i = 1; i<m_mvcdesc.NumView; i++) {
            m_mvcdesc.View[i].ViewId = i;
            m_mvcdesc.View[i].NumAnchorRefsL0 = 1;
            m_mvcdesc.View[i].NumNonAnchorRefsL0 = 1;
        }
        m_mvcdesc.ViewId = new mfxU16[m_mvcdesc.NumView];
        if(m_mvcdesc.ViewId==NULL) return MFX_ERR_MEMORY_ALLOC;
        m_mvcdesc.NumViewId = m_mvcdesc.NumViewIdAlloc = m_mvcdesc.NumView;
        for(i = 0; i<m_mvcdesc.NumView; i++) m_mvcdesc.ViewId[i] = i;
        m_mvcdesc.OP = new mfxMVCOperationPoint[1];
        if(m_mvcdesc.OP==NULL) return MFX_ERR_MEMORY_ALLOC;
        m_mvcdesc.NumOP = m_mvcdesc.NumOPAlloc = 1;
        m_mvcdesc.OP[0].LevelIdc = 0;
        m_mvcdesc.OP[0].NumTargetViews = m_mvcdesc.NumView;
        m_mvcdesc.OP[0].NumViews =  m_mvcdesc.NumView;
        m_mvcdesc.OP[0].TargetViewId = m_mvcdesc.ViewId;
        m_mvcdesc.OP[0].TemporalId = 0;
    } else if(depinfo->ViewId==NULL) {
        // Initialization case 2, NumView + View dependency defined
        MFX_INTERNAL_CPY(m_mvcdesc.View, depinfo->View, m_mvcdesc.NumView * sizeof(mfxMVCViewDependency));

        m_mvcdesc.ViewId = new mfxU16[m_mvcdesc.NumView];
        if(m_mvcdesc.ViewId==NULL) return MFX_ERR_MEMORY_ALLOC;
        m_mvcdesc.NumViewId = m_mvcdesc.NumViewIdAlloc = m_mvcdesc.NumView;
        for(i = 0; i<m_mvcdesc.NumView; i++) m_mvcdesc.ViewId[i] = m_mvcdesc.View[i].ViewId;

        m_mvcdesc.OP = new mfxMVCOperationPoint[1];
        if(m_mvcdesc.OP==NULL) return MFX_ERR_MEMORY_ALLOC;
        m_mvcdesc.NumOP = m_mvcdesc.NumOPAlloc = 1;
        m_mvcdesc.OP[0].LevelIdc = 0;
        m_mvcdesc.OP[0].NumTargetViews = m_mvcdesc.NumView;
        m_mvcdesc.OP[0].NumViews =  m_mvcdesc.NumView;
        m_mvcdesc.OP[0].TargetViewId = m_mvcdesc.ViewId;
        m_mvcdesc.OP[0].TemporalId = 0;
    } else {
        // Initialization case 1, all fields are defined
        MFX_INTERNAL_CPY(m_mvcdesc.View, depinfo->View, m_mvcdesc.NumView * sizeof(mfxMVCViewDependency));

        m_mvcdesc.ViewId = new mfxU16[depinfo->NumViewId];
        if(m_mvcdesc.ViewId==NULL) return MFX_ERR_MEMORY_ALLOC;
        m_mvcdesc.NumViewId = m_mvcdesc.NumViewIdAlloc = depinfo->NumViewId;
        MFX_INTERNAL_CPY(m_mvcdesc.ViewId, depinfo->ViewId, depinfo->NumViewId * sizeof(mfxU16));

        m_mvcdesc.OP = new mfxMVCOperationPoint[depinfo->NumOP];
        if(m_mvcdesc.OP==NULL) return MFX_ERR_MEMORY_ALLOC;
        m_mvcdesc.NumOP = m_mvcdesc.NumOPAlloc = depinfo->NumOP;
        MFX_INTERNAL_CPY(m_mvcdesc.OP, depinfo->OP, depinfo->NumOP * sizeof(mfxMVCOperationPoint));

        for(i=0; i<m_mvcdesc.NumOP; i++) {
            ptrdiff_t offset = depinfo->OP[i].TargetViewId - depinfo->ViewId;
            m_mvcdesc.OP[i].TargetViewId = m_mvcdesc.ViewId + offset;
        }

    }

    return MFX_ERR_NONE;
}

void DistributeBrcParamters(mfxVideoParam * par_in, mfxVideoParam * base_in, mfxVideoParam * dependent_in, mfxU32 numviews)
{
    if (par_in->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        const mfxF32 base_sf = 0.2f;

        mfxVideoInternalParam par = *par_in;
        mfxVideoInternalParam base = *base_in;
        mfxVideoInternalParam dependent = *dependent_in;

        dependent.calcParam.TargetKbps       = (mfxU32)floor((par.calcParam.TargetKbps       / (numviews + base_sf)) + 0.5f);
        dependent.calcParam.BufferSizeInKB   = (mfxU16)floor((par.calcParam.BufferSizeInKB   / (numviews + base_sf)) + 0.5f);
        dependent.calcParam.InitialDelayInKB = (mfxU16)floor((par.calcParam.InitialDelayInKB / (numviews + base_sf)) + 0.5f);
        dependent.calcParam.MaxKbps          = (mfxU32)floor((par.calcParam.MaxKbps          / (numviews + base_sf)) + 0.5f);

        base.calcParam.TargetKbps       = par.calcParam.TargetKbps       - (numviews - 1) * dependent.calcParam.TargetKbps;
        base.calcParam.BufferSizeInKB   = par.calcParam.BufferSizeInKB   - (numviews - 1) * dependent.calcParam.BufferSizeInKB;
        base.calcParam.InitialDelayInKB = par.calcParam.InitialDelayInKB - (numviews - 1) * dependent.calcParam.InitialDelayInKB;
        base.calcParam.MaxKbps          = par.calcParam.MaxKbps          - (numviews - 1) * dependent.calcParam.MaxKbps;

        base.GetCalcParams(base_in);
        dependent.GetCalcParams(dependent_in);
    }
}

mfxStatus MFXVideoENCODEMVC::Init(mfxVideoParam* par_in)
{
    UMC::MemoryAllocatorParams pParams;
    mfxStatus st, QueryStatus;
    mfxVideoParam* par = par_in;
    Status status;
    mfxU32 i, idx;

    if(m_isinitialized) return MFX_ERR_UNDEFINED_BEHAVIOR;
    if(par == NULL) return MFX_ERR_NULL_PTR;

    st = CheckExtBuffers_H264enc( par->ExtParam, par->NumExtParam );
    if (st != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if(GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if(GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS ))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOption* opts = GetExtCodingOptions( par->ExtParam, par->NumExtParam );
    mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtCodingOptionDDI* extOptDdi = (mfxExtCodingOptionDDI *) GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DDI);
    mfxExtMVCSeqDesc   *depinfo = (mfxExtMVCSeqDesc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    mfxExtCodingOption2* opts2 = (mfxExtCodingOption2*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );

    if (extOptDdi) { m_ConstQP = extOptDdi->ConstQP; }

    // Stereo profiles must be initialized with partialy filled mfxExtMVCSeqDesc
    if(depinfo==NULL) return MFX_ERR_INVALID_VIDEO_PARAM;
    if(depinfo->NumView > 1024) return MFX_ERR_INVALID_VIDEO_PARAM;

    m_numviews = depinfo->NumView;

    mfxVideoParam            checked[2];
    mfxExtCodingOption       checked_ext;
    mfxExtVideoSignalInfo    checked_videoSignalInfo;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtCodingOption2      checked_ext2;

    mfxExtBuffer            *ptr_checked_ext[5] = {0,};
    mfxU16                   ext_counter = 0;

    st = InitAllocMVCSeqDesc(depinfo); // Initialize m_mvcdesc performing required allocation
    if (st != MFX_ERR_NONE) return st;

    checked[0] = *par;
    if (opts) {
        checked_ext = *opts;
        ptr_checked_ext[ext_counter++] = &checked_ext.Header;
    } else {
        memset(&checked_ext, 0, sizeof(checked_ext));
        checked_ext.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
        checked_ext.Header.BufferSz = sizeof(checked_ext);
    }
    if (videoSignalInfo) {
        checked_videoSignalInfo = *videoSignalInfo;
        ptr_checked_ext[ext_counter++] = &checked_videoSignalInfo.Header;
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (opts2) {
        checked_ext2 = *opts2;
        ptr_checked_ext[ext_counter++] = &checked_ext2.Header;
    }

    ptr_checked_ext[ext_counter++] = &m_mvcdesc.Header;

    checked[0].ExtParam    = ptr_checked_ext;
    checked[0].NumExtParam = ext_counter;

    // Initial sanity check and possible bitrate adjustment
    for(i=0; i<par->NumExtParam; i++) {
        if(par->ExtParam[i]->BufferId == MFX_EXTBUFF_MVC_SEQ_DESC) {
            par->ExtParam[i] = (mfxExtBuffer *)&m_mvcdesc; break;
        }
    }
    QueryStatus = Query(par, checked);
    for(i=0; i<par->NumExtParam; i++) {
        if(par->ExtParam[i]->BufferId == MFX_EXTBUFF_MVC_SEQ_DESC) {
            par->ExtParam[i] = (mfxExtBuffer *)depinfo; break;
        }
    }
    if (QueryStatus != MFX_ERR_NONE && QueryStatus != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && QueryStatus != MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (opaqAllocReq)
        opaqAllocReq = &checked_opaqAllocReq;

    if (opts2)
        opts2 = &checked_ext2;

    if ((checked[0].IOPattern & 0xffc8) || (checked[0].IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (checked[0].IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
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

    // Prepare BRC related parameters to match base view settings with bv/dv scale factor
    checked[1] = checked[0];

    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        DistributeBrcParamters(par, &checked[0], &checked[1], m_numviews);
    }

    m_initValues.FrameWidth = par->mfx.FrameInfo.Width;
    m_initValues.FrameHeight = par->mfx.FrameInfo.Height;

    mfxVideoInternalParam tmp0 = checked[0];
    mfxVideoInternalParam tmp1 = checked[1];

    checked[0].mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    st = CheckProfileLevelLimits_H264enc(&tmp0, true);
    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    st = CheckProfileLevelLimits_H264enc(&tmp1, true);
    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (checked->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        tmp0.GetCalcParams(checked);
        tmp1.GetCalcParams(checked + 1);
    }

/*
    // workaround for default entropy_coding_mode_flag with opts == 0
    // need to set correct profile for TU 5
    if (!opts) {
        ptr_checked_ext[ext_counter++] = &bv_checked_ext.Header;
        bv_checked.NumExtParam = ext_counter;
    }
*/

    m_allocator = new mfx_UMC_MemAllocator;
    if (!m_allocator) return MFX_ERR_MEMORY_ALLOC;

    m_allocator->InitMem(&pParams, m_core);

    // Allocate and initialize zero terminated arrays of pointers of SPS and PPS
    m_num_sps = 3;
    m_sps_views = new H264SeqParamSet [m_num_sps];
    if(!m_sps_views) return MFX_ERR_MEMORY_ALLOC;
    memset(m_sps_views, 0, m_num_sps*sizeof(H264SeqParamSet));
    for(i=0; i<m_num_sps-1; i++) {
        m_sps_views[i].profile_idc = H264_MAIN_PROFILE;
        m_sps_views[i].chroma_format_idc = 1;
        m_sps_views[i].bit_depth_luma = 8;
        m_sps_views[i].bit_depth_chroma = 8;
        m_sps_views[i].bit_depth_aux = 8;
        m_sps_views[i].alpha_opaque_value = 8;
        m_sps_views[i].seq_parameter_set_id = i;
    }

    m_num_pps = 3;
    m_pps_views = new H264PicParamSet [m_num_pps];
    if(!m_pps_views) return MFX_ERR_MEMORY_ALLOC;
    memset(m_pps_views, 0, m_num_pps*sizeof(H264PicParamSet));
    for(i=0; i<m_num_pps-1; i++) {
        m_pps_views[i].seq_parameter_set_id = (i)?1:0;
        m_pps_views[i].pic_parameter_set_id = i;
    }

    // Allocate and initialize views
    m_views = new mfxMVCView [m_numviews];
    if(!m_views) return MFX_ERR_MEMORY_ALLOC;
    memset(m_views, 0, m_numviews*sizeof(mfxMVCView));

    m_au_count = 0;
    m_au_view_count = 0;
    m_mvcjobs.reserve(m_numviews);
    m_mvcjobs.resize(m_numviews);

    checked[0].mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    m_mvcext_sz = 1;

    for(i=0; i<m_numviews; i++) {
        idx = (i) ? 1 : 0;
    #define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
        H264ENC_CALL_NEW(status, H264CoreEncoder, m_views[i].enc);
    #undef H264ENC_MAKE_NAME
        if (status != UMC_OK) { st = MFX_ERR_NOT_INITIALIZED; }
        m_views[i].enc->m_SeqParamSet = m_sps_views + idx;
        m_views[i].enc->m_PicParamSet = m_pps_views + idx;
        st = InitMVCView(checked+idx, &checked_ext, i);
        if (st != MFX_ERR_NONE) return st;
        H264CoreEncoder_SetSequenceParameters_8u16s(m_views[i].enc);
        H264CoreEncoder_SetPictureParameters_8u16s(m_views[i].enc);
        if (opts)
            m_views[i].enc->m_info.m_viewOutput = MFX_CODINGOPTION_ON == opts->ViewOutput;
        else
            m_views[i].enc->m_info.m_viewOutput = false;
    }

    st = InitAllocMVCExtension();
    if (st != MFX_ERR_NONE) return st;

    for(i=0; i<m_num_sps-1; i++) {
        m_sps_views[i].mvc_extension_payload = m_mvcext_data;
        m_sps_views[i].mvc_extension_size = m_mvcext_sz;
    }

    // Only last view should write EOSQ and EOST if needed
    for(i=0; i<m_numviews-1; i++) {
        m_views[i].m_putEOSeq = false;
        m_views[i].m_putEOStream = false;
    }

    if(checked[0].mfx.FrameInfo.PicStruct!=MFX_PICSTRUCT_PROGRESSIVE) {
        Ipp32u  bsSize =  m_numviews * m_views[0].enc->m_PaddedSize.width * m_views[0].enc->m_PaddedSize.height * sizeof(Ipp8u) * 2;

        m_bf_data = (Ipp8u*)H264_Malloc(bsSize + DATA_ALIGN);
        if (m_bf_data == NULL) return MFX_ERR_MEMORY_ALLOC;

        m_bf_stream.SetBufferPointer(m_bf_data, bsSize);
    } else {
        m_bf_data = NULL;
    }

#ifdef H264_NEW_THREADING
    m_taskParams.threads_data = NULL;

    vm_event_set_invalid(&m_taskParams.parallel_region_done);
    if (VM_OK != vm_event_init(&m_taskParams.parallel_region_done, 0, 0))
        return MFX_ERR_MEMORY_ALLOC;

    m_taskParams.threads_data = (threadSpecificDataH264*)H264_Malloc(sizeof(threadSpecificDataH264)  * m_views[0].m_mfxVideoParam.mfx.NumThread);
    if (0 == m_taskParams.threads_data)
        return MFX_ERR_MEMORY_ALLOC;

    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;
    for (mfxI32 i = 0; i < m_views[0].enc->m_info.numThreads; i++)
    {
        m_taskParams.threads_data[i].core_enc = NULL;
        m_taskParams.threads_data[i].state = NULL;
        m_taskParams.threads_data[i].slice = 0;
        m_taskParams.threads_data[i].slice_qp_delta_default = 0;
        m_taskParams.threads_data[i].default_slice_type = INTRASLICE;
    }
    m_taskParams.single_thread_done = false;
#else
    !!!ASSERT!!!
#endif // H264_NEW_THREADING

    if (st == MFX_ERR_NONE)
    {
        m_isinitialized = true;
        return (QueryStatus != MFX_ERR_NONE) ? QueryStatus : (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
    }
    else
        return st;
}

mfxStatus MFXVideoENCODEMVC::Close()
{
    if( !m_isinitialized ) return MFX_ERR_NOT_INITIALIZED;

    mfxU32 i;
    for(i=0; i<m_numviews; i++) {
        ReleaseBufferedFrames(m_views[i].enc);
        H264CoreEncoder_Close_8u16s(m_views[i].enc);
        H264EncoderFrameList_clearFrameList_8u16s(&m_views[i].enc->m_dpb);
        H264EncoderFrameList_clearFrameList_8u16s(&m_views[i].enc->m_cpb);
        if (m_views[i].enc->m_pReconstructFrame_allocated) {
            H264EncoderFrame_Destroy_8u16s(m_views[i].enc->m_pReconstructFrame_allocated);
            H264_Free(m_views[i].enc->m_pReconstructFrame_allocated);
            m_views[i].enc->m_pReconstructFrame_allocated = 0;
        }
        if (m_views[i].m_brc)
        {
          m_views[i].m_brc->Close();
          delete m_views[i].m_brc;
          m_views[i].m_brc = 0;
        }

        if (m_views[i].m_useAuxInput || m_views[i].m_useSysOpaq) {
            //st = m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
            m_core->FreeFrames(&m_views[i].m_response);
            m_views[i].m_response.NumFrameActual = 0;
            m_views[i].m_useAuxInput = false;
            m_views[i].m_useSysOpaq = false;
            m_views[i].m_useAuxInput = false;
        }

        if (m_views[i].m_useVideoOpaq) {
            m_core->FreeFrames(&m_views[i].m_response_alien);
            m_views[i].m_response_alien.NumFrameActual = 0;
            m_views[i].m_useVideoOpaq = false;
        }

        H264CoreEncoder_Destroy_8u16s(m_views[i].enc);
        H264_Free(m_views[i].enc);
        m_views[i].enc = NULL;
        while(!m_mvcjobs[i].empty()) {
            m_core->DecreaseReference(&(m_mvcjobs[i].front().surface->Data));
            m_mvcjobs[i].pop();
        }
    }

    if(m_views) { delete[] m_views; m_views = 0; }
    if(m_mvcext_data) { delete[] m_mvcext_data; m_mvcext_data = NULL; }
    if(m_sps_views) { delete[] m_sps_views; m_sps_views = NULL; }
    if(m_pps_views) { delete[] m_pps_views; m_pps_views = NULL; }
    if(m_mvcdesc.View) { delete[] m_mvcdesc.View; m_mvcdesc.View = 0; }
    if(m_mvcdesc.ViewId) { delete[] m_mvcdesc.ViewId; m_mvcdesc.ViewId = 0; }
    if(m_mvcdesc.OP) { delete[] m_mvcdesc.OP; m_mvcdesc.OP = 0; }


    if (m_allocator) {
        delete m_allocator;
        m_allocator = 0;
    }

    vm_event_destroy(&m_taskParams.parallel_region_done);
    m_mvcjobs.clear();
    
    if (m_taskParams.threads_data)
    {
        H264_Free(m_taskParams.threads_data);
        m_taskParams.threads_data = 0;
    }

    if(m_bf_data) {
        H264_Free(m_bf_data);
        m_bf_data = NULL;
    }

    m_isinitialized = false;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::ReleaseBufferedFrames(H264CoreEncoder_8u16s *enc)
{
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

mfxStatus MFXVideoENCODEMVC::Reset(mfxVideoParam * par_in)
{
    mfxStatus st, QueryStatus;
    UMC::H264EncoderParams videoParams;
    Status status = UMC_OK;

    if(!m_isinitialized) return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(par_in == NULL) return MFX_ERR_NULL_PTR;

    if (par_in->mfx.CodecId != MFX_CODEC_AVC ||
        (par_in->mfx.CodecProfile != MFX_PROFILE_AVC_STEREO_HIGH && par_in->mfx.CodecProfile != MFX_PROFILE_AVC_MULTIVIEW_HIGH))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    st = CheckExtBuffers_H264enc( par_in->ExtParam, par_in->NumExtParam );
    if (st != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if(GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if(GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS ))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtMVCSeqDesc *depinfo = (mfxExtMVCSeqDesc*)GetExtBuffer(par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
    mfxExtOpaqueSurfaceAlloc *opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );

    // checks for opaque memory
    if (!(m_views[0].m_mfxVideoParam.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) && (par_in->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if (opaqAllocReq != 0 && opaqAllocReq->In.NumSurface != m_views[0].m_mfxVideoParam.mfx.GopRefDist * (m_numviews + 1))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // Stereo profiles must be initialized with partialy filled mfxExtMVCSeqDesc
    if(depinfo == NULL)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if(depinfo->NumView > 1024)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if (par_in->Protected)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_initValues.FrameWidth < par_in->mfx.FrameInfo.Width ||
        m_initValues.FrameHeight < par_in->mfx.FrameInfo.Height ||
        m_views[0].m_mfxVideoParam.mfx.FrameInfo.ChromaFormat != par_in->mfx.FrameInfo.ChromaFormat ||
        (par_in->mfx.GopRefDist && m_views[0].m_mfxVideoParam.mfx.GopRefDist != par_in->mfx.GopRefDist) ||
        (par_in->mfx.NumSlice != 0 && m_views[0].enc->m_info.num_slices != par_in->mfx.NumSlice) ||
        (par_in->mfx.NumRefFrame && m_views[0].m_mfxVideoParam.mfx.NumRefFrame != par_in->mfx.NumRefFrame) ||
        m_views[1].m_mfxVideoParam.mfx.CodecProfile != par_in->mfx.CodecProfile ||
        (par_in->mfx.RateControlMethod && par_in->mfx.RateControlMethod != m_views[0].m_mfxVideoParam.mfx.RateControlMethod)
        /* m_views[0].m_mfxVideoParam.mfx.CodecLevel != par_in->mfx.CodecLevel || */)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // save obsolete SPS
    H264SeqParamSet obsoleteSeqParamSet = *m_views[0].enc->m_SeqParamSet;

    // Release locked frames always
    for(mfxU32 iii = 0; iii < m_numviews; iii++)
        ReleaseBufferedFrames(m_views[iii].enc);

    st = CheckExtBuffers_H264enc( par_in->ExtParam, par_in->NumExtParam );
    if (st != MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOption* opts = GetExtCodingOptions( par_in->ExtParam, par_in->NumExtParam );
    mfxExtCodingOptionSPSPPS* optsSP = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtPictureTimingSEI* picTimingSei = (mfxExtPictureTimingSEI*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI );
    mfxExtCodingOption2* opts2 = (mfxExtCodingOption2*)GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );
    if(GetExtBuffer( par_in->ExtParam, par_in->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA ))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxVideoParam checked;

    mfxExtMVCSeqDesc checked_depinfo;
    mfxExtCodingOption checked_ext;
    mfxExtCodingOptionSPSPPS checked_extSP;
    mfxExtVideoSignalInfo checked_videoSignalInfo;
    mfxExtOpaqueSurfaceAlloc checked_opaqAllocReq;
    mfxExtPictureTimingSEI checked_picTimingSei;
    mfxExtCodingOption2      checked_ext2;
    mfxExtBuffer *ptr_checked_ext[7] = {0,};
    mfxU16 ext_counter = 1; // There is at least mfxExtMVCSeqDesc attached to VideoParams
    checked = *par_in;
    checked_depinfo = *depinfo;
    ptr_checked_ext[0] = &checked_depinfo.Header;

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
    }
    if (opaqAllocReq) {
        checked_opaqAllocReq = *opaqAllocReq;
        ptr_checked_ext[ext_counter++] = &checked_opaqAllocReq.Header;
    }
    if (picTimingSei) {
        checked_picTimingSei = *picTimingSei;
        ptr_checked_ext[ext_counter++] = &checked_picTimingSei.Header;
    }
    if (opts2) {
        checked_ext2 = *opts2;
        ptr_checked_ext[ext_counter++] = &checked_ext2.Header;
    }
    checked.ExtParam = ptr_checked_ext;
    checked.NumExtParam = ext_counter;

    SetDefaultParamForReset(checked, m_views[0].m_mfxVideoParam);

    mfxVideoInternalParam inInt = *par_in;

    if (par_in->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        mfxU32 TargetKbps = 0, BufferSizeInKB = 0, InitialDelayInKB = 0, MaxKbps = 0;

        for (mfxU32 i = 0; i < m_numviews; i++)
        {
            TargetKbps += m_views[i].m_mfxVideoParam.calcParam.TargetKbps;
            BufferSizeInKB += m_views[i].m_mfxVideoParam.calcParam.BufferSizeInKB;
            InitialDelayInKB += m_views[i].m_mfxVideoParam.calcParam.InitialDelayInKB;
            MaxKbps += m_views[i].m_mfxVideoParam.calcParam.MaxKbps;
        }

        if ((inInt.calcParam.MaxKbps != 0 && inInt.calcParam.MaxKbps != MaxKbps) ||
            (inInt.calcParam.BufferSizeInKB != 0 && inInt.calcParam.BufferSizeInKB != BufferSizeInKB) ||
            (inInt.calcParam.InitialDelayInKB != 0 && inInt.calcParam.InitialDelayInKB != InitialDelayInKB))
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    st = Query(&checked, &checked);

    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && st != MFX_WRN_PARTIAL_ACCELERATION)
    {
        if (st == MFX_ERR_UNSUPPORTED && optsSP == 0) // SPS/PPS header isn't attached. Error is in mfxVideoParam. Return MFX_ERR_INVALID_VIDEO_PARAM
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else if (st == MFX_ERR_UNSUPPORTED) // SPSPPS header is attached. Return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        else
            return st;
    }
    QueryStatus = st;

    inInt = checked;
    mfxVideoInternalParam* par = &inInt;

    opts = &checked_ext;

    if (opts2)
        opts2 = &checked_ext2;

    // Should be done after Query so that it returns INVALID_VIDEO_PARAM if NumView is invalid, otherwise error code returned here is wrong
    if (depinfo->NumView != m_numviews)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0 ||
        par->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 || par->calcParam.TargetKbps == 0 ||
        par->mfx.FrameInfo.FrameRateExtN == 0 || par->mfx.FrameInfo.FrameRateExtD == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    //if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0 ||
    //    par->mfx.FrameInfo.Width > 0xff0 || par->mfx.FrameInfo.Height > 0xff0 ||
    //    par->mfx.FrameInfo.CropX + par->mfx.FrameInfo.CropW > par->mfx.FrameInfo.Width ||
    //    par->mfx.FrameInfo.CropY + par->mfx.FrameInfo.CropH > par->mfx.FrameInfo.Height)
    //    return MFX_ERR_INVALID_VIDEO_PARAM;

    //if ((par->mfx.FrameInfo.Width & 15) !=0)
    //    return MFX_ERR_INVALID_VIDEO_PARAM;
    //if ((par->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))  == MFX_PICSTRUCT_PROGRESSIVE) {
    //    if ( par->mfx.FrameInfo.Height & 15 )
    //        return MFX_ERR_INVALID_VIDEO_PARAM;
    //    if ( par->mfx.NumSlice > ((par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height) >> 8))
    //        return MFX_ERR_INVALID_VIDEO_PARAM;
    //} else {
    //  if ( par->mfx.FrameInfo.Height & 31 )
    //    return MFX_ERR_INVALID_VIDEO_PARAM;
    //  if ( par->mfx.NumSlice > ((par->mfx.FrameInfo.Width * par->mfx.FrameInfo.Height) >> 9))
    //      return MFX_ERR_INVALID_VIDEO_PARAM;
    //}

    //if (par->calcParam.TargetKbps < 10)
    //    par->calcParam.TargetKbps = 10;

    const mfxU16 MFX_IOPATTERN_IN = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY;
    if ((par->IOPattern & ~MFX_IOPATTERN_IN) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    //if (par->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
    //    return MFX_ERR_INVALID_VIDEO_PARAM;

    //mfxU16 targetKbps = par->calcParam.TargetKbps;
    //mfxU16 maxKbps = par->calcParam.MaxKbps;
    //mfxU16 bufferSizeInKB = par->calcParam.BufferSizeInKB;
    //mfxU16 codecProfile = par->mfx.CodecProfile;
    //mfxU16 codecLevel = par->mfx.CodecLevel;

    //Set coding options
    if( opts != NULL ){
        if( opts->MaxDecFrameBuffering > 0 && opts->MaxDecFrameBuffering < par->mfx.NumRefFrame ){
            //par->mfx.NumRefFrame = opts->MaxDecFrameBuffering;
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    for(mfxU32 iii = 0; iii < m_numviews; iii++)
    {
        m_views[iii].enc->m_info.m_Analyse_on = 0;
        m_views[iii].enc->m_info.m_Analyse_restrict = 0;
        m_views[iii].enc->m_info.m_Analyse_ex = 0;
    }

    // convert new params (including parameters from attached SPS/PPS headers (if any))
    memset(&videoParams.m_SeqParamSet, 0, sizeof(UMC::H264SeqParamSet));
    memset(&videoParams.m_PicParamSet, 0, sizeof(UMC::H264PicParamSet));
    videoParams.use_ext_sps = videoParams.use_ext_pps = false;
    st = ConvertVideoParam_H264enc(par, &videoParams);
    MFX_CHECK_STS(st);

    videoParams.m_is_mvc_profile = true;

    // to set profile/level and related vars
    CheckProfileLevelLimits_H264enc(par, true);

    mfxVideoParam base(*par);
    mfxVideoParam dependent(*par);
    DistributeBrcParamters(par, &base, &dependent, m_numviews);

   // get previous (before Reset) encoding params
    for(mfxU32 iii = 0; iii < m_numviews; iii++)
    {
        UMC::VideoBrcParams brcParams;

        st = ConvertVideoParamBack_H264enc(&m_views[iii].m_mfxVideoParam, m_views[iii].enc);
        MFX_CHECK_STS(st);

        if (m_initValues.FrameWidth < par->mfx.FrameInfo.Width ||
            m_initValues.FrameHeight < par->mfx.FrameInfo.Height ||
            m_views[iii].m_mfxVideoParam.mfx.FrameInfo.ChromaFormat != par->mfx.FrameInfo.ChromaFormat ||
            (par->mfx.GopRefDist && m_views[iii].m_mfxVideoParam.mfx.GopRefDist != par->mfx.GopRefDist) ||
            (par->mfx.NumSlice != 0 && m_views[iii].enc->m_info.num_slices != par->mfx.NumSlice) ||
            (par->mfx.NumRefFrame && m_views[iii].m_mfxVideoParam.mfx.NumRefFrame != par->mfx.NumRefFrame) ||
            (iii != 0 && m_views[iii].m_mfxVideoParam.mfx.CodecProfile != par->mfx.CodecProfile) ||
            (par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && m_views[iii].enc->m_info.coding_type == 0))
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

        // apply new params
        m_views[iii].enc->m_info = videoParams;
        m_views[iii].enc->m_info.m_is_base_view = iii ? false : true;

        //par->calcParam.TargetKbps = targetKbps;
        //par->calcParam.MaxKbps = maxKbps;
        //par->calcParam.BufferSizeInKB = bufferSizeInKB;
        //par->mfx.CodecProfile = codecProfile;
        //par->mfx.CodecLevel = codecLevel;

        st = ConvertVideoParam_Brc(iii == 0 ? &base : &dependent, &brcParams);
        MFX_CHECK_STS(st);
        brcParams.GOPPicSize = m_views[iii].enc->m_info.key_frame_controls.interval;
        brcParams.GOPRefDist = m_views[iii].enc->m_info.B_frame_rate + 1;
        brcParams.profile = m_views[iii].enc->m_info.profile_idc;
        brcParams.level = m_views[iii].enc->m_info.level_idc;

        // ignore brc params to preserve current BRC HRD states in case of HRD-compliant encoding
        if ((!m_views[iii].enc->m_SeqParamSet->vui_parameters_present_flag ||
            !m_views[iii].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag ||
            (opts && opts->VuiNalHrdParameters == MFX_CODINGOPTION_OFF)) && par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
            if (m_views[iii].m_brc){
                m_views[iii].m_brc->Close();
            } else
                m_views[iii].m_brc = new UMC::H264BRC;
            status = m_views[iii].m_brc->Init(&brcParams);
            st = h264enc_ConvertStatus(status);
            MFX_CHECK_STS(st);
        } else if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
            if (m_views[iii].m_brc)
                status = m_views[iii].m_brc->Reset(&brcParams);
            else {
                m_views[iii].m_brc = new UMC::H264BRC;
                status = m_views[iii].m_brc->Init(&brcParams);
            }
            st = h264enc_ConvertStatus(status);
            MFX_CHECK_STS(st);
        }

        if (par->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
            m_views[iii].m_brc = NULL;
            m_views[iii].enc->brc = NULL;
        }
        else
            m_views[iii].enc->brc = m_views[iii].m_brc;

        // Set low initial QP values to prevent BRC panic mode if superfast mode
        if(par->mfx.TargetUsage==MFX_TARGETUSAGE_BEST_SPEED && m_views[iii].m_brc) {
            if(m_views[iii].m_brc->GetQP(I_PICTURE) < 30) {
                m_views[iii].m_brc->SetQP(30, I_PICTURE);
                m_views[iii].m_brc->SetQP(30, B_PICTURE);
            }
        }

        //Reset frame count
        m_views[iii].m_frameCountSync = 0;
        m_views[iii].m_frameCountBufferedSync = 0;
        m_views[iii].m_frameCount = 0;
        m_views[iii].m_frameCountBuffered = 0;
        m_views[iii].m_lastIDRframe = 0;
        m_views[iii].m_lastIframe = 0;
        m_views[iii].m_lastRefFrame = 0;

        if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
            m_views[iii].enc->m_info.coding_type = 0; // frames only
        else {
            if(opts && opts->FramePicture == MFX_CODINGOPTION_ON)
                m_views[iii].enc->m_info.coding_type = 2; // MBAFF
            else
                m_views[iii].enc->m_info.coding_type = 3; // PicAFF
        }

        if (m_views[iii].enc->m_info.m_Analyse_on){
            if (m_views[iii].enc->m_info.transform_8x8_mode_flag == 0)
                m_views[iii].enc->m_info.m_Analyse_on &= ~ANALYSE_I_8x8;
            if (m_views[iii].enc->m_info.entropy_coding_mode == 0)
                m_views[iii].enc->m_info.m_Analyse_on &= ~(ANALYSE_RD_OPT | ANALYSE_RD_MODE | ANALYSE_B_RD_OPT);
            if (m_views[iii].enc->m_info.coding_type && m_views[iii].enc->m_info.coding_type != 2) // TODO: remove coding_type != 2 when MBAFF is implemented
                m_views[iii].enc->m_info.m_Analyse_on &= ~ANALYSE_ME_AUTO_DIRECT;
            if (par->mfx.GopOptFlag & MFX_GOP_STRICT)
                m_views[iii].enc->m_info.m_Analyse_on &= ~ANALYSE_FRAME_TYPE;
        }

        m_views[iii].m_mfxVideoParam.mfx = par->mfx;
        m_views[iii].m_mfxVideoParam.IOPattern = par->IOPattern;
        if (opts != 0)
            m_views[iii].m_extOption = *opts;

        // set like in par-files
        if(par->mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            m_views[iii].enc->m_info.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_CBR;
        else if(par->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            m_views[iii].enc->m_info.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_QUANT;
        else
            m_views[iii].enc->m_info.rate_controls.method = (H264_Rate_Control_Method)H264_RCM_VBR;

        status = H264CoreEncoder_Reset_8u16s(m_views[iii].enc);

        m_views[iii].enc->m_FieldStruct = (Ipp8u)par->mfx.FrameInfo.PicStruct;

        st = ConvertVideoParamBack_H264enc(&m_views[iii].m_mfxVideoParam, m_views[iii].enc);
        MFX_CHECK_STS(st);

        if (m_views[iii].enc->free_slice_mode_enabled)
            m_views[iii].m_mfxVideoParam.mfx.NumSlice = 0;

        //Set specific parameters
        m_views[iii].enc->m_info.write_access_unit_delimiters = 1;
        if(opts != NULL)
        {
            if(opts->MaxDecFrameBuffering != MFX_CODINGOPTION_UNKNOWN && opts->MaxDecFrameBuffering <= par->mfx.NumRefFrame)
            {
                m_views[iii].enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag = true;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.motion_vectors_over_pic_boundaries_flag = 1;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.max_bytes_per_pic_denom = 0;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.max_bits_per_mb_denom = 0;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_horizontal = 11;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.log2_max_mv_length_vertical = 11;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.num_reorder_frames = 0;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.max_dec_frame_buffering = opts->MaxDecFrameBuffering;
            }
            if(opts->AUDelimiter == MFX_CODINGOPTION_OFF)
            {
                m_views[iii].enc->m_info.write_access_unit_delimiters = 0;
                //}else{
                //    enc->m_info.write_access_unit_delimiters = 0;
            }

            if(opts->EndOfSequence == MFX_CODINGOPTION_ON)
                m_views[iii].m_putEOSeq = true;
            else
                m_views[iii].m_putEOSeq = false;

            if(opts->EndOfStream == MFX_CODINGOPTION_ON)
                m_views[iii].m_putEOStream = true;
            else
                m_views[iii].m_putEOStream = false;

            if(opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                m_views[iii].m_putTimingSEI = true;
            else
                m_views[iii].m_putTimingSEI = false;

            if(opts->ResetRefList == MFX_CODINGOPTION_ON)
                m_views[iii].m_resetRefList = true;
            else
                m_views[iii].m_resetRefList = false;
        }

        m_views[iii].enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
        if(par->mfx.FrameInfo.AspectRatioW != 0 && par->mfx.FrameInfo.AspectRatioH != 0)
        {
            m_views[iii].enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc =
                (Ipp8u)ConvertSARtoIDC_H264enc(par->mfx.FrameInfo.AspectRatioW, par->mfx.FrameInfo.AspectRatioH);
            if (m_views[iii].enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 0)
            {
                m_views[iii].enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = 255; //Extended SAR
                m_views[iii].enc->m_SeqParamSet->vui_parameters.sar_width = par->mfx.FrameInfo.AspectRatioW;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.sar_height = par->mfx.FrameInfo.AspectRatioH;
            }
        }

        if (par->mfx.FrameInfo.FrameRateExtD && par->mfx.FrameInfo.FrameRateExtN)
        {
            m_views[iii].enc->m_SeqParamSet->vui_parameters.timing_info_present_flag = 1;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.num_units_in_tick = par->mfx.FrameInfo.FrameRateExtD;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.time_scale = par->mfx.FrameInfo.FrameRateExtN * 2;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = 1;
        }
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;

        m_views[iii].enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = 0;
        m_views[iii].enc->m_SeqParamSet->vui_parameters.chroma_loc_info_present_flag = 0;
        m_views[iii].enc->m_info.m_viewOutput = false;

        if (opts != NULL)
        {
            if(opts->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN || opts->VuiNalHrdParameters == MFX_CODINGOPTION_ON)
            {
                m_views[iii].enc->m_SeqParamSet->vui_parameters.low_delay_hrd_flag = 0;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
                if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
                    m_views[iii].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 1;
                else
                {
                    m_views[iii].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
                    if (opts->PicTimingSEI != MFX_CODINGOPTION_ON)
                        m_views[iii].enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 0;
                }
            }
            else
            {
                m_views[iii].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag = 0;
                if (opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                    m_views[iii].enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = 1;
            }

            m_views[iii].enc->m_info.m_viewOutput = MFX_CODINGOPTION_ON == opts->ViewOutput;
        }

        if (m_views[iii].enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag == 1)
        {
            VideoBrcParams  curBRCParams;

            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_cnt_minus1 = 0;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale = 0;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale = 3;

            if (m_views[iii].enc->brc)
            {
                m_views[iii].enc->brc->GetParams(&curBRCParams);

                if (curBRCParams.maxBitrate)
                    m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_value_minus1[0] =
                    (curBRCParams.maxBitrate >> (6 + m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale)) - 1;
                else
                    status = UMC_ERR_FAILED;

                if (curBRCParams.HRDBufferSizeBytes)
                    m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_value_minus1[0] =
                    (curBRCParams.HRDBufferSizeBytes  >> (1 + m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale)) - 1;
                else
                    status = UMC_ERR_FAILED;

                m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.cbr_flag[0] = (curBRCParams.BRCMode == BRC_CBR) ? 1 : 0;
            }
            else
                status = UMC_ERR_NOT_ENOUGH_DATA;

            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = 23; // default
            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = 23; // default
            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.dpb_output_delay_length_minus1 = 23; // default
            m_views[iii].enc->m_SeqParamSet->vui_parameters.hrd_params.time_offset_length = 24; // default
        }

        // set VUI video signal info
        if (videoSignalInfo != NULL) {
            m_views[iii].enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = 1;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.video_format = (Ipp8u)videoSignalInfo->VideoFormat;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.video_full_range_flag = videoSignalInfo->VideoFullRange ? 1 : 0;
            m_views[iii].enc->m_SeqParamSet->vui_parameters.colour_description_present_flag = videoSignalInfo->ColourDescriptionPresent ? 1 : 0;
            if (m_views[iii].enc->m_SeqParamSet->vui_parameters.colour_description_present_flag) {
                m_views[iii].enc->m_SeqParamSet->vui_parameters.colour_primaries = (Ipp8u)videoSignalInfo->ColourPrimaries;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.transfer_characteristics = (Ipp8u)videoSignalInfo->TransferCharacteristics;
                m_views[iii].enc->m_SeqParamSet->vui_parameters.matrix_coefficients = (Ipp8u)videoSignalInfo->MatrixCoefficients;
            }
        }
        else {
            m_views[iii].enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = 0;
        }

        m_views[iii].enc->m_info.is_resol_changed = m_views[iii].enc->m_SeqParamSet->frame_width_in_mbs != obsoleteSeqParamSet.frame_width_in_mbs ||
            m_views[iii].enc->m_SeqParamSet->frame_height_in_mbs != obsoleteSeqParamSet.frame_height_in_mbs ? true : false;

        m_views[iii].enc->m_info.m_is_base_view = iii ? false : true;

#ifdef H264_HRD_CPB_MODEL
        enc->m_SEIData.m_tickDuration = ((Ipp64f)enc->m_SeqParamSet->vui_parameters.num_units_in_tick) / enc->m_SeqParamSet->vui_parameters.time_scale;
#endif // H264_HRD_CPB_MODEL
    }


#ifdef H264_NEW_THREADING
    m_taskParams.single_thread_selected = 0;
    m_taskParams.thread_number = 0;
    m_taskParams.parallel_encoding_finished = false;

    for (mfxI32 i = 0; i < m_views[0].enc->m_info.numThreads; i++)
    {
        m_taskParams.threads_data[i].core_enc = NULL;
        m_taskParams.threads_data[i].state = NULL;
        m_taskParams.threads_data[i].slice = 0;
        m_taskParams.threads_data[i].slice_qp_delta_default = 0;
        m_taskParams.threads_data[i].default_slice_type = INTRASLICE;
    }

    m_taskParams.single_thread_done = false;
#endif // H264_NEW_THREADING

    if(status == UMC_OK)
        m_isinitialized = true;
    st = h264enc_ConvertStatus(status);

    m_au_count = 0;

    if (st == MFX_ERR_NONE)
    {
        for(mfxU32 iii = 0; iii < m_numviews; iii++)
        {
            if (m_views[iii].m_mfxVideoParam.AsyncDepth != par->AsyncDepth)
            {
                m_views[iii].m_mfxVideoParam.AsyncDepth = par->AsyncDepth;
                st = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                break;
            }
        }

        if (QueryStatus != MFX_ERR_NONE && QueryStatus != MFX_WRN_PARTIAL_ACCELERATION)
            return QueryStatus;
        else
            return st;
    }
    else
        return st;
}

mfxStatus MFXVideoENCODEMVC::Query(mfxVideoParam *par_in, mfxVideoParam *par_out)
{
    mfxU32      isCorrected = 0;
    mfxU32      isInvalid = 0;
    mfxStatus   st;

    MFX_CHECK_NULL_PTR1(par_out)
    CHECK_VERSION(par_in->Version);
    CHECK_VERSION(par_out->Version);

    // Zero input should be handled by AVC query
    if( par_in == NULL ) return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxU16          cropSampleMask, correctCropTop, correctCropBottom;
    mfxU32          cnt, i, j, numViews = 1;

    // Input must be MVC codec/profile
    if(par_in->mfx.CodecId != MFX_CODEC_AVC) return MFX_ERR_UNSUPPORTED;
    if(par_in->mfx.CodecProfile != MFX_PROFILE_AVC_MULTIVIEW_HIGH && par_in->mfx.CodecProfile != MFX_PROFILE_AVC_STEREO_HIGH) return MFX_ERR_UNSUPPORTED;

    mfxVideoInternalParam inInt = *par_in;
    mfxVideoInternalParam outInt = *par_out;
    mfxVideoInternalParam *in = &inInt;
    mfxVideoInternalParam *out = &outInt;

    // Check compatibility of input and output Ext buffers
    if ((in->NumExtParam != out->NumExtParam) || (in->NumExtParam &&((in->ExtParam == 0) != (out->ExtParam == 0))))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    // Check for valid H264 extended buffers
    st = CheckExtBuffers_H264enc( in->ExtParam, in->NumExtParam ); MFX_CHECK_STS(st);
    st = CheckExtBuffers_H264enc( out->ExtParam, out->NumExtParam ); MFX_CHECK_STS(st);

    // Check for forbiden extended buffers for MVC
    if(GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA )) return MFX_ERR_UNSUPPORTED;
    if(GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA )) return MFX_ERR_UNSUPPORTED;
    if(GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS )) return MFX_ERR_UNSUPPORTED;
    if(GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS )) return MFX_ERR_UNSUPPORTED;

    // Get allowed extended buffers for MVC
    mfxExtCodingOption *opts_in     = GetExtCodingOptions( in->ExtParam, in->NumExtParam );
    mfxExtCodingOption *opts_out    = GetExtCodingOptions( out->ExtParam, out->NumExtParam );
    mfxExtMVCSeqDesc   *depinfo_in  = (mfxExtMVCSeqDesc*)GetExtBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
    mfxExtMVCSeqDesc   *depinfo_out = (mfxExtMVCSeqDesc*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
    mfxExtVideoSignalInfo *videoSignalInfo_in  = (mfxExtVideoSignalInfo*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtVideoSignalInfo *videoSignalInfo_out = (mfxExtVideoSignalInfo*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );
    mfxExtCodingOption2* opts2_in = (mfxExtCodingOption2*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );
    mfxExtCodingOption2* opts2_out = (mfxExtCodingOption2*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_CODING_OPTION2 );

    if((opts_in==0) != (opts_out==0) || (opts2_in == 0) != (opts2_out == 0) || (depinfo_in==0) != (depinfo_out==0) || (videoSignalInfo_in == 0) != (videoSignalInfo_out == 0))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(depinfo_in) {
        if(depinfo_out->NumViewAlloc && depinfo_out->View!=NULL && depinfo_out->View!=depinfo_in->View)
            memset(depinfo_out->View, 0, depinfo_out->NumViewAlloc*sizeof(mfxMVCViewDependency));
        if(depinfo_out->NumViewIdAlloc && depinfo_out->ViewId!=NULL && depinfo_out->ViewId!=depinfo_in->ViewId)
            memset(depinfo_out->ViewId, 0, depinfo_out->NumViewId * sizeof(mfxU16));
        if(depinfo_out->NumOPAlloc && depinfo_out->OP!=NULL && depinfo_out->OP!=depinfo_in->OP)
            memset(depinfo_out->OP, 0, depinfo_out->NumOPAlloc*sizeof(mfxMVCOperationPoint));

//        if(depinfo_in->NumView > 1024 || depinfo_in->NumView < 2) {
        if(depinfo_in->NumView != 2) {
            depinfo_out->NumView = 0;
            isInvalid ++;
        } else {
            depinfo_out->NumView = depinfo_in->NumView;
            if (depinfo_out->View != NULL && depinfo_in->View != NULL)
            {
                cnt = MIN(depinfo_out->NumView, MIN(depinfo_out->NumViewAlloc, depinfo_in->NumViewAlloc));
                for(j=0; j<cnt; j++) {
                    MFX_INTERNAL_CPY(depinfo_out->View + j, depinfo_in->View + j, sizeof(mfxMVCViewDependency));
                    if(depinfo_out->View[j].ViewId >= 1024) {depinfo_out->View[j].ViewId = 0; isInvalid ++; }
                    if(depinfo_out->View[j].NumAnchorRefsL0 >= 16) {depinfo_out->View[j].NumAnchorRefsL0 = 0; isInvalid ++; }
                    if(depinfo_out->View[j].NumAnchorRefsL1 >= 16) {depinfo_out->View[j].NumAnchorRefsL1 = 0; isInvalid ++; }
                    if(depinfo_out->View[j].NumNonAnchorRefsL0 >= 16) {depinfo_out->View[j].NumNonAnchorRefsL0 = 0; isInvalid ++; }
                    if(depinfo_out->View[j].NumNonAnchorRefsL1 >= 16) {depinfo_out->View[j].NumNonAnchorRefsL1 = 0; isInvalid ++; }
                    for(i=0;i<16;i++) {
                        if(depinfo_out->View[j].AnchorRefL0[i] >= 1024) {depinfo_out->View[j].AnchorRefL0[i] = 0; isInvalid ++; }
                        if(depinfo_out->View[j].AnchorRefL1[i] >= 1024) {depinfo_out->View[j].AnchorRefL1[i] = 0; isInvalid ++; }
                        if(depinfo_out->View[j].NonAnchorRefL0[i] >= 1024) {depinfo_out->View[j].NonAnchorRefL0[i] = 0; isInvalid ++; }
                        if(depinfo_out->View[j].NonAnchorRefL1[i] >= 1024) {depinfo_out->View[j].NonAnchorRefL1[i] = 0; isInvalid ++; }
                    }
                }
            }
        }

        depinfo_out->NumViewId = depinfo_in->NumViewId;
        if (depinfo_out->ViewId != NULL && depinfo_in->ViewId != NULL)
        {
            cnt = MIN(depinfo_out->NumViewId, MIN(depinfo_out->NumViewIdAlloc, depinfo_in->NumViewIdAlloc));
            for(j=0; j<cnt; j++) {
                if(depinfo_in->ViewId[j]<1024) {
                    depinfo_out->ViewId[j] = depinfo_in->ViewId[j];
                } else {
                    depinfo_out->ViewId[j] = 0; isInvalid ++;
                }
            }
        }

        depinfo_out->NumOP = depinfo_in->NumOP;
        if (depinfo_out->OP != NULL && depinfo_in->OP)
        {
            cnt = MIN(depinfo_out->NumOP, MIN(depinfo_out->NumOPAlloc, depinfo_in->NumOPAlloc));
            for(j=0; j<cnt; j++) {
                if(depinfo_in->OP[j].LevelIdc == 0 || ConvertCUCLevelToUMC(depinfo_in->OP[j].LevelIdc)) {
                    depinfo_out->OP[j].LevelIdc = depinfo_in->OP[j].LevelIdc;
                } else {
                    depinfo_out->OP[j].LevelIdc = 0; isInvalid ++;
                }
                if(depinfo_in->OP[j].TemporalId < 8) {
                    depinfo_out->OP[j].TemporalId = depinfo_in->OP[j].TemporalId;
                } else {
                    depinfo_out->OP[j].TemporalId = 0; isInvalid ++;
                }
                depinfo_out->OP[j].NumViews = depinfo_in->OP[j].NumViews;
                depinfo_out->OP[j].NumTargetViews = depinfo_in->OP[j].NumTargetViews;

                if(depinfo_out->ViewId) {
                    ptrdiff_t offset = depinfo_in->OP[j].TargetViewId - depinfo_in->ViewId;
                    if(offset>=0 && offset<(mfxI32)depinfo_out->NumViewIdAlloc) {
                        depinfo_out->OP[j].TargetViewId = depinfo_out->ViewId + offset;
                    } else {
                        isInvalid++;
                    }
                }
            }
        }
        numViews = depinfo_in->NumView;
    } else {
        numViews = 2; // guess the minimal number of views
        isInvalid ++;
    }
    // check video signal info
    if (videoSignalInfo_in) {
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

    if (in->mfx.FrameInfo.FourCC != 0 && in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12) {
        out->mfx.FrameInfo.FourCC = 0;
        isInvalid ++;
    } else
        out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;

    if (in->Protected != 0) isInvalid ++;

    out->Protected = 0;
    out->AsyncDepth = in->AsyncDepth;

    //Check for valid framerate, height and width
    if( (in->mfx.FrameInfo.Width & 15) || in->mfx.FrameInfo.Width > 4096 /*|| in->mfx.FrameInfo.Width < 16*/ ) {
        out->mfx.FrameInfo.Width = 0;
        isInvalid ++;
    } else out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
    if( (in->mfx.FrameInfo.Height & 15) || in->mfx.FrameInfo.Height > 4096 /*|| in->mfx.FrameInfo.Height < 16*/ ) {
        out->mfx.FrameInfo.Height = 0;
        isInvalid ++;
    } else out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
    if( ((!in->mfx.FrameInfo.FrameRateExtN) && in->mfx.FrameInfo.FrameRateExtD) ||
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
            isCorrected ++;
            if (in->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            else if (in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
                out->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            else
                out->IOPattern = 0;
    }

    // profile and level can be corrected
    switch( in->mfx.CodecProfile ){
        case MFX_PROFILE_AVC_MAIN:
        case MFX_PROFILE_AVC_BASELINE:
        case MFX_PROFILE_AVC_HIGH:
        case MFX_PROFILE_AVC_STEREO_HIGH:
        case MFX_PROFILE_UNKNOWN:
            out->mfx.CodecProfile = in->mfx.CodecProfile;
            break;
        default:
            isInvalid ++;
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

#if defined (__ICL)
    //error #186: pointless comparison of unsigned integer with zero
#pragma warning(disable:186)
#endif

    if( in->mfx.TargetUsage < MFX_TARGETUSAGE_UNKNOWN || in->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED) {
        isCorrected ++;
        out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
    }
    else out->mfx.TargetUsage = in->mfx.TargetUsage;

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

    if (opts2_in) {
        CHECK_OPTION(opts2_in->BitrateLimit, opts2_out->BitrateLimit, isCorrected);
        opts2_out->MaxFrameSize = 0; // frame size limit isn't supported at the moment
         // Intra refresh isn't supported for MVC at the moment
        opts2_out->IntRefType = 0;
        opts2_out->IntRefCycleSize = 0;
        opts2_out->IntRefQPDelta = 0;
        if (opts2_in->MaxFrameSize || opts2_in->IntRefCycleSize || opts2_in->IntRefQPDelta || opts2_in->IntRefType)
            isCorrected ++;
    }

    if ((in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT)) != in->mfx.GopOptFlag)
        isCorrected ++;
    out->mfx.GopOptFlag = in->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);
    out->mfx.IdrInterval = in->mfx.IdrInterval;

    if (in->mfx.RateControlMethod != 0 &&
        in->mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
        in->mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        in->mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
        in->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        out->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        isCorrected ++;
    } else out->mfx.RateControlMethod = in->mfx.RateControlMethod;

    out->calcParam.BufferSizeInKB = in->calcParam.BufferSizeInKB;
    if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        out->calcParam.TargetKbps = in->calcParam.TargetKbps;
        out->calcParam.InitialDelayInKB = in->calcParam.InitialDelayInKB;
        if (out->mfx.FrameInfo.Width && out->mfx.FrameInfo.Height && out->mfx.FrameInfo.FrameRateExtD && out->calcParam.TargetKbps &&
            (opts2_out == 0 || (opts2_out && opts2_out->BitrateLimit != MFX_CODINGOPTION_OFF))) {
            // last denominator 700 gives about 1 Mbps for 1080p x 30
            mfxU32 minBitRate = (mfxU32)((mfxF64)out->mfx.FrameInfo.Width * out->mfx.FrameInfo.Height * 12 // size of raw image (luma + chroma 420) in bits
                                         * out->mfx.FrameInfo.FrameRateExtN / out->mfx.FrameInfo.FrameRateExtD / 1000 / 700);
            minBitRate *= numViews;
            if (minBitRate > out->calcParam.TargetKbps) {
                out->calcParam.TargetKbps = (mfxU16)minBitRate;
                isCorrected ++;
            }
            mfxU32 AveFrameKB = out->calcParam.TargetKbps * out->mfx.FrameInfo.FrameRateExtD / out->mfx.FrameInfo.FrameRateExtN / 8;
            if (out->calcParam.BufferSizeInKB != 0 && out->calcParam.BufferSizeInKB < 2 * AveFrameKB) {
                out->calcParam.BufferSizeInKB = (mfxU16)(2 * AveFrameKB);
                isCorrected ++;
            }
            if (   out->calcParam.InitialDelayInKB != 0 
                && out->calcParam.InitialDelayInKB < AveFrameKB
                && out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
                out->calcParam.InitialDelayInKB = (mfxU16)AveFrameKB;
                isCorrected ++;
            }
        }

        if (   in->calcParam.MaxKbps != 0 
            && in->calcParam.MaxKbps < out->calcParam.TargetKbps
            && out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
            out->calcParam.MaxKbps = out->calcParam.TargetKbps;
        } else
            out->calcParam.MaxKbps = in->calcParam.MaxKbps;
    }
    // check for correct QPs for const QP mode
    else {
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
        opts_out->MESearchType = opts_in->MESearchType;
        opts_out->MECostType = opts_in->MECostType;
        CHECK_OPTION (opts_in->EndOfSequence, opts_out->EndOfSequence, isCorrected);
        CHECK_OPTION (opts_in->FramePicture, opts_out->FramePicture, isCorrected);
        CHECK_OPTION (opts_in->CAVLC, opts_out->CAVLC, isCorrected);
        //CHECK_OPTION (opts_in->RefPicListReordering, opts_out->RefPicListReordering);
        CHECK_OPTION (opts_in->ResetRefList, opts_out->ResetRefList, isCorrected);
        CHECK_OPTION (opts_in->ViewOutput, opts_out->ViewOutput, isCorrected);
        if (MFX_CODINGOPTION_ON == opts_in->ViewOutput && depinfo_in) {
            if (depinfo_in->ViewId && depinfo_in->ViewId[0] != 0) {
                depinfo_in->ViewId[0] = 0;
                isCorrected++;
            }
            if (depinfo_in->View && depinfo_in->View[0].ViewId != 0) {
                depinfo_in->View[0].ViewId = 0;
                isCorrected++;
            }
        }
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
        CHECK_OPTION (opts_in->VuiNalHrdParameters, opts_out->VuiNalHrdParameters, isCorrected);
        if (MFX_CODINGOPTION_ON == opts_in->FieldOutput)
        {
            isInvalid++;
            opts_out->FieldOutput = MFX_CODINGOPTION_UNKNOWN;
        }
        else if (MFX_CODINGOPTION_UNKNOWN == opts_in->FieldOutput || MFX_CODINGOPTION_OFF == opts_in->FieldOutput)
            opts_out->FieldOutput = opts_in->FieldOutput;
        else
        {
            isCorrected++;
            opts_out->FieldOutput = MFX_CODINGOPTION_UNKNOWN;
        }
    } else if (opts_out) {
        memset(opts_out, 0, sizeof(*opts_out));
    }

    *par_out = *out;
    if (out->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        out->GetCalcParams(par_out);

    if (isInvalid)
        return MFX_ERR_UNSUPPORTED;
    if (isCorrected)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_NULL_PTR1(request)

    // check for valid IOPattern
    mfxU16 IOPatternIn = par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);
    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0) ||
        ((IOPatternIn != MFX_IOPATTERN_IN_VIDEO_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_OPAQUE_MEMORY)))
       return MFX_ERR_INVALID_VIDEO_PARAM;

    if (par->Protected != 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxU16 nFrames = par->mfx.GopRefDist; // 1 for current and GopRefDist - 1 for reordering

    if( par->mfx.GopRefDist == 0 ){ //Aligned with ConvertVideoParam_H264enc and Query
        nFrames += 1; // 1 for current
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
    request->Info = par->mfx.FrameInfo;

    if(par->mfx.CodecProfile==MFX_PROFILE_AVC_MULTIVIEW_HIGH || par->mfx.CodecProfile==MFX_PROFILE_AVC_STEREO_HIGH) {
        mfxExtMVCSeqDesc *depinfo_in = (mfxExtMVCSeqDesc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
        if(depinfo_in) {
            request->NumFrameMin *= (depinfo_in->NumView + 1);
            request->NumFrameSuggested *= (depinfo_in->NumView + 1);
        } else {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY){
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }else if(par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } else {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::GetEncodeStat(mfxEncodeStat *stat)
{
    if( !m_isinitialized ) return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat)
    memset(stat, 0, sizeof(mfxEncodeStat));

    stat->NumCachedFrame = m_views[0].m_frameCountBufferedSync; //(mfxU32)m_inFrames.size();
    stat->NumBit = m_views[0].m_totalBits;
    stat->NumFrame = m_views[0].m_encodedFrames;

    for(mfxU32 i=1; i<m_numviews; i++) {
        stat->NumCachedFrame += m_views[i].m_frameCountBufferedSync; //(mfxU32)m_inFrames.size();
        stat->NumBit += m_views[i].m_totalBits;
        stat->NumFrame += m_views[i].m_encodedFrames;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *inputSurface, mfxBitstream *bs)
{
    mfxStatus st;
    UMC::VideoData m_data_in;
    UMC::MediaData m_data_out;
    mfxU32 minBufferSize;
    mfxBitstream *bitstream = 0;
    mfxU8* dataPtr;
    mfxU32 initialDataLength = 0, initialBFDataLength = 0, fullDataOffset = 0;

    H264ENC_UNREFERENCED_PARAMETER(pInternalParams);

    if( !m_isinitialized ) return MFX_ERR_NOT_INITIALIZED;

    // inputSurface can be opaque. Get real surface from core
    mfxFrameSurface1 *surface = GetOriginalSurface(inputSurface);

    if (inputSurface != surface) { // input surface is opaque surface
        assert(surface);
        assert(inputSurface);
        surface->Data.FrameOrder = inputSurface->Data.FrameOrder;
        surface->Data.TimeStamp = inputSurface->Data.TimeStamp;
        surface->Data.Corrupted = inputSurface->Data.Corrupted;
        surface->Data.DataFlag = inputSurface->Data.DataFlag;
        surface->Info = inputSurface->Info;
    }

    if (surface) {
        if(m_oidxmap[surface->Info.FrameId.ViewId]!=-1)
            m_cview=m_views + m_oidxmap[surface->Info.FrameId.ViewId];
        else
            return MFX_ERR_INVALID_HANDLE;
    } else {
        if(m_views[m_numviews-1].m_frameCountBuffered) {
            mfxU32 pos, minpos = 0, buffcnt = m_views[0].m_frameCountBuffered;
            for ( pos = 0; pos < m_numviews; ++pos ) {
                if(m_views[pos].m_frameCountBuffered > buffcnt) { minpos = pos; break; }
            }
            m_cview = m_views + minpos;
        } else {
             return MFX_ERR_MORE_DATA;
        }
    }

    Status res = UMC_OK;

    H264CoreEncoder_8u16s *enc = m_cview->enc;
    mfxFrameSurface1 *initialSurface = surface;

    // bs could be NULL if input frame is buffered
    if (bs) {
        bitstream = bs;

        fullDataOffset = bitstream->DataOffset + bitstream->DataLength;
        dataPtr = bitstream->Data + fullDataOffset;

        initialDataLength = bitstream->DataLength;
        initialBFDataLength = (mfxU32)m_bf_stream.GetDataSize();

        //Check for enough bitstream buffer size
        if (enc->brc && enc->m_SeqParamSet->vui_parameters_present_flag && enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag)
        {
            VideoBrcParams  curBRCParams;
            enc->brc->GetParams(&curBRCParams);
            minBufferSize = curBRCParams.HRDBufferSizeBytes;
        }
        else if (enc->m_info.rate_controls.method != H264_RCM_QUANT)
            minBufferSize = (mfxU32)(enc->m_info.info.bitrate / enc->m_info.info.framerate * 2 / 8);
        else
            minBufferSize = 0;

        if( bitstream->MaxLength - fullDataOffset - (mfxU32)m_bf_stream.GetDataSize() < minBufferSize && enc->m_info.rate_controls.method != H264_RCM_QUANT )
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        m_data_out.SetBufferPointer(dataPtr, bitstream->MaxLength - fullDataOffset);
        m_data_out.SetDataSize(0);
    }

    if (surface)
    {
        // check PicStruct in surface
        mfxU16 picStruct;
        st = AnalyzePicStruct(m_cview, surface->Info.PicStruct, &picStruct);

        if (st < MFX_ERR_NONE) // error occured
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
        m_data_in.Init(enc->m_info.info.clip_info.width, enc->m_info.info.clip_info.height, cf, 8);

        bool locked = false;
        if (m_cview->m_useAuxInput) { // copy from d3d to internal frame in system memory

            // Lock internal. FastCopy to use locked, to avoid additional lock/unlock
            st = m_core->LockFrame(m_cview->m_auxInput.Data.MemId, &m_cview->m_auxInput.Data);
            MFX_CHECK_STS(st);

            st = m_core->DoFastCopyWrapper(&m_cview->m_auxInput,
                                           MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                           surface,
                                           MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            MFX_CHECK_STS(st);

//            m_core->DecreaseReference(&(surface->Data)); // not here to keep related mfxEncodeCtrl

            m_cview->m_auxInput.Data.FrameOrder = surface->Data.FrameOrder;
            m_cview->m_auxInput.Data.TimeStamp = surface->Data.TimeStamp;
            surface = &m_cview->m_auxInput; // replace input pointer
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

        if (!surface->Data.Y || surface->Data.Pitch > 0x7fff || surface->Data.Pitch < enc->m_info.info.clip_info.width || !surface->Data.Pitch ||
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

        if (m_cview->m_mfxVideoParam.mfx.EncodedOrder)
            m_cview->m_frameCount = surface->Data.FrameOrder;

        // need to call always now
        res = Encode(enc, &m_data_in, &m_data_out, m_cview->m_frameCount, picStruct, ctrl, initialSurface);

        if (m_cview->m_useAuxInput) {
            m_core->UnlockFrame(m_cview->m_auxInput.Data.MemId, &m_cview->m_auxInput.Data);
        } else {
            if (locked)
                m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));
        }
        m_cview->m_frameCount ++;
    } else {
        res = Encode(enc, 0, &m_data_out, m_cview->m_frameCount, 0, 0, 0);
    }

    mfxStatus mfxRes = MFX_ERR_MORE_DATA;
    if (UMC_OK == res && enc->m_pCurrentFrame && bs) {
        //Copy timestamp to output.
        mfxFrameSurface1* surf = 0;
        bs->TimeStamp = GetMfxTimeStamp(m_data_out.GetTime());
        surf = (mfxFrameSurface1*)enc->m_pCurrentFrame->m_pAux[1];
        if (surf) {
            bs->TimeStamp = surf->Data.TimeStamp;
            bs->DecodeTimeStamp = CalculateDTSFromPTS_H264enc(
                m_cview->m_mfxVideoParam.mfx.FrameInfo,
                enc->m_SEIData.PictureTiming.dpb_output_delay,
                bs->TimeStamp); 
            m_core->DecreaseReference(&(surf->Data));
            enc->m_pCurrentFrame->m_pAux[0] = enc->m_pCurrentFrame->m_pAux[1] = 0;
        }

        // Set FrameType
        mfxU16 frame_type = 0;
        switch (enc->m_pCurrentFrame->m_PicCodType) { // set pic type for frame or 1st field
            case INTRAPIC: frame_type |= MFX_FRAMETYPE_I; break;
            case PREDPIC:  frame_type |= MFX_FRAMETYPE_P; break;
            case BPREDPIC: frame_type |= MFX_FRAMETYPE_B; break;
            default: return MFX_ERR_UNKNOWN;
        }
        if (enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE) { // surface was encoded as 2 fields - set pic type for 2nd field
            switch (enc->m_pCurrentFrame->m_PicCodTypeFields[1]) {
                case INTRAPIC: frame_type |= MFX_FRAMETYPE_xI; break;
                case PREDPIC:  frame_type |= MFX_FRAMETYPE_xP; break;
                case BPREDPIC: frame_type |= MFX_FRAMETYPE_xB; break;
                default: return MFX_ERR_UNKNOWN;
            }
        }

        if( enc->m_pCurrentFrame->m_bIsIDRPic ){
            frame_type |= MFX_FRAMETYPE_IDR;
        }
        if( enc->m_pCurrentFrame->m_RefPic ){
            frame_type |= MFX_FRAMETYPE_REF;
            if (enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                frame_type |= MFX_FRAMETYPE_xREF;
        }
        bs->FrameType = frame_type;

        //Set pic structure
        bs->PicStruct = enc->m_pCurrentFrame->m_coding_type;

        if( m_data_out.GetDataSize() != 0 ) mfxRes = MFX_ERR_NONE;
    } else {
        mfxRes = h264enc_ConvertStatus(res);
        if (mfxRes == MFX_ERR_MORE_DATA)
            mfxRes = MFX_ERR_NONE;
    }

    // bs could be NULL if input frame is buffered
    if (bs) {
        mfxI32 bytes = (mfxU32) m_data_out.GetDataSize();
        bitstream->DataLength += bytes;

        if( UMC_OK == res && m_cview->m_putEOSeq && enc->m_pCurrentFrame && enc->m_pCurrentFrame->m_putEndOfSeq) {
            WriteEndOfSequence(bs);
            enc->m_pCurrentFrame->m_putEndOfSeq = false;
        }

        if( m_cview->m_putEOStream && surface == NULL){
            WriteEndOfStream(bs);
        }

        if(bitstream->DataLength != initialDataLength)
          m_cview->m_encodedFrames++;

        m_cview->m_totalBits += (bitstream->DataLength - initialDataLength)*8;
        m_cview->m_totalBits += ((mfxU32)m_bf_stream.GetDataSize() - initialBFDataLength)*8;
    }

    return mfxRes;
}

//mfxStatus MFXVideoENCODEMVC::InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts)
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

mfxStatus MFXVideoENCODEMVC::GetVideoParam(mfxVideoParam *par)
{
    if( !m_isinitialized ) return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par)
    CHECK_VERSION(par->Version);
    mfxVideoInternalParam tmp;

    mfxU32      i;

    if (CheckExtBuffers_H264enc( par->ExtParam, par->NumExtParam ) != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;
    mfxExtCodingOption *opts = GetExtCodingOptions( par->ExtParam, par->NumExtParam );
    mfxExtMVCSeqDesc   *depinfo = (mfxExtMVCSeqDesc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );

    mfxStatus st = ConvertVideoParamBack_H264enc(&m_views[0].m_mfxVideoParam, m_views[0].enc); // not needed if is done in Init and Reset
    MFX_CHECK_STS(st);

    // copy structures and extCodingOption if exist in dst
    par->mfx = m_views[0].m_mfxVideoParam.mfx;
    tmp = m_views[0].m_mfxVideoParam;
    par->Protected = 0;
    par->AsyncDepth = m_views[0].m_mfxVideoParam.AsyncDepth;
    par->IOPattern = m_views[0].m_mfxVideoParam.IOPattern;

    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        for(i=1; i<m_numviews; i++) {
            tmp.calcParam.TargetKbps += m_views[i].m_mfxVideoParam.calcParam.TargetKbps;
            tmp.calcParam.InitialDelayInKB += m_views[i].m_mfxVideoParam.calcParam.InitialDelayInKB;
            tmp.calcParam.BufferSizeInKB += m_views[i].m_mfxVideoParam.calcParam.BufferSizeInKB;
            tmp.calcParam.MaxKbps += m_views[i].m_mfxVideoParam.calcParam.MaxKbps;
        }
        tmp.GetCalcParams(par);
    }
    par->mfx.CodecProfile = m_views[1].m_mfxVideoParam.mfx.CodecProfile;

    if (opts != NULL) {
        m_views[0].m_extOption.EndOfSequence = (mfxU16)( m_views[0].m_putEOSeq ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_views[0].m_extOption.ResetRefList  = (mfxU16)( m_views[0].m_resetRefList ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_views[0].m_extOption.EndOfStream   = (mfxU16)( m_views[0].m_putEOStream ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        m_views[0].m_extOption.PicTimingSEI  = (mfxU16)( m_views[0].m_putTimingSEI ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF );
        *opts = m_views[0].m_extOption;
    }

    if(depinfo != NULL) {
        depinfo->NumView = m_mvcdesc.NumView;
        depinfo->NumViewId = m_mvcdesc.NumViewId;
        depinfo->NumOP = m_mvcdesc.NumOP;
        if(depinfo->NumView <= depinfo->NumViewAlloc) {
            if(depinfo->View != NULL) {
                MFX_INTERNAL_CPY(depinfo->View, m_mvcdesc.View, m_mvcdesc.NumView*sizeof(mfxMVCViewDependency));
            } else {
                return MFX_ERR_NULL_PTR;
            }
        } else {
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
        if(depinfo->NumViewId <= depinfo->NumViewIdAlloc) {
            if(depinfo->ViewId != NULL) {
                MFX_INTERNAL_CPY(depinfo->ViewId, m_mvcdesc.ViewId, m_mvcdesc.NumViewId*sizeof(mfxU16));
            } else {
                return MFX_ERR_NULL_PTR;
            }
        } else {
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
        if(depinfo->NumOP <= depinfo->NumOPAlloc) {
            if(depinfo->OP != NULL) {
                MFX_INTERNAL_CPY(depinfo->OP, m_mvcdesc.OP, m_mvcdesc.NumOP*sizeof(mfxMVCOperationPoint));
                for(i=0; i<depinfo->NumOP; i++) {
                    ptrdiff_t offset = m_mvcdesc.OP[i].TargetViewId - m_mvcdesc.ViewId;
                    depinfo->OP[i].TargetViewId = depinfo->ViewId + offset;
                }
            } else {
                return MFX_ERR_NULL_PTR;
            }
        } else {
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::GetFrameParam(mfxFrameParam * /*par*/)
{
    return MFX_ERR_UNSUPPORTED;
}
//
//mfxStatus MFXVideoENCODEMVC::GetSliceParam(mfxSliceParam *par)
//{
//    MFX_CHECK_NULL_PTR1(par)
//    *par = m_mfxSliceParam;
//    return MFX_ERR_NONE;
//}

// selects buffered B to become references after normal reference frame came
// number of ref B should be <= num_ref_frames - 2
void MFXVideoENCODEMVC::MarkBRefs(mfxI32 lastIPdist)
{
    H264CoreEncoder_8u16s* enc = m_cview->enc;

    if ( enc->m_info.treat_B_as_reference && lastIPdist > 2 && enc->m_SeqParamSet->num_ref_frames > 2) {
        // distribute B refs with at least 1 frame gap. Later can be done more smart.
        mfxI32 maxB = lastIPdist; // intervals
        mfxI32 maxBrefs = lastIPdist/2;
        mfxI32 pos = 0;
        mfxI32 dist;
        if (maxBrefs > enc->m_SeqParamSet->num_ref_frames - 2)
            maxBrefs = enc->m_SeqParamSet->num_ref_frames - 2;
        maxBrefs += 1; // intervals

        for (dist = lastIPdist-1; dist>0; dist--) { // distance from current !B to oldest B in a row
            pos += maxBrefs;
            if (pos >= maxB) {
                sH264EncoderFrame_8u16s *pCurr;
                pos -= maxB;
                // the frame to be refB, look for it by POC
                for (pCurr = enc->m_cpb.m_pTail; pCurr; pCurr = pCurr->m_pPreviousFrame)
                    if (enc->m_pCurrentFrame->m_PicOrderCnt[0]/2 - pCurr->m_PicOrderCnt[0]/2 == dist) {
                        // must be pCurr->m_PicCodType == BPREDPIC && !pCurr->m_RefPic
                        pCurr->m_RefPic = 1; // make B frame reference
                        break;
                    }
            }
        }
    }
}

// select frame type on VPP analysis results
// or psy-analysis if ANALYSE_AHEAD set
// performed now on previous to latest, so, having next frame in cpb
// returns 1 if appeared B frame without L1 refs, 0 otherwise
// bNotLast for PsyAnalyze means have next frame, for !Psy - have current (new)
mfxI32 MFXVideoENCODEMVC::SelectFrameType_forLast( mfxI32 frameNum, bool bNotLast, mfxEncodeCtrl *ctrl )
{
    mfxExtVppAuxData* optsSA = 0;
    bool scene_change = false;
    bool close_gop = false;
    bool internal_detect = false;
    H264CoreEncoder_8u16s* enc = m_cview->enc;
    bool allowTypeChange = !m_cview->m_mfxVideoParam.mfx.EncodedOrder && !(m_cview->m_mfxVideoParam.mfx.GopOptFlag & MFX_GOP_STRICT);
    EnumPicCodType eOriginalType;

    if (!bNotLast && !(enc->m_Analyse_ex & ANALYSE_PSY_AHEAD)) { // terminate GOP, no input
        if( enc->m_pLastFrame && enc->m_pLastFrame->m_PicCodType == BPREDPIC && !enc->m_pLastFrame->m_wasEncoded) {
            if(allowTypeChange) {
                enc->m_pLastFrame->m_PicCodType = PREDPIC;
                enc->m_pLastFrame->m_RefPic = 1;
            } else
                return 1; // B without L1 refs
        }
        return 0;
    }

    if (!frameNum || (Ipp32s)(frameNum-m_cview->m_lastIframe) >= enc->m_info.key_frame_controls.interval)
        eOriginalType = INTRAPIC;
    else if ((Ipp32s)(frameNum-m_cview->m_lastRefFrame) > enc->m_info.B_frame_rate)
        eOriginalType = PREDPIC;
    else
        eOriginalType = BPREDPIC;

    if (allowTypeChange) {
        if (ctrl)
            optsSA = (mfxExtVppAuxData*)GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA );
        if (optsSA) // vpp_aux present and filled
            scene_change = (optsSA->SceneChangeRate > 90
                // || optsSA->SpatialComplexity && optsSA->TemporalComplexity && optsSA->SpatialComplexity*4 <= 3*optsSA->TemporalComplexity
                );
        if (!scene_change) {
            internal_detect = ((enc->m_Analyse & ANALYSE_FRAME_TYPE) != 0);
        }

        //if (optsSA) printf("\ts:%4d t:%4d chance:%3d%c%c ", optsSA->SpatialComplexity, optsSA->TemporalComplexity, optsSA->SceneChangeRate,
        //    optsSA->SpatialComplexity*4 <= 3*optsSA->TemporalComplexity ? 'I':' ',
        //    optsSA->SpatialComplexity*2 <= 3*optsSA->TemporalComplexity ? 'P':' ');
    } else
        optsSA = 0; // no need to detect

    if (!bNotLast && eOriginalType == BPREDPIC && allowTypeChange ) {
        eOriginalType = PREDPIC; // avoid tail B (display order)
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

    //if (bNotLast)
    {
        EnumPicCodType ePictureType;
        //enc->m_pCurrentFrame->m_PicOrderCounterAccumulated = frameNum*2; // TODO: find Frame order for EncodedOrder
        //enc->m_pCurrentFrame->m_PicOrderCnt[0] = enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;
        //enc->m_pCurrentFrame->m_PicOrderCnt[1] = enc->m_pCurrentFrame->m_PicOrderCounterAccumulated + 1;

        enc->m_pCurrentFrame->m_bIsIDRPic = 0; // TODO: decide on IDR for EncodedOrder
        if (m_cview->m_mfxVideoParam.mfx.EncodedOrder) {
            ePictureType = enc->m_pCurrentFrame->m_PicCodType;
        }
        else if ( !frameNum || eOriginalType == INTRAPIC || scene_change ) {
            ePictureType = INTRAPIC;
        } else if (eOriginalType == PREDPIC  || (optsSA && optsSA->SpatialComplexity*2 <= 3*optsSA->TemporalComplexity && optsSA->TemporalComplexity)) {
            ePictureType = PREDPIC;
        } else
            ePictureType = BPREDPIC; // B can be changed later
        enc->m_pCurrentFrame->m_PicCodType = ePictureType;

        if(m_cview->m_mfxVideoParam.mfx.EncodedOrder && m_cview!=m_views) {
            enc->m_pCurrentFrame->m_RefPic = m_views[0].enc->m_pCurrentFrame->m_RefPic;
            enc->m_pCurrentFrame->m_bIsIDRPic = (ctrl && (ctrl->FrameType & MFX_FRAMETYPE_IDR)!=0);
        } else {
            enc->m_pCurrentFrame->m_RefPic = ePictureType != BPREDPIC;
        }

        if (internal_detect && !(enc->m_Analyse_ex & ANALYSE_PSY_TYPE_FR)) { // can change B->P->I
            H264CoreEncoder_FrameTypeDetect_8u16s(enc);
            ePictureType = enc->m_pCurrentFrame->m_PicCodType;
        }

        if (ePictureType == INTRAPIC)
            if (!frameNum || (Ipp32s)(frameNum - m_cview->m_lastIDRframe) >= enc->m_info.key_frame_controls.interval * (enc->m_info.key_frame_controls.idr_interval)+1
                || (ctrl && (ctrl->FrameType & MFX_FRAMETYPE_IDR))) {
                    enc->m_pCurrentFrame->m_bIsIDRPic = 1;
            }

        if (ePictureType == INTRAPIC || ePictureType == PREDPIC)
            if (enc->m_info.treat_B_as_reference)
                MarkBRefs(frameNum - m_cview->m_lastRefFrame);
        if (ePictureType == INTRAPIC) {
            m_cview->m_lastIframe = frameNum;
            m_cview->m_lastRefFrame = frameNum;
            if (enc->m_pCurrentFrame->m_bIsIDRPic)
                m_cview->m_lastIDRframe = frameNum;
        } else if (ePictureType == PREDPIC)
            m_cview->m_lastRefFrame = frameNum;
        //else //if ( ((frameNum-m_lastRefFrame)&1) == 0 && enc->m_info.treat_B_as_reference)
        //    enc->m_pCurrentFrame->m_RefPic = 0; // make B frames reference later, if needed

        close_gop = ePictureType == INTRAPIC && (m_cview->m_mfxVideoParam.mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT)) == MFX_GOP_CLOSED;
        //enc->m_pCurrentFrame->m_FrameNum = frameNum;
        //enc->m_PicOrderCnt = frameNum - enc->m_PicOrderCnt_Accu; // here???
    }
    //Close previous GOP in the end, on scene change, on IDR (need all I when CLOSED_GOP is set?)
    // if last input proposed as B, change it to P
    if ( scene_change || enc->m_pCurrentFrame->m_bIsIDRPic || close_gop) {
        if( enc->m_pLastFrame && enc->m_pLastFrame->m_PicCodType == BPREDPIC && !enc->m_pLastFrame->m_wasEncoded) {
            if(allowTypeChange) {
                enc->m_pLastFrame->m_PicCodType = PREDPIC;
                enc->m_pLastFrame->m_RefPic = 1;
            } else {
                // encode all the remaining B frames in current GOP with no L1 refs
                for (H264EncoderFrame_8u16s *fr = enc->m_cpb.m_pHead; fr; fr = fr->m_pFutureFrame)
                    if (!fr->m_wasEncoded && fr->m_PicCodType == BPREDPIC)
                        fr->m_noL1RefForB = true;
            }
        }
    }

    // control insertion of "End of sequence" NALU
    // if B frame buffer is used then choose frame for EOSeq insertion when current frame is IDR
    if (frameNum && enc->m_pCurrentFrame->m_bIsIDRPic && m_cview->m_frameCountBuffered) {
        H264EncoderFrame_8u16s *fr, *frEOSeq = 0;
        for (fr = enc->m_cpb.m_pHead; fr; fr = fr->m_pFutureFrame) {
            if (!fr->m_wasEncoded) {
                if (!frEOSeq || fr->m_PicCodType > frEOSeq->m_PicCodType ||
                    ((fr->m_PicCodType == BPREDPIC && frEOSeq->m_PicCodType == BPREDPIC) && ((frEOSeq->m_RefPic && !fr->m_RefPic) || (frEOSeq->m_RefPic == fr->m_RefPic && fr->m_PicOrderCnt[0] > frEOSeq->m_PicOrderCnt[0]))) ||
                    (fr->m_PicCodType != BPREDPIC && fr->m_PicCodType == frEOSeq->m_PicCodType && fr->m_PicOrderCnt[0] > frEOSeq->m_PicOrderCnt[0]))
                    frEOSeq = fr;
            }
        }
        if (frEOSeq)
            frEOSeq->m_putEndOfSeq = true;
    }

    // if B frame buffer isn't used then then check next frame for IDR to insert EOSeq
    if (!m_cview->m_frameCountBuffered && (Ipp32s)((frameNum + 1) - m_cview->m_lastIframe) >= enc->m_info.key_frame_controls.interval &&
        (Ipp32s)((frameNum + 1) - m_cview->m_lastIDRframe) >= enc->m_info.key_frame_controls.interval * (enc->m_info.key_frame_controls.idr_interval+1))
            enc->m_pCurrentFrame->m_putEndOfSeq = true;

    // if current is B and no next frame or need to encode B in current GOP with no L1 refs
    if (((scene_change || enc->m_pCurrentFrame->m_bIsIDRPic || close_gop) && !allowTypeChange) ||
        (enc->m_pCurrentFrame->m_PicCodType == BPREDPIC && !bNotLast))
        return 1;

    return 0;
}

#define PIXTYPE Ipp8u

Status MFXVideoENCODEMVC::Encode(
    void* state,
    VideoData* src,
    MediaData* dst,
    Ipp32s frameNum,
    Ipp16u picStruct,
    mfxEncodeCtrl *ctrl,
    mfxFrameSurface1 *surface)
{

    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s*)state;
    Status ps = UMC_OK;
    core_enc->m_pCurrentFrame = 0;
    core_enc->m_field_index = 0;
    ////Ipp32s isRef = 0;


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
        core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated = frameNum*2; // TODO: find Frame order for EncodedOrder
        // POC for fields is set assuming that encoding order and display order are equal (no reordering of fields)
        core_enc->m_pCurrentFrame->m_PicOrderCnt[0] = core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;
        core_enc->m_pCurrentFrame->m_PicOrderCnt[1] = core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated + 1;

        core_enc->m_pCurrentFrame->m_PQUANT[0] = core_enc->m_pCurrentFrame->m_PQUANT[1] = 0;

        // set per-frame const QP (if any)
        if (ctrl && core_enc->m_info.rate_controls.method == H264_RCM_QUANT)
            core_enc->m_pCurrentFrame->m_PQUANT[0] = core_enc->m_pCurrentFrame->m_PQUANT[1] = (Ipp8u)ctrl->QP;

        if (m_cview->m_mfxVideoParam.mfx.EncodedOrder) { // set frame/fields types and per-frame QP in EncodedOrder
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
        core_enc->m_pCurrentFrame->m_FrameTag = surface->Data.FrameOrder; // mark UMC frame with it's mfx FrameOrder
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
    //SelectFrameType_forLast(frameNum, src, (mfxEncodeCtrl*)pInternalParams);
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

    //if (core_enc->m_pCurrentFrame) {
    //    Ipp32s uNumMBs = core_enc->m_HeightInMBs * core_enc->m_WidthInMBs;
    //    printf("\nfr:%3d  %d/%d \t%d/%d \t%d/%d \t%d/%d\t", frameNum,
    //        core_enc->m_pCurrentFrame->psyStatic.dc/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].dc/uNumMBs,
    //        core_enc->m_pCurrentFrame->psyStatic.bgshift/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].bgshift/uNumMBs,
    //        core_enc->m_pCurrentFrame->psyStatic.details/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].details/uNumMBs,
    //        core_enc->m_pCurrentFrame->psyStatic.interl/uNumMBs, core_enc->m_pCurrentFrame->psyDyna[0].interl/uNumMBs);

    //    //static int head = 0;
    //    //FILE* csv = fopen("psy.csv", head?"at":"wt");
    //    //if (!head) {
    //    //    fprintf (csv, "num; dc; sh; det; int; Ddc; Dsh; Ddet; Dint\n");
    //    //    head=1;
    //    //}
    //    //fprintf (csv, "%d; %d; %d; %d; %d; %d; %d; %d; %d\n", frameNum,
    //    //    core_enc->m_pCurrentFrame->psyStatic.dc/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyStatic.bgshift/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyStatic.details/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyStatic.interl/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyDyna[0].dc/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyDyna[0].bgshift/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyDyna[0].details/uNumMBs,
    //    //    core_enc->m_pCurrentFrame->psyDyna[0].interl/uNumMBs
    //    //);
    //    //fclose(csv);
    //}

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
            frameNum --;
        }
    }
#endif // USE_PSYINFO

    mfxI32 noL1 = 0;
    if(core_enc->m_pCurrentFrame) {
        bool notLastFrame = (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ? (core_enc->m_pNextFrame != 0) : true;
        ctrl = (mfxEncodeCtrl*)core_enc->m_pCurrentFrame->m_pAux[0];
        // Scene analysis info from VPP is inside
        noL1 = SelectFrameType_forLast(frameNum, notLastFrame, ctrl);

        if (core_enc->m_pCurrentFrame)
            core_enc->m_pLastFrame = core_enc->m_pCurrentFrame;

        H264EncoderFrame_InitRefPicListResetCount_8u16s(core_enc->m_pCurrentFrame, 0);
        H264EncoderFrame_InitRefPicListResetCount_8u16s(core_enc->m_pCurrentFrame, 1);
        if (core_enc->m_pCurrentFrame->m_bIsIDRPic)
            H264EncoderFrameList_IncreaseRefPicListResetCount_8u16s(&core_enc->m_cpb, core_enc->m_pCurrentFrame);
    } else if (!(core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD))
        noL1 = SelectFrameType_forLast(frameNum, false, 0); // to terminate GOP


#if 1 // UMC-level reordering. Redundant if MFX-level reordering

    //////////////////////////////////////////////////////////////////////////
    // After next call core_enc->m_pCurrentFrame points
    // not to the input frame, but
    // to the frame to be encoded
    //////////////////////////////////////////////////////////////////////////
    EnumPicCodType  origFrameType = INTRAPIC;

    if (m_cview->m_mfxVideoParam.mfx.EncodedOrder == 0) {
        Ipp32u noahead = (core_enc->m_Analyse_ex & ANALYSE_PSY_AHEAD) ? 0 : 1;
        if (m_cview->m_frameCount >= noahead && m_cview->m_frameCountBuffered < (mfxU32)core_enc->m_info.B_frame_rate + 1 - noahead && core_enc->m_pCurrentFrame) {
            // these B_frame_rate frames go to delay (B-reorder) buffer
            m_cview->m_frameCountBuffered++;
            core_enc->m_pCurrentFrame = 0;
            return UMC_OK; // return OK from async part in case of buffering
        } else {
            if (core_enc->m_pCurrentFrame || m_cview->m_frameCountBuffered > 0)
            {
                if(m_cview==m_views) {
                    core_enc->m_pCurrentFrame = H264EncoderFrameList_findOldestToEncode_8u16s(
                        &core_enc->m_cpb,
                        &core_enc->m_dpb,
                        noL1 ? 0 : core_enc->m_l1_cnt_to_start_B,
                        core_enc->m_info.treat_B_as_reference);
                } else {
                    Ipp32u base_poc = m_views[0].enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;
                    core_enc->m_pCurrentFrame = core_enc->m_cpb.m_pHead;
                    while(core_enc->m_pCurrentFrame) {
                        if(!core_enc->m_pCurrentFrame->m_wasEncoded && core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated == base_poc) break;
                        core_enc->m_pCurrentFrame = core_enc->m_pCurrentFrame->m_pFutureFrame;
                    }
                }
            }
            if (!core_enc->m_pCurrentFrame) {
                if (m_cview->m_frameCountBuffered==0)
                    m_cview->m_frameCountBuffered = 1; // end of encoding, place for breakpoint
                m_cview->m_frameCountBuffered --;
            }
            else {
                if(!src) m_cview->m_frameCountBuffered --;
                // set pic types for fields
                origFrameType = core_enc->m_pCurrentFrame->m_PicCodType;
                if(m_cview!=m_views) {
                    core_enc->m_pCurrentFrame->m_PicCodType = m_views[0].enc->m_pCurrentFrame->m_PicCodType;
                    core_enc->m_pCurrentFrame->m_RefPic = m_views[0].enc->m_pCurrentFrame->m_RefPic;
                    core_enc->m_pCurrentFrame->m_bIsIDRPic = m_views[0].enc->m_pCurrentFrame->m_bIsIDRPic;
                    origFrameType = core_enc->m_pCurrentFrame->m_PicCodType;

                    mfxU16  extra_l0_refnum = (core_enc->m_pCurrentFrame->m_bIsIDRPic) ? m_cview->m_dependency.NumAnchorRefsL0 : m_cview->m_dependency.NumNonAnchorRefsL0;
                    mfxU16  extra_l1_refnum = (core_enc->m_pCurrentFrame->m_bIsIDRPic) ? m_cview->m_dependency.NumAnchorRefsL1 : m_cview->m_dependency.NumNonAnchorRefsL1;

                    if(core_enc->m_pCurrentFrame->m_PicCodType==INTRAPIC && extra_l0_refnum) core_enc->m_pCurrentFrame->m_PicCodType = PREDPIC;
                    if(core_enc->m_pCurrentFrame->m_PicCodType== PREDPIC && extra_l1_refnum) core_enc->m_pCurrentFrame->m_PicCodType = BPREDPIC;
                }
                core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] = core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = core_enc->m_pCurrentFrame->m_PicCodType;
                if (core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC && core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE) // encode as "IP" field pair
                    core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] = PREDPIC;
            }
        }
    } else {
        origFrameType = core_enc->m_pCurrentFrame->m_PicCodType;
    }

//if (core_enc->m_pCurrentFrame)
//    printf("\t%4d  in:%3d T:%2d A:%3d  out:%3d T:%2d A:%3d\n", frameNum,
//        core_enc->m_pLastFrame ? core_enc->m_pLastFrame->m_PicOrderCnt[0]/2 : 999,
//        core_enc->m_pLastFrame ? core_enc->m_pLastFrame->m_PicCodType : 99,
//        core_enc->m_pLastFrame ? core_enc->m_pLastFrame->m_PicOrderCounterAccumulated/2 : 999,
//        core_enc->m_pCurrentFrame ? core_enc->m_pCurrentFrame->m_PicOrderCnt[0]/2 : 999,
//        core_enc->m_pCurrentFrame ? core_enc->m_pCurrentFrame->m_PicCodType : 99,
//        core_enc->m_pCurrentFrame ? core_enc->m_pCurrentFrame->m_PicOrderCounterAccumulated/2 : 999);

    if (core_enc->m_pCurrentFrame){

        // set const QP for frame or both fields
        if (core_enc->m_info.rate_controls.method == H264_RCM_QUANT) {
            ps = SetConstQP(m_cview->m_mfxVideoParam.mfx.EncodedOrder, core_enc->m_pCurrentFrame, &core_enc->m_info);
            if (ps != UMC_OK)
                return ps;
        }

        core_enc->m_pCurrentFrame->m_noL1RefForB = false;

        if (core_enc->m_pCurrentFrame->m_bIsIDRPic)
            core_enc->m_uFrameCounter = 0;
        core_enc->m_pCurrentFrame->m_FrameNum = core_enc->m_uFrameCounter;

        //            if( core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC || core_enc->m_info.treat_B_as_reference )
        if( core_enc->m_pCurrentFrame->m_PicCodType != BPREDPIC || core_enc->m_pCurrentFrame->m_RefPic )
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
#endif // UMC-level reordering

    EnumPicCodType ePictureType = core_enc->m_pCurrentFrame->m_PicCodType;
    EnumPicClass    ePic_Class;

    if (core_enc->m_pCurrentFrame->m_bIsIDRPic || core_enc->m_uFrames_Num == 0) {
        ePic_Class = IDR_PIC;
        core_enc->m_l1_cnt_to_start_B = core_enc->m_info.num_ref_to_start_code_B_slice;
    } else {
        switch (origFrameType) {
            case INTRAPIC:
            case PREDPIC:
                ePic_Class = REFERENCE_PIC;
                break;
            case BPREDPIC:
                if(m_cview->m_mfxVideoParam.mfx.EncodedOrder && m_cview!=m_views)
                    ePic_Class = core_enc->m_pCurrentFrame->m_RefPic ? REFERENCE_PIC : DISPOSABLE_PIC;
                else
                    ePic_Class = core_enc->m_info.treat_B_as_reference && core_enc->m_pCurrentFrame->m_RefPic ? REFERENCE_PIC : DISPOSABLE_PIC;
                break;
            default:
                VM_ASSERT(false);
                ePic_Class = IDR_PIC;
                break;
        }
    }

    core_enc->m_svc_layer.svc_ext.priority_id = 0;
    core_enc->m_svc_layer.svc_ext.no_inter_layer_pred_flag = 1;
    //    core_enc->m_svc_layer.svc_ext.temporal_id = core_enc->m_pCurrentFrame->m_temporalId;
    core_enc->m_svc_layer.svc_ext.use_ref_base_pic_flag = 0;
    core_enc->m_svc_layer.svc_ext.discardable_flag = 0;
    core_enc->m_svc_layer.svc_ext.output_flag = 1;
    core_enc->m_svc_layer.svc_ext.reserved_three_2bits = 3; /* is needed for ref bit extractor */

    core_enc->m_svc_layer.svc_ext.store_ref_base_pic_flag = 0;
    core_enc->m_svc_layer.svc_ext.adaptive_ref_base_pic_marking_mode_flag = 0;

    // ePictureType is a reference variable.  It is updated by CompressFrame if the frame is internally forced to a key frame.
    ps = H264CoreEncoder_CompressFrame(state, ePictureType, ePic_Class, dst, (mfxEncodeCtrl*)core_enc->m_pCurrentFrame->m_pAux[0]);
    dst->SetTime( core_enc->m_pCurrentFrame->pts_start, core_enc->m_pCurrentFrame->pts_end);

    if (ps == UMC_OK)
        core_enc->m_pCurrentFrame->m_wasEncoded = true;
    H264CoreEncoder_CleanDPB_8u16s(state);
    if (ps == UMC_OK)
        core_enc->m_info.numEncodedFrames++;

    return ps;
}


mfxStatus MFXVideoENCODEMVC::WriteEndOfSequence(mfxBitstream *bs)
{
   H264BsReal_8u16s rbs;
   rbs.m_pbsRBSPBase = rbs.m_base.m_pbs = 0;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H264BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_EOSEQ, s, 0);
   return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMVC::WriteEndOfStream(mfxBitstream *bs)
{
   H264BsReal_8u16s rbs;
   rbs.m_pbsRBSPBase = rbs.m_base.m_pbs = 0;
   bool s = false;
   mfxU8* ptrToWrite = bs->Data + bs->DataOffset + bs->DataLength;
   bs->DataLength += H264BsReal_EndOfNAL_8u16s( &rbs, ptrToWrite, 0, NAL_UT_EOSTREAM, s, 0);
   return MFX_ERR_NONE;
}

// Fill UMC structures for reordering and cut of ref pic lists. For now in 2 cases:
// 1) External ref pic list control information provided via mfxEncodeCtrl (only progressive encoding w/0 B-frames is supported)
// 2) "IP" field pair is being encoded
Status MFXVideoENCODEMVC::H264CoreEncoder_ModifyRefPicList( H264CoreEncoder_8u16s* core_enc, mfxEncodeCtrl *ctrl)
{
    Ipp32s i;
    Ipp8u maxNumActiveRefsInL0, maxNumActiveRefsInL1;
    Ipp8u maxNumActiveRefsByStandard = core_enc->m_SliceHeader.field_pic_flag ? 32 : 16;
    bool m_use_intra_mbs_only = false;
    Ipp8u fieldIdx = core_enc->m_field_index;

    maxNumActiveRefsInL0 = maxNumActiveRefsInL1 = maxNumActiveRefsByStandard;
    core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 0;
    core_enc->m_ReorderInfoL0[fieldIdx].num_entries = 0;

    // try to get ref pic list control info
    mfxExtAVCRefListCtrl *pRefPicListCtrl = 0;
    if (ctrl && ctrl->ExtParam && ctrl->NumExtParam)
        pRefPicListCtrl = (mfxExtAVCRefListCtrl*)GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);

    // apply ref list control info if provided
    // only L0 ref list is configurable
    // recommended frames are reordered to beginning of the list (preserving order in ref list)
    // then other not prohibited frames are appended to recommended frames (preserving order in ref list)
    // if pRefPicListCtrl->NumRefIdxL0Active != 0 it's applied to ref list (and ignored if it's 0)
    // ref list L0 is cut by length (recommendedCount + notProhibitedCount) if last is nonzero and (recommendedCount + notProhibitedCount) < if pRefPicListCtrl->NumRefIdxL0Active
    // if (recommendedCount + notProhibitedCount == 0) then no reordering is applied and P frame is encoded using only Intra MBs
    if (pRefPicListCtrl && core_enc->m_pCurrentFrame->m_PicCodType == PREDPIC &&
        core_enc->m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE ) { // should be progressive encoding to apply ref pic list control info

        Ipp8u recommendedMap[16], notProhibitedMap[16];
        Ipp8u recommendedCount = 0; // number of recommended frames that are not prohibited
        Ipp8u notProhibitedCount = 0; // number of not recommended frames that are not prohibited
        Ipp8u prohibitedCount = 0; // number of prohibited frames
        Ipp8u numRefFramesL0 = 0; // number of ref frames in ref pic list L0
        bool need_to_reorder = false;

        Ipp32s currPicNum = core_enc->m_pCurrentFrame->m_PicNum[0];
        Ipp32s picNumL0PredWrap = currPicNum; // wrapped prediction for reordered picNum

        if (pRefPicListCtrl->NumRefIdxL0Active != 0)
            maxNumActiveRefsInL0 = pRefPicListCtrl->NumRefIdxL0Active;

        // here ref pic lists in m_pCurrentFrame contain obsolete data. This memory could be temporarily used to form ref list to fill reordering structures
        H264EncoderFrame_8u16s **pRefPicListL0 = core_enc->m_pCurrentFrame->m_pRefPicList[0].m_RefPicListL0.m_RefPicList;
        for (i=0; i<MAX_NUM_REF_FRAMES; i++) pRefPicListL0[i] = 0;
        H264EncoderFrame_8u16s *pFrm = core_enc->m_dpb.m_pHead;

        // form ref pic list L0
        while (pFrm) {
            if (pFrm->m_wasEncoded && pFrm->m_isShortTermRef[0] && pFrm->m_isShortTermRef[1]) {
                pRefPicListL0[currPicNum - pFrm->m_PicNum[0] - 1] = pFrm;
                numRefFramesL0 ++;
            }
            pFrm = pFrm->m_pFutureFrame;
        };

        // store counts and ref indexes of recommended frames and not prohibited frames
        // if frame is in both recommended and prohibited lists it's treated as prohibited
        i = 0;
        while (pRefPicListL0[i]) {

            if (IsRejected(pRefPicListL0[i]->m_FrameTag, pRefPicListCtrl))
                prohibitedCount ++;
            else {
                if (IsPreferred(pRefPicListL0[i]->m_FrameTag, pRefPicListCtrl)) {
                    recommendedMap[recommendedCount] = i;
                    recommendedCount ++;
                }
                else {
                    notProhibitedMap[notProhibitedCount] = i;
                    notProhibitedCount ++;
                }
            }
            i ++;
        }


        need_to_reorder = (recommendedCount && (recommendedCount != numRefFramesL0)) || (notProhibitedCount && prohibitedCount);

        if (need_to_reorder) {
            core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 1;

            // reorder recommended ref frames to beginning of ref pic list
            for (i = 0; i < recommendedCount; i ++) {
                if (picNumL0PredWrap >= pRefPicListL0[recommendedMap[i]]->m_PicNum[0]) {
                    core_enc->m_ReorderInfoL0[fieldIdx].reordering_of_pic_nums_idc[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = 0; // reordering_of_pic_nums_idc
                    core_enc->m_ReorderInfoL0[fieldIdx].reorder_value[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = picNumL0PredWrap - pRefPicListL0[recommendedMap[i]]->m_PicNum[0]; // abs_diff_pic_num
                }
                else {
                    core_enc->m_ReorderInfoL0[fieldIdx].reordering_of_pic_nums_idc[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = 1; // reordering_of_pic_nums_idc
                    core_enc->m_ReorderInfoL0[fieldIdx].reorder_value[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = pRefPicListL0[recommendedMap[i]]->m_PicNum[0] - picNumL0PredWrap; // abs_diff_pic_num
                }
                picNumL0PredWrap = pRefPicListL0[recommendedMap[i]]->m_PicNum[0];
                core_enc->m_ReorderInfoL0[fieldIdx].num_entries ++;
                if (core_enc->m_ReorderInfoL0[fieldIdx].num_entries >= maxNumActiveRefsInL0)
                    break;
            }

            // reorder not prohibited ref frames after recommended
            for (i = 0; i < notProhibitedCount; i ++) {
                if (core_enc->m_ReorderInfoL0[fieldIdx].num_entries >= maxNumActiveRefsInL0)
                    break;
                if (picNumL0PredWrap >= pRefPicListL0[notProhibitedMap[i]]->m_PicNum[0]) {
                    core_enc->m_ReorderInfoL0[fieldIdx].reordering_of_pic_nums_idc[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = 0; // reordering_of_pic_nums_idc
                    core_enc->m_ReorderInfoL0[fieldIdx].reorder_value[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = picNumL0PredWrap - pRefPicListL0[notProhibitedMap[i]]->m_PicNum[0]; // abs_diff_pic_num
                }
                else {
                    core_enc->m_ReorderInfoL0[fieldIdx].reordering_of_pic_nums_idc[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = 1; // reordering_of_pic_nums_idc
                    core_enc->m_ReorderInfoL0[fieldIdx].reorder_value[core_enc->m_ReorderInfoL0[fieldIdx].num_entries] = pRefPicListL0[notProhibitedMap[i]]->m_PicNum[0] - picNumL0PredWrap; // abs_diff_pic_num
                }
                picNumL0PredWrap = pRefPicListL0[notProhibitedMap[i]]->m_PicNum[0];
                core_enc->m_ReorderInfoL0[fieldIdx].num_entries ++;
            }

            // set curr_slice->num_ref_idx_lx_active to cut ref pic list
            maxNumActiveRefsInL0 = MIN(maxNumActiveRefsInL0, recommendedCount + notProhibitedCount);
        } else {
            core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 0;
            core_enc->m_ReorderInfoL0[fieldIdx].num_entries = 0;
            if (prohibitedCount == numRefFramesL0) {
                m_use_intra_mbs_only = true;
                maxNumActiveRefsInL0 = 1;
            }
        }
    }

    // "IP" pair is analog of key frame. I-field should be the only reference for P-field in the pair.
    // Prepare reordering of I-field to RefIdxL0 = 0 and cut of ref pic list for P-field
    // don't do reordering: 1) for Closed GOP and "IDR,P" pairs; 2) if reordering was specified before explicitly
    // here core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 should be 0. Could be nonzero only if reordering was specified before explicitly
    if (core_enc->m_pCurrentFrame->m_PicCodTypeFields[0] == INTRAPIC && core_enc->m_pCurrentFrame->m_PicCodTypeFields[1] == PREDPIC && core_enc->m_field_index &&
        core_enc->m_pCurrentFrame->m_bIsIDRPic == false && core_enc->m_info.use_reset_refpiclist_for_intra == false && core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 == 0) {
        VM_ASSERT(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); // should be interlaced encoding
        maxNumActiveRefsInL0 = 1;
        core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 1;
        core_enc->m_ReorderInfoL0[fieldIdx].num_entries = 1;
        core_enc->m_ReorderInfoL0[fieldIdx].reordering_of_pic_nums_idc[0] = 0; // subtract abs_diff_pic_num from predicted value of picNumL0
        core_enc->m_ReorderInfoL0[fieldIdx].reorder_value[0] = 1; // CurrPicNum - PicNum(I-field) = 1. CurrPicNum is a predicted value of picNumL0
    }

    Ipp8u startSliceNum = core_enc->m_info.num_slices * (core_enc->m_field_index ? 1 : 0);
    for(i = startSliceNum; i < startSliceNum + core_enc->m_info.num_slices; i ++) {
        H264Slice_8u16s *curr_slice = core_enc->m_Slices + i;
        curr_slice->m_MaxNumActiveRefsInL0 = maxNumActiveRefsInL0;
        if ((core_enc->m_ReorderInfoL0[fieldIdx].num_entries > 0 && core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0) ||
            maxNumActiveRefsInL0 < maxNumActiveRefsByStandard)
            curr_slice->b_PerformModificationOfL0 = true; // signal that ref pic list modification is required
        else
            curr_slice->b_PerformModificationOfL0 = false;
        curr_slice->b_PerformModificationOfL1 = false;
        curr_slice->m_use_intra_mbs_only = m_use_intra_mbs_only;
    }

    return UMC_OK;
}

Status MFXVideoENCODEMVC::H264CoreEncoder_CompressFrame(
    void* state,
    EnumPicCodType& ePictureType,
    EnumPicClass& ePic_Class,
    MediaData* tf_stream,
    mfxEncodeCtrl *ctrl)
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status status = UMC_OK;
    Ipp32s slice;
    MediaData  *dst, *pDst[2] = { tf_stream, &m_bf_stream };
    size_t      pDataSize[2];
    bool bufferOverflowFlag = false;
    bool buffersNotFull = true;
    Ipp32s bitsPerFrame = 0;
    Ipp32s brcRecode = 0;
    Ipp32s payloadBits = 0;
    Ipp32s brc_data_size;
    H264EncoderFrame_8u16s tempRefFrame;
    Ipp32s picStartDataSize = 0;
    bool isRefForIVP = false;
    Ipp8u  nal_header_ext[3] = {0,};

    H264ENC_UNREFERENCED_PARAMETER(ePictureType);


#ifdef SLICE_CHECK_LIMIT
    Ipp32s numSliceMB;
#endif // SLICE_CHECK_LIMIT

    pDataSize[0] = pDst[0]->GetDataSize(); pDataSize[1] = pDst[1]->GetDataSize(); dst = pDst[0];
    //   fprintf(stderr,"frame=%d\n", core_enc->m_uFrames_Num);
    if (core_enc->m_Analyse & ANALYSE_RECODE_FRAME /*&& core_enc->m_pReconstructFrame == NULL //could be size change*/){ //make init reconstruct buffer
        core_enc->m_pReconstructFrame = core_enc->m_pReconstructFrame_allocated;
        core_enc->m_pReconstructFrame->m_bottom_field_flag[0] = core_enc->m_pCurrentFrame->m_bottom_field_flag[0];
        core_enc->m_pReconstructFrame->m_bottom_field_flag[1] = core_enc->m_pCurrentFrame->m_bottom_field_flag[1];
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
        dst = pDst[core_enc->m_field_index];
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
                //if (core_enc->m_uFrames_Num == 0) //temporarily disabled
                {
                    H264CoreEncoder_SetSequenceParameters_8u16s(state);
                    H264CoreEncoder_SetPictureParameters_8u16s(state);
                }
                // Toggle the idr_pic_id on and off so that adjacent IDRs will have different values
                // This is done here because it is done per frame and not per slice.
//                         core_enc->m_SliceHeader.idr_pic_id ^= 0x1;
                core_enc->m_SliceHeader.idr_pic_id++;
                core_enc->m_SliceHeader.idr_pic_id &= 0xff; //Restrict to 255 to reduce number of bits(max value 65535 in standard)
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
        if (!(core_enc->m_Analyse & ANALYSE_RECODE_FRAME))
            core_enc->m_pReconstructFrame = core_enc->m_pCurrentFrame;
        // reset bitstream object before begin compression

        core_enc->mvc_header.non_idr_flag = ePic_Class != IDR_PIC;
        core_enc->mvc_header.priority_id = 0;
        core_enc->mvc_header.view_id = core_enc->m_view_id;
        core_enc->mvc_header.temporal_id = 0;
        core_enc->mvc_header.anchor_pic_flag = ePic_Class == IDR_PIC;
        if(core_enc->mvc_header.anchor_pic_flag) {
            core_enc->mvc_header.inter_view_flag = m_views[m_oidxmap[core_enc->m_view_id]].m_is_ref4anchor;
        } else {
            core_enc->mvc_header.inter_view_flag = m_views[m_oidxmap[core_enc->m_view_id]].m_is_ref4nonanchor;
        }
        core_enc->mvc_header.reserved_one_bit = 1;

        isRefForIVP = (core_enc->mvc_header.anchor_pic_flag) ? m_cview->m_is_ref4anchor : m_cview->m_is_ref4nonanchor;

        Ipp32s i;
        for (i = 0; i < core_enc->m_info.num_slices*((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1); i++)
            H264BsReal_Reset_8u16s(core_enc->m_pbitstreams[i]);
#if defined ALPHA_BLENDING_H264
        if (!alpha)
#endif // ALPHA_BLENDING_H264
        {
            payloadBits = (Ipp32s)dst->GetDataSize();
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

            H264MVCNALHeader *mvc_header = &core_enc->mvc_header;
            nal_header_ext[0] = (mvc_header->non_idr_flag << 6) | mvc_header->priority_id;
            nal_header_ext[1] = (mvc_header->view_id >> 2);
            nal_header_ext[2] = ((mvc_header->view_id & 3) << 6) |
                (mvc_header->temporal_id << 3) |
                (mvc_header->anchor_pic_flag << 2) |
                (mvc_header->inter_view_flag << 1) |
                (mvc_header->reserved_one_bit);

            if (core_enc->m_info.m_is_base_view || core_enc->m_info.m_viewOutput) {
                status = H264CoreEncoder_encodeFrameHeader_8u16s(state, core_enc->m_bs1, dst, (ePic_Class == IDR_PIC), startPicture);
                if (status != UMC_OK) goto done;

                status = H264CoreEncoder_encodeSEI(state, core_enc->m_info.m_is_base_view, core_enc->m_bs1, dst, (ePic_Class == IDR_PIC), startPicture, ctrl, (Ipp8u)brcRecode);
                if (status != UMC_OK) goto done;

                // In non view output mode insert nesting SEI for dependent view
                if (!core_enc->m_info.m_viewOutput)
                    status = H264CoreEncoder_encodeSEI(state, false, core_enc->m_bs1, dst, (ePic_Class == IDR_PIC), startPicture, ctrl, (Ipp8u)brcRecode);
                if (status != UMC_OK) goto done;

                if (!core_enc->m_info.m_viewOutput) // Write prefix only if not in BD mode
                {
                    size_t oldsize = dst->GetDataSize();
                    dst->SetDataSize(oldsize + H264BsReal_EndOfNAL_8u16s(core_enc->m_bs1, (Ipp8u*)dst->GetDataPointer() + oldsize, (ePic_Class != DISPOSABLE_PIC),
                        NAL_UT_PREFIX, startPicture, nal_header_ext));
                }
                // Start writing slices from the beginning of main bitstream.
                // It is possible because call to EndOfNAL already copied all contents of main
                // encoder bitstream to application bitstream.
                H264BsReal_Reset_8u16s(core_enc->m_bs1);
            }
            payloadBits = (Ipp32s)((dst->GetDataSize() - payloadBits) << 3);
            picStartDataSize = (Ipp32s)dst->GetDataSize();
        }
        status = H264CoreEncoder_Start_Picture_8u16s(state, &ePic_Class, core_enc->m_PicType);
        if (status != UMC_OK)
            goto done;
        Ipp32s slice_qp_delta_default = core_enc->m_Slices[0].m_slice_qp_delta;
        // update PicNum for ref pics and specify mmco for Closed GOP
        H264CoreEncoder_UpdateRefPicListCommon_8u16s(state);
        // specify ref pic list reordering and cut
        H264CoreEncoder_ModifyRefPicList( core_enc, ctrl);
        // prepare all ref pic marking here
        H264CoreEncoder_PrepareRefPicMarking(core_enc, ctrl, ePic_Class, 0);
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
            // Signal scheruler that all busy threads should be unleashed
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
                    dst->SetDataSize(dst->GetDataSize() + H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(core_enc->m_Slices[slice].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize(), (ePic_Class != DISPOSABLE_PIC),
#if defined ALPHA_BLENDING_H264
                        (alpha) ? NAL_UT_MFXVideoENCODEMVCNOPART :
#endif // ALPHA_BLENDING_H264
                        ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE), startPicture), 0);
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
            Ipp64s ticks_total = 0;
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

        //Write slice to the stream in order, copy Slice RBSP to the end of the output buffer after adding start codes and SC emulation prevention.
#ifdef SLICE_CHECK_LIMIT
        if( !core_enc->m_MaxSliceSize ){
#endif // SLICE_CHECK_LIMIT
        for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++){
            size_t oldsize = dst->GetDataSize();
            size_t bufsize = dst->GetBufferSize();
            if(oldsize + (Ipp32s)H264BsBase_GetBsSize(core_enc->m_Slices[slice].m_pbitstream) + 5 /* possible extra bytes */ > bufsize) {
                bufferOverflowFlag = true;
            } else {
                NAL_Unit_Type uUnitType;
                if (core_enc->m_info.m_is_base_view) {
                    uUnitType = ((ePic_Class == IDR_PIC) ? NAL_UT_IDR_SLICE : NAL_UT_SLICE);
                } else {
                    uUnitType = NAL_UT_CODED_SLICE_EXTENSION;
                }
                //Write to output bitstream
                dst->SetDataSize(oldsize + H264BsReal_EndOfNAL_8u16s(core_enc->m_Slices[slice].m_pbitstream, (Ipp8u*)dst->GetDataPointer() + oldsize, (ePic_Class != DISPOSABLE_PIC),
#if defined ALPHA_BLENDING_H264
                    (alpha) ? NAL_UT_LAYERNOPART :
#endif // ALPHA_BLENDING_H264
                    uUnitType, startPicture, nal_header_ext));
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

#define FRAME_SIZE_RATIO_THRESH_MAX 16.0
#define FRAME_SIZE_RATIO_THRESH_MIN 8.0
#define TOTAL_SIZE_RATIO_THRESH 1.5
        if (!core_enc->brc) // tmp!!!
        if (core_enc->m_info.rate_controls.method == H264_RCM_VBR || core_enc->m_info.rate_controls.method == H264_RCM_CBR) {
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
            bitsPerFrame = (Ipp32s)((dst->GetDataSize() - brc_data_size) << 3);
            hrdSts = core_enc->brc->PostPackFrame(picType, bitsPerFrame, payloadBits, brcRecode);
            if (m_ConstQP)
            {
                core_enc->brc->SetQP(m_ConstQP, I_PICTURE);
                core_enc->brc->SetQP(m_ConstQP, P_PICTURE);
                core_enc->brc->SetQP(m_ConstQP, B_PICTURE);
                hrdSts = BRC_OK;
            }
            if (BRC_OK != hrdSts && (core_enc->m_Analyse & ANALYSE_RECODE_FRAME)) {
                if (!(hrdSts & BRC_NOT_ENOUGH_BUFFER)) {
                    dst->SetDataSize(brc_data_size);
                    brcRecode = UMC::BRC_RECODE_QP;
                    if (ePic_Class == IDR_PIC && (!core_enc->m_field_index)) {
                      core_enc->m_SliceHeader.idr_pic_id--;
                      core_enc->m_SliceHeader.idr_pic_id &= 0xff; //Restrict to 255 to reduce number of bits(max value 65535 in standard)
                    }
                    core_enc->m_field_index--;
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
                        core_enc->brc->PostPackFrame(picType, bitsize, payloadBits + bitsize - bitsPerFrame, 1);
                        dst->SetDataSize(brc_data_size + (bitsize >> 3));
                        brcRecode = 0;
//                           status = UMC::UMC_OK;
                    } else {
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
                        hrdSts = core_enc->brc->PostPackFrame(picType, bitsPerFrame, payloadBits, 2);
                        if (hrdSts == BRC_OK)
                        {
                            brcRecode = 0;
                            goto end_of_frame;
                        }
                        else
                        {
                            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                            goto done;
                        }
                    }
                }
            } else
              brcRecode = 0;
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
        core_enc->m_deblocking_IL = 0;
        core_enc->m_deblocking_stage2 = 0;
        for (slice = (Ipp32s)core_enc->m_info.num_slices*core_enc->m_field_index; slice < core_enc->m_info.num_slices*(core_enc->m_field_index+1); slice++){
            if (core_enc->m_Slices[slice].status != UMC_OK){
                // It is unreachable in the current implementation, so there is no problem!!!
                core_enc->m_bMakeNextFrameKey = true;
                VM_ASSERT(0);// goto done;
            }else {
                if((ePic_Class != DISPOSABLE_PIC || isRefForIVP) && !(core_enc->m_Analyse_ex & ANALYSE_DEBLOCKING_OFF)) {
                    H264CoreEncoder_DeblockSlice_8u16s(state, core_enc->m_Slices + slice, core_enc->m_Slices[slice].m_first_mb_in_slice + core_enc->m_WidthInMBs*core_enc->m_HeightInMBs*core_enc->m_field_index, core_enc->m_Slices[slice].m_MB_Counter);
                }
            }
        }
#endif
end_of_frame:
        if (ePic_Class != DISPOSABLE_PIC || isRefForIVP)
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
            core_enc->m_HeightInMBs <<= (Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); //Do we need it here?
            dst->SetDataSize(brc_data_size);
        }
brc_recode: ;
    } // end of field loop

    bitsPerFrame  = (Ipp32s)((pDst[0]->GetDataSize() - pDataSize[0]) << 3);
    bitsPerFrame += (Ipp32s)((pDst[1]->GetDataSize() - pDataSize[1]) << 3);
    core_enc->m_total_bits_encoded += bitsPerFrame;

#ifdef H264_DUMP_RECON
    {
        char fname[128];
        mfxU8 buf[2048];
        FILE *f;
        Ipp32s W = core_enc->m_WidthInMBs*16;
        Ipp32s H = core_enc->m_HeightInMBs*16;
        mfxU8* fbuf = 0;
        sprintf(fname, "rec%dx%d_l%d.yuv", W, H, core_enc->m_view_id);
        if (core_enc->m_PicType == BPREDPIC &&
            core_enc->m_pCurrentFrame->m_pRefPicList[0].m_RefPicListL1.m_RefPicList[0] &&
            core_enc->m_pCurrentFrame->m_pRefPicList[0].m_RefPicListL1.m_RefPicList[0]->m_PicOrderCnt[0] >
                core_enc->m_pCurrentFrame->m_PicOrderCnt[0] ) { // simple reorder for B: read last ref, replacing with B, write ref
            fbuf = new mfxU8[W*H*3/2];
            f = fopen(fname,"r+b");
            fseek(f, -W*H*3/2, SEEK_END);
            fread(fbuf, 1, W*H*3/2, f);
            //fclose(f);
            //f = fopen(fname,"r+b");
            fseek(f, -W*H*3/2, SEEK_END);
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
            fwrite(fbuf, 1, W*H*3/2, f);
            delete[] fbuf;
        }
        fclose(f);
    }
#endif // H264_DUMP_RECON
    if( core_enc->m_Analyse & ANALYSE_RECODE_FRAME ){
        H264EncoderFrame_exchangeFrameYUVPointers_8u16s(core_enc->m_pReconstructFrame, core_enc->m_pCurrentFrame);
        core_enc->m_pReconstructFrame = NULL;
    }
    if (dst->GetDataSize() == 0) {
        core_enc->m_bMakeNextFrameKey = true;
        status = UMC_ERR_FAILED;
        goto done;
    }
#ifdef H264_STAT
    core_enc->hstats.addFrame( ePictureType, dst->GetDataSize());
#endif
    core_enc->m_uFrames_Num++;
done:
#ifdef H264_HRD_CPB_MODEL
    core_enc->m_SEIData.m_recentFrameEncodedSize = bitsPerFrame;
#endif // H264_HRD_CPB_MODEL
    H264CoreEncoder_updateSEIData( core_enc );
    return status;
}

static inline Ipp32s MSBIT (Ipp32u arg)
{
    Ipp32s res;
    for( res = 0; arg; arg >>= 1, res++);
    return res;
}

mfxU32 MFXVideoENCODEMVC::GetSEISize_BufferingPeriod(
    const H264SeqParamSet& seq_parms,
    const Ipp8u NalHrdBpPresentFlag,
    const Ipp8u VclHrdBpPresentFlag)
{
    mfxU32 data_size, data_size_bits;

    Ipp8u seq_parameter_set_id, cpb_cnt_minus1_hrd, cpb_cnt_minus1_vcl, initial_cpb_removal_delay_length_minus1;

    seq_parameter_set_id = seq_parms.seq_parameter_set_id;

    if (seq_parms.vui_parameters_present_flag)
    {
        if (seq_parms.vui_parameters.nal_hrd_parameters_present_flag)
        {
            cpb_cnt_minus1_hrd = seq_parms.vui_parameters.hrd_params.cpb_cnt_minus1;
            cpb_cnt_minus1_vcl = 0;
            initial_cpb_removal_delay_length_minus1 = seq_parms.vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1;
        }
        else if (seq_parms.vui_parameters.vcl_hrd_parameters_present_flag)
        {
            cpb_cnt_minus1_hrd = 0;
            cpb_cnt_minus1_vcl = seq_parms.vui_parameters.vcl_hrd_params.cpb_cnt_minus1;
            initial_cpb_removal_delay_length_minus1 = seq_parms.vui_parameters.vcl_hrd_params.initial_cpb_removal_delay_length_minus1;
        }
        else
        {
            cpb_cnt_minus1_hrd = 0;
            cpb_cnt_minus1_vcl = 0;
            initial_cpb_removal_delay_length_minus1 = 23;
        }
    }
    else
    {
        cpb_cnt_minus1_hrd = 0;
        cpb_cnt_minus1_vcl = 0;
        initial_cpb_removal_delay_length_minus1 = 23;
    }

    data_size_bits = MSBIT(seq_parameter_set_id) * 2 + 1;
    data_size_bits += (NalHrdBpPresentFlag * (cpb_cnt_minus1_hrd + 1) + VclHrdBpPresentFlag * (cpb_cnt_minus1_vcl + 1)) * (initial_cpb_removal_delay_length_minus1 + 1) * 2;
    data_size = (data_size_bits + 7) >> 3;

    //payload type
    data_size += 1;
    //payload size
    while( data_size >= 255 ){
        data_size += 1;
        data_size -= 255;
    }
    data_size += 1;

    return data_size;
}

mfxU32 MFXVideoENCODEMVC::GetSEISize_PictureTiming(
    const H264SeqParamSet& seq_parms,
    const Ipp8u CpbDpbDelaysPresentFlag,
    const Ipp8u pic_struct_present_flag,
    const H264SEIData_PictureTiming& timing_data)
{
    Ipp32s i, NumClockTS=0;
    mfxU32 data_size, data_size_bits = 0;

    Ipp8u cpb_removal_delay_length_minus1, dpb_output_delay_length_minus1, time_offset_length;

    if (seq_parms.vui_parameters_present_flag)
    {
        if (seq_parms.vui_parameters.nal_hrd_parameters_present_flag)
        {
            cpb_removal_delay_length_minus1 =  seq_parms.vui_parameters.hrd_params.cpb_removal_delay_length_minus1;
            dpb_output_delay_length_minus1 =  seq_parms.vui_parameters.hrd_params.dpb_output_delay_length_minus1;
            time_offset_length = seq_parms.vui_parameters.hrd_params.time_offset_length;
        }
        else if (seq_parms.vui_parameters.vcl_hrd_parameters_present_flag)
        {
            cpb_removal_delay_length_minus1 =  seq_parms.vui_parameters.vcl_hrd_params.cpb_removal_delay_length_minus1;
            dpb_output_delay_length_minus1 =  seq_parms.vui_parameters.vcl_hrd_params.dpb_output_delay_length_minus1;
            time_offset_length = seq_parms.vui_parameters.vcl_hrd_params.time_offset_length;
        }
        else
        {
            cpb_removal_delay_length_minus1 =  23;
            dpb_output_delay_length_minus1 =  23;
            time_offset_length = 24;
        }
    }
    else
    {
        cpb_removal_delay_length_minus1 =  23;
        dpb_output_delay_length_minus1 =  23;
        time_offset_length = 24;
    }

    if (CpbDpbDelaysPresentFlag)
    {
        data_size_bits += cpb_removal_delay_length_minus1 + dpb_output_delay_length_minus1 + 2;
    }

    if (pic_struct_present_flag)
    {
        if (timing_data.pic_struct > 8)
            return 0;
        NumClockTS = (timing_data.pic_struct <= 2) ? 1 : (timing_data.pic_struct <= 4 || timing_data.pic_struct == 7) ? 2 : 3;
        data_size_bits += (4 + NumClockTS); // pic_struct and clock_timestamp_flags

        for (i = 0; i < NumClockTS; i ++)
        {
            Ipp32u timestamp_data_size_bits = timing_data.full_timestamp_flag[i] ? 17 : (1 +
                timing_data.seconds_flag[i] * (7 + timing_data.minutes_flag[i]*(7 + timing_data.hours_flag[i]*5)));
            data_size_bits += timing_data.clock_timestamp_flag[i] * (19 +
            timestamp_data_size_bits + time_offset_length);
        }

    }

    data_size = (data_size_bits + 7) >> 3;

    //payload type
    data_size += 1;
    //payload size
    while( data_size >= 255 ){
        data_size += 1;
        data_size -= 255;
    }
    data_size += 1;
    return data_size;
}

Status MFXVideoENCODEMVC::PutSEI_MVCScalableNesting(H264BsReal_8u16s* bs, mfxU32 size)
{
    //payload type
    H264BsReal_PutBits_8u16s(bs, SEI_TYPE_MVC_SCALABLE_NESTING, 8);

    mfxU32 data_size, data_size_bits = size * 8;
    data_size_bits += 1; // operation_point_flag
    data_size_bits += MSBIT(0) * 2 + 1; // num_view_components_op_minus1
    data_size_bits += 10; // sei_op_view_id[ 0 ]
    data_size_bits += 3; // sei_op_temporal_id
    data_size = (data_size_bits + 7) >> 3;

    //payload size
    while (data_size >= 255)
    {
        H264BsReal_PutBits_8u16s(bs, 0xff, 8);
        data_size -= 255;
    }
    H264BsReal_PutBits_8u16s(bs, data_size, 8);

    H264BsReal_PutBits_8u16s(bs, 1, 1); // operation_point_flag 
    H264BsReal_PutVLCCode_8u16s(bs, 0); // num_view_components_op_minus1
    H264BsReal_PutBits_8u16s(bs, m_cview->m_dependency.ViewId, 10); // sei_op_view_id[ 0 ]
    H264BsReal_PutBits_8u16s(bs, m_mvcdesc.OP->TemporalId, 3); // sei_op_temporal_id

    while (H264BsBase_GetBsOffset(&(bs->m_base)) & 0x07)
        H264BsReal_PutBits_8u16s(bs, 0, 1);

    return UMC_OK;
}

Status MFXVideoENCODEMVC::H264CoreEncoder_encodeSEI(
    void* state,
    bool is_base_view_sei,
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

    need_insert[SEI_TYPE_RECOVERY_POINT] = 0;
    need_insert[SEI_TYPE_PAN_SCAN_RECT] = 0;
    need_insert[SEI_TYPE_FILLER_PAYLOAD] = 0;
    need_insert[SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35] = 0;

    if (core_enc->m_SeqParamSet->vui_parameters_present_flag)
    {
        NalHrdBpPresentFlag = core_enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag;
        VclHrdBpPresentFlag = core_enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag;
        pic_struct_present_flag = core_enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag;
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

    need_insert[SEI_TYPE_BUFFERING_PERIOD] = (need_insert[SEI_TYPE_BUFFERING_PERIOD] || bIDR_Pic);
    need_insert[SEI_TYPE_BUFFERING_PERIOD] = (need_insert[SEI_TYPE_BUFFERING_PERIOD] && (NalHrdBpPresentFlag || VclHrdBpPresentFlag));

    if (need_insert[SEI_TYPE_BUFFERING_PERIOD])
    {
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
                core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0] = initial_cpb_removal_delay;
            } else
                ps = UMC_ERR_NOT_ENOUGH_DATA;
#endif // H264_HRD_CPB_MODEL

            if (brcParams.HRDBufferSizeBytes != 0 &&  brcParams.maxBitrate != 0 )
                core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay_offset[0] = (Ipp32u)((Ipp64f)(brcParams.HRDBufferSizeBytes << 3) / brcParams.maxBitrate * 90000 - core_enc->m_SEIData.BufferingPeriod.nal_initial_cpb_removal_delay[0]);
            else
                ps = UMC_ERR_NOT_ENOUGH_DATA;
        }
        else
            ps = UMC_ERR_NOT_ENOUGH_DATA;

        if (ps != UMC_OK)
            return ps;

        // Insert mvc_scalable_nesting header before BP SEI message
        if (!is_base_view_sei)
        {
            mfxU32 size = GetSEISize_BufferingPeriod(
                *core_enc->m_SeqParamSet,
                NalHrdBpPresentFlag,
                VclHrdBpPresentFlag);

            if (0 == size)
                return UMC_ERR_FAILED;

            if (core_enc->m_info.m_viewOutput)
            {
                // get HRD values from base view
                core_enc->m_SEIData.BufferingPeriod = m_views[0].enc->m_SEIData.BufferingPeriod;
            }

            ps = PutSEI_MVCScalableNesting(bs, size);

            if (ps != UMC_OK)
                return ps;
        }

        ps = H264BsReal_PutSEI_BufferingPeriod_8u16s(
            bs,
            *core_enc->m_SeqParamSet,
            NalHrdBpPresentFlag,
            VclHrdBpPresentFlag,
            core_enc->m_SEIData.BufferingPeriod);

        if (ps != UMC_OK)
            return ps;

        // Finish NAL unit because for MVC scalable nesting SEI each of them has to be in its own NAL
        if (!is_base_view_sei)
        {
            H264BsBase_WriteTrailingBits(&bs->m_base);
            size_t oldsize = dst->GetDataSize();

            dst->SetDataSize( oldsize +
                H264BsReal_EndOfNAL_8u16s( bs, (Ipp8u*)dst->GetDataPointer() + oldsize, 0, NAL_UT_SEI, startPicture, 0));
        }
        else
            sei_inserted = 1;
    }

    // insert GOP_structure_map()
    // ignore GOP_structure_map() if it is effective for non I picture or for 2nd field
    if(is_base_view_sei && GOP_structure_map >= 0 && core_enc->m_pCurrentFrame->m_PicCodType == INTRAPIC && !core_enc->m_field_index) {
        mfxPayload* pl = ctrl->Payload[GOP_structure_map];
        mfxU32 data_bytes = pl->NumBit>>3;
        //Copy data
        for( mfxU32 k=0; k<data_bytes; k++ )
            H264BsReal_PutBits_8u16s(bs, pl->Data[k], 8);
        sei_inserted = 1;
    }

    // insert cc_data()
    if(is_base_view_sei && Closed_caption >= 0) {
        mfxPayload* pl = ctrl->Payload[Closed_caption];
        mfxU32 data_bytes = pl->NumBit>>3;
        //Copy data
        for( mfxU32 k=0; k<data_bytes; k++ )
            H264BsReal_PutBits_8u16s(bs, pl->Data[k], 8);
        sei_inserted = 1;
    }

    need_insert[SEI_TYPE_PIC_TIMING] = (CpbDpbDelaysPresentFlag || pic_struct_present_flag);
    if (!need_insert[SEI_TYPE_BUFFERING_PERIOD] && bIDR_Pic)
        need_insert[SEI_TYPE_PIC_TIMING] = 0;

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

        if (is_cur_pic_field && core_enc->m_field_index)
        {
            if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
            {
                core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated += (m_cview->m_frameCount ? core_enc->m_SEIData.PictureTiming.cpb_removal_delay : 0);
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
            core_enc->m_SEIData.PictureTiming.initial_dpb_output_delay = (core_enc->m_info.B_frame_rate == 0) ? 0 : ((core_enc->m_info.B_frame_rate > 1) && core_enc->m_info.treat_B_as_reference && (core_enc->m_info.num_ref_frames > 2)) ? 4 : 2;

        core_enc->m_SEIData.PictureTiming.dpb_output_delay = core_enc->m_SEIData.PictureTiming.initial_dpb_output_delay +
        /*core_enc->m_PicOrderCnt_Accu*2 +*/ core_enc->m_pCurrentFrame->m_PicOrderCnt[core_enc->m_field_index] -
        (core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated + ((m_cview->m_frameCount || (is_cur_pic_field && core_enc->m_field_index)) ? core_enc->m_SEIData.PictureTiming.cpb_removal_delay : 0));

        if (core_enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag)
            core_enc->m_SEIData.PictureTiming.dpb_output_delay += core_enc->m_pCurrentFrame->m_DeltaTfi;

        if( core_enc->m_SliceHeader.field_pic_flag ){
            if( core_enc->m_SliceHeader.bottom_field_flag ){
                core_enc->m_SEIData.PictureTiming.pic_struct = 2; // bottom field
            }else{
                core_enc->m_SEIData.PictureTiming.pic_struct = 1; // top field
            }
        }else
            switch( core_enc->m_pCurrentFrame->m_coding_type ){
            case MFX_PICSTRUCT_PROGRESSIVE:
                core_enc->m_SEIData.PictureTiming.pic_struct = 0;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF: // progressive frame, displayed as field TFF
            case MFX_PICSTRUCT_FIELD_TFF: // interlaced frame (coded as MBAFF), displayed as field TFF
                core_enc->m_SEIData.PictureTiming.pic_struct = 3;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF: // progressive frame, displayed as field BFF
            case MFX_PICSTRUCT_FIELD_BFF: // interlaced frame (coded as MBAFF), displayed as field BFF
                core_enc->m_SEIData.PictureTiming.pic_struct = 4;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED:
                core_enc->m_SEIData.PictureTiming.pic_struct = 5;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_REPEATED:
                core_enc->m_SEIData.PictureTiming.pic_struct = 6;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_DOUBLING:
                core_enc->m_SEIData.PictureTiming.pic_struct = 7;
                break;
            case MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_TRIPLING:
                core_enc->m_SEIData.PictureTiming.pic_struct = 8;
                break;
            default:
                core_enc->m_SEIData.PictureTiming.pic_struct = 0;
                break;
        }

        core_enc->m_SEIData.PictureTiming.clock_timestamp_flag[0] = 0;

        // Insert mvc_scalable_nesting header before BP SEI message
        if (!is_base_view_sei)
        {
            mfxU32 size = GetSEISize_PictureTiming(
                *core_enc->m_SeqParamSet,
                CpbDpbDelaysPresentFlag,
                pic_struct_present_flag,
                core_enc->m_SEIData.PictureTiming);

            if (0 == size)
                return UMC_ERR_FAILED;

            ps = PutSEI_MVCScalableNesting(bs, size);

            if (ps != UMC_OK)
                return ps;
        }

        ps = H264BsReal_PutSEI_PictureTiming_8u16s(
            bs,
            *core_enc->m_SeqParamSet,
            CpbDpbDelaysPresentFlag,
            pic_struct_present_flag,
            core_enc->m_SEIData.PictureTiming);

        if (ps != UMC_OK)
            return ps;

        if (is_cur_pic_field && core_enc->m_field_index)
        {
            if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
                core_enc->m_SEIData.PictureTiming.cpb_removal_delay = 0; // set to 0 for further incrementing by 2
            else
                core_enc->m_SEIData.PictureTiming.cpb_removal_delay --; // reset cpb_removal_delay for re-encode loop
        }

        // Finish NAL unit because for MVC scalable nesting SEI each of them has to be in its own NAL
        if (!is_base_view_sei)
        {
            H264BsBase_WriteTrailingBits(&bs->m_base);
            size_t oldsize = dst->GetDataSize();

            dst->SetDataSize( oldsize +
                H264BsReal_EndOfNAL_8u16s( bs, (Ipp8u*)dst->GetDataPointer() + oldsize, 0, NAL_UT_SEI, startPicture, 0));
        }
        else
            sei_inserted = 1;
    }

    // write external payloads to bit stream
    if(is_base_view_sei && need_insert_external_payload){
        for( mfxI32 i = core_enc->m_field_index; i<ctrl->NumPayload; i+=is_cur_pic_field ? 2:1 ){ // odd payloads for the 1st field, even for the 2nd
            if (i != GOP_structure_map && i != Closed_caption) {
                mfxPayload* pl = ctrl->Payload[i];
                mfxU32 data_bytes = pl->NumBit>>3;
                //Copy data
                for( mfxU32 k=0; k<data_bytes; k++ )
                    H264BsReal_PutBits_8u16s(bs, pl->Data[k], 8);
            }
        }
        sei_inserted = 1;
    }

    if (is_base_view_sei && need_insert[SEI_TYPE_PAN_SCAN_RECT])
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
    }

    if (is_base_view_sei && need_insert[SEI_TYPE_FILLER_PAYLOAD])
    {
        ps = H264BsReal_PutSEI_FillerPayload_8u16s(bs,37);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
    }

    if (is_base_view_sei && need_insert[SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35])
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
    }

    if (is_base_view_sei && need_insert[SEI_TYPE_RECOVERY_POINT])
    {
        ps = H264BsReal_PutSEI_RecoveryPoint_8u16s(bs,4,1,1,2);

        if (ps != UMC_OK)
            return ps;

        sei_inserted = 1;
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
    }

#endif // no copyright and preset

    if (sei_inserted)
    {
        H264BsBase_WriteTrailingBits(&bs->m_base);
        size_t oldsize = dst->GetDataSize();

        dst->SetDataSize( oldsize +
            H264BsReal_EndOfNAL_8u16s( bs, (Ipp8u*)dst->GetDataPointer() + oldsize, 0, NAL_UT_SEI, startPicture, 0));
    }

    MFX_INTERNAL_CPY(core_enc->m_SEIData.m_insertedSEI, need_insert, 36 * sizeof(Ipp8u));


    return ps;
}

Status MFXVideoENCODEMVC::H264CoreEncoder_updateSEIData( void* state )
{
    H264CoreEncoder_8u16s* core_enc = (H264CoreEncoder_8u16s *)state;
    Status ps = UMC_OK;

    if (core_enc->m_SEIData.m_insertedSEI[SEI_TYPE_BUFFERING_PERIOD])
    {
        core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated += (m_cview->m_frameCount ? core_enc->m_SEIData.PictureTiming.cpb_removal_delay : 0);
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
/*
mfxMVCView::mfxMVCView(mfxStatus *stat)):
  m_frameCountSync(0),
  m_totalBits(0),
  m_encodedFrames(0),
  m_Initialized(false),
  m_putEOSeq(false),
  m_putEOStream(false),
  m_putTimingSEI(false),
  m_resetRefList(false),
  m_useAuxInput(false)
{
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
    H264ENC_CALL_NEW(res, H264CoreEncoder, enc);
#undef H264ENC_MAKE_NAME
    if (res != UMC_OK) *stat = MFX_ERR_NOT_INITIALIZED;
}
mfxMVCView::~mfxMVCView(){
}
*/

#endif // MFX_ENABLE_H264_VIDEO_ENCODE
#endif // MFX_ENABLE_MVC_VIDEO_ENCODE
/* EOF */
