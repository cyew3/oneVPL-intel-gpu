/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: libmfx_core_factory.h

\* ****************************************************************************** */

#ifndef __LIBMFX_CORE_FACTORY_H__
#define __LIBMFX_CORE_FACTORY_H__

#include "mfxvideo++int.h"


class FactoryCORE
{
public:
    static VideoCORE* CreateCORE(eMFXVAType va_type, 
                                 mfxU32 adapterNum, 
                                 mfxU32 numThreadsAvailable, 
                                 mfxSession session = NULL);
};

#endif