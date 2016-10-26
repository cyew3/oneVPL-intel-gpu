//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef UMC_H264_CORE_ENC_H__
#define UMC_H264_CORE_ENC_H__

#include "umc_memory_allocator.h"
#include "umc_h264_defs.h"
#include "umc_h264_bs.h"
#include "umc_h264_enc_cpb.h"
#include "umc_h264_avbr.h"
#include "vm_debug.h"

// These are used to index into Block_RLE below.
#define U_DC_RLE 48     // Used in Intra Prediciton Modes
#define V_DC_RLE 49     // Used in Intra Prediciton Modes
#define Y_DC_RLE 50     // Used in Intra 16x16 Prediciton Mode

#define ANALYSE_I_4x4               (1 << 0)
#define ANALYSE_I_8x8               (1 << 1)
#define ANALYSE_P_4x4               (1 << 2)
#define ANALYSE_P_8x8               (1 << 3)
#define ANALYSE_B_4x4               (1 << 4)
#define ANALYSE_B_8x8               (1 << 5)
#define ANALYSE_SAD                 (1 << 6)
#define ANALYSE_ME_EARLY_EXIT       (1 << 7)
#define ANALYSE_ME_ALL_REF          (1 << 8)
#define ANALYSE_ME_CHROMA           (1 << 9)
#define ANALYSE_ME_SUBPEL           (1 << 10)
#define ANALYSE_CBP_EMPTY           (1 << 11)
#define ANALYSE_RECODE_FRAME        (1 << 12)
#define ANALYSE_ME_AUTO_DIRECT      (1 << 13)
#define ANALYSE_FRAME_TYPE          (1 << 14)
#define ANALYSE_FLATNESS            (1 << 15)
#define ANALYSE_RD_MODE             (1 << 16)
#define ANALYSE_RD_OPT              (1 << 17)
#define ANALYSE_B_RD_OPT            (1 << 18)
#define ANALYSE_CHECK_SKIP_PREDICT  (1 << 19)
#define ANALYSE_CHECK_SKIP_INTPEL   (1 << 20)
#define ANALYSE_CHECK_SKIP_BESTCAND (1 << 21)
#define ANALYSE_CHECK_SKIP_SUBPEL   (1 << 22)
#define ANALYSE_SPLIT_SMALL_RANGE   (1 << 23)
#define ANALYSE_ME_EXT_CANDIDATES   (1 << 24)
#define ANALYSE_ME_SUBPEL_SAD       (1 << 25)
#define ANALYSE_INTRA_IN_ME         (1 << 26)
#define ANALYSE_ME_FAST_MULTIREF    (1 << 27)
#define ANALYSE_FAST_INTRA          (1 << 28)
#define ANALYSE_ME_PRESEARCH        (1 << 29)
#define ANALYSE_ME_CONTINUED_SEARCH (1 << 30)
#define ANALYSE_ME_BIDIR_REFINE     (1 << 31)

// m_Analyse_ex flags, not fully compatible with other flags
#define ANALYSE_SUPERFAST           (1 << 0)
#define ANALYSE_DEBLOCKING_OFF      (1 << 1)

#define ANALYSE_PSY_STAT_MB             (1 << 2)
#define ANALYSE_PSY_STAT_FRAME          (1 << 3)
#define ANALYSE_PSY_DYN_MB              (1 << 4)
#define ANALYSE_PSY_DYN_FRAME           (1 << 5)
#define ANALYSE_PSY_SCENECHANGE         (1 << 6)
#define ANALYSE_PSY_INTERLACE           (1 << 7)
#define ANALYSE_PSY_TYPE_FR             (1 << 8)
#define ANALYSE_PSY_AHEAD               (ANALYSE_PSY_DYN_MB | ANALYSE_PSY_DYN_FRAME | ANALYSE_PSY_SCENECHANGE | ANALYSE_PSY_TYPE_FR)
#define ANALYSE_PSY                     (ANALYSE_PSY_AHEAD | ANALYSE_PSY_STAT_MB | ANALYSE_PSY_STAT_FRAME | ANALYSE_PSY_INTERLACE)
#define ANALYSE_PSY_MB                  (ANALYSE_PSY_DYN_MB | ANALYSE_PSY_STAT_MB)
#define ANALYSE_PSY_FRAME               (ANALYSE_PSY_DYN_FRAME | ANALYSE_PSY_STAT_FRAME)

//Optimal quantization levels
#define OPT_QUANT_INTER_RD 2
#define OPT_QUANT_INTER 0
#define OPT_QUANT_INTRA16x16 0
#define OPT_QUANT_INTRA4x4 1
#define OPT_QUANT_INTRA8x8 1

// Picture structure and decorative flags
typedef enum
{
    H264_PICSTRUCT_UNKNOWN           = 0x00,
    H264_PICSTRUCT_PROGRESSIVE       = 0x01,
    H264_PICSTRUCT_FIELD_TFF         = 0x02,
    H264_PICSTRUCT_FIELD_BFF         = 0x04,
    H264_PICSTRUCT_FIELD_REPEATED    = 0x10,
    H264_PICSTRUCT_FRAME_DOUBLING    = 0x20,
    H264_PICSTRUCT_FRAME_TRIPLING    = 0x40,
} H264PicStruct;

#define CALC_16x16_INTRA_MB_TYPE(slice, mode, nc, ac) (1+IntraMBTypeOffset[slice]+mode+4*nc+12*ac)
#define CALC_4x4_INTRA_MB_TYPE(slice) (IntraMBTypeOffset[slice])
#define CALC_PCM_MB_TYPE(slice) (IntraMBTypeOffset[slice]+25);


namespace UMC_H264_ENCODER
{
    /* Common for 8u and 16u */

    inline Ipp8s GetReferenceField(
        Ipp8s *pFields,
        Ipp32s RefIndex)
    {
        if (RefIndex < 0)
            return -1;
        else {
            VM_ASSERT(pFields[RefIndex] >= 0);
            return pFields[RefIndex];
        }
    }

    // forward declaration of internal types
    typedef void (*H264CoreEncoder_DeblockingFunction)(void* state, Ipp32u nMBAddr);

    /* End of: Common for 8u and 16u */

#define PIXBITS 8
#include "umc_h264_core_enc_tmpl.h"
#undef PIXBITS

#if defined BITDEPTH_9_12

#define PIXBITS 16
#include "umc_h264_core_enc_tmpl.h"
#undef PIXBITS

#endif // BITDEPTH_9_12

} // namespace UMC_H264_ENCODER

#endif // UMC_H264_CORE_ENC_H__

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
