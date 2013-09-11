//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
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
extern ALIGNED_SSE2 signed int rounder_4[];
extern ALIGNED_SSE2 unsigned char kfh_shuff[];

extern ALIGNED_SSE2 signed short kfh_0000[];
extern ALIGNED_SSE2 signed short kfh_0001[];
extern ALIGNED_SSE2 signed short kfh_0002[];
extern ALIGNED_SSE2 signed short kfh_0003[];
extern ALIGNED_SSE2 signed short kfh_0004[];
extern ALIGNED_SSE2 signed short kfh_0005[];
extern ALIGNED_SSE2 signed short kfh_0006[];
extern ALIGNED_SSE2 signed short kfh_0007[];
extern ALIGNED_SSE2 signed short kfh_0008[];
extern ALIGNED_SSE2 signed short kfh_0009[];
extern ALIGNED_SSE2 signed short kfh_0010[];
extern ALIGNED_SSE2 signed short kfh_0011[];
extern ALIGNED_SSE2 signed short kfh_0012[];
extern ALIGNED_SSE2 signed short kfh_0013[];
extern ALIGNED_SSE2 signed short kfh_0014[];
extern ALIGNED_SSE2 signed short kfh_0015[];

extern ALIGNED_SSE2 signed short kfh_0100[];
extern ALIGNED_SSE2 signed short kfh_0101[];
extern ALIGNED_SSE2 signed short kfh_0102[];
extern ALIGNED_SSE2 signed short kfh_0103[];
extern ALIGNED_SSE2 signed short kfh_0104[];
extern ALIGNED_SSE2 signed short kfh_0105[];
extern ALIGNED_SSE2 signed short kfh_0106[];
extern ALIGNED_SSE2 signed short kfh_0107[];
extern ALIGNED_SSE2 signed short kfh_0108[];
extern ALIGNED_SSE2 signed short kfh_0109[];
extern ALIGNED_SSE2 signed short kfh_0110[];
extern ALIGNED_SSE2 signed short kfh_0111[];
extern ALIGNED_SSE2 signed short kfh_0112[];
extern ALIGNED_SSE2 signed short kfh_0113[];
extern ALIGNED_SSE2 signed short kfh_0114[];
extern ALIGNED_SSE2 signed short kfh_0115[];

extern ALIGNED_SSE2 signed short kfh_0200[];
extern ALIGNED_SSE2 signed short kfh_0201[];
extern ALIGNED_SSE2 signed short kfh_0202[];
extern ALIGNED_SSE2 signed short kfh_0203[];
extern ALIGNED_SSE2 signed short kfh_0204[];
extern ALIGNED_SSE2 signed short kfh_0205[];
extern ALIGNED_SSE2 signed short kfh_0206[];
extern ALIGNED_SSE2 signed short kfh_0207[];

extern ALIGNED_SSE2 signed short kfh_0300[];
extern ALIGNED_SSE2 signed short kfh_0301[];
extern ALIGNED_SSE2 signed short kfh_0400[];

// koefs for vertical 1-D transform
extern ALIGNED_SSE2 signed int rounder_1k[];

extern ALIGNED_SSE2 signed short kfv_0000[];
extern ALIGNED_SSE2 signed short kfv_0001[];
extern ALIGNED_SSE2 signed short kfv_0002[];
extern ALIGNED_SSE2 signed short kfv_0003[];
extern ALIGNED_SSE2 signed short kfv_0004[];
extern ALIGNED_SSE2 signed short kfv_0005[];
extern ALIGNED_SSE2 signed short kfv_0006[];
extern ALIGNED_SSE2 signed short kfv_0007[];
extern ALIGNED_SSE2 signed short kfv_0008[];
extern ALIGNED_SSE2 signed short kfv_0009[];
extern ALIGNED_SSE2 signed short kfv_0010[];
extern ALIGNED_SSE2 signed short kfv_0011[];
extern ALIGNED_SSE2 signed short kfv_0012[];
extern ALIGNED_SSE2 signed short kfv_0013[];
extern ALIGNED_SSE2 signed short kfv_0014[];
extern ALIGNED_SSE2 signed short kfv_0015[];

extern ALIGNED_SSE2 signed short kfv_0100[];
extern ALIGNED_SSE2 signed short kfv_0101[];
extern ALIGNED_SSE2 signed short kfv_0102[];
extern ALIGNED_SSE2 signed short kfv_0103[];
extern ALIGNED_SSE2 signed short kfv_0104[];
extern ALIGNED_SSE2 signed short kfv_0105[];
extern ALIGNED_SSE2 signed short kfv_0106[];
extern ALIGNED_SSE2 signed short kfv_0107[];
extern ALIGNED_SSE2 signed short kfv_0108[];
extern ALIGNED_SSE2 signed short kfv_0109[];
extern ALIGNED_SSE2 signed short kfv_0110[];
extern ALIGNED_SSE2 signed short kfv_0111[];
extern ALIGNED_SSE2 signed short kfv_0112[];
extern ALIGNED_SSE2 signed short kfv_0113[];
extern ALIGNED_SSE2 signed short kfv_0114[];
extern ALIGNED_SSE2 signed short kfv_0115[];

extern ALIGNED_SSE2 signed int kfv_0200[];
extern ALIGNED_SSE2 signed int kfv_0201[];
extern ALIGNED_SSE2 signed int kfv_0202[];
extern ALIGNED_SSE2 signed int kfv_0203[];
extern ALIGNED_SSE2 signed int kfv_0204[];
extern ALIGNED_SSE2 signed int kfv_0205[];
extern ALIGNED_SSE2 signed int kfv_0206[];
extern ALIGNED_SSE2 signed int kfv_0207[];
extern ALIGNED_SSE2 signed int kfv_0208[];
extern ALIGNED_SSE2 signed int kfv_0209[];
extern ALIGNED_SSE2 signed int kfv_0210[];
extern ALIGNED_SSE2 signed int kfv_0211[];
extern ALIGNED_SSE2 signed int kfv_0212[];
extern ALIGNED_SSE2 signed int kfv_0213[];
extern ALIGNED_SSE2 signed int kfv_0214[];
extern ALIGNED_SSE2 signed int kfv_0215[];

extern ALIGNED_SSE2 signed int kfv_0300[];
extern ALIGNED_SSE2 signed int kfv_0301[];
extern ALIGNED_SSE2 signed int kfv_0302[];
extern ALIGNED_SSE2 signed int kfv_0303[];
extern ALIGNED_SSE2 signed int kfv_0304[];
extern ALIGNED_SSE2 signed int kfv_0305[];
extern ALIGNED_SSE2 signed int kfv_0306[];
extern ALIGNED_SSE2 signed int kfv_0307[];

// 32-bits constants
extern signed int rounder_64[];
extern signed int rounder_2048[];

// 16-bits constants
extern signed short koef0000[];
extern signed short koef0001[];
extern signed short koef0002[];
extern signed short koef0003[];
extern signed short koef0004[];
extern signed short koef0005[];
extern signed short koef0006[];
extern signed short koef0007[];
extern signed short koef0008[];
extern signed short koef0009[];
extern signed short koef0010[];
extern signed short koef00010[];
extern signed short koef02030[];
extern signed short koef04050[];
extern signed short koef06070[];
extern signed short koef08090[];
extern signed short koef10110[];
extern signed short koef12130[];
extern signed short koef14150[];
extern signed short koef00011[];
extern signed short koef02031[];
extern signed short koef04051[];
extern signed short koef06071[];
extern signed short koef08091[];
extern signed short koef10111[];
extern signed short koef12131[];
extern signed short koef14151[];
extern signed short koef00012[];
extern signed short koef00013[];
extern signed short koef02032[];
extern signed short koef02033[];
extern signed short koef04052[];
extern signed short koef04053[];
extern signed short koef06072[];
extern signed short koef06073[];
extern signed short koef08092[];
extern signed short koef08093[];
extern signed short koef10112[];
extern signed short koef10113[];
extern signed short koef12132[]; 
extern signed short koef12133[];
extern signed short koef14152[];
extern signed short koef14153[];

extern unsigned int repos[32];
}

#endif
#endif
