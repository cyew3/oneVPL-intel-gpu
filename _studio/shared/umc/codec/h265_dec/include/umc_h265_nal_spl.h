//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_NAL_SPL_H
#define __UMC_H265_NAL_SPL_H

#include <vector>
#include "umc_h265_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_h265_heap.h"

namespace UMC_HEVC_DECODER
{

// Big endian to little endian converter class
class SwapperBase
{
public:
    virtual ~SwapperBase() {}

    virtual void SwapMemory(Ipp8u *pDestination, size_t &nDstSize, Ipp8u *pSource, size_t nSrcSize, std::vector<Ipp32u> *pRemovedOffsets) = 0;
    virtual void SwapMemory(MemoryPiece * pMemDst, MemoryPiece * pMemSrc, std::vector<Ipp32u> *pRemovedOffsets) = 0;
};

// NAL unit start code search class
class StartCodeIteratorBase
{
public:

    StartCodeIteratorBase()
        : m_pSource(0)
        , m_nSourceSize(0)
        , m_pSourceBase(0)
        , m_nSourceBaseSize(0)
        , m_suggestedSize(10 * 1024)
    {
    }

    virtual ~StartCodeIteratorBase()
    {
    }

    virtual Ipp32s Init(UMC::MediaData * pSource)
    {
        m_pSourceBase = m_pSource = (Ipp8u *) pSource->GetDataPointer();
        m_nSourceBaseSize = m_nSourceSize = pSource->GetDataSize();
        return 0;
    }

    Ipp32s GetCurrentOffset()
    {
        return (Ipp32s)(m_pSource - m_pSourceBase);
    }

    // Set maximum NAL unit size
    virtual void SetSuggestedSize(size_t size)
    {
        if (size > m_suggestedSize)
            m_suggestedSize = size;
    }

    // Returns first NAL unit ID in memory buffer
    virtual Ipp32s CheckNalUnitType(UMC::MediaData * pSource) = 0;
    // Set bitstream pointer to start code address
    virtual Ipp32s MoveToStartCode(UMC::MediaData * pSource) = 0;
    // Set destination bitstream pointer and size to NAL unit
    virtual Ipp32s GetNALUnit(UMC::MediaData * pSource, UMC::MediaData * pDst) = 0;

    virtual void Reset() = 0;

protected:
    Ipp8u * m_pSource;
    size_t  m_nSourceSize;

    Ipp8u * m_pSourceBase;
    size_t  m_nSourceBaseSize;

    size_t  m_suggestedSize;
};

// NAL unit splitter utility class
class NALUnitSplitter_H265
{
public:

    NALUnitSplitter_H265();

    virtual ~NALUnitSplitter_H265();

    // Initialize splitter with default values
    virtual void Init();
    // Free resources
    virtual void Release();

    // Returns first NAL unit ID in memory buffer
    virtual Ipp32s CheckNalUnitType(UMC::MediaData * pSource);
    // Set bitstream pointer to start code address
    virtual Ipp32s MoveToStartCode(UMC::MediaData * pSource);
    // Set destination bitstream pointer and size to NAL unit
    virtual UMC::MediaDataEx * GetNalUnits(UMC::MediaData * in);

    // Reset state
    virtual void Reset();

    // Set maximum NAL unit size
    virtual void SetSuggestedSize(size_t size)
    {
        if (!m_pStartCodeIter)
            return;

        m_pStartCodeIter->SetSuggestedSize(size);
    }

    SwapperBase * GetSwapper()
    {
        return m_pSwapper;
    }

protected:

    SwapperBase *   m_pSwapper;
    StartCodeIteratorBase * m_pStartCodeIter;

    UMC::MediaDataEx m_MediaData;
    UMC::MediaDataEx::_MediaDataEx m_MediaDataEx;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_NAL_SPL_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
