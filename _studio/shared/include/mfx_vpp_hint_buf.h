//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2009 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_VPP_HINT_BUF_H__
#define __MFX_VPP_HINT_BUF_H__

#include "mfxdefs.h"
#include "mfxstructures.h"

typedef struct _mfxVPPHintDenoise {
  mfxExtBuffer    Header;
} mfxVPPHintDenoise;

typedef struct _mfxVPPHintRangeMap {
  mfxExtBuffer    Header;
  mfxU8  RangeMap;
  mfxU8  Reserved[3];
} mfxVPPHintRangeMap;

typedef struct _mfxVPPHintVideoAnalysis {
  mfxExtBuffer    Header;

  /*mfxU32          SpatialComplexity;
  mfxU32          TemporalComplexity;
  mfxU32          SceneChangeRate;*/

} mfxVPPHintVideoAnalysis;

#endif //__MFX_VPP_HINT_BUF_H__