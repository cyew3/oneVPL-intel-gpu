// Copyright (c) 2004-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_ACINTRA_H__
#define __UMC_VC1_COMMON_ACINTRA_H__

#include "umc_vc1_common_defs.h"
//Tables 178 - 184 High motion Intra
//Tables 192 - 198 Low  motion Intra
//Tables 206 - 212 Mid Rate Intra
//Tables 220 - 226 High Rate Intra

//////////////////////////////////////////////////////////////////////////
//////////////////////High Motion Intra///////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 181: High Motion Intra Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_HighMotionIntraDeltaLevelLast0[31];

//VC-1 Table 182: High Motion Intra Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_HighMotionIntraDeltaLevelLast1[38];

//VC-1 Table 183: High Motion Intra Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_HighMotionIntraDeltaRunLast0[20];

//VC-1 Table 184: High Motion Intra Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_HighMotionIntraDeltaRunLast1[7];

//////////////////////////////////////////////////////////////////////////
//////////////////////Low Motion Intra///////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 195: Low Motion Intra Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_LowMotionIntraDeltaLevelLast0[21];

//VC-1 Table 196: Low Motion Intra Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_LowMotionIntraDeltaLevelLast1[27];

//VC-1 Table 197: Low Motion Intra Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_LowMotionIntraDeltaRunLast0[16+1];

//VC-1 Table 198: Low Motion Intra Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_LowMotionIntraDeltaRunLast1[4+1];

//////////////////////////////////////////////////////////////////////////
//////////////////////Mid Rate Intra//////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 209: Mid Rate Intra Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_MidRateIntraDeltaLevelLast0[15];

//VC-1 Table 210: Mid Rate Intra Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_MidRateIntraDeltaLevelLast1[21];

//VC-1 Table 211: Mid Rate Intra Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_MidRateIntraDeltaRunLast0[28];

//VC-1 Table 212: Mid Rate Intra Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_MidRateIntraDeltaRunLast1[9];

//////////////////////////////////////////////////////////////////////////
//////////////////////High Rate Intra//////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 223: High Rate Intra Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_HighRateIntraDeltaLevelLast0[15];

//VC-1 Table 224: High Rate Intra Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_HighRateIntraDeltaLevelLast1[17];

//VC-1 Table 225: High Rate Intra Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_HighRateIntraDeltaRunLast0[57];

//VC-1 Table 226: High Rate Intra Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_HighRateIntraDeltaRunLast1[5];

#endif //__umc_vc1_common_acintra_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
