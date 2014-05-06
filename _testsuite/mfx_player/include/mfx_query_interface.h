/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_icurrent_frame_ctrl.h"
#include "mfx_ivideo_encode.h"

class IVideoEncode;

template <class T>
struct QueryInterfaceMap {
    enum { id = 0};
};

template <>
struct QueryInterfaceMap<IVideoEncode> {
    enum { id = 1};
};

template <>
struct QueryInterfaceMap<ICurrentFrameControl> {
    enum { id = 2};
};
