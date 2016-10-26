//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2007 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_INTERLACE_CBPCY_TABLES_H__
#define __UMC_VC1_COMMON_INTERLACE_CBPCY_TABLES_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 125: interlaced CBPCY table 0
const extern Ipp32s VC1_InterlacedCBPCYTable0[];

//VC-1 Table 126: interlaced CBPCY table 1
const extern Ipp32s VC1_InterlacedCBPCYTable1[];

//VC-1 Table 127: interlaced CBPCY table 2
const extern Ipp32s VC1_InterlacedCBPCYTable2[];

//VC-1 Table 128: interlaced CBPCY table 3
const extern Ipp32s VC1_InterlacedCBPCYTable3[];

//VC-1 Table 129: interlaced CBPCY table 4
const extern Ipp32s VC1_InterlacedCBPCYTable4[];

//VC-1 Table 130: interlaced CBPCY table 5
const extern Ipp32s VC1_InterlacedCBPCYTable5[];

//VC-1 Table 131: interlaced CBPCY table 6
const extern Ipp32s VC1_InterlacedCBPCYTable6[];

//VC-1 Table 132: interlaced CBPCY table 7
const extern Ipp32s VC1_InterlacedCBPCYTable7[];

#endif //__VC1_DEC_INTERLACE_CBPCY_TABLES_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
