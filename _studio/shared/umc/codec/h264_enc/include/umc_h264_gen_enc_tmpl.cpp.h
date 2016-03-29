//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004 - 2016 Intel Corporation. All Rights Reserved.
//

#include <vm_thread.h>

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define H264CoreEncoderType H264ENC_MAKE_NAME(H264CoreEncoder)
#define H264SliceType H264ENC_MAKE_NAME(H264Slice)
#define H264BsFakeType H264ENC_MAKE_NAME(H264BsFake)
#define H264BsRealType H264ENC_MAKE_NAME(H264BsReal)
#define EncoderRefPicListType H264ENC_MAKE_NAME(EncoderRefPicList)
#define H264EncoderFrameType H264ENC_MAKE_NAME(H264EncoderFrame)

//
// Constructor for the EncoderH264 class.
//
Status H264ENC_MAKE_NAME(H264Slice_Create)(
    void* state)
{
    H264SliceType* slice = (H264SliceType *)state;
    memset(slice, 0, sizeof(H264SliceType));
    return UMC_OK;
}


Status H264ENC_MAKE_NAME(H264Slice_Init)(
    void* state,
    H264EncoderParams &info)
{
    H264SliceType* slice_enc = (H264SliceType *)state;
    Ipp32s status;
    Ipp32s allocSize = 256*sizeof(PIXTYPE) // pred for direct
                     + 256*sizeof(PIXTYPE) // pred for direct 8x8
                     + 256*sizeof(PIXTYPE) // temp working for direct
                     + 256*sizeof(PIXTYPE) // pred for BiPred
                     + 256*sizeof(PIXTYPE) // temp buf for BiPred
                     + 256*sizeof(PIXTYPE) // temp buf for ChromaPred
                     + 256*sizeof(PIXTYPE) // MC
                     + 256*sizeof(PIXTYPE) // ME
                     + 8*512*sizeof(PIXTYPE) // MB prediction & reconstruct
                     + 8*256*sizeof(COEFFSTYPE) //MB transform result
                     + 64*sizeof(Ipp16s)     // Diff
                     + 64*sizeof(COEFFSTYPE) // TransformResults
                     + 64*sizeof(COEFFSTYPE) // QuantResult
                     + 64*sizeof(COEFFSTYPE) // DequantResult
                     + 16*sizeof(COEFFSTYPE) // luma dc
                     + 256*sizeof(Ipp16s)    // MassDiff
                     +(256+128)*sizeof(Ipp16s) // residual prediction difference (current-residual)
                     +(256+128)*sizeof(Ipp16s) // TCoeffsRef
//                     +(256+128)*sizeof(Ipp16s) // SCoeffsRef
//                     +(256+128)*sizeof(Ipp16s) // TCoeffsTmp
                     + 512 + ALIGN_VALUE
                     + 3 * (16 * 16 * 3 + 100);  //Bitstreams
    slice_enc->m_pAllocatedMBEncodeBuffer = (Ipp8u*)H264_Malloc(allocSize);
    if (!slice_enc->m_pAllocatedMBEncodeBuffer)
        return(UMC::UMC_ERR_ALLOC);

    // 16-byte align buffer start
    slice_enc->m_pPred4DirectB = (PIXTYPE*)align_pointer<Ipp8u*>(slice_enc->m_pAllocatedMBEncodeBuffer, ALIGN_VALUE);
    slice_enc->m_pPred4DirectB8x8 = slice_enc->m_pPred4DirectB + 256;
    slice_enc->m_pTempBuff4DirectB = slice_enc->m_pPred4DirectB8x8 + 256;
    slice_enc->m_pPred4BiPred = slice_enc->m_pTempBuff4DirectB + 256;
    slice_enc->m_pTempBuff4BiPred = slice_enc->m_pPred4BiPred + 256;
    slice_enc->m_pTempChromaPred = slice_enc->m_pTempBuff4BiPred + 256;

    slice_enc->m_cur_mb.mb4x4.prediction = slice_enc->m_pTempChromaPred + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mb4x4.reconstruct = slice_enc->m_cur_mb.mb4x4.prediction + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mb4x4.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mb4x4.reconstruct + 256); // 256 for pred_intra and 256 for reconstructed blocks

    slice_enc->m_cur_mb.mb8x8.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mb4x4.transform + 256); // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mb8x8.reconstruct = slice_enc->m_cur_mb.mb8x8.prediction + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mb8x8.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mb8x8.reconstruct + 256); // 256 for pred_intra and 256 for reconstructed blocks

    slice_enc->m_cur_mb.mb16x16.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mb8x8.transform + 256); // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mb16x16.reconstruct = slice_enc->m_cur_mb.mb16x16.prediction + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mb16x16.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mb16x16.reconstruct + 256); // 256 for pred_intra and 256 for reconstructed blocks

    slice_enc->m_cur_mb.mbInter.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mb16x16.transform + 256); // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mbInter.reconstruct = slice_enc->m_cur_mb.mbInter.prediction + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mbInter.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mbInter.reconstruct + 256); // 256 for pred_intra and 256 for reconstructed blocks

    slice_enc->m_cur_mb.mbChromaBaseMode.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mbInter.transform + 256);
    slice_enc->m_cur_mb.mbChromaBaseMode.reconstruct = slice_enc->m_cur_mb.mbChromaBaseMode.prediction + 256;
    slice_enc->m_cur_mb.mbChromaBaseMode.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mbChromaBaseMode.reconstruct + 256);

    slice_enc->m_cur_mb.mbBaseMode.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mbChromaBaseMode.transform + 256);
    slice_enc->m_cur_mb.mbBaseMode.reconstruct = slice_enc->m_cur_mb.mbBaseMode.prediction + 256;
    slice_enc->m_cur_mb.mbBaseMode.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mbBaseMode.reconstruct + 256);

    slice_enc->m_cur_mb.mbChromaIntra.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mbBaseMode.transform + 256); // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mbChromaIntra.reconstruct = slice_enc->m_cur_mb.mbChromaIntra.prediction + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mbChromaIntra.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mbChromaIntra.reconstruct + 256); // 256 for pred_intra and 256 for reconstructed blocks

    slice_enc->m_cur_mb.mbChromaInter.prediction = (PIXTYPE*)(slice_enc->m_cur_mb.mbChromaIntra.transform + 256); // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mbChromaInter.reconstruct = slice_enc->m_cur_mb.mbChromaInter.prediction + 256; // 256 for pred_intra and 256 for reconstructed blocks
    slice_enc->m_cur_mb.mbChromaInter.transform = (COEFFSTYPE*)(slice_enc->m_cur_mb.mbChromaInter.reconstruct + 256); // 256 for pred_intra and 256 for reconstructed blocks

    slice_enc->m_cur_mb.m_pResPredDiff = (Ipp16s*)(slice_enc->m_cur_mb.mbChromaInter.transform + 256);  // residual prediction difference (current-residual)
    slice_enc->m_cur_mb.m_pResPredDiffC = (Ipp16s*)(slice_enc->m_cur_mb.m_pResPredDiff + 256); // -*- chroma
    slice_enc->m_cur_mb.m_pTCoeffsRef = (Ipp16s*)(slice_enc->m_cur_mb.m_pResPredDiffC + 128);  // residual prediction difference (current-residual)
//    slice_enc->m_cur_mb.m_pSCoeffsRef = (Ipp16s*)(slice_enc->m_cur_mb.m_pTCoeffsRef + 384);  // residual prediction difference (current-residual)
//    slice_enc->m_cur_mb.m_pTCoeffsTmp = (Ipp16s*)(slice_enc->m_cur_mb.m_pTCoeffsRef + 384);  // residual prediction difference (current-residual)
    slice_enc->m_pMBEncodeBuffer = (PIXTYPE*)(slice_enc->m_cur_mb.m_pTCoeffsRef + 384); // support 420 only

    //Init bitstreams
    if (slice_enc->fakeBitstream == NULL)
    {
        slice_enc->fakeBitstream = (H264BsFakeType *)H264_Malloc(sizeof(H264BsFakeType));
        if (!slice_enc->fakeBitstream)
            return UMC_ERR_ALLOC;
    }

    H264ENC_MAKE_NAME(H264BsFake_Create)(slice_enc->fakeBitstream, 0, 0, info.chroma_format_idc, status);

    Ipp32s i;
    for (i = 0; i < 9; i++)
    {
        if (slice_enc->fBitstreams[i] == NULL)
        {
            slice_enc->fBitstreams[i] = (H264BsFakeType *)H264_Malloc(sizeof(H264BsFakeType));
            if (!slice_enc->fBitstreams[i])
                return UMC_ERR_ALLOC;
        }

        H264ENC_MAKE_NAME(H264BsFake_Create)(slice_enc->fBitstreams[i], 0, 0, info.chroma_format_idc, status);
    }

    return(UMC_OK);
}

void H264ENC_MAKE_NAME(H264Slice_Destroy)(
    void* state)
{
    H264SliceType* slice_enc = (H264SliceType *)state;
    if(slice_enc->m_pAllocatedMBEncodeBuffer != NULL) {
        H264_Free(slice_enc->m_pAllocatedMBEncodeBuffer);
        slice_enc->m_pAllocatedMBEncodeBuffer = NULL;
    }
    slice_enc->m_pPred4DirectB = NULL;
    slice_enc->m_pPred4BiPred = NULL;
    slice_enc->m_pTempBuff4DirectB = NULL;
    slice_enc->m_pTempBuff4BiPred = NULL;
    slice_enc->m_pMBEncodeBuffer = NULL;
    slice_enc->m_cur_mb.mb4x4.prediction = NULL;
    slice_enc->m_cur_mb.mb4x4.reconstruct = NULL;
    slice_enc->m_cur_mb.mb4x4.transform = NULL;
    slice_enc->m_cur_mb.mb8x8.prediction = NULL;
    slice_enc->m_cur_mb.mb8x8.reconstruct = NULL;
    slice_enc->m_cur_mb.mb8x8.transform = NULL;

    if (slice_enc->fakeBitstream != NULL){
        H264_Free(slice_enc->fakeBitstream);
        slice_enc->fakeBitstream = NULL;
    }

    Ipp32s i;
    for (i = 0; i < 9; i++)
    {
        if(slice_enc->fBitstreams[i] != NULL)
        {
            H264_Free(slice_enc->fBitstreams[i]);
            slice_enc->fBitstreams[i] = NULL;
        }
    }
}

Status H264ENC_MAKE_NAME(H264CoreEncoder_Create)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264EncoderParams_Create(&core_enc->m_info);

    core_enc->m_pBitStream = NULL;
    core_enc->memAlloc = NULL;
    core_enc->profile_frequency = 1;
    core_enc->m_iProfileIndex = 0;
    core_enc->m_is_mb_data_initialized = false;
    core_enc->m_pAllocEncoderInst = NULL;
    core_enc->m_pbitstreams = NULL;
    core_enc->m_FieldStruct = 0;
    core_enc->m_Analyse = 0;
    core_enc->m_Analyse_ex = 0;

    core_enc->m_bs1 = NULL;
    H264ENC_MAKE_NAME(H264EncoderFrameList_Create)(&core_enc->m_dpb, NULL);
    H264ENC_MAKE_NAME(H264EncoderFrameList_Create)(&core_enc->m_cpb, NULL);

    memset(core_enc->m_LTFrameIdxUseFlags, 0, 16);

    core_enc->m_NumShortTermRefFrames = 0;
    core_enc->m_NumLongTermRefFrames = 0;

    core_enc->m_pNextFrame = 0;
    core_enc->m_uIntraFrameInterval = 0;
    core_enc->m_uIDRFrameInterval = 0;
////    core_enc->m_PicOrderCnt = 0;
    core_enc->m_PicOrderCnt_LastIDR = 0;
    core_enc->m_pParsedDataNew = 0;
    core_enc->m_pMbsData = 0;
    core_enc->m_pReconstructFrame = NULL;
    core_enc->m_pReconstructFrame_allocated = NULL;
    core_enc->m_l1_cnt_to_start_B = 0;
    core_enc->m_pMBOffsets = NULL;
    core_enc->m_EmptyThreshold = NULL;
    core_enc->m_DirectBSkipMEThres = NULL;
    core_enc->m_PSkipMEThres = NULL;
    core_enc->m_BestOf5EarlyExitThres = NULL;
    core_enc->use_implicit_weighted_bipred = false;
    core_enc->m_is_cur_pic_afrm = false;
    core_enc->m_Slices = NULL;
    core_enc->m_MbDataPicStruct = 0;
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    core_enc->m_Slices_MBT = NULL;
    core_enc->mbReady_MBT = NULL;
    core_enc->lSR_MBT = NULL;
    core_enc->gd_MBT = NULL;
    vm_mutex_set_invalid(&core_enc->mutexIncRow);
#ifdef H264_RECODE_PCM
    core_enc->m_nPCM = 0;
    core_enc->m_mbPCM = NULL;
#endif //H264_RECODE_PCM
#ifdef MB_THREADING_VM
    core_enc->m_ThreadDataVM = NULL;
    core_enc->m_ThreadVM_MBT = NULL;
#endif // MB_THREADING_VM
#ifdef MB_THREADING_TW
#ifdef MB_THREADING_VM
    core_enc->mMutexMT = NULL;
#else // MB_THREADING_VM
#ifdef _OPENMP
    core_enc->mLockMT = NULL;
#endif // _OPENMP
#endif // MB_THREADING_VM
#endif // MB_THREADING_TW
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

#ifdef H264_INTRA_REFRESH
    core_enc->m_refrType = 0;
    core_enc->m_refrSpeed = 1;
    core_enc->m_stripeWidth = 1;
    core_enc->m_QPDelta = 0;
    core_enc->m_firstLineInStripe = -1;
    core_enc->m_periodCount = 0;
    core_enc->m_stopRefreshFrames = 0;
    core_enc->m_stopFramesCount = 0;
#endif // H264_INTRA_REFRESH
    core_enc->m_total_bits_encoded = 0;

    H264_AVBR_Create(&core_enc->avbr);
    core_enc->m_PaddedSize.width = 0;
    core_enc->m_PaddedSize.height = 0;
    core_enc->brc = 0;

    // Initialize the BRCState local variables based on the default
    // settings in core_enc->m_info.

    // If these assertions fail, then uTargetFrmSize needs to be set to
    // something other than 0.
    // Initialize the sequence parameter set structure.

    // Move default initialization to Init
    core_enc->m_SeqParamSet = NULL;
    core_enc->m_PicParamSet = NULL;
    core_enc->m_is_sps_alloc = false;
    core_enc->m_is_pps_alloc = false;

    // Initialize the slice header structure.
    memset(&core_enc->m_SliceHeader, 0, sizeof(H264SliceHeader));
    core_enc->m_SliceHeader.direct_spatial_mv_pred_flag = 1;

    // Initialize the SEI data structure
    memset(&core_enc->m_SEIData, 0, sizeof(H264SEIData));

    // Clear the SVC data structure
    memset(&core_enc->m_svc_layer, 0, sizeof(core_enc->m_svc_layer));
    core_enc->m_constrained_intra_pred_flag = 0;
    core_enc->QualityNum = 1;
    core_enc->TempIdMax = 0;
    core_enc->rateDivider = 1;

    core_enc->m_DirectTypeStat[0] = 0;
    core_enc->m_DirectTypeStat[1] = 0;

#ifdef SLICE_THREADING_LOAD_BALANCING
    // Load balancing for slice level multithreading
    core_enc->slice_load_balancing_enabled = 0;
    core_enc->m_B_ticks_per_macroblock = 0; // Array of timings for every P macroblock of the previous frame
    core_enc->m_P_ticks_per_macroblock = 0; // Array of timings for every B macroblock of the previous frame
    core_enc->m_B_ticks_per_frame = 0; // Total time used to encode all macroblocks in the frame
    core_enc->m_P_ticks_per_frame = 0; // Total time used to encode all macroblocks in the frame
    core_enc->m_B_ticks_data_available = 0;
    core_enc->m_P_ticks_data_available = 0;
#endif // SLICE_THREADING_LOAD_BALANCING

    core_enc->eFrameSeq = (H264EncoderFrameType **)H264_Malloc(1 * sizeof(H264EncoderFrameType*));
    if (!core_enc->eFrameSeq)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_Destroy)(state);
        return UMC_ERR_ALLOC;
    }

    core_enc->eFrameType = (EnumPicCodType *)H264_Malloc(1 * sizeof(EnumPicCodType));;
    if (!core_enc->eFrameType)
    {
        H264ENC_MAKE_NAME(H264CoreEncoder_Destroy)(state);
        return UMC_ERR_ALLOC;
    }

    core_enc->eFrameType[0] = PREDPIC;
    return UMC_OK;
}


/**********************************************************************
 *  EncoderH264 destructor.
 **********************************************************************/
void H264ENC_MAKE_NAME(H264CoreEncoder_Destroy)(
    void* state)
{
    if (state) {
        H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
        // release the noise reduction prefilter
        H264ENC_MAKE_NAME(H264CoreEncoder_Close)(state);

        if (NULL == core_enc->brc)
            H264_AVBR_Destroy(&core_enc->avbr);
        H264EncoderParams_Destroy(&core_enc->m_info);
        H264ENC_MAKE_NAME(H264EncoderFrameList_Destroy)(&core_enc->m_dpb);
        H264ENC_MAKE_NAME(H264EncoderFrameList_Destroy)(&core_enc->m_cpb);
        if(core_enc->m_is_sps_alloc) delete core_enc->m_SeqParamSet;
        if(core_enc->m_is_pps_alloc) delete core_enc->m_PicParamSet;
    }
}

//
// move all frames in WaitingForRef to ReadyToEncode
//
Status H264ENC_MAKE_NAME(H264CoreEncoder_MoveFromCPBToDPB)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
 //   EnumPicCodType  ePictureType;
    H264ENC_MAKE_NAME(H264EncoderFrameList_RemoveFrame)(&core_enc->m_cpb, core_enc->m_pCurrentFrame);
    H264ENC_MAKE_NAME(H264EncoderFrameList_insertAtCurrent)(&core_enc->m_dpb, core_enc->m_pCurrentFrame);
    return UMC_OK;
}

Status H264ENC_MAKE_NAME(H264CoreEncoder_CleanDPB)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264EncoderFrameType *pFrm = H264ENC_MAKE_NAME(H264EncoderFrameList_findNextDisposable)(&core_enc->m_dpb);
    //   EnumPicCodType  ePictureType;
    Status      ps = UMC_OK;
    while (pFrm != NULL)
    {
        H264ENC_MAKE_NAME(H264EncoderFrameList_RemoveFrame)(&core_enc->m_dpb, pFrm);
        H264ENC_MAKE_NAME(H264EncoderFrameList_insertAtCurrent)(&core_enc->m_cpb, pFrm);
        pFrm = H264ENC_MAKE_NAME(H264EncoderFrameList_findNextDisposable)(&core_enc->m_dpb);
    }
    return ps;
}

// Get first vacant long-term frame index
Ipp8u H264ENC_MAKE_NAME(H264CoreEncoder_GetFreeLTIdx)(
    void* state)
{
    Ipp8u i;
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;

    // find first vacant long-term frame index
    for (i = 0; i < 16; i++)
        if (core_enc->m_LTFrameIdxUseFlags[i] == 0)
            break;

    if (i < 16)
        core_enc->m_LTFrameIdxUseFlags[i] = 1; // mark it as occupied

    return i;
}

//  Mark all LT frame indexes as free
void H264ENC_MAKE_NAME(H264CoreEncoder_ClearAllLTIdxs)(
    H264CoreEncoderType* state)
{
    memset(state->m_LTFrameIdxUseFlags, 0, 16);
}

//  Mark particular LT frame index as free
void H264ENC_MAKE_NAME(H264CoreEncoder_ClearLTIdx)(
    H264CoreEncoderType* state,
    Ipp8u idx)
{
    if (idx < 16)
        state->m_LTFrameIdxUseFlags[idx] = 0;
}

/*************************************************************
 *  Name: SetSequenceParameters
 *  Description:  Fill in the Sequence Parameter Set for this
 *  sequence.  Can only change at an IDR picture.
 *************************************************************/
void H264ENC_MAKE_NAME(H264CoreEncoder_SetSequenceParameters)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    if (core_enc->m_svc_layer.svc_ext.dependency_id == 0 &&
        core_enc->QualityNum <= 1 && core_enc->TempIdMax <= 0)
        core_enc->m_SeqParamSet->profile_idc = core_enc->m_info.profile_idc;
    else if (core_enc->m_info.profile_idc == H264_SCALABLE_BASELINE_PROFILE)
        core_enc->m_SeqParamSet->profile_idc = H264_SCALABLE_BASELINE_PROFILE;
    else
        core_enc->m_SeqParamSet->profile_idc = H264_SCALABLE_HIGH_PROFILE;

    // We don't meet any of these constraints yet
    if (core_enc->m_info.use_ext_sps) {
        core_enc->m_SeqParamSet->constraint_set0_flag = core_enc->m_info.m_SeqParamSet.constraint_set0_flag;
        core_enc->m_SeqParamSet->constraint_set1_flag = core_enc->m_info.m_SeqParamSet.constraint_set1_flag;
        core_enc->m_SeqParamSet->constraint_set2_flag = core_enc->m_info.m_SeqParamSet.constraint_set2_flag;
        core_enc->m_SeqParamSet->constraint_set3_flag = core_enc->m_info.m_SeqParamSet.constraint_set3_flag;
        core_enc->m_SeqParamSet->constraint_set4_flag = core_enc->m_info.m_SeqParamSet.constraint_set4_flag;
        core_enc->m_SeqParamSet->constraint_set5_flag = core_enc->m_info.m_SeqParamSet.constraint_set5_flag;
    } else {
        core_enc->m_SeqParamSet->constraint_set0_flag = 0;
        core_enc->m_SeqParamSet->constraint_set1_flag = core_enc->m_info.m_ext_constraint_flags[1];
        core_enc->m_SeqParamSet->constraint_set2_flag = 0;
        core_enc->m_SeqParamSet->constraint_set3_flag = 0;
        core_enc->m_SeqParamSet->constraint_set4_flag = core_enc->m_info.m_ext_constraint_flags[4];
        core_enc->m_SeqParamSet->constraint_set5_flag = core_enc->m_info.m_ext_constraint_flags[5];
    }

    if (core_enc->m_info.profile_idc == H264_SCALABLE_BASELINE_PROFILE ||
        core_enc->m_info.profile_idc == H264_SCALABLE_HIGH_PROFILE)
        core_enc->m_SeqParamSet->seq_parameter_set_id = core_enc->m_svc_layer.svc_ext.dependency_id;  // for SVC only
    // Do not update: SPS ID is either 0 for AVC or set externally for MVC
    if (core_enc->m_info.m_ext_SPS_id)
         core_enc->m_SeqParamSet->seq_parameter_set_id = (Ipp8s)core_enc->m_info.m_ext_SPS_id;

    core_enc->m_SeqParamSet->log2_max_frame_num = core_enc->m_info.use_ext_sps ? core_enc->m_info.m_SeqParamSet.log2_max_frame_num : 8;

    // Setup pic_order_cnt_type based on use of B frames.
    // Note! pic_order_cnt_type == 1 is not implemented

    // The following are not transmitted in either case below, and are
    // just initialized here to be nice.
    core_enc->m_SeqParamSet->delta_pic_order_always_zero_flag = 0;
    core_enc->m_SeqParamSet->offset_for_non_ref_pic = 0;
    core_enc->m_SeqParamSet->poffset_for_ref_frame = NULL;
    core_enc->m_SeqParamSet->num_ref_frames_in_pic_order_cnt_cycle = 0;

    if (core_enc->m_info.use_ext_sps == true) {
        core_enc->m_SeqParamSet->pic_order_cnt_type = core_enc->m_info.m_SeqParamSet.pic_order_cnt_type;
        core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb = core_enc->m_info.m_SeqParamSet.log2_max_pic_order_cnt_lsb;
    } else {
        if (core_enc->m_info.B_frame_rate == 0 && (core_enc->m_info.coding_type == 0 || core_enc->m_info.coding_type == 2)) // TODO: remove coding_type == 2 when MBAFF is implemented
        {
            core_enc->m_SeqParamSet->pic_order_cnt_type = 2;
            core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb = 0;
            // Right now this only supports simple P frame patterns (e.g. H264PPPP...)
        } else {
            //Ipp32s log2_max_poc = (Ipp32u)log(((Ipp64f)core_enc->m_info.B_frame_rate +
            //    core_enc->m_info.num_ref_to_start_code_B_slice)/log((Ipp64f)2) + 1) << 1;

            Ipp32u maxpoc = (core_enc->m_info.B_frame_rate<<((core_enc->m_info.treat_B_as_reference==2)?1:0))+ core_enc->m_info.num_ref_frames;
            if (core_enc->TempIdMax > 0) {
                Ipp32u maxdist = core_enc->TemporalScaleList[core_enc->TempIdMax];
                if (maxpoc < maxdist)
                maxpoc = maxdist;
            }

            if (maxpoc > 3) { // means IPP_MAX(log2_max_poc, 4) > 4
                Ipp32s log2_max_poc; //= (Ipp32s) (log((Ipp64f)maxpoc) / log(2.0)) + 3; // 3=1+1+1=round+multiply by 2 in counting+devide by 2 in comparison
                for (maxpoc >>= 3, log2_max_poc = 2+3; maxpoc; maxpoc>>=1, log2_max_poc++);
                core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb = log2_max_poc;
            } else {
                core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb = 4;
            }

            if (core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb > 16)
            {
                VM_ASSERT(false);
                core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb = 16;
            }

            core_enc->m_SeqParamSet->pic_order_cnt_type = 0;
            // Right now this only supports simple B frame patterns (e.g. IBBPBBP...)
        }
    }
    core_enc->m_SeqParamSet->num_ref_frames = core_enc->m_info.num_ref_frames;

    if (core_enc->m_info.use_ext_sps)
        core_enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = core_enc->m_info.m_SeqParamSet.gaps_in_frame_num_value_allowed_flag;
    // Note!  NO code after this point supports pic_order_cnt_type == 1
    // Always zero because we don't support field encoding
    core_enc->m_SeqParamSet->offset_for_top_to_bottom_field = 0;

    core_enc->m_SeqParamSet->frame_mbs_only_flag = (core_enc->m_info.coding_type && core_enc->m_info.coding_type != 2)? 0: 1; // TODO: remove coding_type != 2 when MBAFF is implemented

    if (core_enc->m_svc_layer.svc_ext.dependency_id != 0 ||
        core_enc->QualityNum > 1 || core_enc->TempIdMax > 0)
        core_enc->m_SeqParamSet->gaps_in_frame_num_value_allowed_flag = core_enc->TempIdMax > 0; //0;
    core_enc->m_SeqParamSet->mb_adaptive_frame_field_flag = core_enc->m_info.coding_type>3; // TODO: add coding_type = 2 when pure MBAFF is implemented

    // If set to 1, 8x8 blocks in Direct Mode always use 1 MV,
    // obtained from the "outer corner" 4x4 block, regardless
    // of how the CoLocated 8x8 is split into subblocks.  If this
    // is 0, then the 8x8 in Direct Mode is subdivided exactly as
    // the Colocated 8x8, with the appropriate number of derived MVs.
    core_enc->m_SeqParamSet->direct_8x8_inference_flag =
        core_enc->m_info.use_direct_inference || !core_enc->m_SeqParamSet->frame_mbs_only_flag ? 1 : 0;

    // Picture Dimensions in MBs
    core_enc->m_SeqParamSet->frame_width_in_mbs = ((core_enc->m_info.info.clip_info.width+15)>>4);
    core_enc->m_SeqParamSet->frame_height_in_mbs = ((core_enc->m_info.info.clip_info.height+(16<<(1 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1)>>4) >> (1 - core_enc->m_SeqParamSet->frame_mbs_only_flag);
    Ipp32s frame_height_in_mbs = core_enc->m_SeqParamSet->frame_height_in_mbs << (1 - core_enc->m_SeqParamSet->frame_mbs_only_flag);

    // If the width & height in MBs doesn't match the image dimensions or
    // cropped rectangle isn't equal to full image then do cropping in the decoder
    if (core_enc->m_info.frame_crop_x > 0 || core_enc->m_info.frame_crop_y > 0 ||
        (core_enc->m_info.frame_crop_w > 0 && core_enc->m_info.frame_crop_w < core_enc->m_info.info.clip_info.width) ||
        (core_enc->m_info.frame_crop_h > 0 && core_enc->m_info.frame_crop_h < core_enc->m_info.info.clip_info.height))
    {
        core_enc->m_SeqParamSet->frame_cropping_flag = 1;
        core_enc->m_SeqParamSet->frame_crop_left_offset = (core_enc->m_info.frame_crop_x + (UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc] - 1)) /
            UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc];
        if (core_enc->m_info.frame_crop_w)
            core_enc->m_SeqParamSet->frame_crop_right_offset = (core_enc->m_info.info.clip_info.width -
            (core_enc->m_info.frame_crop_x + core_enc->m_info.frame_crop_w) + (UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc] -1 )) /
            UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc];
        else
            core_enc->m_SeqParamSet->frame_crop_right_offset = 0;
        core_enc->m_SeqParamSet->frame_crop_top_offset = (core_enc->m_info.frame_crop_y + ((UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1)) /
            (UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag));
        if (core_enc->m_info.frame_crop_h)
            core_enc->m_SeqParamSet->frame_crop_bottom_offset = (core_enc->m_info.info.clip_info.height -
            (core_enc->m_info.frame_crop_y + core_enc->m_info.frame_crop_h) + ((UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1)) /
            (UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag));
        else
            core_enc->m_SeqParamSet->frame_crop_bottom_offset = 0;
    } else if (((core_enc->m_SeqParamSet->frame_width_in_mbs<<4) != core_enc->m_info.info.clip_info.width) ||
        ((frame_height_in_mbs << 4) != core_enc->m_info.info.clip_info.height)) {
        core_enc->m_SeqParamSet->frame_cropping_flag = 1;
        core_enc->m_SeqParamSet->frame_crop_left_offset = 0;
        core_enc->m_SeqParamSet->frame_crop_right_offset =
            ((core_enc->m_SeqParamSet->frame_width_in_mbs<<4) - core_enc->m_info.info.clip_info.width)/UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc];
        core_enc->m_SeqParamSet->frame_crop_top_offset = 0;
        core_enc->m_SeqParamSet->frame_crop_bottom_offset =
            ((frame_height_in_mbs<<4) - core_enc->m_info.info.clip_info.height)/(UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag));
    } else {
        core_enc->m_SeqParamSet->frame_cropping_flag = 0;
        core_enc->m_SeqParamSet->frame_crop_left_offset = 0;
        core_enc->m_SeqParamSet->frame_crop_right_offset = 0;
        core_enc->m_SeqParamSet->frame_crop_top_offset = 0;
        core_enc->m_SeqParamSet->frame_crop_bottom_offset = 0;
    }

    core_enc->m_SeqParamSet->level_idc = core_enc->m_info.level_idc;

    core_enc->m_SeqParamSet->chroma_format_idc              = (Ipp8s)core_enc->m_info.chroma_format_idc;
    core_enc->m_SeqParamSet->bit_depth_luma                 = core_enc->m_info.bit_depth_luma;
    core_enc->m_SeqParamSet->bit_depth_chroma               = core_enc->m_info.bit_depth_chroma;
    core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag = core_enc->m_info.qpprime_y_zero_transform_bypass_flag;
    core_enc->m_SeqParamSet->seq_scaling_matrix_present_flag = false;

    core_enc->m_SeqParamSet->bit_depth_aux                  = core_enc->m_info.bit_depth_aux;
    core_enc->m_SeqParamSet->alpha_incr_flag                = core_enc->m_info.alpha_incr_flag;
    core_enc->m_SeqParamSet->alpha_opaque_value             = core_enc->m_info.alpha_opaque_value;
    core_enc->m_SeqParamSet->alpha_transparent_value        = core_enc->m_info.alpha_transparent_value;
    core_enc->m_SeqParamSet->aux_format_idc                 = core_enc->m_info.aux_format_idc;

    if (core_enc->m_SeqParamSet->aux_format_idc != 0)
    {
        core_enc->m_SeqParamSet->pack_sequence_extension = 1;
    }

    // Precalculate these values so we have them for later (repeated) use.
    core_enc->m_SeqParamSet->MaxMbAddress = (core_enc->m_SeqParamSet->frame_width_in_mbs * frame_height_in_mbs) - 1;
    H264ENC_MAKE_NAME(H264CoreEncoder_SetDPBSize)(state);

    //Scaling matrices
    if( core_enc->m_info.use_default_scaling_matrix){
        Ipp32s i;
        //setup matrices that will be used
        core_enc->m_SeqParamSet->seq_scaling_matrix_present_flag = true;
        // 4x4 matrices
        for( i=0; i<6; i++ ) core_enc->m_SeqParamSet->seq_scaling_list_present_flag[i] = true;
        //Copy default
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[0], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[1], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[2], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[3], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[4], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[5], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));

        // 8x8 matrices
        core_enc->m_SeqParamSet->seq_scaling_list_present_flag[6] = true;
        core_enc->m_SeqParamSet->seq_scaling_list_present_flag[7] = true;

        //Copy default scaling matrices
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_8x8[0], UMC_H264_ENCODER::DefaultScalingList8x8[1], 64*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_8x8[1], UMC_H264_ENCODER::DefaultScalingList8x8[1], 64*sizeof(Ipp8u));
    }else{
        core_enc->m_SeqParamSet->seq_scaling_matrix_present_flag = false;
        //Copy default
/*            MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[0], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[1], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[2], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[3], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[4], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_4x4[5], UMC_H264_ENCODER::FlatScalingList4x4, 16*sizeof(Ipp8u));
*/
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_8x8[0], UMC_H264_ENCODER::FlatScalingList8x8, 64*sizeof(Ipp8u));
        MFX_INTERNAL_CPY(core_enc->m_SeqParamSet->seq_scaling_list_8x8[1], UMC_H264_ENCODER::FlatScalingList8x8, 64*sizeof(Ipp8u));
    }

    //Generate new scaling matrices for use in transform
    Ipp32s qp_rem,i;
/*     for( i=0; i < 6; i++ )
         for( qp_rem = 0; qp_rem<6; qp_rem++ ) {
             ippiGenScaleLevel4x4_H264_8u16s_D2( core_enc->m_SeqParamSet->seq_scaling_list_4x4[i],
                                     core_enc->m_SeqParamSet->seq_scaling_inv_matrix_4x4[i][qp_rem],
                                     core_enc->m_SeqParamSet->seq_scaling_matrix_4x4[i][qp_rem],
                                     qp_rem );

        }
*/
    for( i=0; i<2; i++)
        for( qp_rem=0; qp_rem<6; qp_rem++ )
            ippiGenScaleLevel8x8_H264_8u16s_D2(core_enc->m_SeqParamSet->seq_scaling_list_8x8[i],
                8,
                core_enc->m_SeqParamSet->seq_scaling_inv_matrix_8x8[i][qp_rem],
                core_enc->m_SeqParamSet->seq_scaling_matrix_8x8[i][qp_rem],
                qp_rem);

    if (core_enc->m_info.use_ext_sps) { //
        core_enc->m_SeqParamSet->vui_parameters_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters_present_flag;
        if (core_enc->m_SeqParamSet->vui_parameters_present_flag) {
            // set VUI flags from external SPS
            core_enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.aspect_ratio_info_present_flag;
            core_enc->m_SeqParamSet->vui_parameters.overscan_info_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.overscan_info_present_flag;
            core_enc->m_SeqParamSet->vui_parameters.overscan_appropriate_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.overscan_appropriate_flag;
            core_enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.video_signal_type_present_flag;
            core_enc->m_SeqParamSet->vui_parameters.timing_info_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.timing_info_present_flag;
            core_enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.fixed_frame_rate_flag;
            core_enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.vcl_hrd_parameters_present_flag;
            core_enc->m_SeqParamSet->vui_parameters.pic_struct_present_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.pic_struct_present_flag;
            core_enc->m_SeqParamSet->vui_parameters.fixed_frame_rate_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.fixed_frame_rate_flag;
            core_enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag = core_enc->m_info.m_SeqParamSet.vui_parameters.bitstream_restriction_flag;
        }
    } else {
        core_enc->m_SeqParamSet->vui_parameters_present_flag = 1;
        core_enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
    }

    // SVC extension
    core_enc->m_SeqParamSet->inter_layer_deblocking_filter_control_present_flag = 1; //KL 1 to be able to disable
    core_enc->m_SeqParamSet->extended_spatial_scalability = 0;
    core_enc->m_SeqParamSet->chroma_phase_x_plus1 = 0;
    core_enc->m_SeqParamSet->chroma_phase_y_plus1 = 0;
    core_enc->m_SeqParamSet->seq_ref_layer_chroma_phase_x_plus1 = 0;
    core_enc->m_SeqParamSet->seq_ref_layer_chroma_phase_y_plus1 = 0;

    core_enc->m_SeqParamSet->slice_header_restriction_flag = 1;

    core_enc->m_SeqParamSet->seq_scaled_ref_layer_left_offset = 0;
    core_enc->m_SeqParamSet->seq_scaled_ref_layer_top_offset = 0;
    core_enc->m_SeqParamSet->seq_scaled_ref_layer_right_offset = 0;
    core_enc->m_SeqParamSet->seq_scaled_ref_layer_bottom_offset = 0;

    if ((core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_BASELINE_PROFILE) ||
        (core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_HIGH_PROFILE)) {
        UMC::H264LayerResizeParameter *rsz = &core_enc->m_svc_layer.resize;

        core_enc->m_SeqParamSet->inter_layer_deblocking_filter_control_present_flag = 1; //KL 1 to be able to disable
        // copy offset values to sps
        core_enc->m_SeqParamSet->extended_spatial_scalability = (Ipp8u)rsz->extended_spatial_scalability;
        core_enc->m_SeqParamSet->seq_scaled_ref_layer_left_offset = rsz->leftOffset;
        core_enc->m_SeqParamSet->seq_scaled_ref_layer_top_offset = rsz->topOffset;
        core_enc->m_SeqParamSet->seq_scaled_ref_layer_right_offset = rsz->rightOffset;
        core_enc->m_SeqParamSet->seq_scaled_ref_layer_bottom_offset = rsz->bottomOffset;

        if (core_enc->m_SeqParamSet->chroma_format_idc == 1 || core_enc->m_SeqParamSet->chroma_format_idc == 2) {
            core_enc->m_SeqParamSet->chroma_phase_x_plus1 = 0; // 1???
            core_enc->m_SeqParamSet->seq_ref_layer_chroma_phase_x_plus1 = core_enc->m_SeqParamSet->chroma_phase_x_plus1;
        }

        if (core_enc->m_SeqParamSet->chroma_format_idc == 1) {
            core_enc->m_SeqParamSet->chroma_phase_y_plus1 = 0;
            core_enc->m_SeqParamSet->seq_ref_layer_chroma_phase_y_plus1 = core_enc->m_SeqParamSet->chroma_phase_y_plus1;
        }

        if (core_enc->m_SeqParamSet->extended_spatial_scalability == 1) {
            if (core_enc->m_SeqParamSet->chroma_format_idc > 0) {
                core_enc->m_SeqParamSet->seq_ref_layer_chroma_phase_x_plus1 = 0;
                core_enc->m_SeqParamSet->seq_ref_layer_chroma_phase_y_plus1 = 0;
            }
        }

        // Proper values are set outside during Init
        //core_enc->m_SeqParamSet->seq_tcoeff_level_prediction_flag = 1;
        //if (core_enc->m_SeqParamSet->seq_tcoeff_level_prediction_flag) {
        //    core_enc->m_SeqParamSet->adaptive_tcoeff_level_prediction_flag = 1;
        //}

        core_enc->m_SeqParamSet->slice_header_restriction_flag = 0;
    } else { // Not SVC profile
        core_enc->m_SeqParamSet->seq_tcoeff_level_prediction_flag = 0;
        core_enc->m_SeqParamSet->adaptive_tcoeff_level_prediction_flag = 0;
    }

} // SetSequenceParameters

/*************************************************************
 *  Name: SetPictureParameters
 *  Description:  Fill in the Picture Parameter Set for this
 *  sequence.  Can only change at an IDR picture.
 *************************************************************/
void H264ENC_MAKE_NAME(H264CoreEncoder_SetPictureParameters)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    if (core_enc->m_info.profile_idc == H264_SCALABLE_BASELINE_PROFILE ||
        core_enc->m_info.profile_idc == H264_SCALABLE_HIGH_PROFILE)
        core_enc->m_PicParamSet->pic_parameter_set_id = core_enc->m_svc_layer.svc_ext.dependency_id; // for SVC only

    // Do not update: PPS ID is either 0 for AVC or set externaly for MVC
    if (core_enc->m_info.m_ext_PPS_id)
         core_enc->m_PicParamSet->pic_parameter_set_id = (Ipp8s)core_enc->m_info.m_ext_PPS_id;

    // Assumes that there is only one sequence param set to choose from
    core_enc->m_PicParamSet->seq_parameter_set_id = core_enc->m_SeqParamSet->seq_parameter_set_id;

    core_enc->m_PicParamSet->entropy_coding_mode = core_enc->m_info.entropy_coding_mode;

    core_enc->m_PicParamSet->pic_order_present_flag = core_enc->m_info.use_ext_pps ? core_enc->m_info.m_PicParamSet.pic_order_present_flag : core_enc->m_info.coding_type > 0 ? 1 : 0;

    core_enc->m_PicParamSet->weighted_pred_flag = 0;

    // We use implicit weighted prediction (2) when B frame rate can
    // benefit from it.  When B_Frame_rate == 0 or 1, it doesn't matter,
    // so we do what is faster (0).

    core_enc->m_PicParamSet->weighted_bipred_idc = core_enc->m_info.use_implicit_weighted_bipred ? 2 : 0;

    if (core_enc->m_info.use_ext_pps) {
        core_enc->m_PicParamSet->pic_init_qp = core_enc->m_info.m_PicParamSet.pic_init_qp;
        core_enc->m_PicParamSet->pic_init_qs = core_enc->m_info.m_PicParamSet.pic_init_qs;     // Not used
        core_enc->m_PicParamSet->chroma_qp_index_offset = core_enc->m_info.m_PicParamSet.chroma_qp_index_offset;
        core_enc->m_PicParamSet->deblocking_filter_variables_present_flag = core_enc->m_info.m_PicParamSet.deblocking_filter_variables_present_flag;
    }
    else {
        // Default to P frame constant quality at time of an IDR
        if (core_enc->m_info.rate_controls.method == H264_RCM_VBR ||
            core_enc->m_info.rate_controls.method == H264_RCM_CBR ||
            core_enc->m_info.rate_controls.method == H264_RCM_VBR_SLICE ||
            core_enc->m_info.rate_controls.method == H264_RCM_CBR_SLICE
            ){
            //core_enc->m_PicParamSet->pic_init_qp = (Ipp8s)H264_AVBR_GetQP(&core_enc->avbr, INTRAPIC);
            core_enc->m_PicParamSet->pic_init_qp = 26;
        }else if(core_enc->m_info.rate_controls.method == H264_RCM_QUANT){
            core_enc->m_PicParamSet->pic_init_qp = core_enc->m_info.rate_controls.quantP;
        }
        core_enc->m_PicParamSet->pic_init_qs = 26;     // Not used
        core_enc->m_PicParamSet->chroma_qp_index_offset = 0;
        core_enc->m_PicParamSet->deblocking_filter_variables_present_flag = 1;
    }

    //if ((core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_BASELINE_PROFILE) ||
    //    (core_enc->m_SeqParamSet->profile_idc == H264_SCALABLE_HIGH_PROFILE))
    //    core_enc->m_PicParamSet->constrained_intra_pred_flag = 1;
    core_enc->m_PicParamSet->constrained_intra_pred_flag = core_enc->m_constrained_intra_pred_flag;

    // We don't do redundant slices...
    core_enc->m_PicParamSet->redundant_pic_cnt_present_flag = 0;
    core_enc->m_PicParamSet->pic_scaling_matrix_present_flag = 0;
    // In the future, if flexible macroblock ordering is
    // desired, then a macroblock allocation map will need
    // to be coded and the value below updated accordingly.
    core_enc->m_PicParamSet->num_slice_groups = 1;     // Hard coded for now
    core_enc->m_PicParamSet->SliceGroupInfo.slice_group_map_type = 0;
    core_enc->m_PicParamSet->SliceGroupInfo.t3.pic_size_in_map_units = 0;
    core_enc->m_PicParamSet->SliceGroupInfo.t3.pSliceGroupIDMap = NULL;

    // I guess these need to be 1 or greater since they are written as "minus1".
    if (core_enc->m_info.use_ext_pps) {
        core_enc->m_PicParamSet->num_ref_idx_l0_active = core_enc->m_info.m_PicParamSet.num_ref_idx_l0_active;
        core_enc->m_PicParamSet->num_ref_idx_l1_active = core_enc->m_info.m_PicParamSet.num_ref_idx_l1_active;
    } else {
        core_enc->m_PicParamSet->num_ref_idx_l0_active = 1;
        core_enc->m_PicParamSet->num_ref_idx_l1_active = 1;
    }

    core_enc->m_PicParamSet->transform_8x8_mode_flag = core_enc->m_info.transform_8x8_mode_flag;

} // SetPictureParameters

/*************************************************************
 *  Name: SetSliceHeaderCommon
 *  Description:  Given the ePictureType and core_enc->m_info
 *                fill in the slice header for this slice
 ************************************************************/
void H264ENC_MAKE_NAME(H264CoreEncoder_SetSliceHeaderCommon)(
    void* state,
    H264EncoderFrameType* pCurrentFrame )
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s frame_num = pCurrentFrame->m_FrameNum;
    core_enc->m_SliceHeader.frame_num = frame_num % (1 << core_enc->m_SeqParamSet->log2_max_frame_num);

    // Assumes there is only one picture parameter set to choose from
    core_enc->m_SliceHeader.pic_parameter_set_id = core_enc->m_PicParamSet->pic_parameter_set_id;

    core_enc->m_SliceHeader.field_pic_flag = (Ipp8u)(pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE);
    core_enc->m_SliceHeader.bottom_field_flag = (Ipp8u)(pCurrentFrame->m_bottom_field_flag[core_enc->m_field_index]);
    core_enc->m_SliceHeader.MbaffFrameFlag = (core_enc->m_SeqParamSet->mb_adaptive_frame_field_flag)&&(!core_enc->m_SliceHeader.field_pic_flag);
    core_enc->m_SliceHeader.delta_pic_order_cnt_bottom = (core_enc->m_pCurrentFrame->m_coding_type & H264_PICSTRUCT_FIELD_BFF) ? -1 : 1;

    //core_enc->m_TopPicOrderCnt = core_enc->m_PicOrderCnt;
    //core_enc->m_BottomPicOrderCnt = core_enc->m_TopPicOrderCnt + 1; // ????

    if (core_enc->m_SeqParamSet->pic_order_cnt_type == 0) {
        core_enc->m_SliceHeader.pic_order_cnt_lsb = (
        H264ENC_MAKE_NAME(H264EncoderFrame_PicOrderCnt)(
                    core_enc->m_pCurrentFrame,
                    core_enc->m_field_index,
                    0) - core_enc->m_PicOrderCnt_LastIDR)
         & ~(0xffffffff << core_enc->m_SeqParamSet->log2_max_pic_order_cnt_lsb);
    }

    // structures for adaptive ref pic matking will be filled in function H264CoreEncoder_PrepareRefPicMarking()
    core_enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 0;
    core_enc->m_DecRefPicMarkingInfo[core_enc->m_field_index].num_entries = 0;

    // structures for ref pic list modification will be filled in function H264CoreEncoder_ModifyRefPicList()
    core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l0 = 0;
    core_enc->m_ReorderInfoL0[core_enc->m_field_index].num_entries = 0;
    core_enc->m_SliceHeader.ref_pic_list_reordering_flag_l1 = 0;
    core_enc->m_ReorderInfoL1[core_enc->m_field_index].num_entries = 0;

    for(Ipp32s i = 0; i < core_enc->m_info.num_slices*((core_enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE)+1); i++) { //TODO fix for AFRM
        H264SliceType *curr_slice = core_enc->m_Slices + i;
        curr_slice->num_ref_idx_l0_active = 0;
        curr_slice->num_ref_idx_l1_active = 0;
        curr_slice->m_use_intra_mbs_only = false;

        curr_slice->m_disable_deblocking_filter_idc = core_enc->m_info.deblocking_filter_idc;
    }
}

/*************************************************************
 *  Name:         H264CoreEncoder_encodeSPSPPS
 *  Description:  Initialize and write SPS/PPS headers.
 ************************************************************/
Status H264ENC_MAKE_NAME(H264CoreEncoder_encodeSPSPPS)(
    void* state,
    H264BsRealType* bs,
    MediaData* dstSPS,
    MediaData* dstPPS)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Status ps = UMC_OK;
    bool StartPicture = false;

    H264CoreEncoder_SetSequenceParameters_8u16s(state);
    H264CoreEncoder_SetPictureParameters_8u16s(state);

    ps = H264ENC_MAKE_NAME(H264BsReal_PutSeqParms)(bs, *core_enc->m_SeqParamSet);

    H264BsBase_WriteTrailingBits(&bs->m_base);

    // Copy Sequence Parms RBSP to the end of the output buffer after
    // Adding start codes and SC emulation prevention.
    size_t oldsize = dstSPS->GetDataSize();
    dstSPS->SetDataSize(
        oldsize +
            H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                bs,
                (Ipp8u*)dstSPS->GetDataPointer() + oldsize,
                1,
                NAL_UT_SPS,
                StartPicture,
                0));

    ps = H264ENC_MAKE_NAME(H264BsReal_PutPicParms)(
        bs,
        *core_enc->m_PicParamSet,
        *core_enc->m_SeqParamSet);

    H264BsBase_WriteTrailingBits(&bs->m_base);

    // Copy Picture Parms RBSP to the end of the output buffer after
    // Adding start codes and SC emulation prevention.
    oldsize = dstPPS->GetDataSize();
    dstPPS->SetDataSize(
        oldsize +
            H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                bs,
                (Ipp8u*)dstPPS->GetDataPointer() + oldsize,
                1,
                NAL_UT_PPS,
                StartPicture,
                0));

    return ps;
}

/*************************************************************
 *  Name:         encodeFrameHeader
 *  Description:  Write out the frame header to the bit stream. // To be unused in MFX
 * ATTENTION !!! - use function from mfx_h264_encode !!!
 ************************************************************/
Status H264ENC_MAKE_NAME(H264CoreEncoder_encodeFrameHeader)(
    void* state,
    H264BsRealType* bs,
    MediaData* dst,
    bool bIDR_Pic,
    bool& startPicture )
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Status ps = UMC_OK;

    // First, write a access unit delimiter for the frame.
    if (core_enc->m_info.write_access_unit_delimiters &&
        // Don't write delimiter before dependent view in case view output is on
        !(core_enc->m_info.m_is_mvc_profile && core_enc->m_info.m_viewOutput && !core_enc->m_info.m_is_base_view))
    {
        ps = H264ENC_MAKE_NAME(H264BsReal_PutPicDelimiter)(bs, core_enc->m_PicType);

        H264BsBase_WriteTrailingBits(&bs->m_base);

        // Copy PicDelimiter RBSP to the end of the output buffer after
        // Adding start codes and SC emulation prevention.
        size_t oldsize = dst->GetDataSize();
        dst->SetDataSize(
            oldsize +
                H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                    bs,
                    (Ipp8u*)dst->GetDataPointer() + oldsize,
                    0,
                    NAL_UT_AUD,
                    startPicture,
                    0));
    }

    // Write dependency delimiter before dependent view
    if (core_enc->m_info.m_is_mvc_profile && core_enc->m_info.m_viewOutput && !core_enc->m_info.m_is_base_view)
    {
        ps = H264ENC_MAKE_NAME(H264BsReal_PutPicDelimiter)(bs, core_enc->m_PicType);

        H264BsBase_WriteTrailingBits(&bs->m_base);

        size_t oldsize = dst->GetDataSize();
        dst->SetDataSize(oldsize +
            H264BsReal_EndOfNAL_8u16s(bs,
            (Ipp8u*)dst->GetDataPointer() + oldsize,
            0,
            NAL_UT_DEPENDENCY_DELIMETER,
            startPicture,
            0));
    }

    // If this is an IDR picture, write the seq and pic parameter sets
    // SVC doesn't enter here
    bool putSet = false;

    if (core_enc->m_svc_layer.isActive) {
        if ((core_enc->m_svc_layer.svc_ext.quality_id == 0) ||
            ((core_enc->m_svc_layer.svc_ext.dependency_id == 0) &&
            (core_enc->m_svc_layer.svc_ext.quality_id == 1)))
            putSet = true;
    }
    putSet = putSet && (core_enc->m_svc_layer.svc_ext.dependency_id == 0); // conflicts with "quality_id == 0"

    if (bIDR_Pic)
    {
        if(core_enc->m_info.m_is_mvc_profile) {
            if(!core_enc->m_info.m_is_base_view && !core_enc->m_info.m_viewOutput)
                // If MVC && dependent view - skip SPS/PPS output, done at base view
                return ps;

            H264SeqParamSet   *sps_views = core_enc->m_SeqParamSet;
            H264PicParamSet   *pps_views = core_enc->m_PicParamSet;
            size_t             oldsize;
            do {
                // Write the seq_parameter_set_rbsp
                ps = H264ENC_MAKE_NAME(H264BsReal_PutSeqParms)(bs, *sps_views);

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Sequence Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                oldsize = dst->GetDataSize();
                if(sps_views->profile_idc==H264_MULTIVIEWHIGH_PROFILE || sps_views->profile_idc==H264_STEREOHIGH_PROFILE) {
                    dst->SetDataSize(
                        oldsize +
                            H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                                bs,
                                (Ipp8u*)dst->GetDataPointer() + oldsize,
                                1,
                                NAL_UT_SUBSET_SPS,
                                startPicture,
                                0));
                } else {
                    dst->SetDataSize(
                        oldsize +
                        H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                        bs,
                        (Ipp8u*)dst->GetDataPointer() + oldsize,
                        1,
                        NAL_UT_SPS,
                        startPicture, 0));
                }

                if(sps_views->pack_sequence_extension) {
                    // Write the seq_parameter_set_extension_rbsp when needed.
                    ps = H264ENC_MAKE_NAME(H264BsReal_PutSeqExParms)(bs, *sps_views);

                    H264BsBase_WriteTrailingBits(&bs->m_base);

                    // Copy Sequence Parms RBSP to the end of the output buffer after
                    // Adding start codes and SC emulation prevention.
                    oldsize = dst->GetDataSize();
                    dst->SetDataSize(
                        oldsize +
                            H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                                bs,
                                (Ipp8u*)dst->GetDataPointer() + oldsize,
                                1,
                                NAL_UT_SEQEXT,
                                startPicture,
                                0));
                }

                if (core_enc->m_info.m_viewOutput)
                    break;

                sps_views++;
            } while (sps_views->profile_idc);

            sps_views = core_enc->m_SeqParamSet;
            do {
                ps = H264ENC_MAKE_NAME(H264BsReal_PutPicParms)(
                    bs,
                    *pps_views,
                    // sps_views is an array of SPS. First view points to the beginning of it, second view
                    // points to a second SPS. So in view output when writing second view SPS there should be zero offset,
                    // but when view output is off, first view has to write both SPS from this array, so for second
                    // view offset should be positive.
                    *(sps_views + (core_enc->m_info.m_viewOutput ? 0 : pps_views->seq_parameter_set_id)));

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Picture Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                oldsize = dst->GetDataSize();
                dst->SetDataSize(
                    oldsize +
                        H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                            bs,
                            (Ipp8u*)dst->GetDataPointer() + oldsize,
                            1,
                            NAL_UT_PPS,
                            startPicture,
                            0));

                if (core_enc->m_info.m_viewOutput)
                    break;

                pps_views++;
            } while (pps_views->seq_parameter_set_id);

        } else if (putSet) {
            if (core_enc->m_svc_layer.isActive) {
                if ((core_enc->m_svc_layer.svc_ext.dependency_id == 0) &&
                    (core_enc->m_svc_layer.svc_ext.quality_id == 0))
                {
                    // Write the SEI scalable
                    ps = H264ENC_MAKE_NAME(H264BsReal_PutSVCScalabilityInfoSEI)(bs, core_enc->svc_ScalabilityInfoSEI);

                    H264BsBase_WriteTrailingBits(&bs->m_base);

                    // Copy Sequence Parms RBSP to the end of the output buffer after
                    // Adding start codes and SC emulation prevention.
                    dst->SetDataSize(
                        dst->GetDataSize() +
                        H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                            bs,
                            (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize(),
                            0,
                            NAL_UT_SEI,
                            startPicture, 0));

                }
            }

            // The loop by svc layers impossible from here, so encodeFrameHeader moved to mfx_h264_encode
            //////for( struct h264Layer* layer = m_layer; layer; layer = layer->src_layer) {
            //////    core_enc = layer->enc;

            // Write the seq_parameter_set_rbsp
            ps = H264ENC_MAKE_NAME(H264BsReal_PutSeqParms)(bs, *core_enc->m_SeqParamSet);

            H264BsBase_WriteTrailingBits(&bs->m_base);

            // Copy Sequence Parms RBSP to the end of the output buffer after
            // Adding start codes and SC emulation prevention.
            size_t oldsize = dst->GetDataSize();
            NAL_Unit_Type NalType = NAL_UT_SPS;
            if (core_enc->m_svc_layer.isActive) {
                if ((core_enc->m_svc_layer.svc_ext.dependency_id > 0) ||
                    (core_enc->m_svc_layer.svc_ext.quality_id > 0))
                {
                    NalType = NAL_UT_SUBSET_SPS;
                }
            }
            dst->SetDataSize(
                oldsize +
                    H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                        bs,
                        (Ipp8u*)dst->GetDataPointer() + oldsize,
                        1,
                        NalType,
                        startPicture,
                        0));

            if(core_enc->m_SeqParamSet->pack_sequence_extension) {
                // Write the seq_parameter_set_extension_rbsp when needed.
                ps = H264ENC_MAKE_NAME(H264BsReal_PutSeqExParms)(bs, *core_enc->m_SeqParamSet);

                H264BsBase_WriteTrailingBits(&bs->m_base);

                // Copy Sequence Parms RBSP to the end of the output buffer after
                // Adding start codes and SC emulation prevention.
                oldsize = dst->GetDataSize();
                dst->SetDataSize(
                    oldsize +
                        H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                            bs,
                            (Ipp8u*)dst->GetDataPointer() + oldsize,
                            1,
                            NAL_UT_SEQEXT,
                            startPicture,
                            0));
            }
            //////}
            //////core_enc = (H264CoreEncoder_8u16s *)state; // restore core_enc

            //////for( struct h264Layer* layer = m_layer; layer; layer = layer->src_layer) {
            //////    core_enc = layer->enc;
            ps = H264ENC_MAKE_NAME(H264BsReal_PutPicParms)(
                bs,
                *core_enc->m_PicParamSet,
                *core_enc->m_SeqParamSet);

            H264BsBase_WriteTrailingBits(&bs->m_base);

            // Copy Picture Parms RBSP to the end of the output buffer after
            // Adding start codes and SC emulation prevention.
            oldsize = dst->GetDataSize();
            dst->SetDataSize(
                oldsize +
                    H264ENC_MAKE_NAME(H264BsReal_EndOfNAL)(
                        bs,
                        (Ipp8u*)dst->GetDataPointer() + oldsize,
                        1,
                        NAL_UT_PPS,
                        startPicture,
                        0));
            //////}
            //////core_enc = (H264CoreEncoder_8u16s *)state; // restore core_enc
        }
    }

    return ps;
}

// ===========================================================================
//
// Apply the "Profile Rules" to this profile frame type
//
//
//      Option         Single Layer
// ------------   -------------------------
// Progressive
//      Stills
//                 Purge frames in the
//                 Ready and Wait queues.
//                 Reset the profile index.
//
//                -------------------------
// Key frame
//  interval       Wait for next 'P' in the
//                 profile to make an I frame
//                 Do process intervening B
//                 frames in the profile.
//                 No profile index reset.
//                 The key frame interval
//                 counter restarts after
//                 the key frame is sent.
//                -------------------------
// Forced Key
//      Frame      Regardless of the profile
//                 frame type; purge the
//                 the Ready queue.
//                 If destuctive key frame request
//                 then purge the Wait queue
//                 Reset profile index.
//
//                -------------------------
// switch B
// frame to P      If this profile frame type
//                 indicates a 'B' it will be
//                 en-queued, encoded and
//                 emitted as a P frame. Prior
//                 frames (P|B) in the queue
//                 are correctly emitted using
//                 the (new) P as a ref frame
//
// ------------   -------------------------
EnumPicCodType H264ENC_MAKE_NAME(H264CoreEncoder_DetermineFrameType)(
    void* state,
    Ipp32s)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    EnumPicCodType ePictureType = core_enc->eFrameType[core_enc->m_iProfileIndex];

    if (core_enc->m_uFrames_Num == 0)
    {
        core_enc->m_iProfileIndex++;
        if (core_enc->m_iProfileIndex == core_enc->profile_frequency)
            core_enc->m_iProfileIndex = 0;

        core_enc->m_bMakeNextFrameKey = false;
        return INTRAPIC;
    }

    // see if the layer's key frame interval has expired
    if (H264_KFCM_INTERVAL == core_enc->m_info.key_frame_controls.method)
    {
        // a value of one means every frame is a key frame
        // The interval can be expressed as: "Every Nth frame is a key frame"
        core_enc->m_uIntraFrameInterval--;
        if (1 == core_enc->m_uIntraFrameInterval)
        {
            core_enc->m_bMakeNextFrameKey = true;
        }
    }

    // change to an I frame
    if (core_enc->m_bMakeNextFrameKey)
    {
        ePictureType = INTRAPIC;

        // When a frame is forced INTRA, which happens internally for the
        // first frame, explicitly by the application, on a decode error
        // or for CPU scalability - the interval counter gets reset.
        if (H264_KFCM_INTERVAL == core_enc->m_info.key_frame_controls.method)
        {
            core_enc->m_uIntraFrameInterval = core_enc->m_info.key_frame_controls.interval + 1;
            core_enc->m_uIDRFrameInterval--;
            if (1 == core_enc->m_uIDRFrameInterval)
            {
                if (core_enc->m_iProfileIndex != 1 && core_enc->profile_frequency > 1) // if sequence PBBB was completed
                {   // no
                    ePictureType = PREDPIC;
                    core_enc->m_uIDRFrameInterval = 2;
                    core_enc->m_uIntraFrameInterval = 2;
                    core_enc->m_l1_cnt_to_start_B = 1;
                } else {
                    core_enc->m_bMakeNextFrameIDR = true;
                    core_enc->m_uIDRFrameInterval = core_enc->m_info.key_frame_controls.idr_interval + 1;
                    core_enc->m_iProfileIndex = 0;
                }
            } else {
                if (!core_enc->m_info.m_do_weak_forced_key_frames)
                {
                    core_enc->m_iProfileIndex = 0;
                }
            }
        }

        core_enc->m_bMakeNextFrameKey = false;
    }

    core_enc->m_iProfileIndex++;
    if (core_enc->m_iProfileIndex == core_enc->profile_frequency)
        core_enc->m_iProfileIndex = 0;

    return ePictureType;
}


/*************************************************************
*  Name:         EncodeDummyPicture
*  Description:  Writes out a blank picture to the bitstream in
case of buffer overflow.
************************************************************/
Status H264ENC_MAKE_NAME(H264CoreEncoder_EncodeDummyPicture)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264SliceType *curr_slice = &core_enc->m_Slices[core_enc->m_info.num_slices*core_enc->m_field_index];
    Ipp32s uMB;
    Ipp8u lastFlag = 0;
    Status status = UMC_OK;
    H264BsRealType* pBitstream = (H264BsRealType *)curr_slice->m_pbitstream;
    Ipp32s picSizeInMBs = core_enc->m_HeightInMBs * core_enc->m_WidthInMBs;

    H264ENC_MAKE_NAME(H264BsReal_Reset)(pBitstream);

    if (core_enc->m_PicType != PREDPIC && core_enc->m_PicType != BPREDPIC && core_enc->m_PicType != INTRAPIC)
    {
        status = UMC_ERR_FAILED;
        goto done;
    }

    // only 1 slice in dummy picture. Common slice header is used
    H264ENC_MAKE_NAME(H264BsReal_PutSliceHeader)(
        pBitstream,
        core_enc->m_SliceHeader,
        *core_enc->m_PicParamSet,
        *core_enc->m_SeqParamSet,
        core_enc->m_PicClass,
        curr_slice,
        core_enc->m_DecRefPicMarkingInfo[core_enc->m_field_index],
        core_enc->m_ReorderInfoL0[core_enc->m_field_index],
        core_enc->m_ReorderInfoL1[core_enc->m_field_index],
        &core_enc->m_svc_layer.svc_ext);

    if (core_enc->m_info.entropy_coding_mode) // CABAC
    {
        H264ENC_MAKE_NAME(H264BsReal_ResetBitStream_CABAC)(pBitstream);
        if (curr_slice->m_slice_type == BPREDSLICE)
        {
            H264ENC_MAKE_NAME(H264BsReal_InitializeContextVariablesInter_CABAC)(
                pBitstream,
                curr_slice->m_iLastXmittedQP,
                curr_slice->m_cabac_init_idc);
            for (uMB=0; uMB < picSizeInMBs; uMB++)
            {
                // mb_skip_flag = 1
                H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(
                    curr_slice->m_pbitstream,
                    ctxIdxOffset[MB_SKIP_FLAG_B],
                    1);
                // end_of_slice_flag
                lastFlag = (uMB == (picSizeInMBs - 1));
                H264ENC_MAKE_NAME(H264BsReal_EncodeFinalSingleBin_CABAC)(pBitstream, lastFlag);
            }
        }
        else if (curr_slice->m_slice_type == PREDSLICE)
        {
            H264ENC_MAKE_NAME(H264BsReal_InitializeContextVariablesInter_CABAC)(
                pBitstream,
                curr_slice->m_iLastXmittedQP,
                curr_slice->m_cabac_init_idc);
            for (uMB=0; uMB < picSizeInMBs; uMB++)
            {
                // mb_skip_flag = 1
                H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(
                    curr_slice->m_pbitstream,
                    ctxIdxOffset[MB_SKIP_FLAG_P_SP],
                    1);
                // end_of_slice_flag
                lastFlag = (uMB == (picSizeInMBs - 1));
                H264ENC_MAKE_NAME(H264BsReal_EncodeFinalSingleBin_CABAC)(pBitstream, lastFlag);
            }
        }
        else
        {
            H264ENC_MAKE_NAME(H264BsReal_InitializeContextVariablesIntra_CABAC)(
                pBitstream,
                curr_slice->m_iLastXmittedQP);
            for (uMB=0; uMB < picSizeInMBs; uMB++)
            {
                Ipp32s left_n, top_n, mb_type, ctx_inc;
                top_n = (uMB >= core_enc->m_WidthInMBs) ? MBTYPE_INTRA_16x16 : NUMBER_OF_MBTYPES; // MB pred type of top MB
                left_n = (uMB % core_enc->m_WidthInMBs) ? MBTYPE_INTRA_16x16 : NUMBER_OF_MBTYPES; // MB pred type of left MB
                mb_type = (top_n == MBTYPE_INTRA_16x16) ? 1 : 3; // mb_type: I_16x16_2_0_0 if MB is in the 1st row, I_16x16_0_0_0 otherwise

                // encode mb_type
                H264ENC_MAKE_NAME(H264BsReal_MBTypeInfo_CABAC)(
                    curr_slice->m_pbitstream,
                    curr_slice->m_slice_type,
                    mb_type,
                    MBTYPE_INTRA_16x16,
                    (MB_Type)left_n,
                    (MB_Type)top_n,
                    0, 0);

                // intra_chroma_pred_mode: DC
                if (core_enc->m_info.chroma_format_idc)
                    H264ENC_MAKE_NAME(H264BsReal_ChromaIntraPredMode_CABAC)(curr_slice->m_pbitstream, 0, 0, 0);

                //mb_qp_delta: 0
                H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(
                    curr_slice->m_pbitstream,
                    ctxIdxOffset[MB_QP_DELTA],
                    0);

                // coded_block_flag: 0
                ctx_inc = ((left_n == NUMBER_OF_MBTYPES) ? 1 : 0) + 2 * ((top_n == NUMBER_OF_MBTYPES) ? 1 : 0);
                H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(
                    curr_slice->m_pbitstream,
                    ctxIdxOffsetFrameCoded[CODED_BLOCK_FLAG] +
                        ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_LUMA_DC_LEVELS] +
                        ctx_inc,
                    0);

                // end_of_slice_flag
                lastFlag = (uMB == (picSizeInMBs - 1));
                H264ENC_MAKE_NAME(H264BsReal_EncodeFinalSingleBin_CABAC)(pBitstream, lastFlag);
            }
        }
        H264ENC_MAKE_NAME(H264BsReal_TerminateEncode_CABAC)(pBitstream);
    }
    else // CAVLC
    {
        if (curr_slice->m_slice_type == INTRASLICE)
        {
            for (uMB=0; uMB < picSizeInMBs; uMB++)
            {
                if (uMB < core_enc->m_WidthInMBs)
                    H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)(pBitstream, 3); // mb_type: I_16x16_2_0_0 - DC prediction
                else
                    H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)(pBitstream, 1); // mb_type: I_16x16_0_0_0 - vert prediction
                if (core_enc->m_info.chroma_format_idc)
                    H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)(pBitstream, 0); // intra_chroma_pred_mode: DC
                H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)(pBitstream, 0); // mb_qp_delta: 0
                H264ENC_MAKE_NAME(H264BsReal_PutBit)(pBitstream, 1); // coeff_token: 1
            }
            H264BsBase_WriteTrailingBits(curr_slice->m_pbitstream);
        }
        else
        {
            H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)(pBitstream, picSizeInMBs); // mb_skip_run == PicSizeInMbs
            H264BsBase_WriteTrailingBits(curr_slice->m_pbitstream);
        }
    }

    for (uMB = picSizeInMBs * core_enc->m_field_index; uMB < picSizeInMBs * (core_enc->m_field_index + 1); uMB ++)
        core_enc->m_pCurrentFrame->m_mbinfo.mbs[uMB].mbtype = MBTYPE_SKIPPED;
done:
    return status;
}

Status H264ENC_MAKE_NAME(H264CoreEncoder_Init)(
    void* state,
    BaseCodecParams* init,
    MemoryAllocator* pMemAlloc)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;

    // Check if SPS/PPS poiters were initialized externaly and allocate if not (AVC mode)
    if(core_enc->m_SeqParamSet==NULL) {
        core_enc->m_SeqParamSet = new H264SeqParamSet;
        if(core_enc->m_SeqParamSet==NULL) return UMC_ERR_ALLOC;

        core_enc->m_is_sps_alloc = true;
        memset(core_enc->m_SeqParamSet, 0, sizeof(H264SeqParamSet));
        core_enc->m_SeqParamSet->profile_idc = H264_MAIN_PROFILE;
        core_enc->m_SeqParamSet->chroma_format_idc = 1;
        core_enc->m_SeqParamSet->bit_depth_luma = 8;
        core_enc->m_SeqParamSet->bit_depth_chroma = 8;
        core_enc->m_SeqParamSet->bit_depth_aux = 8;
        core_enc->m_SeqParamSet->alpha_opaque_value = 8;
    }

    if( core_enc->m_PicParamSet==NULL ) {
        core_enc->m_PicParamSet = new H264PicParamSet;
        if( core_enc->m_PicParamSet==NULL ) return UMC_ERR_ALLOC;

        core_enc->m_is_pps_alloc = true;
        memset(core_enc->m_PicParamSet, 0, sizeof(H264PicParamSet));
    }

    H264EncoderParams *info = DynamicCast<H264EncoderParams, BaseCodecParams> (init);
    if(info == NULL) {
        VideoEncoderParams *VideoParams = DynamicCast<VideoEncoderParams, BaseCodecParams> (init);
        if(VideoParams == NULL) {
            return(UMC::UMC_ERR_INIT);
        }
        core_enc->m_info.info.clip_info.width       = VideoParams->info.clip_info.width;
        core_enc->m_info.info.clip_info.height      = VideoParams->info.clip_info.height;
        core_enc->m_info.info.bitrate         = VideoParams->info.bitrate;
        core_enc->m_info.info.framerate       = VideoParams->info.framerate;
        //core_enc->m_info.numFramesToEncode = VideoParams->numFramesToEncode;
        core_enc->m_info.numEncodedFrames  = VideoParams->numEncodedFrames;
        core_enc->m_info.numThreads      = VideoParams->numThreads;
        core_enc->m_info.qualityMeasure  = VideoParams->qualityMeasure;
        info = &core_enc->m_info;
    }else
        core_enc->m_info = *info;

    core_enc->m_FieldStruct = 0;
    Status status = H264ENC_MAKE_NAME(H264CoreEncoder_CheckEncoderParameters)(state);
    if (status != UMC_OK)
        return status;
    H264ENC_MAKE_NAME(H264CoreEncoder_SetSequenceParameters)(state);

    if (core_enc->m_SeqParamSet->frame_cropping_flag)
    {
        core_enc->m_info.frame_crop_x = core_enc->m_SeqParamSet->frame_crop_left_offset *
            UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc];
        core_enc->m_info.frame_crop_y = core_enc->m_SeqParamSet->frame_crop_top_offset *
            UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag);
        core_enc->m_info.frame_crop_w = core_enc->m_info.info.clip_info.width -
            core_enc->m_SeqParamSet->frame_crop_right_offset * UMC_H264_ENCODER::SubWidthC[core_enc->m_SeqParamSet->chroma_format_idc] -
            core_enc->m_info.frame_crop_x;
        core_enc->m_info.frame_crop_h = core_enc->m_info.info.clip_info.height -
            core_enc->m_SeqParamSet->frame_crop_bottom_offset * UMC_H264_ENCODER::SubHeightC[core_enc->m_SeqParamSet->chroma_format_idc]*(2 - core_enc->m_SeqParamSet->frame_mbs_only_flag) -
            core_enc->m_info.frame_crop_y;
    }

    //VUI parameters
    core_enc->m_SeqParamSet->vui_parameters_present_flag = 1;
    core_enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag = 1;
    core_enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc = 1;

    core_enc->m_SubME_Algo = core_enc->m_info.mv_subpel_search_method;
    core_enc->m_Analyse = ANALYSE_I_4x4 | ANALYSE_ME_SUBPEL | ANALYSE_CHECK_SKIP_INTPEL | ANALYSE_CHECK_SKIP_BESTCAND | ANALYSE_CHECK_SKIP_SUBPEL;
    if (core_enc->m_info.transform_8x8_mode_flag)
        core_enc->m_Analyse |= ANALYSE_I_8x8;
    core_enc->m_Analyse |= ANALYSE_FRAME_TYPE;
    //core_enc->m_Analyse |= ANALYSE_ME_CONTINUED_SEARCH;
    //core_enc->m_Analyse |= ANALYSE_FAST_INTRA; // QS <= 3
    //core_enc->m_Analyse |= ANALYSE_ME_SUBPEL_SAD; // QS <= 3
    //core_enc->m_Analyse |= ANALYSE_FLATNESS; // QS <= 1
    //core_enc->m_Analyse |= ANALYSE_INTRA_IN_ME; // QS <= 1
    //core_enc->m_Analyse |= ANALYSE_ME_FAST_MULTIREF; // QS <= 1
    //core_enc->m_Analyse |= ANALYSE_ME_EARLY_EXIT;
    //core_enc->m_Analyse |= ANALYSE_CBP_EMPTY;
    //core_enc->m_Analyse |= ANALYSE_SPLIT_SMALL_RANGE;
    //core_enc->m_Analyse |= ANALYSE_ME_EXT_CANDIDATES;
    core_enc->m_Analyse |= ANALYSE_RECODE_FRAME;
    if (core_enc->m_info.me_split_mode != 0)
        core_enc->m_Analyse |= ANALYSE_P_8x8 | ANALYSE_B_8x8;
    if (core_enc->m_info.me_split_mode > 1)
        core_enc->m_Analyse |= ANALYSE_P_4x4 | ANALYSE_B_4x4;
    if (core_enc->m_info.direct_pred_mode == 2)
        core_enc->m_Analyse |= ANALYSE_ME_AUTO_DIRECT;
    if (core_enc->m_info.m_QualitySpeed == 0)
        core_enc->m_Analyse |= ANALYSE_SAD;
    if (core_enc->m_info.m_QualitySpeed <= 1)
        core_enc->m_Analyse |= ANALYSE_FLATNESS | ANALYSE_INTRA_IN_ME | ANALYSE_ME_FAST_MULTIREF | ANALYSE_ME_PRESEARCH;
//    if (core_enc->m_info.m_QualitySpeed >= 2)
//        core_enc->m_Analyse |= ANALYSE_ME_CHROMA;
    if ((core_enc->m_info.m_QualitySpeed >= 2) && core_enc->m_info.entropy_coding_mode)
        core_enc->m_Analyse |= ANALYSE_RD_MODE;
    if ((core_enc->m_info.m_QualitySpeed >= 3) && core_enc->m_info.entropy_coding_mode)
        core_enc->m_Analyse |= ANALYSE_RD_OPT | ANALYSE_B_RD_OPT;
    if (core_enc->m_info.m_QualitySpeed <= 3)
        core_enc->m_Analyse |= ANALYSE_ME_SUBPEL_SAD | ANALYSE_FAST_INTRA;
    if (core_enc->m_info.m_QualitySpeed >= 4)
        core_enc->m_Analyse |= /*ANALYSE_B_RD_OPT |*/ ANALYSE_ME_BIDIR_REFINE;
    if (core_enc->m_info.m_QualitySpeed >= 5)
        core_enc->m_Analyse |= ANALYSE_ME_ALL_REF;
    if ((core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_RD_OPT))
        core_enc->m_Analyse |= ANALYSE_ME_CHROMA;
    if (core_enc->m_info.coding_type > 0 && core_enc->m_info.coding_type != 2) { // TODO: remove coding_type != 2 when MBAFF is implemented
        core_enc->m_Analyse &= ~ANALYSE_ME_AUTO_DIRECT;
        core_enc->m_info.direct_pred_mode = 1;
        //core_enc->m_Analyse &= ~ANALYSE_FRAME_TYPE;
    }

    // apply additional features and restrictions if provided
    core_enc->m_Analyse &= ~core_enc->m_info.m_Analyse_restrict;
    core_enc->m_Analyse |= core_enc->m_info.m_Analyse_on;
    core_enc->m_Analyse_ex = core_enc->m_info.m_Analyse_ex;

    core_enc->m_is_mvc_profile = core_enc->m_info.m_is_mvc_profile;
    core_enc->m_view_id = 0;
    //External memory allocator
    core_enc->memAlloc = pMemAlloc;
    core_enc->m_dpb.memAlloc = pMemAlloc;
    core_enc->m_cpb.memAlloc = pMemAlloc;

    Ipp32s numOfSliceEncs = core_enc->m_info.num_slices*((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1);
    H264ENC_CALL_NEW_ARR(status, H264Slice, core_enc->m_Slices, numOfSliceEncs);
    if (status != UMC_OK)
        return status;
    Ipp32s i;
    for (i = 0; i < numOfSliceEncs; i++) {
        status = H264ENC_MAKE_NAME(H264Slice_Init)(&core_enc->m_Slices[i], core_enc->m_info);
        if (status != UMC_OK)
            return status;
    }
    core_enc->profile_frequency = core_enc->m_info.B_frame_rate + 1;
    if (core_enc->eFrameSeq)
        H264_Free(core_enc->eFrameSeq);
    core_enc->eFrameSeq = (H264EncoderFrameType **)H264_Malloc((core_enc->profile_frequency + 1) * sizeof(H264EncoderFrameType *));
    for (i = 0; i <= core_enc->profile_frequency; i++)
        core_enc->eFrameSeq[i] = NULL;
    if (core_enc->eFrameType != NULL)
        H264_Free(core_enc->eFrameType);
    core_enc->eFrameType = (EnumPicCodType *)H264_Malloc(core_enc->profile_frequency * sizeof(EnumPicCodType));
    core_enc->eFrameType[0] = PREDPIC;
    for (i = 1; i < core_enc->profile_frequency; i++)
        core_enc->eFrameType[i] = BPREDPIC;

    // Set up for the 8 bit/default frame rate
    core_enc->m_uFrames_Num = 0;
    core_enc->m_uFrameCounter = 0;
    core_enc->m_SEIData.PictureTiming.cpb_removal_delay = 0;
    core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated = 0;
    core_enc->m_SEIData.PictureTiming.dpb_output_delay = 2;
    core_enc->m_SEIData.PictureTiming.DeltaTfiAccumulated = 0;
#ifdef H264_HRD_CPB_MODEL
    core_enc->m_SEIData.tai_n = 0;
    core_enc->m_SEIData.tai_n_minus_1 = 0;
    core_enc->m_SEIData.tai_earliest = 0;
    core_enc->m_SEIData.taf_n = 0;
    core_enc->m_SEIData.taf_n_minus_1 = 0;
    core_enc->m_SEIData.trn_n = 0;
    core_enc->m_SEIData.trn_nb = 0;
    core_enc->m_SEIData.m_tickDuration = 0;
#endif // H264_HRD_CPB_MODEL
    memset(core_enc->m_SEIData.m_insertedSEI,0, 36 * sizeof(Ipp8u));
    core_enc->m_PaddedSize.width  = (core_enc->m_info.info.clip_info.width  + 15) & ~15;
    core_enc->m_PaddedSize.height = (core_enc->m_info.info.clip_info.height + (16<<(1 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1) & ~((16<<(1 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1);
    core_enc->m_WidthInMBs = core_enc->m_PaddedSize.width >> 4;
    core_enc->m_HeightInMBs  = core_enc->m_PaddedSize.height >> 4;
    core_enc->m_Pitch = CalcPitchFromWidth(core_enc->m_PaddedSize.width, sizeof(PIXTYPE)) / sizeof(PIXTYPE);
    core_enc->m_bMakeNextFrameKey = true; // Ensure that we always start with a key frame.
    core_enc->m_bMakeNextFrameIDR = false;
    if (H264_KFCM_INTERVAL == core_enc->m_info.key_frame_controls.method) {
        core_enc->m_uIntraFrameInterval = core_enc->m_info.key_frame_controls.interval + 1;
        core_enc->m_uIDRFrameInterval = core_enc->m_info.key_frame_controls.idr_interval + 1;
    } else {
        core_enc->m_uIDRFrameInterval = core_enc->m_uIntraFrameInterval = 0;
    }
    memset(&core_enc->m_DecRefPicMarkingInfo,0,sizeof(core_enc->m_DecRefPicMarkingInfo));
    memset(&core_enc->m_ReorderInfoL0,0,sizeof(core_enc->m_ReorderInfoL0));
    memset(&core_enc->m_ReorderInfoL1,0,sizeof(core_enc->m_ReorderInfoL1));
    memset(core_enc->m_LTFrameIdxUseFlags, 0, 16);

    core_enc->m_NumShortTermRefFrames = 0;
    core_enc->m_NumLongTermRefFrames = 0;
    if ((core_enc->m_EmptyThreshold = (Ipp32u*)H264_Malloc(52*sizeof(Ipp32u))) == NULL)
        return UMC_ERR_ALLOC;
    if ((core_enc->m_DirectBSkipMEThres = (Ipp32u*)H264_Malloc(52*sizeof(Ipp32u))) == NULL)
        return UMC_ERR_ALLOC;
    if ((core_enc->m_PSkipMEThres = (Ipp32u*)H264_Malloc(52*sizeof(Ipp32u))) == NULL)
        return UMC_ERR_ALLOC;
    if ((core_enc->m_BestOf5EarlyExitThres = (Ipp32s*)H264_Malloc(52*sizeof(Ipp32s))) == NULL)
        return UMC_ERR_ALLOC;
    if (core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag) {
        ippsZero_8u((Ipp8u*)core_enc->m_EmptyThreshold, 52 * sizeof(*core_enc->m_EmptyThreshold));
        ippsZero_8u((Ipp8u*)core_enc->m_DirectBSkipMEThres, 52 * sizeof(*core_enc->m_DirectBSkipMEThres));
        ippsZero_8u((Ipp8u*)core_enc->m_PSkipMEThres, 52 * sizeof(*core_enc->m_PSkipMEThres));
        ippsZero_8u((Ipp8u*)core_enc->m_BestOf5EarlyExitThres, 52 * sizeof(*core_enc->m_BestOf5EarlyExitThres));
    } else {
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::EmptyThreshold, (Ipp8u*)core_enc->m_EmptyThreshold, 52 * sizeof(*core_enc->m_EmptyThreshold));
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::DirectBSkipMEThres, (Ipp8u*)core_enc->m_DirectBSkipMEThres, 52 * sizeof(*core_enc->m_DirectBSkipMEThres));
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::PSkipMEThres, (Ipp8u*)core_enc->m_PSkipMEThres, 52 * sizeof(*core_enc->m_PSkipMEThres));
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::BestOf5EarlyExitThres, (Ipp8u*)core_enc->m_BestOf5EarlyExitThres, 52 * sizeof(*core_enc->m_BestOf5EarlyExitThres));
    }

    Ipp32u bsSize = core_enc->m_PaddedSize.width * core_enc->m_PaddedSize.height * sizeof(PIXTYPE) * 2;
    bsSize += (bsSize >> 1) + 4096;
    // TBD: see if buffer size can be reduced
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    if (core_enc->m_info.entropy_coding_mode == 0)
        bsSize += bsSize;
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

//#ifndef ALT_BITSTREAM_ALLOC //TRY
    core_enc->m_pAllocEncoderInst = (Ipp8u*)H264_Malloc(numOfSliceEncs * bsSize + DATA_ALIGN);
    if (core_enc->m_pAllocEncoderInst == NULL)
        return UMC_ERR_ALLOC;
    core_enc->m_pBitStream = align_pointer<Ipp8u*>(core_enc->m_pAllocEncoderInst, DATA_ALIGN);
//#endif // ALT_BITSTREAM_ALLOC //TRY

    core_enc->m_pbitstreams = (H264BsRealType**)H264_Malloc(numOfSliceEncs * sizeof(H264BsRealType*));
    if (core_enc->m_pbitstreams == NULL)
        return UMC_ERR_ALLOC;

#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    if (core_enc->m_info.entropy_coding_mode == 0)
        bsSize >>= 1;
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

    for (i = 0; i < numOfSliceEncs; i++) {
        core_enc->m_pbitstreams[i] = (H264BsRealType*)H264_Malloc(sizeof(H264BsRealType));
        if (!core_enc->m_pbitstreams[i])
            return UMC_ERR_ALLOC;
/*
#ifdef ALT_BITSTREAM_ALLOC //TRY
        Ipp8u * tmpBitstreamPtr = (Ipp8u*)H264_Malloc(bsSize);
        H264ENC_MAKE_NAME(H264BsReal_Create)(core_enc->m_pbitstreams[i], tmpBitstreamPtr, bsSize, core_enc->m_info.chroma_format_idc, status);
#else // ALT_BITSTREAM_ALLOC
        */
        H264ENC_MAKE_NAME(H264BsReal_Create)(core_enc->m_pbitstreams[i], core_enc->m_pBitStream + i * bsSize, bsSize, core_enc->m_info.chroma_format_idc, status);
//#endif // ALT_BITSTREAM_ALLOC //TRY
        if (status != UMC_OK)
            return status;
        core_enc->m_Slices[i].m_pbitstream = (H264BsBase*)core_enc->m_pbitstreams[i];
    }
    core_enc->m_bs1 = core_enc->m_pbitstreams[0]; // core_enc->m_bs1 is the main stream.

    Ipp32s nMBCount = core_enc->m_WidthInMBs * core_enc->m_HeightInMBs * ((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1);
#ifdef DEBLOCK_THREADING
    core_enc->mbs_deblock_ready = (Ipp8u*)H264_Malloc(sizeof(Ipp8u) * nMBCount);
    memset((void*)core_enc->mbs_deblock_ready, 0, sizeof( Ipp8u )*nMBCount);
#endif
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    // TODO: do we need them if no MBT?
    H264ENC_CALL_NEW_ARR(status, H264Slice, core_enc->m_Slices_MBT, core_enc->m_info.numThreads);
    if (status != UMC_OK)
        return status;
    for (i = 0; i < core_enc->m_info.numThreads; i ++) {
        status = H264ENC_MAKE_NAME(H264Slice_Init)(&core_enc->m_Slices_MBT[i], core_enc->m_info);
        if (status != UMC_OK)
            return status;
        core_enc->m_Slices_MBT[i].m_pbitstream = (H264BsBase*)H264_Malloc(sizeof(H264BsRealType));
        if (!core_enc->m_Slices_MBT[i].m_pbitstream)
            return UMC_ERR_ALLOC;
        if(core_enc->m_info.entropy_coding_mode == 0)
            status = H264ENC_MAKE_NAME(H264BsReal_Create)((H264BsRealType*)core_enc->m_Slices_MBT[i].m_pbitstream, core_enc->m_pBitStream + numOfSliceEncs * bsSize + i * (bsSize / core_enc->m_info.numThreads), bsSize / core_enc->m_info.numThreads, core_enc->m_info.chroma_format_idc, status);
        else
            status = H264ENC_MAKE_NAME(H264BsReal_Create)((H264BsRealType*)core_enc->m_Slices_MBT[i].m_pbitstream, 0, 0, core_enc->m_info.chroma_format_idc, status); // the buffer itself won't be used
        if (status != UMC_OK)
            return status;
    }
    core_enc->mbReady_MBT = (Ipp32s*)H264_Malloc(sizeof(Ipp32s) * core_enc->m_HeightInMBs * ((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1));
    if (!core_enc->mbReady_MBT)
        return UMC_ERR_ALLOC;
    if (core_enc->m_info.entropy_coding_mode == 0) {
        core_enc->lSR_MBT = (Ipp32s*)H264_Malloc(sizeof(Ipp32s) * core_enc->m_HeightInMBs * ((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1));
        if (!core_enc->lSR_MBT)
            return UMC_ERR_ALLOC;
    } else {
        core_enc->gd_MBT = (H264ENC_MAKE_NAME(GlobalData_MBT)*)H264_Malloc(sizeof(H264ENC_MAKE_NAME(GlobalData_MBT)) * nMBCount);
        if (!core_enc->gd_MBT)
            return UMC_ERR_ALLOC;
    }

    vm_mutex_set_invalid(&core_enc->mutexIncRow);
    vm_mutex_init(&core_enc->mutexIncRow);
#ifdef H264_RECODE_PCM
    core_enc->m_mbPCM = (Ipp32s*)H264_Malloc(core_enc->m_HeightInMBs * core_enc->m_WidthInMBs * 2 * sizeof(Ipp32s));
    if (!core_enc->m_mbPCM)
        return UMC_ERR_ALLOC;
#endif //H264_RECODE_PCM
#ifdef MB_THREADING_VM
    core_enc->m_ThreadVM_MBT = (vm_thread*)H264_Malloc(sizeof(vm_thread) * core_enc->m_info.numThreads);
    if (!core_enc->m_ThreadVM_MBT)
        return UMC_ERR_ALLOC;
    core_enc->m_ThreadDataVM = (H264ENC_MAKE_NAME(ThreadDataVM_MBT)*)H264_Malloc(sizeof(H264ENC_MAKE_NAME(ThreadDataVM_MBT)) * core_enc->m_info.numThreads);
    if (!core_enc->m_ThreadDataVM)
        return UMC_ERR_ALLOC;
#endif // MB_THREADING_VM
#ifdef MB_THREADING_TW
#ifdef MB_THREADING_VM
    core_enc->mMutexMT = (vm_mutex*)H264_Malloc(sizeof(vm_mutex) * nMBCount);
    if (!core_enc->mMutexMT)
        return UMC_ERR_ALLOC;
    for (i = 0; i < nMBCount; i ++) {
        vm_mutex_set_invalid(core_enc->mMutexMT + i);
        vm_mutex_init(core_enc->mMutexMT + i);
    }
#else // MB_THREADING_VM
#ifdef _OPENMP
    core_enc->mLockMT = (omp_lock_t*)H264_Malloc(sizeof(omp_lock_t) * nMBCount);
    if (!core_enc->mLockMT)
        return UMC_ERR_ALLOC;
    for (i = 0; i < nMBCount; i ++)
        omp_init_lock(core_enc->mLockMT + i);
#endif // _OPENMP
#endif // MB_THREADING_VM
#endif // MB_THREADING_TW
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

#ifdef SLICE_THREADING_LOAD_BALANCING
    if (core_enc->slice_load_balancing_enabled)
    {
        // Load balancing for slice level multithreading
        core_enc->m_B_ticks_per_macroblock = (Ipp64u*)H264_Malloc(sizeof(Ipp64s) * nMBCount);
        if (!core_enc->m_B_ticks_per_macroblock)
            return UMC_ERR_ALLOC;
        core_enc->m_P_ticks_per_macroblock = (Ipp64u*)H264_Malloc(sizeof(Ipp64s) * nMBCount);
        if (!core_enc->m_P_ticks_per_macroblock)
            return UMC_ERR_ALLOC;
        core_enc->m_B_ticks_data_available = 0;
        core_enc->m_P_ticks_data_available = 0;
    }
#endif // SLICE_THREADING_LOAD_BALANCING

    Ipp32s len = (sizeof(H264MacroblockMVs) +
                  sizeof(H264MacroblockMVs) +
                  sizeof(H264MacroblockCoeffsInfo) +
                  sizeof(H264MacroblockLocalInfo) +
                  sizeof(H264MacroblockIntraTypes) +
                  sizeof(T_EncodeMBOffsets)
                 ) * nMBCount + ALIGN_VALUE * 6;
    len += 3*COEFFICIENTS_BUFFER_SIZE*nMBCount*sizeof(COEFFSTYPE) + ALIGN_VALUE * 3;
    len += nMBCount*256*sizeof(COEFFSTYPE)*3/2 + ALIGN_VALUE * 3; //420 only
    len += nMBCount*256*sizeof(COEFFSTYPE)*3/2 + ALIGN_VALUE * 3; //420 only
    len += nMBCount*256*sizeof(COEFFSTYPE)*3/2 + ALIGN_VALUE * 3; //420 only
    core_enc->m_pParsedDataNew = (Ipp8u*)H264_Malloc(len);
    if (NULL == core_enc->m_pParsedDataNew)
        return UMC_ERR_ALLOC;
    core_enc->m_mbinfo.MVDeltas[0] = align_pointer<H264MacroblockMVs *> (core_enc->m_pParsedDataNew, ALIGN_VALUE);
    core_enc->m_mbinfo.MVDeltas[1] = align_pointer<H264MacroblockMVs *> (core_enc->m_mbinfo.MVDeltas[0] + nMBCount, ALIGN_VALUE);
    core_enc->m_mbinfo.MacroblockCoeffsInfo = align_pointer<H264MacroblockCoeffsInfo *> (core_enc->m_mbinfo.MVDeltas[1] + nMBCount, ALIGN_VALUE);
    core_enc->m_mbinfo.mbs = align_pointer<H264MacroblockLocalInfo *> (core_enc->m_mbinfo.MacroblockCoeffsInfo + nMBCount, ALIGN_VALUE);
    core_enc->m_mbinfo.intra_types = align_pointer<H264MacroblockIntraTypes *> (core_enc->m_mbinfo.mbs + nMBCount, ALIGN_VALUE);
    core_enc->m_pMBOffsets = align_pointer<T_EncodeMBOffsets*> (core_enc->m_mbinfo.intra_types + nMBCount, ALIGN_VALUE);

    core_enc->m_SavedMacroblockCoeffs = align_pointer<COEFFSTYPE *> (core_enc->m_pMBOffsets + nMBCount, ALIGN_VALUE);
    core_enc->m_SavedMacroblockTCoeffs = align_pointer<COEFFSTYPE *> (core_enc->m_SavedMacroblockCoeffs + COEFFICIENTS_BUFFER_SIZE*nMBCount, ALIGN_VALUE);
    core_enc->m_ReservedMacroblockTCoeffs = align_pointer<COEFFSTYPE *> (core_enc->m_SavedMacroblockTCoeffs + COEFFICIENTS_BUFFER_SIZE*nMBCount, ALIGN_VALUE);

    core_enc->m_pYResidual = align_pointer<COEFFSTYPE *> (core_enc->m_ReservedMacroblockTCoeffs + COEFFICIENTS_BUFFER_SIZE*nMBCount, ALIGN_VALUE);
    core_enc->m_pUResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pYResidual + 256*nMBCount, ALIGN_VALUE);
    core_enc->m_pVResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pUResidual + 64*nMBCount, ALIGN_VALUE);

    core_enc->m_pYInputResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pVResidual + 64*nMBCount, ALIGN_VALUE);
    core_enc->m_pUInputResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pYInputResidual + 256*nMBCount, ALIGN_VALUE);
    core_enc->m_pVInputResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pUInputResidual + 64*nMBCount, ALIGN_VALUE);

    core_enc->m_pYInputBaseResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pVInputResidual + 64*nMBCount, ALIGN_VALUE);
    core_enc->m_pUInputBaseResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pYInputBaseResidual + 256*nMBCount, ALIGN_VALUE);
    core_enc->m_pVInputBaseResidual = align_pointer<COEFFSTYPE *> (core_enc->m_pUInputBaseResidual + 64*nMBCount, ALIGN_VALUE);

    if (core_enc->m_svc_layer.svc_ext.dependency_id == 0 && core_enc->QualityNum > 1) {
        Ipp32u len = (sizeof(H264MacroblockMVs) + sizeof(H264MacroblockMVs) + sizeof(H264MacroblockRefIdxs) + sizeof(H264MacroblockRefIdxs) + sizeof(H264MacroblockGlobalInfo)) * nMBCount + ALIGN_VALUE * 5;
        core_enc->m_pMbsData = (Ipp8u*)H264_Malloc(len);
        ippsZero_8u(core_enc->m_pMbsData, len);
        // set pointer(s)
        core_enc->m_mbinfo_saved.MV[0] = align_pointer<H264MacroblockMVs *> (core_enc->m_pMbsData, ALIGN_VALUE);
        core_enc->m_mbinfo_saved.MV[1] = align_pointer<H264MacroblockMVs *> (core_enc->m_mbinfo_saved.MV[0]+ nMBCount, ALIGN_VALUE);
        core_enc->m_mbinfo_saved.RefIdxs[0] = align_pointer<H264MacroblockRefIdxs *> (core_enc->m_mbinfo_saved.MV[1] + nMBCount, ALIGN_VALUE);
        core_enc->m_mbinfo_saved.RefIdxs[1] = align_pointer<H264MacroblockRefIdxs *> (core_enc->m_mbinfo_saved.RefIdxs[0] + nMBCount, ALIGN_VALUE);
        core_enc->m_mbinfo_saved.mbs = align_pointer<H264MacroblockGlobalInfo *> (core_enc->m_mbinfo_saved.RefIdxs[1] + nMBCount, ALIGN_VALUE);
    }

    // Block offset -- initialize table, indexed by current block (0-23)
    // with the value to add to the offset of the block into the plane
    // to advance to the next block
    for (i = 0; i < 16; i++) {
        // 4 Cases to cover:
        if (!(i & 1)) {   // Even # blocks, next block to the right
            core_enc->m_EncBlockOffsetInc[0][i] = 4;
            core_enc->m_EncBlockOffsetInc[1][i] = 4;
        } else if (!(i & 2)) {  // (1,5,9 & 13), down and one to the left
            core_enc->m_EncBlockOffsetInc[0][i] = (core_enc->m_Pitch<<2) - 4;
            core_enc->m_EncBlockOffsetInc[1][i] = (core_enc->m_Pitch<<3) - 4;
        } else if (i == 7) { // beginning of next row
            core_enc->m_EncBlockOffsetInc[0][i] = (core_enc->m_Pitch<<2) - 12;
            core_enc->m_EncBlockOffsetInc[1][i] = (core_enc->m_Pitch<<3) - 12;
        } else { // (3 & 11) up and one to the right
            core_enc->m_EncBlockOffsetInc[0][i] = -(Ipp32s)((core_enc->m_Pitch<<2) - 4);
            core_enc->m_EncBlockOffsetInc[1][i] = -(Ipp32s)((core_enc->m_Pitch<<3) - 4);
        }
    }
    Ipp32s last_block = 16+(4<<core_enc->m_info.chroma_format_idc) - 1; // - last increment
    core_enc->m_EncBlockOffsetInc[0][last_block] = 0;
    core_enc->m_EncBlockOffsetInc[1][last_block] = 0;
    switch (core_enc->m_info.chroma_format_idc) {
        case 1:
        case 2:
            for (i = 16; i < last_block; i++){
                if (i & 1) {
                    core_enc->m_EncBlockOffsetInc[0][i] = (core_enc->m_Pitch<<2) - 4;
                    core_enc->m_EncBlockOffsetInc[1][i] = (core_enc->m_Pitch<<3) - 4;
                } else {
                    core_enc->m_EncBlockOffsetInc[0][i] = 4;
                    core_enc->m_EncBlockOffsetInc[1][i] = 4;
                }
            }
            break;
        case 3:
            //Copy from luma
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[0][16], &core_enc->m_EncBlockOffsetInc[0][0], 16*sizeof(Ipp32s));
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[0][32], &core_enc->m_EncBlockOffsetInc[0][0], 16*sizeof(Ipp32s));
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[1][16], &core_enc->m_EncBlockOffsetInc[1][0], 16*sizeof(Ipp32s));
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[1][32], &core_enc->m_EncBlockOffsetInc[1][0], 16*sizeof(Ipp32s));
            break;
    }
    core_enc->m_InitialOffsets[0][0] = core_enc->m_InitialOffsets[1][1] = 0;
    core_enc->m_InitialOffsets[0][1] = (Ipp32s)core_enc->m_Pitch;
    core_enc->m_InitialOffsets[1][0] = -(Ipp32s)core_enc->m_Pitch;

    Ipp32s bitDepth = IPP_MAX(core_enc->m_info.bit_depth_aux, IPP_MAX(core_enc->m_info.bit_depth_chroma, core_enc->m_info.bit_depth_luma));
    Ipp32s chromaSampling = core_enc->m_info.chroma_format_idc;
    Ipp32s alpha = core_enc->m_info.aux_format_idc ? 1 : 0;

    core_enc->m_iProfileIndex = 0;
    core_enc->cflags = core_enc->cnotes = 0;
    core_enc->m_pLastFrame = NULL; //Init pointer to last frame

    Ipp32s fs = core_enc->m_info.info.clip_info.width * core_enc->m_info.info.clip_info.height;
    core_enc->m_info.m_SuggestedOutputSize = fs;
    if (alpha)
        core_enc->m_info.m_SuggestedOutputSize += fs;
    if (chromaSampling == 1)
        core_enc->m_info.m_SuggestedOutputSize += fs / 2;
    else if (chromaSampling == 2)
        core_enc->m_info.m_SuggestedOutputSize += fs;
    else if (chromaSampling == 3)
        core_enc->m_info.m_SuggestedOutputSize += fs * 2;
    core_enc->m_info.m_SuggestedOutputSize = core_enc->m_info.m_SuggestedOutputSize * bitDepth / 8;
#ifdef FRAME_QP_FROM_FILE
        FILE* f;
        int qp,fn=0;
        char ft;

        if( (f = fopen(FRAME_QP_FROM_FILE, "r")) == NULL){
            fprintf(stderr,"Can't open file %s\n", FRAME_QP_FROM_FILE);
            exit(-1);
        }
        while(fscanf(f,"%c %d\n",&ft,&qp) == 2){
            frame_type.push_back(ft);
            frame_qp.push_back(qp);
            fn++;
        }
        fclose(f);
        //fprintf(stderr,"frames read = %d\n",fn);
#endif

    core_enc->svc_ResizeMap[0] = 0;
    //core_enc->rateDivider = 1; // is already set outside
#ifdef USE_TEMPTEMP
    core_enc->refctrl = 0;
    core_enc->temptempstart = 0;
    core_enc->temptemplen = 0;
    core_enc->usetemptemp = 0;
#endif // USE_TEMPTEMP
    core_enc->m_tmpBuf0 = 0;
    core_enc->m_tmpBuf1 = 0;
    core_enc->m_tmpBuf2 = 0;

    return UMC_OK;
}


VideoData* H264ENC_MAKE_NAME(H264CoreEncoder_GetReconstructedFrame)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s makeNULL = 0;
   //Deblock for non reference B frames
   if( core_enc->m_pCurrentFrame->m_PicCodType == BPREDPIC && !core_enc->m_pCurrentFrame->m_RefPic ){
    if( core_enc->m_Analyse & ANALYSE_RECODE_FRAME && core_enc->m_pReconstructFrame == NULL){
        core_enc->m_pReconstructFrame = core_enc->m_pCurrentFrame;
        makeNULL = 1;
    }
    Ipp32s field_index, slice;
    for (field_index=0;field_index<=(Ipp8u)(core_enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE);field_index++)
        for (slice = (Ipp32s)core_enc->m_info.num_slices*field_index; slice < core_enc->m_info.num_slices*(field_index+1); slice++)
            H264ENC_MAKE_NAME(H264CoreEncoder_DeblockSlice)(
                state,
                core_enc->m_Slices + slice,
                core_enc->m_Slices[slice].m_first_mb_in_slice + core_enc->m_WidthInMBs * core_enc->m_HeightInMBs * field_index,
                core_enc->m_Slices[slice].m_MB_Counter );
    }
    if( core_enc->m_Analyse & ANALYSE_RECODE_FRAME && makeNULL ){
        core_enc->m_pReconstructFrame = NULL;
    }

    return &core_enc->m_pCurrentFrame->m_data;
}

// Get codec working (initialization) parameter(s)
Status H264ENC_MAKE_NAME(H264CoreEncoder_GetInfo)(
    void* state,
    BaseCodecParams *info)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    H264EncoderParams* pInfo = DynamicCast<H264EncoderParams, BaseCodecParams>(info);

    core_enc->m_info.qualityMeasure = -1;
    core_enc->m_info.info.stream_type = H264_VIDEO;

    if(pInfo)
    {
        *pInfo = core_enc->m_info;
    } else if(info)
    {
        VideoEncoderParams* pVInfo = DynamicCast<VideoEncoderParams, BaseCodecParams>(info);
        if(pVInfo) {
            *pVInfo = core_enc->m_info;
        } else {
            return UMC_ERR_INVALID_STREAM;
        }
    } else {
        return UMC_ERR_NULL_PTR;
    }


    return UMC_OK;
}

const H264SeqParamSet* H264ENC_MAKE_NAME(H264CoreEncoder_GetSeqParamSet)(void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    return core_enc->m_SeqParamSet;
}

const H264PicParamSet* H264ENC_MAKE_NAME(H264CoreEncoder_GetPicParamSet)(void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    return core_enc->m_PicParamSet;
}

Status H264ENC_MAKE_NAME(H264CoreEncoder_Close)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;

    if (core_enc->m_EmptyThreshold) {
        H264_Free(core_enc->m_EmptyThreshold);
        core_enc->m_EmptyThreshold = NULL;
    }
    if(core_enc->m_DirectBSkipMEThres) {
        H264_Free(core_enc->m_DirectBSkipMEThres);
        core_enc->m_DirectBSkipMEThres = NULL;
    }
    if(core_enc->m_PSkipMEThres) {
        H264_Free(core_enc->m_PSkipMEThres);
        core_enc->m_PSkipMEThres = NULL;
    }
    if(core_enc->m_BestOf5EarlyExitThres) {
        H264_Free(core_enc->m_BestOf5EarlyExitThres);
        core_enc->m_BestOf5EarlyExitThres = NULL;
    }
    if (core_enc->m_pbitstreams) {
        Ipp32s i;
        for (i = 0; i < core_enc->m_info.num_slices*((core_enc->m_info.coding_type == 1 || core_enc->m_info.coding_type == 3) + 1); i++) {
            if (core_enc->m_pbitstreams[i]) {
                H264ENC_MAKE_NAME(H264BsReal_Destroy)(core_enc->m_pbitstreams[i]);
                H264_Free(core_enc->m_pbitstreams[i]);
                core_enc->m_pbitstreams[i] = NULL;
            }
        }
        H264_Free(core_enc->m_pbitstreams);
        core_enc->m_pbitstreams = NULL;
    }

#ifndef ALT_BITSTREAM_ALLOC
    if (core_enc->m_pAllocEncoderInst) {
        H264_Free(core_enc->m_pAllocEncoderInst);
        core_enc->m_pAllocEncoderInst = NULL;
    }
#endif // ALT_BITSTREAM_ALLOC
    if( core_enc->eFrameType != NULL ){
        H264_Free(core_enc->eFrameType);
        core_enc->eFrameType = NULL;
    }
    if( core_enc->eFrameSeq != NULL ){
        H264_Free(core_enc->eFrameSeq);
        core_enc->eFrameSeq = NULL;
    }
    core_enc->m_pReconstructFrame = NULL;
    if (core_enc->m_pParsedDataNew) {
        H264_Free(core_enc->m_pParsedDataNew);
        core_enc->m_pParsedDataNew = NULL;
    }
    if (core_enc->m_pMbsData) {
        H264_Free(core_enc->m_pMbsData);
        core_enc->m_pMbsData = NULL;
    }
    if (core_enc->m_Slices != NULL) {
        H264ENC_CALL_DELETE_ARR(H264Slice, core_enc->m_Slices);
        core_enc->m_Slices = NULL;
    }
#ifdef DEBLOCK_THREADING
    H264_Free((void*)core_enc->mbs_deblock_ready);
    core_enc->mbs_deblock_ready = NULL;
#endif
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    if (core_enc->m_Slices_MBT != NULL) {
        Ipp32s i;
        for (i = 0; i < core_enc->m_info.numThreads; i ++) {
            H264ENC_MAKE_NAME(H264BsReal_Destroy)((H264BsRealType *)core_enc->m_Slices_MBT[i].m_pbitstream);
            H264_Free(core_enc->m_Slices_MBT[i].m_pbitstream);
            core_enc->m_Slices_MBT[i].m_pbitstream = NULL;
        }
        H264ENC_CALL_DELETE_ARR(H264Slice, core_enc->m_Slices_MBT);
        core_enc->m_Slices_MBT = NULL;
    }
    H264_Free((void*)core_enc->mbReady_MBT);
    core_enc->mbReady_MBT = NULL;
    if (core_enc->m_info.entropy_coding_mode == 0) {
        H264_Free((void*)core_enc->lSR_MBT);
        core_enc->lSR_MBT = NULL;
    } else {
        H264_Free((void*)core_enc->gd_MBT);
        core_enc->gd_MBT = NULL;
    }
#ifdef H264_RECODE_PCM
    H264_Free((void*)core_enc->m_mbPCM);
    core_enc->m_mbPCM = NULL;
#endif // H264_RECODE_PCM
#ifdef MB_THREADING_VM
    H264_Free((void*)core_enc->m_ThreadVM_MBT);
    core_enc->m_ThreadVM_MBT = NULL;
    H264_Free((void*)core_enc->m_ThreadDataVM);
    core_enc->m_ThreadDataVM = NULL;
    vm_mutex_destroy(&core_enc->mutexIncRow);
#endif // MB_THREADING_VM
#ifdef MB_THREADING_TW
#ifdef MB_THREADING_VM
    if (core_enc->mMutexMT)
    {
        for (int i = 0; i < core_enc->m_HeightInMBs * core_enc->m_WidthInMBs; i ++)
            vm_mutex_destroy(core_enc->mMutexMT + i);
        H264_Free((void*)core_enc->mMutexMT);
        core_enc->mMutexMT = NULL;
    }
#else // MB_THREADING_VM
#ifdef _OPENMP
    if (core_enc->mLockMT)
    {
        for (int i = 0; i < core_enc->m_HeightInMBs * core_enc->m_WidthInMBs; i ++)
            omp_destroy_lock(core_enc->mLockMT + i);
        H264_Free((void*)core_enc->mLockMT);
        core_enc->mLockMT = NULL;
    }
#endif // _OPENMP
#endif // MB_THREADING_TW
#endif // MB_THREADING_VM
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

#ifdef SLICE_THREADING_LOAD_BALANCING
    if (core_enc->slice_load_balancing_enabled)
    {
        // Load balancing for slice level multithreading
        if (core_enc->m_B_ticks_per_macroblock)
        {
            H264_Free(core_enc->m_B_ticks_per_macroblock);
            core_enc->m_B_ticks_per_macroblock = NULL;
        }
        if (core_enc->m_P_ticks_per_macroblock)
        {
            H264_Free(core_enc->m_P_ticks_per_macroblock);
            core_enc->m_P_ticks_per_macroblock = NULL;
        }
    }
#endif // SLICE_THREADING_LOAD_BALANCING
    return UMC_OK;
}

Status H264ENC_MAKE_NAME(H264CoreEncoder_Reset)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32s i;

    if (core_enc->m_info.reset_encoding_pipeline == true) {
        core_enc->m_is_mb_data_initialized = false;
        core_enc->m_PicOrderCnt_LastIDR = 0;
        core_enc->m_l1_cnt_to_start_B = 0;
        core_enc->m_total_bits_encoded = 0;
        core_enc->use_implicit_weighted_bipred = false;
        core_enc->m_is_cur_pic_afrm = false; // ? frame level
        core_enc->m_DirectTypeStat[0]=core_enc->m_DirectTypeStat[1]=0;

            //Clear dpb and cpb
        //H264ENC_MAKE_NAME(H264EncoderFrameList_clearFrameList)(&core_enc->m_cpb);
        //H264ENC_MAKE_NAME(H264EncoderFrameList_clearFrameList)(&core_enc->m_dpb);

        // move all reference and buffered frames from dpb to cpb
        H264EncoderFrameType *pFrm = core_enc->m_dpb.m_pHead;

        while (pFrm != NULL) {
            H264EncoderFrameType *pNext = pFrm->m_pFutureFrame;
            H264ENC_MAKE_NAME(H264EncoderFrameList_RemoveFrame)(&core_enc->m_dpb, pFrm);
            H264ENC_MAKE_NAME(H264EncoderFrameList_insertAtCurrent)(&core_enc->m_cpb, pFrm);
            pFrm->m_isShortTermRef[0] = pFrm->m_isShortTermRef[1] = false;
            pFrm->m_isLongTermRef[0] = pFrm->m_isLongTermRef[1] = false;
            pFrm = pNext;
        }

        pFrm = core_enc->m_cpb.m_pHead;

        // prepare cpb for re-use after Reset
        while (pFrm != NULL) {
            pFrm->m_wasEncoded = true;
            if (core_enc->m_info.is_resol_changed) { // set new resolution
                UMC::ColorFormat cf = NV12; // TODO: support other color formats
                pFrm->m_data.SetImageSize(core_enc->m_info.info.clip_info.width, core_enc->m_info.info.clip_info.height);
                pFrm->m_data.SetColorFormat(cf);
                pFrm->m_data.SetPlanePitch(pFrm->m_pitchBytes, 0); // preserve pitch
                pFrm->m_data.SetPlanePitch(pFrm->m_pitchBytes, 1); // preserve pitch
            }
            pFrm = pFrm->m_pFutureFrame;
        }
    }

    core_enc->m_FieldStruct = 0;
    bool use_mb_threading = core_enc->useMBT;
#ifdef SLICE_THREADING_LOAD_BALANCING
    bool use_slice_load_balancing_enabled = core_enc->slice_load_balancing_enabled;
#endif // SLICE_THREADING_LOAD_BALANCING

    Status status = H264ENC_MAKE_NAME(H264CoreEncoder_CheckEncoderParameters)(state);
    if (status != UMC_OK)
        return status;
    if (!use_mb_threading && core_enc->useMBT)
    {
        // TODO: ?
        // Changing threading mode from slice to MB_THREADING isn't possible because we need
        // to allocate lots of additional memory buffers for MB_THREADING which we didn't do in Init
        core_enc->useMBT = use_mb_threading;
        return UMC_ERR_FAILED;
    }
#ifdef SLICE_THREADING_LOAD_BALANCING
    if (!use_slice_load_balancing_enabled && core_enc->slice_load_balancing_enabled)
    {
        // TODO: ?
        // Enabling slice load balancing mode isn't possible because we need
        // to allocate additional memory buffers for load balancing which we didn't do in Init
        core_enc->slice_load_balancing_enabled = use_slice_load_balancing_enabled;
        return UMC_ERR_FAILED;
    }
#endif // SLICE_THREADING_LOAD_BALANCING

    H264ENC_MAKE_NAME(H264CoreEncoder_SetSequenceParameters)(state);

    core_enc->m_SubME_Algo = core_enc->m_info.mv_subpel_search_method;
    core_enc->m_Analyse = ANALYSE_I_4x4 | ANALYSE_ME_SUBPEL | ANALYSE_CHECK_SKIP_INTPEL | ANALYSE_CHECK_SKIP_BESTCAND | ANALYSE_CHECK_SKIP_SUBPEL;
    if (core_enc->m_info.transform_8x8_mode_flag)
        core_enc->m_Analyse |= ANALYSE_I_8x8;
    core_enc->m_Analyse |= ANALYSE_FRAME_TYPE;
    //core_enc->m_Analyse |= ANALYSE_ME_CONTINUED_SEARCH;
    //core_enc->m_Analyse |= ANALYSE_FAST_INTRA; // QS <= 3
    //core_enc->m_Analyse |= ANALYSE_ME_SUBPEL_SAD; // QS <= 3
    //core_enc->m_Analyse |= ANALYSE_FLATNESS; // QS <= 1
    //core_enc->m_Analyse |= ANALYSE_INTRA_IN_ME; // QS <= 1
    //core_enc->m_Analyse |= ANALYSE_ME_FAST_MULTIREF; // QS <= 1
    //core_enc->m_Analyse |= ANALYSE_ME_EARLY_EXIT;
    //core_enc->m_Analyse |= ANALYSE_CBP_EMPTY;
    //core_enc->m_Analyse |= ANALYSE_SPLIT_SMALL_RANGE;
    //core_enc->m_Analyse |= ANALYSE_ME_EXT_CANDIDATES;
    core_enc->m_Analyse |= ANALYSE_RECODE_FRAME;
    if (core_enc->m_info.me_split_mode != 0)
        core_enc->m_Analyse |= ANALYSE_P_8x8 | ANALYSE_B_8x8;
    if (core_enc->m_info.me_split_mode > 1)
        core_enc->m_Analyse |= ANALYSE_P_4x4 | ANALYSE_B_4x4;
    if (core_enc->m_info.direct_pred_mode == 2)
        core_enc->m_Analyse |= ANALYSE_ME_AUTO_DIRECT;
    if (core_enc->m_info.m_QualitySpeed == 0)
        core_enc->m_Analyse |= ANALYSE_SAD;
    if (core_enc->m_info.m_QualitySpeed <= 1)
        core_enc->m_Analyse |= ANALYSE_FLATNESS | ANALYSE_INTRA_IN_ME | ANALYSE_ME_FAST_MULTIREF | ANALYSE_ME_PRESEARCH;
//    if (core_enc->m_info.m_QualitySpeed >= 2)
//        core_enc->m_Analyse |= ANALYSE_ME_CHROMA;
    if ((core_enc->m_info.m_QualitySpeed >= 2) && core_enc->m_info.entropy_coding_mode)
        core_enc->m_Analyse |= ANALYSE_RD_MODE;
    if ((core_enc->m_info.m_QualitySpeed >= 3) && core_enc->m_info.entropy_coding_mode)
        core_enc->m_Analyse |= ANALYSE_RD_OPT | ANALYSE_B_RD_OPT;
    if (core_enc->m_info.m_QualitySpeed <= 3)
        core_enc->m_Analyse |= ANALYSE_ME_SUBPEL_SAD | ANALYSE_FAST_INTRA;
    if (core_enc->m_info.m_QualitySpeed >= 4)
        core_enc->m_Analyse |= /*ANALYSE_B_RD_OPT |*/ ANALYSE_ME_BIDIR_REFINE;
    if (core_enc->m_info.m_QualitySpeed >= 5)
        core_enc->m_Analyse |= ANALYSE_ME_ALL_REF;
    if ((core_enc->m_Analyse & ANALYSE_RD_MODE) || (core_enc->m_Analyse & ANALYSE_RD_OPT))
        core_enc->m_Analyse |= ANALYSE_ME_CHROMA;
    if (core_enc->m_info.coding_type > 0 && core_enc->m_info.coding_type != 2) { // TODO: remove coding_type != 2 when MBAFF is implemented
        core_enc->m_Analyse &= ~ANALYSE_ME_AUTO_DIRECT;
        core_enc->m_info.direct_pred_mode = 1;
        //core_enc->m_Analyse &= ~ANALYSE_FRAME_TYPE;
    }

    // apply additional features and restrictions if provided
    core_enc->m_Analyse &= ~core_enc->m_info.m_Analyse_restrict;
    core_enc->m_Analyse |= core_enc->m_info.m_Analyse_on;
    core_enc->m_Analyse_ex = core_enc->m_info.m_Analyse_ex;

    core_enc->profile_frequency = core_enc->m_info.B_frame_rate + 1;

    if (core_enc->m_info.reset_encoding_pipeline == true) {

        core_enc->m_SliceHeader.idr_pic_id = 0;

        // Set up for the 8 bit/default frame rate
        core_enc->m_uFrames_Num = 0;
        core_enc->m_uFrameCounter = 0;
        //core_enc->m_SEIData.PictureTiming.cpb_removal_delay = 0;
        core_enc->m_SEIData.PictureTiming.cpb_removal_delay_accumulated = 0;
        core_enc->m_SEIData.PictureTiming.dpb_output_delay = 2;
#ifdef H264_HRD_CPB_MODEL
        core_enc->m_SEIData.tai_n = 0;
        core_enc->m_SEIData.tai_n_minus_1 = 0;
        core_enc->m_SEIData.tai_earliest = 0;
        core_enc->m_SEIData.taf_n = 0;
        core_enc->m_SEIData.taf_n_minus_1 = 0;
        core_enc->m_SEIData.trn_n = 0;
        core_enc->m_SEIData.trn_nb = 0;
        core_enc->m_SEIData.m_tickDuration = 0;
#endif // H264_HRD_CPB_MODEL
        memset(core_enc->m_SEIData.m_insertedSEI,0, 36 * sizeof(Ipp8u));
        core_enc->m_PaddedSize.width  = (core_enc->m_info.info.clip_info.width  + 15) & ~15;
        core_enc->m_PaddedSize.height = (core_enc->m_info.info.clip_info.height + (16<<(1 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1) & ~((16<<(1 - core_enc->m_SeqParamSet->frame_mbs_only_flag)) - 1);
        core_enc->m_WidthInMBs = core_enc->m_PaddedSize.width >> 4;
        core_enc->m_HeightInMBs  = core_enc->m_PaddedSize.height >> 4;
        core_enc->m_bMakeNextFrameKey = true; // Ensure that we always start with a key frame.
        core_enc->m_bMakeNextFrameIDR = false;
        if (H264_KFCM_INTERVAL == core_enc->m_info.key_frame_controls.method) {
            core_enc->m_uIntraFrameInterval = core_enc->m_info.key_frame_controls.interval + 1;
            core_enc->m_uIDRFrameInterval = core_enc->m_info.key_frame_controls.idr_interval + 1;
        } else {
            core_enc->m_uIDRFrameInterval = core_enc->m_uIntraFrameInterval = 0;
        }
        memset(&core_enc->m_DecRefPicMarkingInfo,0,sizeof(core_enc->m_DecRefPicMarkingInfo));
        memset(&core_enc->m_ReorderInfoL0,0,sizeof(core_enc->m_ReorderInfoL0));
        memset(&core_enc->m_ReorderInfoL1,0,sizeof(core_enc->m_ReorderInfoL1));
        memset(core_enc->m_LTFrameIdxUseFlags, 0, 16);

        core_enc->m_NumShortTermRefFrames = 0;
        core_enc->m_NumLongTermRefFrames = 0;

        core_enc->m_pLastFrame = NULL; //Init pointer to last frame

    }

    if (core_enc->m_SeqParamSet->qpprime_y_zero_transform_bypass_flag) {
        ippsZero_8u((Ipp8u*)core_enc->m_EmptyThreshold, 52 * sizeof(*core_enc->m_EmptyThreshold));
        ippsZero_8u((Ipp8u*)core_enc->m_DirectBSkipMEThres, 52 * sizeof(*core_enc->m_DirectBSkipMEThres));
        ippsZero_8u((Ipp8u*)core_enc->m_PSkipMEThres, 52 * sizeof(*core_enc->m_PSkipMEThres));
        ippsZero_8u((Ipp8u*)core_enc->m_BestOf5EarlyExitThres, 52 * sizeof(*core_enc->m_BestOf5EarlyExitThres));
    } else {
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::EmptyThreshold, (Ipp8u*)core_enc->m_EmptyThreshold, 52 * sizeof(*core_enc->m_EmptyThreshold));
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::DirectBSkipMEThres, (Ipp8u*)core_enc->m_DirectBSkipMEThres, 52 * sizeof(*core_enc->m_DirectBSkipMEThres));
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::PSkipMEThres, (Ipp8u*)core_enc->m_PSkipMEThres, 52 * sizeof(*core_enc->m_PSkipMEThres));
        ippsCopy_8u((const Ipp8u*)UMC_H264_ENCODER::BestOf5EarlyExitThres, (Ipp8u*)core_enc->m_BestOf5EarlyExitThres, 52 * sizeof(*core_enc->m_BestOf5EarlyExitThres));
    }

    // Block offset -- initialize table, indexed by current block (0-23)
    // with the value to add to the offset of the block into the plane
    // to advance to the next block
    for (i = 0; i < 16; i++) {
        // 4 Cases to cover:
        if (!(i & 1)) {   // Even # blocks, next block to the right
            core_enc->m_EncBlockOffsetInc[0][i] = 4;
            core_enc->m_EncBlockOffsetInc[1][i] = 4;
        } else if (!(i & 2)) {  // (1,5,9 & 13), down and one to the left
            core_enc->m_EncBlockOffsetInc[0][i] = (core_enc->m_Pitch<<2) - 4;
            core_enc->m_EncBlockOffsetInc[1][i] = (core_enc->m_Pitch<<3) - 4;
        } else if (i == 7) { // beginning of next row
            core_enc->m_EncBlockOffsetInc[0][i] = (core_enc->m_Pitch<<2) - 12;
            core_enc->m_EncBlockOffsetInc[1][i] = (core_enc->m_Pitch<<3) - 12;
        } else { // (3 & 11) up and one to the right
            core_enc->m_EncBlockOffsetInc[0][i] = -(Ipp32s)((core_enc->m_Pitch<<2) - 4);
            core_enc->m_EncBlockOffsetInc[1][i] = -(Ipp32s)((core_enc->m_Pitch<<3) - 4);
        }
    }
    Ipp32s last_block = 16+(4<<core_enc->m_info.chroma_format_idc) - 1; // - last increment
    core_enc->m_EncBlockOffsetInc[0][last_block] = 0;
    core_enc->m_EncBlockOffsetInc[1][last_block] = 0;
    switch (core_enc->m_info.chroma_format_idc) {
        case 1:
        case 2:
            for (i = 16; i < last_block; i++){
                if (i & 1) {
                    core_enc->m_EncBlockOffsetInc[0][i] = (core_enc->m_Pitch<<2) - 4;
                    core_enc->m_EncBlockOffsetInc[1][i] = (core_enc->m_Pitch<<3) - 4;
                } else {
                    core_enc->m_EncBlockOffsetInc[0][i] = 4;
                    core_enc->m_EncBlockOffsetInc[1][i] = 4;
                }
            }
            break;
        case 3:
            //Copy from luma
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[0][16], &core_enc->m_EncBlockOffsetInc[0][0], 16*sizeof(Ipp32s));
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[0][32], &core_enc->m_EncBlockOffsetInc[0][0], 16*sizeof(Ipp32s));
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[1][16], &core_enc->m_EncBlockOffsetInc[1][0], 16*sizeof(Ipp32s));
            MFX_INTERNAL_CPY(&core_enc->m_EncBlockOffsetInc[1][32], &core_enc->m_EncBlockOffsetInc[1][0], 16*sizeof(Ipp32s));
            break;
    }
    core_enc->m_InitialOffsets[0][0] = core_enc->m_InitialOffsets[1][1] = 0;
    core_enc->m_InitialOffsets[0][1] = (Ipp32s)core_enc->m_Pitch;
    core_enc->m_InitialOffsets[1][0] = -(Ipp32s)core_enc->m_Pitch;

    Ipp32s bitDepth = IPP_MAX(core_enc->m_info.bit_depth_aux, IPP_MAX(core_enc->m_info.bit_depth_chroma, core_enc->m_info.bit_depth_luma));
    Ipp32s chromaSampling = core_enc->m_info.chroma_format_idc;
    Ipp32s alpha = core_enc->m_info.aux_format_idc ? 1 : 0;

    core_enc->m_iProfileIndex = 0;
    core_enc->cflags = core_enc->cnotes = 0;

    Ipp32s fs = core_enc->m_info.info.clip_info.width * core_enc->m_info.info.clip_info.height;
    core_enc->m_info.m_SuggestedOutputSize = fs;
    if (alpha)
        core_enc->m_info.m_SuggestedOutputSize += fs;
    if (chromaSampling == 1)
        core_enc->m_info.m_SuggestedOutputSize += fs / 2;
    else if (chromaSampling == 2)
        core_enc->m_info.m_SuggestedOutputSize += fs;
    else if (chromaSampling == 3)
        core_enc->m_info.m_SuggestedOutputSize += fs * 2;
    core_enc->m_info.m_SuggestedOutputSize = core_enc->m_info.m_SuggestedOutputSize * bitDepth / 8;
#ifdef FRAME_QP_FROM_FILE
        FILE* f;
        int qp,fn=0;
        char ft;

        if( (f = fopen(FRAME_QP_FROM_FILE, "r")) == NULL){
            fprintf(stderr,"Can't open file %s\n", FRAME_QP_FROM_FILE);
            exit(-1);
        }
        while(fscanf(f,"%c %d\n",&ft,&qp) == 2){
            frame_type.push_back(ft);
            frame_qp.push_back(qp);
            fn++;
        }
        fclose(f);
        //fprintf(stderr,"frames read = %d\n",fn);
#endif
    return UMC_OK;
//////////////////////////////////////////////////////////////////////////
}

void H264ENC_MAKE_NAME(H264CoreEncoder_SetDPBSize)(
    void* state)
{
    H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
    Ipp32u MaxDPBx2;
    Ipp32u dpbLevel;

    // MaxDPB, per Table A-1, Level Limits
    switch (core_enc->m_info.level_idc)
    {
    case 9:
    case 10:
        MaxDPBx2 = 297;
        break;
    case 11:
        MaxDPBx2 = 675;
        break;
    case 12:
    case 13:
    case 20:
        MaxDPBx2 = 891*2;
        break;
    case 21:
        MaxDPBx2 = 1782*2;
        break;
    case 22:
    case 30:
        MaxDPBx2 = 6075;
        break;
    case 31:
        MaxDPBx2 = 6750*2;
        break;
    case 32:
        MaxDPBx2 = 7680*2;
        break;
    case 40:
    case 41:
    case 42:
        MaxDPBx2 = 12288*2;
        break;
    case 50:
        MaxDPBx2 = 41400*2;
        break;
    default:
    case 51:
        MaxDPBx2 = 69120*2;
        break;
    }

    Ipp32u width, height;

    width = ((core_enc->m_info.info.clip_info.width+15)>>4)<<4;
    height = ((core_enc->m_info.info.clip_info.height+15)>>4) << 4;

    //Restrict maximum DPB size to 3xresolution according to IVT requirements
    //MaxDPBx2 = MIN( MaxDPBx2, (3*((width * height) + ((width * height)>>1))>>9)); // /512

    dpbLevel = (MaxDPBx2 * 512) / ((width * height) + ((width * height)>>1));
    core_enc->m_dpbSize = MIN(16, dpbLevel);
}

Status H264ENC_MAKE_NAME(H264CoreEncoder_SetBRC)(
  void* state,
  VideoBrc *BRC)
{
  H264CoreEncoderType* core_enc = (H264CoreEncoderType *)state;
  core_enc->brc = BRC;
  return UMC_OK;
}

#undef H264BsRealType
#undef H264BsFakeType
#undef H264SliceType
#undef EncoderRefPicListType
#undef H264CoreEncoderType
#undef H264EncoderFrameType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
