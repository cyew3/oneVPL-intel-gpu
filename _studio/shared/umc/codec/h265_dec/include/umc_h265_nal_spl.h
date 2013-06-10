/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_NAL_SPL_H
#define __UMC_H265_NAL_SPL_H

#include <vector>
#include "umc_h265_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h265_heap.h"

namespace UMC_HEVC_DECODER
{

class SwapperBase
{
public:
    virtual ~SwapperBase() {}

    virtual void SwapMemory(Ipp8u *pDestination, size_t &nDstSize, Ipp8u *pSource, size_t nSrcSize, std::vector<Ipp32u> *pRemovedOffsets) = 0;
    virtual void SwapMemory(MemoryPiece * pMemDst, MemoryPiece * pMemSrc, std::vector<Ipp32u> *pRemovedOffsets) = 0;

    virtual void CopyBitStream(Ipp8u *pDestination, Ipp8u *pSource, size_t &nSrcSize) = 0;
};

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

    virtual void SetSuggestedSize(size_t size)
    {
        if (size > m_suggestedSize)
            m_suggestedSize = size;
    }

    virtual Ipp32s GetNext() = 0;

    virtual Ipp32s CheckNalUnitType(UMC::MediaData * pSource) = 0;
    virtual Ipp32s MoveToStartCode(UMC::MediaData * pSource) = 0;
    virtual Ipp32s GetNALUnit(UMC::MediaData * pSource, UMC::MediaData * pDst) = 0;

    virtual void Reset() = 0;

protected:
    Ipp8u * m_pSource;
    size_t  m_nSourceSize;

    Ipp8u * m_pSourceBase;
    size_t  m_nSourceBaseSize;

    size_t  m_suggestedSize;
};

class NALUnitSplitter_H265
{
public:

    NALUnitSplitter_H265(Heap * heap);

    virtual ~NALUnitSplitter_H265();

    virtual void Init();
    virtual void Release();

    virtual Ipp32s CheckNalUnitType(UMC::MediaData * pSource);
    virtual UMC::MediaDataEx * GetNalUnits(UMC::MediaData * in);

    virtual void Reset();

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

    bool m_bWaitForIDR;
    Heap   *   m_pHeap;
    SwapperBase *   m_pSwapper;
    StartCodeIteratorBase * m_pStartCodeIter;

    UMC::MediaDataEx m_MediaData;
    UMC::MediaDataEx::_MediaDataEx m_MediaDataEx;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_NAL_SPL_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
