//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2018 Intel Corporation. All Rights Reserved.
//

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

#define sH264SliceType H264ENC_MAKE_NAME(sH264Slice)
#define H264SliceType H264ENC_MAKE_NAME(H264Slice)
#define sH264CoreEncoderType H264ENC_MAKE_NAME(sH264CoreEncoder)
#define H264CoreEncoderType H264ENC_MAKE_NAME(H264CoreEncoder)
#define DeblockingParametersType H264ENC_MAKE_NAME(DeblockingParameters)
#define DeblockingParametersMBAFFType H264ENC_MAKE_NAME(DeblockingParametersMBAFF)
#define H264CurrentMacroblockDescriptorType H264ENC_MAKE_NAME(H264CurrentMacroblockDescriptor)
#define T_RLE_DataType H264ENC_MAKE_NAME(T_RLE_Data)
#define ME_InfType H264ENC_MAKE_NAME(ME_Inf)
#define H264EncoderFrameType H264ENC_MAKE_NAME(H264EncoderFrame)
#define H264EncoderFrameListType H264ENC_MAKE_NAME(H264EncoderFrameList)
#define EncoderRefPicListType H264ENC_MAKE_NAME(EncoderRefPicList)
#define EncoderRefPicListStructType H264ENC_MAKE_NAME(EncoderRefPicListStruct)
#define H264BsRealType H264ENC_MAKE_NAME(H264BsReal)
#define H264BsFakeType H264ENC_MAKE_NAME(H264BsFake)

typedef struct H264ENC_MAKE_NAME(sDeblockingParameters) DeblockingParametersType;
typedef struct H264ENC_MAKE_NAME(sDeblockingParametersMBAFF) DeblockingParametersMBAFFType;

//public:
Status H264ENC_MAKE_NAME(H264Slice_Create)(
    void* state);

Status H264ENC_MAKE_NAME(H264Slice_Init)(
    void* state,
    H264EncoderParams& info); // Must be called once as parameters are available.

void H264ENC_MAKE_NAME(H264Slice_Destroy)(
    void* state);

typedef struct sH264SliceType
{
//public:
    EnumSliceType m_slice_type; // Type of the current slice.
    Ipp32s        m_slice_number; // Number of the current slice.
    Ipp32s        status;     //Return value from Compress_Slice function
    Ipp8s         m_iLastXmittedQP;
    Ipp32u      m_MB_Counter;
    Ipp32u      m_Intra_MB_Counter;
    Ipp32u      m_uSkipRun;
    Ipp32s      m_prev_dquant;

    Ipp32s      m_is_cur_mb_field;
    bool        m_is_cur_mb_bottom_field;
    Ipp32s      m_first_mb_in_slice;
    Ipp8s       m_slice_qp_delta;            // delta between slice QP and picture QP
    Ipp8u       m_cabac_init_idc;            // CABAC initialization table index (0..2)
    bool        m_use_transform_for_intra_decision;
    bool        m_use_intra_mbs_only; // force encoding with only Intra MBs regardless of slice type

    H264CurrentMacroblockDescriptorType m_cur_mb;

    H264BsBase*     m_pbitstream; // Where this slice is encoded to.
    H264BsFakeType* fakeBitstream;
    H264BsFakeType* fBitstreams[9]; //For INTRA mode selection

    Ipp8u       m_disable_deblocking_filter_idc; // deblock filter control, 0=filter all edges
    Ipp8s       m_slice_alpha_c0_offset;         // deblock filter c0, alpha table offset
    Ipp8s       m_slice_beta_offset;             // deblock filter beta table offset
    Ipp32s     *m_InitialOffset;
    Ipp32s      m_NumRefsInL0List;          // number of ref pictures in complete ref list 0 (past ST + future ST + LT)
    Ipp32s      m_NumRefsInL1List;          // number of ref pictures in complete ref list 1 (past ST + future ST + LT)
    Ipp32s      m_NumForwardSTRefs;         // number of past short-term reference pictures
    Ipp32s      m_NumBackwardSTRefs;        // number of future short-term reference pictures
    Ipp32s      m_NumRefsInLTList;          // number of long-term ref pictures
    Ipp32s      m_MaxNumActiveRefsInL0;           // maximum for number of ref pictures in list L0. Used to configure num_ref_idx_l0_active
    Ipp32s      m_MaxNumActiveRefsInL1;           // maximum for number of ref pictures in list L1. Used to configure num_ref_idx_l1_active
    bool        b_PerformModificationOfL0;    // true if any modification of default L0 is needed
    bool        b_PerformModificationOfL1;    // true if any modification of default L1 is needed
    Ipp8u       num_ref_idx_active_override_flag;   // nonzero: use ref_idx_active from slice header
    Ipp32s      num_ref_idx_l0_active;              // num of ref pics in list 0 used to decode the slice,
    Ipp32s      num_ref_idx_l1_active;              // num of ref pics in list 1 used to decode the slice

    // MB work buffer, allocated buffer pointer for freeing
    Ipp8u*      m_pAllocatedMBEncodeBuffer;
    // m_pAllocatedMBEncodeBuffer is mapped onto the following pointers.
    PIXTYPE*    m_pPred4DirectB;      // the 16x16 MB prediction for direct B mode
    PIXTYPE*    m_pPred4DirectB8x8;   // the 16x16 MB prediction for SVC direct B mode 8x8
    PIXTYPE*    m_pPred4BiPred;       // the 16x16 MB prediction for BiPredicted B Mode
    PIXTYPE*    m_pTempBuff4DirectB;  // 16x16 working buffer for direct B
    PIXTYPE*    m_pTempBuff4BiPred;   // 16x16 working buffer for BiPred B
    PIXTYPE*    m_pTempChromaPred;    // 16x16 working buffer for chroma pred B
    PIXTYPE*    m_pMBEncodeBuffer;    // temp work buffer

    // Buffers for CABAC.
    T_RLE_DataType Block_RLE[51];       // [0-15] Luma, [16-31] Chroma U/Luma1, [32-47] Chroma V/Luma2, [48] Chroma U DC/Luma1 DC, [49] Chroma V DC/Luma2 DC, [50] Luma DC
    T_Block_CABAC_Data Block_CABAC;

    EncoderRefPicListType m_TempRefPicList[2][2];

    Ipp32s      MapColMBToList0[MAX_NUM_REF_FRAMES][2];
    Ipp32s      DistScaleFactor[MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
    Ipp32s      DistScaleFactorMV[MAX_NUM_REF_FRAMES][MAX_NUM_REF_FRAMES];
    Ipp32s      DistScaleFactorAFF[2][2][2][MAX_NUM_REF_FRAMES]; // [curmb field],[ref1field],[ref0field]
    Ipp32s      DistScaleFactorMVAFF[2][2][2][MAX_NUM_REF_FRAMES]; // [curmb field],[ref1field],[ref0field]

#ifdef SLICE_THREADING_LOAD_BALANCING
    Ipp64u m_ticks_per_slice; // Total time used to encode all macroblocks in the slice
#endif // SLICE_THREADING_LOAD_BALANCING
#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    Ipp32s      m_last_mb_in_slice;
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

    // SVC extension
    Ipp8u         doInterIntraPrediction;
    Ipp8u         doSCoeffsPrediction;
    Ipp8u         doTCoeffsPrediction;
    Ipp8u         doMCDirectForSkipped;
    Ipp32s        resPredFlag; // moved core_enc level


    Ipp8u         m_constrained_intra_resampling_flag;
    Ipp8u         m_ref_layer_chroma_phase_x_plus1;
    Ipp8u         m_ref_layer_chroma_phase_y_plus1;
    Ipp8u         m_slice_skip_flag;
    Ipp8u         m_adaptive_prediction_flag;
    Ipp8u         m_default_base_mode_flag;
    Ipp8u         m_adaptive_motion_prediction_flag;
    Ipp8u         m_default_motion_prediction_flag;
    Ipp8u         m_adaptive_residual_prediction_flag;
    Ipp8u         m_default_residual_prediction_flag;
    Ipp8u         m_tcoeff_level_prediction_flag;
    Ipp8u         m_scan_idx_start;
    Ipp8u         m_scan_idx_end;
    Ipp8u         m_base_pred_weight_table_flag;
    Ipp32s        m_ref_layer_dq_id;
    Ipp32u        m_disable_inter_layer_deblocking_filter_idc;
    Ipp32s        m_inter_layer_slice_alpha_c0_offset_div2;
    Ipp32s        m_inter_layer_slice_beta_offset_div2;
    Ipp32s        m_scaled_ref_layer_left_offset;
    Ipp32s        m_scaled_ref_layer_top_offset;
    Ipp32s        m_scaled_ref_layer_right_offset;
    Ipp32s        m_scaled_ref_layer_bottom_offset;
    Ipp32u        m_num_mbs_in_slice_minus1;

    Ipp8s last_QP_deblock;
} H264SliceType;

#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
typedef struct H264ENC_MAKE_NAME(sGlobalData_MBT)
{
    T_Block_CABAC_Data bc;
} H264ENC_MAKE_NAME(GlobalData_MBT);
#ifdef MB_THREADING_VM
typedef struct H264ENC_MAKE_NAME(sThreadDataVM_MBT)
{
    H264CoreEncoderType* core_enc;
    H264SliceType *curr_slice;
    Ipp32s threadNum;
    int *incRow;
} H264ENC_MAKE_NAME(ThreadDataVM_MBT);
#endif // MB_THREADING_VM
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT

inline EncoderRefPicListStructType* H264ENC_MAKE_NAME(GetRefPicList)(
    H264SliceType* curr_slice,
    Ipp32u List,
    Ipp32s mb_cod_type,
    Ipp32s is_bottom_mb)
{
    EncoderRefPicListStructType *pList;
    if (List == LIST_0)
        pList = &curr_slice->m_TempRefPicList[mb_cod_type][is_bottom_mb].m_RefPicListL0;
    else
        pList = &curr_slice->m_TempRefPicList[mb_cod_type][is_bottom_mb].m_RefPicListL1;
    return pList;
}

/* public: */
Status H264ENC_MAKE_NAME(H264CoreEncoder_Create)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_Destroy)(
    void* state);

// Initialize codec with specified parameter(s)
Status H264ENC_MAKE_NAME(H264CoreEncoder_Init)(
    void* state,
    BaseCodecParams *init,
    MemoryAllocator *pMemAlloc);

// Get codec working (initialization) parameter(s)
Status H264ENC_MAKE_NAME(H264CoreEncoder_GetInfo)(
    void* state,
    BaseCodecParams *info);

const H264PicParamSet* H264ENC_MAKE_NAME(H264CoreEncoder_GetPicParamSet)(void* state);
const H264SeqParamSet* H264ENC_MAKE_NAME(H264CoreEncoder_GetSeqParamSet)(void* state);

// Close all codec resources
Status H264ENC_MAKE_NAME(H264CoreEncoder_Close)(
    void* state);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Reset)(
    void* state);

Status H264ENC_MAKE_NAME(H264CoreEncoder_SetParams)(
    void* state,
    BaseCodecParams* params);

VideoData* H264ENC_MAKE_NAME(H264CoreEncoder_GetReconstructedFrame)(
    void* state);

/* protected: */
Status H264ENC_MAKE_NAME(H264CoreEncoder_CheckEncoderParameters)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaNonMBAFF)( //stateless
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBChromaNonMBAFF)( //stateless
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaNonMBAFF)( //stateless
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBChromaNonMBAFF)( //stateless
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaNonMBAFF)( //stateless
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaNonMBAFF)( //stateless
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBLumaMBAFF)(
    void* state,
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block,
    Ipp32s AdditionalDecrement/* = 0*/);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetLeftLocationForCurrentMBChromaMBAFF)(
    void* state,
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBLumaMBAFF)(
    void* state,
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block,
    bool is_deblock_calls);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLocationForCurrentMBChromaMBAFF)(
    void* state,
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopLeftLocationForCurrentMBLumaMBAFF)(
    void* state,
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetTopRightLocationForCurrentMBLumaMBAFF)(
    void* state,
    H264CurrentMacroblockDescriptorType& cur_mb,
    H264BlockLocation *Block);

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_GetColocatedLocation)(
    void* state,
    H264SliceType *curr_slice,
    H264EncoderFrameType *pRefFrame,
    Ipp8u Field,
    Ipp8s& block,
    Ipp8s *scale/* = 0*/);

void H264ENC_MAKE_NAME(H264CoreEncoder_UpdateCurrentMBInfo)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_LoadPredictedMBInfo)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_MBFrameFieldSelect)(
    void* state,
    H264SliceType *curr_slice);

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_ComputeMBFrameFieldCost)(
    void* state,
    H264SliceType *curr_slice,
    bool is_frame);

// update dpb for prediction of current picture
// construct default ref pic list
void H264ENC_MAKE_NAME(H264CoreEncoder_UpdateRefPicListCommon)(
    void* state);

Status H264ENC_MAKE_NAME(H264CoreEncoder_UpdateRefPicList)(
    void* state,
    H264SliceType *curr_slice,
    const EncoderRefPicListType *ref_pic_list,
    const H264SliceHeader &SHdr);

void H264ENC_MAKE_NAME(H264CoreEncoder_InitPSliceRefPicList)(
    void* state,
    bool bIsFieldSlice,
    H264EncoderFrameType **pRefPicList);    // pointer to start of list 0

void H264ENC_MAKE_NAME(H264CoreEncoder_InitBSliceRefPicLists)(
    void* state,
    bool bIsFieldSlice,
    H264EncoderFrameType **pRefPicList0,    // pointer to start of list 0
    H264EncoderFrameType **pRefPicList1);   // pointer to start of list 1

void H264ENC_MAKE_NAME(H264CoreEncoder_InitDistScaleFactor)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s NumL0RefActive,
    Ipp32s NumL1RefActive,
    H264EncoderFrameType **pRefPicList0,
    H264EncoderFrameType **pRefPicList1,
    Ipp8s *pFields0,
    Ipp8s *pFields1);

void H264ENC_MAKE_NAME(H264CoreEncoder_InitMapColMBToList0)( //stateless
    H264SliceType *curr_slice,
    Ipp32s NumL0RefActive,
    H264EncoderFrameType **pRefPicList0,
    H264EncoderFrameType **pRefPicList1);

void H264ENC_MAKE_NAME(H264CoreEncoder_AdjustRefPicListForFields)(
    void* state,
    H264EncoderFrameType **pRefPicList,
    Ipp8s *pFields);

void H264ENC_MAKE_NAME(H264CoreEncoder_ReOrderRefPicList)(
    void* state,
    bool bIsFieldSlice,
    H264EncoderFrameType **pRefPicList,
    Ipp8s *pFields,
    RefPicListReorderInfo *pReorderInfo,
    Ipp32s MaxPicNum,
    Ipp32s NumRefActive);

Status H264ENC_MAKE_NAME(H264CoreEncoder_UpdateRefPicMarking)(
    void* state);

// Encodes blank picture when overflow
Status H264ENC_MAKE_NAME(H264CoreEncoder_EncodeDummyPicture)(
    void* state);

EnumPicCodType H264ENC_MAKE_NAME(H264CoreEncoder_DetermineFrameType)(
    void* state,
    Ipp32s);

//Do we need to start new picture?

Status H264ENC_MAKE_NAME(H264CoreEncoder_encodeSPSPPS)(
    void* state,
    H264BsRealType* bs,
    MediaData* dstSPS,
    MediaData* dstPPS);

Status H264ENC_MAKE_NAME(H264CoreEncoder_encodeFrameHeader)(
    void* state,
    H264BsRealType*,
    MediaData* dst,
    bool bIDR_Pic,
    bool& startPicture);

void H264ENC_MAKE_NAME(H264CoreEncoder_SetSequenceParameters)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_SetPictureParameters)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_SetDPBSize)(
    void* state);

Status H264ENC_MAKE_NAME(H264CoreEncoder_SetBRC)(
  void* state,
  UMC::VideoBrc *BRC);

void H264ENC_MAKE_NAME(H264CoreEncoder_SetSliceHeaderCommon)(
    void* state,
    H264EncoderFrameType*);

void H264ENC_MAKE_NAME(H264CoreEncoder_InferFDFForSkippedMBs)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_setInCropWindow)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_MoveFromCPBToDPB)(
    void* state);

Status H264ENC_MAKE_NAME(H264CoreEncoder_CleanDPB)(
    void* state);

Ipp8u H264ENC_MAKE_NAME(H264CoreEncoder_GetFreeLTIdx)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_ClearAllLTIdxs)(
    H264CoreEncoderType* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_ClearLTIdx)(
    H264CoreEncoderType* state,
    Ipp8u idx);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Start_Picture)(
    void* state,
    const EnumPicClass* pic_class,
    EnumPicCodType pic_type);

// Compress picture slice
Status H264ENC_MAKE_NAME(H264CoreEncoder_Compress_Slice)(
    void* state,
    H264SliceType *curr_slice,
    bool is_first_mb);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_MB_Decision)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u SVCBaseMode_MB_Decision(void* state,
                               H264Slice_8u16s *curr_slice);

Ipp32u SVC_MB_Decision(void* state,
                       H264Slice_8u16s *curr_slice);

int SVC_CheckMBType_BaseMode(H264CurrentMacroblockDescriptor_8u16s *cur_mb);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Compress_Slice_MBAFF)(
    void* state,
    H264SliceType *curr_slice);

// any processing needed after each picture
void H264ENC_MAKE_NAME(H264CoreEncoder_End_Picture)(
    void* state);

/* private: */
void H264ENC_MAKE_NAME(H264CoreEncoder_InitializeMBData)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_Make_MBSlices)(
    void* state);

void H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u uBestSAD,        //Best previous SAD
    Ipp32u *puAIMBSAD);     // return total MB SAD here

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode4x4IntraBlock)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s block);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode8x8IntraBlock)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s block);

void H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChroma_SVC)(
    void* state,
    H264SliceType *curr_slice);

// recalculate reconstruct for TCoeffsPrediction with zero cbp
void H264ENC_MAKE_NAME(H264CoreEncoder_EncodeChromaRec)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneBlock)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrcBlock,     // pointer to upper left pel of source block
    PIXTYPE* pReconBlock,   // pointer to same block in reconstructed picture
    Ipp32u uBlock,          // which 4x4 of the MB (0..15)
    T_AIMode *intra_types,  // selected mode goes here
    PIXTYPE *pPred);        // predictor pels for selected mode goes here

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneMB_16x16)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrc,          // pointer to upper left pel of source MB
    PIXTYPE* pRef,          // pointer to same MB in reference picture
    Ipp32s   pitchPixels,   // of source and ref data in pixels
    T_AIMode *pMode,        // selected mode goes here
    PIXTYPE *pPredBuf);     // predictor pels for selected mode goes here

void H264ENC_MAKE_NAME(PredictIntraLuma16x16)(
    PIXTYPE* pRef,
    Ipp32s pitchPixels,
    PIXTYPE *pPredBuf,
    Ipp32s bitDepth,
    bool leftAvail,
    bool topAvail,
    bool lefttopAvail);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pUSrc,         // pointer to upper left pel of U source MB
    PIXTYPE* pURef,         // pointer to same MB in U reference picture
    PIXTYPE* pVSrc,         // pointer to upper left pel of V source MB
    PIXTYPE* pVRef,         // pointer to same MB in V reference picture
    Ipp32u uPitch,          // of source and ref data
    Ipp8u *pMode,           // selected mode goes here
    PIXTYPE *pUPredBuf,     // U predictor pels for selected mode go here
    PIXTYPE *pVPredBuf);    // V predictor pels for selected mode go here

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectChromaMBs_8x8_NV12)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrc,
    PIXTYPE* pRef,
    Ipp32s uPitch,
    Ipp8u* pMode,
    PIXTYPE *pUPredBuf,
    PIXTYPE *pVPredBuf,
    Ipp32s isCurMBSeparated,
    Ipp32s UseStoredMode = 0);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetPredBlock)( //stateless
    Ipp32u uMode,           // advanced intra mode of the block
    PIXTYPE *pPredBuf,
    PIXTYPE* PredPel);      // predictor pels

void H264ENC_MAKE_NAME(H264CoreEncoder_GetBlockPredPels)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pLeftRefBlock,       // pointer to block in reference picture
    Ipp32u uLeftPitch,              // of source data. Pitch in pixels.
    PIXTYPE* pAboveRefBlock,      // pointer to block in reference picture
    Ipp32u uAbovePitch,             // of source data. Pitch in pixels.
    PIXTYPE* pAboveLeftRefBlock,  // pointer to block in reference picture
    Ipp32u uAboveLeftPitch,         // of source data. Pitch in pixels.
    Ipp32u uBlock,                  // 0..15 for luma blocks only
    PIXTYPE* PredPel);              // result here

void H264ENC_MAKE_NAME(H264CoreEncoder_AdvancedIntraModeSelectOneMacroblock8x8)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u uBestSAD,    // Best previous SAD
    Ipp32u *puAIMBSAD); // return total MB SAD here

void H264ENC_MAKE_NAME(H264CoreEncoder_Filter8x8Pels)( //stateless
    PIXTYPE* pred_pels,
    Ipp32u pred_pels_mask);

void H264ENC_MAKE_NAME(H264CoreEncoder_GetPrediction8x8)(
    void* state,
    T_AIMode mode,
    PIXTYPE* pred_pels,
    Ipp32u pred_pels_mask,
    PIXTYPE* pels);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_AIModeSelectOneMB_8x8)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrc,            // pointer to upper left pel of source MB
    PIXTYPE* pRef,            // pointer to same MB in reference picture
    Ipp32s uBlock,
    T_AIMode* pMode,            // selected mode goes here
    PIXTYPE* pPredBuf);       // predictor pels for selected mode goes here

void H264ENC_MAKE_NAME(H264CoreEncoder_Intra16x16SelectAndPredict)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u *puAIMBSAD,      // return total MB SAD here
    PIXTYPE *pPredBuf);   // return predictor pels here

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_Intra4x4SelectRD)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrcBlock,   // pointer to upper left pel of source block
    PIXTYPE* pReconBlock, // pointer to same block in reconstructed picture
    Ipp32u     uBlock,      // which 4x4 of the MB (0..15)
    T_AIMode*  intra_types, // selected mode goes here
    PIXTYPE* pPred);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_Intra8x8SelectRD)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrc,        // pointer to upper left pel of source MB
    PIXTYPE* pRef,        // pointer to same MB in reference picture
    Ipp32s     uBlock,      // 8x8 block number
    T_AIMode*  pMode,       // selected mode goes here
    PIXTYPE* pPredBuf);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_Intra16x16SelectRD)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE  *pSrc,           // pointer to upper left pel of source MB
    PIXTYPE  *pRef,           // pointer to same MB in reference picture
    Ipp32s      pitchPixels,    // of source and ref data
    T_AIMode   *pMode,          // selected mode goes here
    PIXTYPE  *pPredBuf);      // predictor pels for selected mode goes here

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_IntraSelectChromaRD)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pUSrc,           // pointer to upper left pel of U source MB
    PIXTYPE* pURef,           // pointer to same MB in U reference picture
    PIXTYPE* pVSrc,           // pointer to upper left pel of V source MB
    PIXTYPE* pVRef,           // pointer to same MB in V reference picture
    Ipp32u   uPitch,            // of source and ref data
    Ipp8u*   pMode,             // selected mode goes here
    PIXTYPE *pUPredBuf,       // U predictor pels for selected mode go here
    PIXTYPE *pVPredBuf);

#ifdef USE_NV12
Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_IntraSelectChromaRD_NV12)(
    void* state,
    H264SliceType *curr_slice,
    PIXTYPE* pSrc,           // pointer to upper left pel of U source MB
    PIXTYPE* pRef,           // pointer to same MB in U reference picture
    Ipp32u   uPitch,            // of source and ref data
    Ipp8u*   pMode,             // selected mode goes here
    PIXTYPE *pUPredBuf,       // U predictor pels for selected mode go here
    PIXTYPE *pVPredBuf,
    Ipp32s   isCurMBSeparated);
#endif // USE_NV12

void H264ENC_MAKE_NAME(H264CoreEncoder_Reconstruct_PCM_MB)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_ReconstuctCBP)( //stateless
    H264CurrentMacroblockDescriptorType *cur_mb);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MB_Real)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MB_Fake)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MBHeader_Real)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MBHeader_Fake)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MBLuma_Real)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MBLuma_Fake)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MBChroma_Real)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_Put_MBChroma_Fake)(
    void* state,
    H264SliceType *curr_slice);

Status H264ENC_MAKE_NAME(H264CoreEncoder_PackSubBlockLuma_Real)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u uBlock);

Status H264ENC_MAKE_NAME(H264CoreEncoder_PackSubBlockLuma_Fake)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u uBlock);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_CBP_Real)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_CBP_Fake)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_MacroblockBaseMode_Real)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_MacroblockBaseMode_Fake)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_MBBaseModeFlag_Real)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_MBBaseModeFlag_Fake)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_MBResidualPredictionFlag_Real)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_Encode_MBResidualPredictionFlag_Fake)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)( //stateless
    COEFFSTYPE coeff[],
    Ipp32s ctxBlockCat,
    Ipp32s numcoeff,
    const Ipp32s* dec_single_scan,
    CabacBlock4x4* c_data);

void H264ENC_MAKE_NAME(H264CoreEncoder_ScanSignificant_CABAC)( //stateless
    COEFFSTYPE coeff[],
    Ipp32s ctxBlockCat,
    Ipp32s numcoeff,
    const Ipp32s* dec_single_scan,
    CabacBlock8x8* c_data);

void H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)( //stateless
    const COEFFSTYPE* coeff,
    const Ipp32s* dec_single_scan,
    CabacBlock4x4* c_data);

void H264ENC_MAKE_NAME(H264CoreEncoder_MakeSignificantLists_CABAC)( //stateless
    const COEFFSTYPE* coeff,
    const Ipp32s* dec_single_scan,
    CabacBlock8x8* c_data);

// Encode and reconstruct macroblock
void H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecMB)(
    void* state,
    H264SliceType *curr_slice);

// Encode and reconstruct macroblock
void H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecMB_SVC)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMB)(
    void* state,
    H264SliceType *curr_slice);

// recalculate reconstruct for TCoeffsPrediction with zero cbp
Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec16x16IntraMBRec)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMB)(
    void* state,
    H264SliceType *curr_slice);

// recalculate reconstruct for TCoeffsPrediction with zero cbp
Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRec4x4IntraMBRec)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMB)(
    void* state,
    H264SliceType *curr_slice);

// recalculate reconstruct for TCoeffsPrediction with zero cbp
Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_CEncAndRecInterMBrec)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantIntra16x16_RD)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantIntra_RD)(
    void* state,
    H264SliceType *curr_slice);

Ipp32u H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantInter_RD)(
    void* state,
    H264SliceType *curr_slice,
    Ipp16s *pResPredDiff = NULL);

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaIntra_RD)(
    void* state,
    H264SliceType *curr_slice);

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaIntra_RD_NV12)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s isCurMBSeparated);

void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD)(
    void* state,
    H264SliceType *curr_slice);

#ifdef USE_NV12
void H264ENC_MAKE_NAME(H264CoreEncoder_TransQuantChromaInter_RD_NV12)(
  void* state,
  H264SliceType *curr_slice,
  Ipp32s isCurMBSeparated,
  Ipp16s *pResPredDiff);
#endif // USE_NV12


Ipp32u H264CoreEncoder_TransQuant_RD_BaseMode_8u16s(
  void* state,
  H264SliceType *curr_slice,
  Ipp32s intra,
  Ipp32s residualPrediction);

void H264CoreEncoder_TransQuantChroma_RD_NV12_BaseMode_8u16s(
  void* state,
  H264SliceType *curr_slice,
  PIXTYPE *pSrcSep,
  Ipp32s residualPrediction);

// luma MB motion comp
void H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBLuma)(
    void* state,
    H264SliceType *curr_slice,
    const H264MotionVector* pMVFwd,   // motion vectors in subpel units
    const H264MotionVector* pMVBwd,   // motion vectors in subpel units
    PIXTYPE* pDst);                   // put the resulting block here

void truncateMVs (
                  void* state,
                  H264SliceType *curr_slice,
                  const H264MotionVector* src,   // 16 motion vectors in subpel units
                  H264MotionVector* dst);        // 16 motion vectors in subpel units

// Luma MC for bidir blocks (for SVC base mode)
void MC_bidirMB_luma (
    void* state,
    H264SliceType *curr_slice,
    const H264MotionVector* pMVFwd,   // motion vectors in subpel units
    const H264MotionVector* pMVBwd,   // motion vectors in subpel units
    PIXTYPE* pDst);

// chroma MB motion comp
void H264ENC_MAKE_NAME(H264CoreEncoder_MCOneMBChroma)(
    void* state,
    H264SliceType* curr_slice,
    const H264MotionVector* pMVFwd,   // motion vectors in subpel units
    const H264MotionVector* pMVBwd,   // motion vectors in subpel units
    PIXTYPE* pDst);

void H264ENC_MAKE_NAME(H264CoreEncoder_CalcMVPredictor)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32u block_idx,
    Ipp32u uList,
    Ipp32u uBlocksWide,
    Ipp32u uBlocksHigh,
    ME_InfType* meinf);
    //H264MotionVector *pMVPred);

void H264ENC_MAKE_NAME(H264CoreEncoder_ME_CandList16x16)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32s list_id,
    ME_InfType* meInfo,
    Ipp32s refIdx);

bool H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkip)(
  void* state,
  H264SliceType *curr_slice,
  H264MotionVector &skip_vec,
  Ipp32s residualPredictionFlag,
  Ipp16s *pResBuf,
  Ipp16s *pResBufC);

bool H264ENC_MAKE_NAME(H264CoreEncoder_CheckSkipB)(
    void* state,
    H264SliceType* curr_slice,
    H264MacroblockRefIdxs  ref_direct[2],
    H264MacroblockMVs      mvs_direct[2],
    Ipp32s residualPrediction,
    Ipp16s *pResBuf,
    Ipp16s *pResBufC);

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_ME_P)(
    void* state,
    H264SliceType* curr_slice);

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_ME_B)(
    void* state,
    H264SliceType* curr_slice);

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_BidirRefine)(
    H264SliceType *curr_slice,
    H264CoreEncoderType* core_enc,
    PIXTYPE * pRefL0,
    PIXTYPE * pRefL1,
    H264MotionVector * predictedMVL0,
    H264MotionVector * predictedMVL1,
    Ipp32s * bestSAD,
    Ipp32s * bestSAD16x16);

void H264ENC_MAKE_NAME(H264CoreEncoder_FrameTypeDetect)(
    void* state);

#ifdef USE_PSYINFO
void H264ENC_MAKE_NAME(H264CoreEncoder_FrameAnalysis)(void* state);
#endif // USE_PSYINFO

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32s is8x8,
    Ipp32s bestCost);

#ifdef USE_NV12
Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_P_RDCost_NV12)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32s is8x8,
    Ipp32s isCurMBSeparated,
    Ipp32s bestCost,
    Ipp16s *pResPredDiff = NULL,
    Ipp16s *pResPredDiffC = NULL);
#endif // USE_NV12

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32s is8x8);

Ipp32s H264ENC_MAKE_NAME(H264CoreEncoder_MB_B_RDCost_NV12)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32s is8x8,
    Ipp32s isCurMBSeparated,
    Ipp16s *pResPredDiff = NULL,
    Ipp16s *pResPredDiffC = NULL);

void H264ENC_MAKE_NAME(H264CoreEncoder_ME_CheckCandidate)( //stateless
    ME_InfType* meInfo,
    H264MotionVector& mv);

void H264ENC_MAKE_NAME(H264CoreEncoder_Calc_One_MV_Predictor)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32u uBlock,              // which 4x4 Block (UL Corner, Raster Order)
    Ipp32u uList,               // 0 or 1 for L0 or L1
    Ipp32u uBlocksWide,         // 1, 2, or 4
    Ipp32u uBlocksHigh,         // 1, 2, or 4 (4x16 and 16x4 not permitted)
    H264MotionVector* pMVPred,  // resulting MV predictor
    H264MotionVector* pMVDelta, // resulting MV delta
    bool updateDMV/* = true*/);

void H264ENC_MAKE_NAME(H264CoreEncoder_Skip_MV_Predicted)(
    void* state,
    H264SliceType* curr_slice,
    H264MotionVector* pMVPredicted,
    H264MotionVector* pMVOut);  // Returns Skip MV if not NULL

void H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialRefIdx)(
    void* state,
    H264SliceType* curr_slice,
    T_RefIdx& pRefIndexL0,
    T_RefIdx& pRefIndexL1);

bool H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSpatialMV)(
    void* state,
    H264SliceType* curr_slice,
    H264MacroblockRefIdxs  ref_direct[2],
    H264MacroblockMVs      mvs_direct[2]);     // MVs used returned here.

bool H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectTemporalMV)(
    void* state,
    H264SliceType* curr_slice,
    H264MacroblockRefIdxs ref_direct[2],
    H264MacroblockMVs     mvs_direct[2]);     // MVs used returned here.

void H264ENC_MAKE_NAME(H264CoreEncoder_ComputeDirectSubblockSpatial)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32u block_idx,           // which 4x4 Block (UL Corner, Raster Order)
    Ipp32u uBlocksWide,         // 1, 2, or 4
    Ipp32u uBlocksHigh,         // 1, 2, or 4 (in fact only for B, 8x8)
    H264MacroblockRefIdxs  ref_idxs_direct[2],
    H264MacroblockMVs      mvs_direct[2]);

void H264ENC_MAKE_NAME(H264CoreEncoder_CDirectB8x8_Interp)(
    void* state,
    H264SliceType *curr_slice,
    Ipp32s block,                             // block index in raster order (0-15)
    H264MacroblockRefIdxs  ref_direct[2],     // only correspondent idx and
    H264MacroblockMVs      mvs_direct[2]);    // MV are read

// pointer to future frame buffer
void H264ENC_MAKE_NAME(H264CoreEncoder_CDirectBOneMB_Interp)(
    void* state,
    H264SliceType* curr_slice,
    H264MacroblockRefIdxs ref_direct[2],
    H264MacroblockMVs     mvs_direct[2]); // MVs used returned here.

void H264ENC_MAKE_NAME(H264CoreEncoder_CDirectBOneMB_Interp_Cr)(
    void* state,
    H264SliceType* curr_slice,
    const H264MotionVector* pMVL0,// Fwd motion vectors in subpel units
    const H264MotionVector* pMVL1,// Bwd motion vectors in subpel units
    Ipp8s* pFields0,              //
    Ipp8s* pFields1,              //
    PIXTYPE* pDst,                // put the resulting block here with pitch of 16
    Ipp32s offset,
    IppiSize size);

// Perform deblocking on single slice
void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockSlice)(
    void* state,
    H264SliceType* curr_slice,
    Ipp32u uFirstMB,
    Ipp32u uNumMBs );

// Reset deblocking variables
void H264ENC_MAKE_NAME(H264CoreEncoder_ResetDeblockingVariables)(
    void* state,
    DeblockingParametersType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_ResetDeblockingVariablesMBAFF)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

// Function to do luma deblocking
void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLuma)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLumaVerticalMBAFF)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockLumaHorizontalMBAFF)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

// Function to do chroma deblocking
void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChroma)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChromaVerticalMBAFF)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockChromaHorizontalMBAFF)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersStrengths)(
    void* state,
    DeblockingParametersType *pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_ResetInternalEdges)(
    DeblockingParametersType *pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockISlice)(
    void* state,
    Ipp32u MBAddr);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersISlice)(
    void* state,
    DeblockingParametersType *pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersStrengthsMBAFF)(
    void* state,
    DeblockingParametersMBAFFType *pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockISliceMBAFF)(
    void* state,
    Ipp32u MBAddr);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersISliceMBAFF)(
    void* state,
    DeblockingParametersMBAFFType *pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockPSlice)(
    void* state,
    Ipp32u MBAddr);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockPSliceMBAFF)(
    void* state,
    Ipp32u MBAddr);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice)(
    void* state,
    DeblockingParametersType *pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSliceMBAFF)(
    void* state,
    DeblockingParametersMBAFFType *pParams);

// Prepare deblocking parameters for macroblocks from P slice
// MbPart is 16, MbPart of opposite direction is 16
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblocks from P slice
// MbPart is 8, MbPart of opposite direction is 16
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice8x16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblocks from P slice
// MbPart is 16, MbPart of opposite direction is 8
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice16x8)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblocks from P slice
// MbParts of both directions are 4
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4MBAFFField)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblock from P slice,
// which coded in frame mode, but above macroblock is coded in field mode
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge)(
    void* state,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblock from P slice,
// which coded in frame mode, but left macroblock is coded in field mode
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4MBAFFComplexFrameExternalEdge)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

// Prepare deblocking parameters for macroblock from P slice,
// which coded in field mode, but left macroblock is coded in frame mode
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersPSlice4MBAFFComplexFieldExternalEdge)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockBSlice)(
    void* state,
    Ipp32u MBAddr);

void H264ENC_MAKE_NAME(H264CoreEncoder_DeblockMacroblockBSliceMBAFF)(
    void* state,
    Ipp32u MBAddr);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice)(
    void* state,
    DeblockingParametersType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSliceMBAFF)(
    void* state,
    DeblockingParametersMBAFFType* pParams);

// Prepare deblocking parameters for macroblocks from B slice
// MbPart is 16, MbPart of opposite direction is 16
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblocks from B slice
// MbPart is 8, MbPart of opposite direction is 16
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice8x16)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblocks from B slice
// MbPart is 16, MbPart of opposite direction is 8
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice16x8)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

// Prepare deblocking parameters for macroblocks from B slice
// MbParts of both directions are 4
void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice4)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

void H264ENC_MAKE_NAME(H264CoreEncoder_PrepareDeblockingParametersBSlice4MBAFFField)(
    void* state,
    Ipp32u dir,
    DeblockingParametersType* pParams);

#ifdef USE_TEMPTEMP
struct SVCRefControl {
    Ipp8u tid;
    Ipp8u isLT;
    Ipp8u isRef;
    EnumPicCodType   pcType;
    Ipp32s codorder;
    Ipp32s deadline;
    Ipp32s lastuse;
};
#endif // USE_TEMPTEMP

typedef struct sH264CoreEncoderType
{
//public:
//protected:
    //f
    Ipp32s                        m_Analyse, m_Analyse_ex, m_SubME_Algo;
    Ipp32s                        profile_frequency;
    H264LocalMacroblockDescriptor m_mbinfo;

    Ipp32s                        m_HeightInMBs;
    Ipp32s                        m_WidthInMBs;

    MemoryAllocator*              memAlloc;
    Ipp8u                         *m_pParsedDataNew;
    T_EncodeMBOffsets             *m_pMBOffsets;
    EnumPicCodType                *eFrameType;
    H264EncoderFrameType          **eFrameSeq;
    H264BsRealType*               m_bs1; // Pointer to the main bitstream.
    IppiSize                      m_PaddedSize;
    Ipp8u                         m_FieldStruct;

//public:
    H264EncoderParams               m_info;
    H264_Encoder_Compression_Flags  cflags;
    H264_Encoder_Compression_Notes  cnotes;
    H264EncoderFrameListType        m_cpb;
    H264EncoderFrameListType        m_dpb;
    Ipp32s                          m_dpbSize;
    Ipp8u                           m_LTFrameIdxUseFlags[16]; // LT frame Idxs from 0 to 15. Marked with 1 are in use, marked with 0 aren't in use.
    H264EncoderFrameType*      m_pCurrentFrame;
    H264EncoderFrameType*      m_pLastFrame;     // ptr to last frame
    H264EncoderFrameType*      m_pReconstructFrame;
    H264EncoderFrameType*      m_pReconstructFrame_allocated;

#ifdef H264_INTRA_REFRESH
    Ipp8u     m_refrType; // horizontal, vertical, other
    Ipp8u     m_stripeWidth; // width of refresh stripe in MBs
    Ipp8s     m_QPDelta; // QP delta for Intra refresh MBs
    Ipp16u    m_stopRefreshFrames; // width of refresh stripe in MBs
    Ipp16s    m_firstLineInStripe;
    Ipp16s    m_refrSpeed;
    Ipp32u    m_periodCount;
    Ipp32u    m_stopFramesCount;
#endif // H264_INTRA_REFRESH

    H264EncoderFrameType*      m_pNextFrame;
    // Table to obtain value to advance the 4x4 block offset for the next block.
    Ipp32s                          m_EncBlockOffsetInc[2][48];
    Ipp32s                          m_is_cur_pic_afrm;
    bool                            m_is_mb_data_initialized;

    bool                            m_NeedToCheckMBSliceEdges;
    Ipp32s                          m_field_index;
    Ipp32u                          m_NumShortTermRefFrames;
    Ipp32u                          m_NumLongTermRefFrames;
    DecRefPicMarkingInfo            m_DecRefPicMarkingInfo[2]; // dec ref pic marking info for both fields
    RefPicListReorderInfo           m_ReorderInfoL0[2]; // ref pic list L0 modification info for both fields
    RefPicListReorderInfo           m_ReorderInfoL1[2]; // ref pic list L1 modification info for both fields

    Ipp32s                          m_InitialOffsets[2][2];

    Ipp32s                          m_MaxLongTermFrameIdx;

//protected:
    Ipp8u**                         m_pAllocEncoderInst;

    // flags read by DetermineFrameType while sequencing the profile:
    bool                            m_bMakeNextFrameKey;
    bool                            m_bMakeNextFrameIDR;
    Ipp32s                          m_uIntraFrameInterval;
    Ipp32s                          m_uIDRFrameInterval;
    Ipp32s                          m_l1_cnt_to_start_B;

    H264SliceType *m_Slices; // thread independent slice information.
#ifdef DEBLOCK_THREADING
    volatile Ipp8u      *mbs_deblock_ready;
#endif // _OPENMP

#ifdef H264_RECODE_PCM
    volatile Ipp32u m_nPCM;
    Ipp32s* m_mbPCM;
#endif //H264_RECODE_PCM

#ifndef UMC_RESTRICTED_CODE_MBT
#ifdef MB_THREADING
    bool   useMBT;
    vm_mutex mutexIncRow;
    H264SliceType *m_Slices_MBT; // thread independent slice information.
    volatile Ipp32s *mbReady_MBT;
    Ipp32s *lSR_MBT;
    H264ENC_MAKE_NAME(GlobalData_MBT) *gd_MBT;
#ifdef MB_THREADING_VM
    H264ENC_MAKE_NAME(ThreadDataVM_MBT) *m_ThreadDataVM;
    vm_thread *m_ThreadVM_MBT;
#endif // MB_THREADING_VM
#ifdef MB_THREADING_TW
#ifdef MB_THREADING_VM
    vm_mutex *mMutexMT;
#else // MB_THREADING_VM
#ifdef _OPENMP
    omp_lock_t *mLockMT;
#endif // _OPENMP
#endif // MB_THREADING_VM
#endif // MB_THREADING_TW
#endif // MB_THREADING
#endif // UMC_RESTRICTED_CODE_MBT
    Ipp32s m_iProfileIndex;
    Ipp32u m_uFrames_Num;  // Decode order frame number.
    Ipp32u m_uFrameCounter;
    Ipp32s m_Pitch;
    Ipp64s m_total_bits_encoded; //Bit counter for all stream

//private:
    ///////////////////////////////////////////////////////////////////////
    // Data
    ///////////////////////////////////////////////////////////////////////

    H264BsRealType**    m_pbitstreams; // Array of bitstreams for threads.

    // Which CPU-specific flavor of algorithm to use.
    EnumPicClass            m_PicClass;
    H264SliceHeader         m_SliceHeader;
    H264SeqParamSet         *m_SeqParamSet;
    H264PicParamSet         *m_PicParamSet;
    bool                    m_is_sps_alloc;
    bool                    m_is_pps_alloc;
    EnumPicCodType          m_PicType;

    Ipp32s                  m_PicOrderCnt_LastIDR; // Accumulator to compensate POC resets on IDR frames.


    bool use_implicit_weighted_bipred;

    Ipp16u uNumSlices;                  // Number of Slices in the Frame

    Ipp32u m_slice_length;                // Number of Macroblocks in each slice
    // Except the last slice which may have
    // more macroblocks.

    Ipp32u m_uSliceRemainder;         // Number of extra macroblocks in last slice.

    Ipp32u* m_EmptyThreshold; // empty block threshold table.
    Ipp32u* m_DirectBSkipMEThres;
    Ipp32u* m_PSkipMEThres;
    Ipp32s* m_BestOf5EarlyExitThres;

    // ***************************************************************************
    // Rate control related fields.
    // ***************************************************************************


    Ipp32s  m_DirectTypeStat[2]; // not thread-safe!

    H264SEIData m_SEIData;

    Ipp16u m_MbDataPicStruct;

    //f
    H264_AVBR avbr;
    VideoBrc *brc;

    // true if nSlice is 0, allows not row-based slices
    bool free_slice_mode_enabled;

    // SVC layers
    struct SVCLayerParams m_svc_layer;
    SVCResizeMBPos* svc_ResizeMap[2]; // bounds for intra/inter pred
    Ipp8u *svc_UpsampleMem;
    IppiUpsampleSVCSpec *svc_UpsampleIntraLumaSpec;
    IppiUpsampleSVCSpec *svc_UpsampleIntraChromaSpec;
    IppiUpsampleSVCSpec *svc_UpsampleResidLumaSpec;
    IppiUpsampleSVCSpec *svc_UpsampleResidChromaSpec;
    Ipp8u *svc_UpsampleBuf;

    H264SVCScalabilityInfoSEI* svc_ScalabilityInfoSEI;
    Ipp8u QualityNum;
    Ipp8u TempIdMax;
    Ipp16u TemporalScaleList[8];
    Ipp32s rateDivider;    // highest temporal frame rate ratio between this and highest spatial
#ifdef USE_TEMPTEMP
    SVCRefControl* refctrl; // For temporal; contains index in GOP of the last frame, which refers to it (display order)
    Ipp32s  temptemplen;    // length of the temporal template
    Ipp32s* temptempstart;  // FrameTag for first frame in temporal template
    Ipp32s temptempwidth;   // step back when went to the end of template
    Ipp32s* usetemptemp;     // use temporal template
#endif // USE_TEMPTEMP

    bool m_spatial_resolution_change;
    bool m_restricted_spatial_resolution_change;
    bool m_constrained_intra_pred_flag; // true if used as spatial reference
    bool m_deblocking_IL;
    Ipp32s m_deblocking_stage2;
   // bool m_next_spatial_resolution_change;
    Ipp32u requiredBsBufferSize;
    Ipp8u recodeFlag;
//    Ipp64s m_representationBF;


    // big buffers for svc resampling, provided from upper component
    Ipp32s * m_tmpBuf0; // upmost layer size
    Ipp32s * m_tmpBuf1; // upmost height
    Ipp32s * m_tmpBuf2; // upmost layer size

    COEFFSTYPE *m_SavedMacroblockCoeffs;
    COEFFSTYPE *m_SavedMacroblockTCoeffs;
    COEFFSTYPE *m_ReservedMacroblockTCoeffs;

    COEFFSTYPE *m_pYResidual;
    COEFFSTYPE *m_pUResidual;
    COEFFSTYPE *m_pVResidual;

    COEFFSTYPE *m_pYInputResidual;
    COEFFSTYPE *m_pUInputResidual;
    COEFFSTYPE *m_pVInputResidual;

    // Residuals over base intra are zeroed
    COEFFSTYPE *m_pYInputBaseResidual;
    COEFFSTYPE *m_pUInputBaseResidual;
    COEFFSTYPE *m_pVInputBaseResidual;

    PIXTYPE *m_pYInputRefPlane;
    PIXTYPE *m_pUInputRefPlane;
    PIXTYPE *m_pVInputRefPlane;

    PIXTYPE *m_pYDeblockingILPlane;
    PIXTYPE *m_pUDeblockingILPlane;
    PIXTYPE *m_pVDeblockingILPlane;

    //Ipp32s resPredFlag; // moved to slice (thread) level

#ifdef SLICE_THREADING_LOAD_BALANCING
    // Load balancing for slice level multithreading
    bool slice_load_balancing_enabled;
    Ipp64u *m_B_ticks_per_macroblock; // Array of timings for every P macroblock of the previous frame
    Ipp64u *m_P_ticks_per_macroblock; // Array of timings for every B macroblock of the previous frame
    Ipp64u m_B_ticks_per_frame; // Total time used to encode all macroblocks in the frame
    Ipp64u m_P_ticks_per_frame; // Total time used to encode all macroblocks in the frame
    Ipp8u m_B_ticks_data_available;
    Ipp8u m_P_ticks_data_available;
#endif // SLICE_THREADING_LOAD_BALANCING

    Ipp16s              m_view_id;
    bool                m_is_mvc_profile;
    H264MVCNALHeader    mvc_header;

    Ipp8u           *m_pMbsData;
    H264GlobalMacroblocksDescriptor m_mbinfo_saved;

    H264CoreEncoderType *ref_enc;
} H264CoreEncoderType;

void H264ENC_MAKE_NAME(ExpandPlane)(
    PIXTYPE* StartPtr,
    Ipp32s   frameWidth,
    Ipp32s   frameHeight,
    Ipp32s   pitchPixels,
    Ipp32s   pels);

void H264ENC_MAKE_NAME(ExpandPlane_NV12)(
    PIXTYPE* StartPtr,
    Ipp32s   frameWidth,
    Ipp32s   frameHeight,
    Ipp32s   pitchPixels,
    Ipp32s   pels);

void H264ENC_MAKE_NAME(PlanarPredictLuma)(
    PIXTYPE* pBlock,
    Ipp32u uPitch,
    PIXTYPE* pPredBuf,
    Ipp32s bitDepth);

void H264ENC_MAKE_NAME(PlanarPredictChroma)(
    PIXTYPE* pBlock,
    Ipp32u uPitch,
    PIXTYPE* pPredBuf,
    Ipp32s bitDepth,
    Ipp32s idc);

//-------- H264CoreEncoder<class PixType, class CoeffsType> -----------// end

#undef sH264CoreEncoderType
#undef H264CoreEncoderType
#undef DeblockingParametersType
#undef DeblockingParametersMBAFFType
#undef H264BsRealType
#undef H264BsFakeType
#undef sH264SliceType
#undef H264SliceType
#undef H264CurrentMacroblockDescriptorType
#undef T_RLE_DataType
#undef ME_InfType
#undef H264EncoderFrameType
#undef H264EncoderFrameListType
#undef EncoderRefPicListType
#undef EncoderRefPicListStructType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
