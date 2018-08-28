//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2010-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TOOLS_H
#define __MFX_TOOLS_H

#include <mfxvideo++int.h>

namespace MFX
{
    unsigned int CreateUniqId();
}

inline
bool IsHWLib(void)
{
#ifdef MFX_VA
    return true;
#else // !MFX_VA
    return false;
#endif // MFX_VA

} // bool IsHWLib(void)

#endif // __MFX_TOOLS_H
