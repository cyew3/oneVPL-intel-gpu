//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CABAC_H__
#define __MFX_H265_CABAC_H__

namespace H265Enc {

enum // Syntax element type for HEVC
{
    QT_CBF_HEVC                     = 0,
    QT_ROOT_CBF_HEVC                = 1,
    LAST_X_HEVC                     = 2,
    LAST_Y_HEVC                     = 3,
    SIG_COEFF_GROUP_FLAG_HEVC       = 4,
    SIG_FLAG_HEVC                   = 5,
    ONE_FLAG_HEVC                   = 6,
    ABS_FLAG_HEVC                   = 7,
    TRANS_SUBDIV_FLAG_HEVC          = 8,
    TRANSFORMSKIP_FLAG_HEVC         = 9,
    CU_TRANSQUANT_BYPASS_FLAG_CTX   = 10,
    SPLIT_CODING_UNIT_FLAG_HEVC     = 11,
    SKIP_FLAG_HEVC                  = 12,
    MERGE_FLAG_HEVC                 = 13,
    MERGE_IDX_HEVC                  = 14,
    PART_SIZE_HEVC                  = 15,
    AMP_SPLIT_POSITION_HEVC         = 16,
    PRED_MODE_HEVC                  = 17,
    INTRA_LUMA_PRED_MODE_HEVC       = 18,
    INTRA_CHROMA_PRED_MODE_HEVC     = 19,
    INTER_DIR_HEVC                  = 20,
    MVD_HEVC                        = 21,
    REF_FRAME_IDX_HEVC              = 22,
    DQP_HEVC                        = 23,
    MVP_IDX_HEVC                    = 24,
    SAO_MERGE_FLAG_HEVC             = 25,
    SAO_TYPE_IDX_HEVC               = 26,
    END_OF_SLICE_FLAG_HEVC          = 27,

    MAIN_SYNTAX_ELEMENT_NUMBER_HEVC
};

enum {
    RD_CU_ALL = 0,
    RD_CU_SPLITFLAG = 1,
    RD_CU_MODES = 2,
    RD_CU_ALL_EXCEPT_COEFFS = 3,
};

#define CTX(bs,a) ((bs)->m_base.context_array+tab_ctxIdxOffset[a])

static const Ipp32s NUM_CTX_TU               = 152;
static const Ipp32s NUM_SIG_FLAG_CTX_LUMA    =  27;
static const Ipp32s NUM_ONE_FLAG_CTX         = 24;      ///< number of context models for greater than 1 flag
static const Ipp32s NUM_ONE_FLAG_CTX_LUMA    = 16;      ///< number of context models for greater than 1 flag of luma
static const Ipp32s NUM_ONE_FLAG_CTX_CHROMA  =  8;      ///< number of context models for greater than 1 flag of chroma
static const Ipp32s NUM_ABS_FLAG_CTX         =  6;      ///< number of context models for greater than 2 flag
static const Ipp32s NUM_ABS_FLAG_CTX_LUMA    =  4;      ///< number of context models for greater than 2 flag of luma
static const Ipp32s NUM_ABS_FLAG_CTX_CHROMA  =  2;      ///< number of context models for greater than 2 flag of chroma
static const Ipp32s NUM_QT_CBF_CTX           =  5;
static const Ipp32s NUM_CTX_LAST_FLAG_XY     = 15;

} // namespace

#endif // __MFX_H265_CABAC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
