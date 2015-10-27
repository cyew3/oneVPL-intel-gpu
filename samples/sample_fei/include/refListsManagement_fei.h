//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2015 Intel Corporation. All Rights Reserved.
//

#ifndef __REF_LISTS_MANAGEMENT_FEI_H__
#define __REF_LISTS_MANAGEMENT_FEI_H__

#include "sample_fei_defs.h"

void UpdateDpbFrames(iTask& task, mfxU32 field, mfxU32 frameNumMax);
void InitRefPicList(iTask& task, mfxU32 field);
void ModifyRefPicLists(mfxVideoParam & video, iTask& task, mfxU32 field);
void MarkDecodedRefPictures(mfxVideoParam & video, iTask & task, mfxU32 field);

#endif // __REF_LISTS_MANAGEMENT_FEI_H__