/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2020 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_iclonebale.h"
#include "fast_copy.h"

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
    inline mfxStatus SetInputFrameSize(mfxU16 inWidth, mfxU16 inHeight)
    {
        m_inWidth = inWidth;
        m_inHeight = inHeight;
        return MFX_ERR_NONE;
    }
protected:
    mfxStatus FastCopy(mfxU8* pDst, mfxU32 dstPitch, mfxU8* pSrc, mfxU32 srcPitch, IppiSize roi, int flag)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "BitstreamConverterCopy");

        if (NULL == pDst || NULL == pSrc)
        {
            return MFX_ERR_NULL_PTR;
        }

        /* The purpose of mutex here is to make the Copy() atomic.
         * Without it CPU utilization grows dramatically due to cache trashing.
         */
        static UMC::Mutex mutex; // This is thread-safe since C++11
        UMC::AutomaticUMCMutex guard(mutex);

        IppStatus sts = ippiCopyManaged_8u_C1R(pSrc, srcPitch, pDst, dstPitch, roi, flag);

        return sts == ippStsNoErr ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
    }

    const mfxU32 m_inFourCC;
    const mfxU32 m_outFourCC;
    mfxU16 m_inWidth;
    mfxU16 m_inHeight;
};

class IBitstreamConverterFactory
{
public:
    virtual ~IBitstreamConverterFactory() {}
    virtual IBitstreamConverter* MakeConverter(mfxU32 inFourcc, mfxU32 outFourcc) = 0;
};