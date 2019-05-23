/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2019 Intel Corporation. All Rights Reserved.
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
    IBitstreamConverter(mfxU32 inFourCC, mfxU32 outFourCC)
        : m_inFourCC(inFourCC)
        , m_outFourCC(outFourCC)
    {}
    virtual mfxStatus Transform(mfxBitstream * bs, mfxFrameSurface1 *surface) = 0;
    std::pair<mfxU32, mfxU32>GetFourCCPair() { return std::make_pair(m_inFourCC, m_outFourCC); }
protected:
    const mfxU32 m_inFourCC;
    const mfxU32 m_outFourCC;
};

class IBitstreamConverterFactory
{
public:
    virtual ~IBitstreamConverterFactory() {}
    virtual IBitstreamConverter* MakeConverter(mfxU32 inFourcc, mfxU32 outFourcc) = 0;
};