/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "ts_surface.h"

class tsSurfaceProvider {
public:
    virtual mfxFrameSurface1* NextSurface() = 0;
    virtual ~tsSurfaceProvider() {};

    static std::unique_ptr<tsSurfaceProvider> CreateIvfVP9Decoder(const char *fileName);
};
