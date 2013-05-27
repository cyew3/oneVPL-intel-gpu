/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: mfxstructures.h

*******************************************************************************/

#pragma once

//extension to mediasdk bitstream, that silently can be casted 
struct mfxBitstream2 : mfxBitstream
{
    mfxU16 DependencyId;//splitter might want to put source information, analog to FrameId in surface
    bool   isNull; // whether to use null pointer instead of this object

    //since it is not a POD, value will be default initialized, we have to create a ctor to use zero initializing
    mfxBitstream2()
    {
        MFX_ZERO_MEM(*this);
    }
};

