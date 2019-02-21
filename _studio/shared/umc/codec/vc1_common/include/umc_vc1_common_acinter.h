// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_ACINTER_H__
#define __UMC_VC1_COMMON_ACINTER_H__

#include "umc_vc1_common_defs.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////High Motion Inter///////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 188: High Motion Inter Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_HighMotionInterDeltaLevelLast0[27];

//VC-1 Table 189: High Motion Inter Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_HighMotionInterDeltaLevelLast1[37];

//VC-1 Table 190: High Motion Inter Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_HighMotionInterDeltaRunLast0[24];

//VC-1 Table 191: High Motion Inter Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_HighMotionInterDeltaRunLast1[10];



//////////////////////////////////////////////////////////////////////////
//////////////////////Low Motion Inter///////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 202: Low Motion Inter Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_LowMotionInterDeltaLevelLast0[30];

//VC-1 Table 203: Low Motion Inter Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_LowMotionInterDeltaLevelLast1[44];

//VC-1 Table 204: Low Motion Inter Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_LowMotionInterDeltaRunLast0[15];

//VC-1 Table 205: Low Motion Inter Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_LowMotionInterDeltaRunLast1[6];

//////////////////////////////////////////////////////////////////////////
//////////////////////Mid Rate Inter//////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 216: Mid Rate Inter Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_MidRateInterDeltaLevelLast0[27];

//VC-1 Table 217: Mid Rate Inter Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_MidRateInterDeltaLevelLast1[41];

//VC-1 Table 218: Mid Rate Inter Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_MidRateInterDeltaRunLast0[13];

//VC-1 Table 219: Mid Rate Inter Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_MidRateInterDeltaRunLast1[4];

//////////////////////////////////////////////////////////////////////////
//////////////////////High Rate Inter/////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//VC-1 Table 230: High Rate Inter Delta Level Indexed by Run Table (Last = 0)
const extern int8_t VC1_HighRateInterDeltaLevelLast0[25];

//VC-1 Table 231: High Rate Inter Delta Level Indexed by Run Table (Last = 1)
const extern int8_t VC1_HighRateInterDeltaLevelLast1[31];

//VC-1 Table 232: High Rate Inter Delta Run Indexed by Level Table (Last = 0)
const extern int8_t VC1_HighRateInterDeltaRunLast0[33];

//VC-1 Table 233: High Rate Inter Delta Run Indexed by Level Table (Last = 1)
const extern int8_t VC1_HighRateInterDeltaRunLast1[5];

#endif //__umc_vc1_common_acinter_H__
#endif //MFX_ENABLE_VC1_VIDEO_DECODE

