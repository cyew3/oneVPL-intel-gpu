//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H265_TRANSFORM_CONSTS_H__
#define __MFX_H265_TRANSFORM_CONSTS_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

namespace MFX_HEVC_PP
{

#define ALIGNED_SSE2 ALIGN_DECL(16)

// koefs for horizontal 1-D transform
extern ALIGNED_SSE2 const signed int rounder_8[4];
extern ALIGNED_SSE2 const signed int rounder_16[4];
extern ALIGNED_SSE2 const signed int rounder_32[4];
extern ALIGNED_SSE2 const unsigned char kfh_shuff[];

extern ALIGNED_SSE2 const signed short kfh_0000[8];
extern ALIGNED_SSE2 const signed short kfh_0001[8];
extern ALIGNED_SSE2 const signed short kfh_0002[8];
extern ALIGNED_SSE2 const signed short kfh_0003[8];
extern ALIGNED_SSE2 const signed short kfh_0004[8];
extern ALIGNED_SSE2 const signed short kfh_0005[8];
extern ALIGNED_SSE2 const signed short kfh_0006[8];
extern ALIGNED_SSE2 const signed short kfh_0007[8];
extern ALIGNED_SSE2 const signed short kfh_0008[8];
extern ALIGNED_SSE2 const signed short kfh_0009[8];
extern ALIGNED_SSE2 const signed short kfh_0010[8];
extern ALIGNED_SSE2 const signed short kfh_0011[8];
extern ALIGNED_SSE2 const signed short kfh_0012[8];
extern ALIGNED_SSE2 const signed short kfh_0013[8];
extern ALIGNED_SSE2 const signed short kfh_0014[8];
extern ALIGNED_SSE2 const signed short kfh_0015[8];

extern ALIGNED_SSE2 const signed short kfh_0100[8];
extern ALIGNED_SSE2 const signed short kfh_0101[8];
extern ALIGNED_SSE2 const signed short kfh_0102[8];
extern ALIGNED_SSE2 const signed short kfh_0103[8];
extern ALIGNED_SSE2 const signed short kfh_0104[8];
extern ALIGNED_SSE2 const signed short kfh_0105[8];
extern ALIGNED_SSE2 const signed short kfh_0106[8];
extern ALIGNED_SSE2 const signed short kfh_0107[8];
extern ALIGNED_SSE2 const signed short kfh_0108[8];
extern ALIGNED_SSE2 const signed short kfh_0109[8];
extern ALIGNED_SSE2 const signed short kfh_0110[8];
extern ALIGNED_SSE2 const signed short kfh_0111[8];
extern ALIGNED_SSE2 const signed short kfh_0112[8];
extern ALIGNED_SSE2 const signed short kfh_0113[8];
extern ALIGNED_SSE2 const signed short kfh_0114[8];
extern ALIGNED_SSE2 const signed short kfh_0115[8];

extern ALIGNED_SSE2 const signed short kfh_0200[8];
extern ALIGNED_SSE2 const signed short kfh_0201[8];
extern ALIGNED_SSE2 const signed short kfh_0202[8];
extern ALIGNED_SSE2 const signed short kfh_0203[8];
extern ALIGNED_SSE2 const signed short kfh_0204[8];
extern ALIGNED_SSE2 const signed short kfh_0205[8];
extern ALIGNED_SSE2 const signed short kfh_0206[8];
extern ALIGNED_SSE2 const signed short kfh_0207[8];

extern ALIGNED_SSE2 const signed short kfh_0300[8];
extern ALIGNED_SSE2 const signed short kfh_0301[8];
extern ALIGNED_SSE2 const signed short kfh_0400[8];

// koefs for vertical 1-D transform
extern ALIGNED_SSE2 const signed int rounder_1k[];

extern ALIGNED_SSE2 const signed short kfv_0000[8];
extern ALIGNED_SSE2 const signed short kfv_0001[8];
extern ALIGNED_SSE2 const signed short kfv_0002[8];
extern ALIGNED_SSE2 const signed short kfv_0003[8];
extern ALIGNED_SSE2 const signed short kfv_0004[8];
extern ALIGNED_SSE2 const signed short kfv_0005[8];
extern ALIGNED_SSE2 const signed short kfv_0006[8];
extern ALIGNED_SSE2 const signed short kfv_0007[8];
extern ALIGNED_SSE2 const signed short kfv_0008[8];
extern ALIGNED_SSE2 const signed short kfv_0009[8];
extern ALIGNED_SSE2 const signed short kfv_0010[8];
extern ALIGNED_SSE2 const signed short kfv_0011[8];
extern ALIGNED_SSE2 const signed short kfv_0012[8];
extern ALIGNED_SSE2 const signed short kfv_0013[8];
extern ALIGNED_SSE2 const signed short kfv_0014[8];
extern ALIGNED_SSE2 const signed short kfv_0015[8];

extern ALIGNED_SSE2 const signed short kfv_0100[8];
extern ALIGNED_SSE2 const signed short kfv_0101[8];
extern ALIGNED_SSE2 const signed short kfv_0102[8];
extern ALIGNED_SSE2 const signed short kfv_0103[8];
extern ALIGNED_SSE2 const signed short kfv_0104[8];
extern ALIGNED_SSE2 const signed short kfv_0105[8];
extern ALIGNED_SSE2 const signed short kfv_0106[8];
extern ALIGNED_SSE2 const signed short kfv_0107[8];
extern ALIGNED_SSE2 const signed short kfv_0108[8];
extern ALIGNED_SSE2 const signed short kfv_0109[8];
extern ALIGNED_SSE2 const signed short kfv_0110[8];
extern ALIGNED_SSE2 const signed short kfv_0111[8];
extern ALIGNED_SSE2 const signed short kfv_0112[8];
extern ALIGNED_SSE2 const signed short kfv_0113[8];
extern ALIGNED_SSE2 const signed short kfv_0114[8];
extern ALIGNED_SSE2 const signed short kfv_0115[8];

extern ALIGNED_SSE2 const signed int kfv_0200[8];
extern ALIGNED_SSE2 const signed int kfv_0201[8];
extern ALIGNED_SSE2 const signed int kfv_0202[8];
extern ALIGNED_SSE2 const signed int kfv_0203[8];
extern ALIGNED_SSE2 const signed int kfv_0204[8];
extern ALIGNED_SSE2 const signed int kfv_0205[8];
extern ALIGNED_SSE2 const signed int kfv_0206[8];
extern ALIGNED_SSE2 const signed int kfv_0207[8];
extern ALIGNED_SSE2 const signed int kfv_0208[8];
extern ALIGNED_SSE2 const signed int kfv_0209[8];
extern ALIGNED_SSE2 const signed int kfv_0210[8];
extern ALIGNED_SSE2 const signed int kfv_0211[8];
extern ALIGNED_SSE2 const signed int kfv_0212[8];
extern ALIGNED_SSE2 const signed int kfv_0213[8];
extern ALIGNED_SSE2 const signed int kfv_0214[8];
extern ALIGNED_SSE2 const signed int kfv_0215[8];

extern ALIGNED_SSE2 const signed int kfv_0300[8];
extern ALIGNED_SSE2 const signed int kfv_0301[8];
extern ALIGNED_SSE2 const signed int kfv_0302[8];
extern ALIGNED_SSE2 const signed int kfv_0303[8];
extern ALIGNED_SSE2 const signed int kfv_0304[8];
extern ALIGNED_SSE2 const signed int kfv_0305[8];
extern ALIGNED_SSE2 const signed int kfv_0306[8];
extern ALIGNED_SSE2 const signed int kfv_0307[8];

// 32-bits constants
extern const signed int rounder_64[8];
extern const signed int rounder_2048[8];
extern const signed int rounder_1024[8];
extern const signed int rounder_512[8];
extern const signed int rounder_256[8];
extern const signed int rounder_128[8];

// 16-bits constants
extern const signed short koef0000[8];
extern const signed short koef0001[8];
extern const signed short koef0002[8];
extern const signed short koef0003[8];
extern const signed short koef0004[8];
extern const signed short koef0005[8];
extern const signed short koef0006[8];
extern const signed short koef0007[8];
extern const signed short koef0008[8];
extern const signed short koef0009[8];
extern const signed short koef0010[8];
extern const signed short koef00010[8];
extern const signed short koef02030[8];
extern const signed short koef04050[8];
extern const signed short koef06070[8];
extern const signed short koef08090[8];
extern const signed short koef10110[8];
extern const signed short koef12130[8];
extern const signed short koef14150[8];
extern const signed short koef00011[8];
extern const signed short koef02031[8];
extern const signed short koef04051[8];
extern const signed short koef06071[8];
extern const signed short koef08091[8];
extern const signed short koef10111[8];
extern const signed short koef12131[8];
extern const signed short koef14151[8];
extern const signed short koef00012[8];
extern const signed short koef00013[8];
extern const signed short koef02032[8];
extern const signed short koef02033[8];
extern const signed short koef04052[8];
extern const signed short koef04053[8];
extern const signed short koef06072[8];
extern const signed short koef06073[8];
extern const signed short koef08092[8];
extern const signed short koef08093[8];
extern const signed short koef10112[8];
extern const signed short koef10113[8];
extern const signed short koef12132[8]; 
extern const signed short koef12133[8];
extern const signed short koef14152[8];
extern const signed short koef14153[8];

extern const unsigned int repos[32];
}

#endif
#endif
