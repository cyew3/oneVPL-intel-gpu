/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_iclonebale.h"

//used by source reader to provide different chroma formats to encoder
class IBitstreamConverter
    : public ICloneable
{
    DECLARE_CLONE(IBitstreamConverter);
public:
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) = 0;
    virtual std::pair<mfxU32, mfxU32> GetTransformType(int nPosition) = 0;
};

class IBitstreamConverterFactory
{
public:
    virtual ~IBitstreamConverterFactory() {}
    virtual IBitstreamConverter * Create(mfxU32 inFourcc, mfxU32 outFourcc) = 0;
};