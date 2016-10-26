//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __HUFF_TABLES_FP_H
#define __HUFF_TABLES_FP_H

#include "ippdc.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern Ipp32s vlcShifts[];
extern Ipp32s vlcOffsets[];
extern Ipp32s vlcTypes[];
extern Ipp32s vlcTuples[];
extern Ipp32s vlcTableSizes[];
extern Ipp32s vlcNumSubTables[];
extern Ipp32s *vlcSubTablesSizes[];
extern IppsVLCTable_32s *vlcBooks[];

#ifdef  __cplusplus
}
#endif


#endif//__HUFF_TABLES_H
