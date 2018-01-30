//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//
#ifndef _TREE_H_
#define _TREE_H_

#include "asc_structures.h"

bool SCDetectRF( mfxI32 diffMVdiffVal, mfxU32 RsCsDiff,   mfxU32 MVDiff,   mfxU32 Rs,       mfxU32 AFD,
                 mfxU32 CsDiff,        mfxI32 diffTSC,    mfxU32 TSC,      mfxU32 gchDC,    mfxI32 diffRsCsdiff,
                 mfxU32 posBalance,    mfxU32 SC,         mfxU32 TSCindex, mfxU32 Scindex,  mfxU32 Cs,
                 mfxI32 diffAFD,       mfxU32 negBalance, mfxU32 ssDCval,  mfxU32 refDCval, mfxU32 RsDiff,
                 mfxU8 control);

#endif //_TREE_H_