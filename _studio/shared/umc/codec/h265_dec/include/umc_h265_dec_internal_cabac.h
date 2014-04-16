/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_INTERNAL_CABAC_H
#define __UMC_H265_DEC_INTERNAL_CABAC_H

namespace UMC_HEVC_DECODER
{

// Syntax element type for HEVC
enum
{
    SPLIT_CODING_UNIT_FLAG_HEVC     = 0,
    SKIP_FLAG_HEVC                  = 1,
    MERGE_FLAG_HEVC                 = 2,
    MERGE_IDX_HEVC                  = 3,
    PART_SIZE_HEVC                  = 4,
    AMP_SPLIT_POSITION_HEVC         = 5,
    PRED_MODE_HEVC                  = 6,
    INTRA_LUMA_PRED_MODE_HEVC       = 7,
    INTRA_CHROMA_PRED_MODE_HEVC     = 8,
    INTER_DIR_HEVC                  = 9,
    MVD_HEVC                        = 10,
    REF_FRAME_IDX_HEVC              = 11,
    DQP_HEVC                        = 12,
    QT_CBF_HEVC                     = 13,
    QT_ROOT_CBF_HEVC                = 14,
    SIG_COEFF_GROUP_FLAG_HEVC       = 15,
    SIG_FLAG_HEVC                   = 16,
    LAST_X_HEVC                     = 17,
    LAST_Y_HEVC                     = 18,
    ONE_FLAG_HEVC                   = 19,
    ABS_FLAG_HEVC                   = 20,
    MVP_IDX_HEVC                    = 21,
    SAO_MERGE_FLAG_HEVC             = 22,
    SAO_TYPE_IDX_HEVC               = 23,
    TRANS_SUBDIV_FLAG_HEVC          = 24,
    TRANSFORM_SKIP_HEVC             = 25,
    TRANSQUANT_BYPASS_HEVC          = 26,
    MAIN_SYNTAX_ELEMENT_NUMBER_HEVC
};

#define NUM_CTX 186

// CABAC contexts initialization values offset in a common table of all contexts
// ML: OPT: Moved into header to allow accesses be resolved at compile time
const
Ipp32u ctxIdxOffsetHEVC[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC] =
{
    0,   // SPLIT_CODING_UNIT_FLAG_HEVC
    3,   // SKIP_FLAG_HEVC
    6,   // MERGE_FLAG_HEVC
    7,   // MERGE_IDX_HEVC
    8,   // PART_SIZE_HEVC
    12,  // AMP_SPLIT_POSITION_HEVC
    13,  // PRED_MODE_HEVC
    14,  // INTRA_LUMA_PRED_MODE_HEVC
    15,  // INTRA_CHROMA_PRED_MODE_HEVC
    17,  // INTER_DIR_HEVC
    22,  // MVD_HEVC
    24,  // REF_FRAME_IDX_HEVC
    26,  // DQP_HEVC
    29,  // QT_CBF_HEVC
    39,  // QT_ROOT_CBF_HEVC
    40,  // SIG_COEFF_GROUP_FLAG_HEVC
    44,  // SIG_FLAG_HEVC
    86,  // LAST_X_HEVC
    116, // LAST_Y_HEVC
    146, // ONE_FLAG_HEVC
    170, // ABS_FLAG_HEVC
    176, // MVP_IDX_HEVC
    178, // SAO_MERGE_FLAG_HEVC
    179, // SAO_TYPE_IDX_HEVC
    180, // TRANS_SUBDIV_FLAG_HEVC
    183, // TRANSFORM_SKIP_HEVC
    185  // TRANSQUANT_BYPASS_HEVC
};

} // namespace UMC_HEVC_DECODER

#endif //__UMC_H265_DEC_INTERNAL_CABAC_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
