//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2010 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#include <string.h>
#include <stddef.h>
#include <math.h>
#include "ippdefs.h"
#include "vm_time.h"
#include "vm_interlocked.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_core_enc.h"
#include "umc_h264_tables.h"
#include "umc_h264_to_ipp.h"
#include "umc_h264_bme.h"
#include "umc_h264_defs.h"
#include "umc_h264_wrappers.h"

// Table to obtain edge info for a 4x4 block of a MB. The table entry when
// OR'd with the edge info for the MB, results in edge info for the block.
//
//  H264 4x4 Block ordering in a 16x16 Macroblock and edge assignments
//
//  ULC = Upper Left Corner, U = Upper Edge
//  L = Left Edge, R = Right Edge
//
//               luma (Y)                chroma (U)          chroma (V)
//
//        +-U--+-U--+-U--+-U--+         +-U--+-U--+         +-U--+-U--+
//        |    |    |    |    |         |    |    |         |    |    |
// ULC--> L 0  | 1  | 4  | 5  R  ULC--> L 16 | 17 R  ULC--> L 20 | 21 R
//        |    |    |    |    |         |    |    |         |    |    |
//        +----+----+----+----+         +----+----+         +----+----+
//        |    |    |    |    |         |    |    |         |    |    |
//        L 2  | 3  | 6  | 7  R         L 18 | 19 R         L 22 | 23 R
//        |    |    |    |    |         |    |    |         |    |    |
//        +----+----+----+----+         +----+----+         +----+----+
//        |    |    |    |    |
//        L 8  | 9  | 12 | 13 R
//        |    |    |    |    |
//        +----+----+----+----+
//        |    |    |    |    |
//        L 10 | 11 | 14 | 15 R
//        |    |    |    |    |
//        +----+----+----+----+
//
//  This table provides easy look-up by block number to determine
//  which edges is does NOT border on.


namespace UMC_H264_ENCODER
{

const H264MotionVector null_mv = {0};

inline Ipp32s convertLumaQP(Ipp32s QP, Ipp32s bit_depth_src, Ipp32s bit_depth_dst)
{
    Ipp32s qpy = QP + 6*(bit_depth_src - 8);
    Ipp32s qpDstOffset = 6*(bit_depth_dst - 8);
    Ipp32s minQpDst = -qpDstOffset;
    Ipp32s maxQpDst = 51;
    qpy = qpy - qpDstOffset;
    qpy = (qpy < minQpDst)? minQpDst : (qpy > maxQpDst)? maxQpDst : qpy;
    return(qpy);
}

#define PIXBITS 8
#include "umc_h264_core_enc_tmpl.cpp.h"
#undef PIXBITS

#if defined BITDEPTH_9_12

#define PIXBITS 16
#include "umc_h264_core_enc_tmpl.cpp.h"
#undef PIXBITS

#endif // BITDEPTH_9_12

} //namespace UMC_H264_ENCODER

#endif //UMC_ENABLE_H264_VIDEO_ENCODER

