/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_nal_spl.h"
#include "mfx_common.h" //  for trace routines

namespace UMC_HEVC_DECODER
{
// NAL unit definitions
enum
{
    NAL_UNITTYPE_SHIFT_H265     = 1,
    NAL_UNITTYPE_BITS_H265      = 0x7e,
};

// Change memory region to little endian for reading with 32-bit DWORDs and remove start code emulation prevention byteps
void SwapMemoryAndRemovePreventingBytes_H265(void *pDestination, size_t &nDstSize, void *pSource, size_t nSrcSize, std::vector<Ipp32u> *pRemovedOffsets);

static const Ipp8u start_code_prefix[] = {0, 0, 0, 1};

// Search bitstream for start code
static Ipp32s FindStartCode(const Ipp8u *pb, size_t &nSize)
{
    // there is no data
    if ((Ipp32s) nSize < 4)
        return -1;

    // find start code
    while ((4 <= nSize) && ((0 != pb[0]) ||
                            (0 != pb[1]) ||
                            (1 != pb[2])))
    {
        pb += 1;
        nSize -= 1;
    }

    if (4 <= nSize)
        return ((pb[0] << 24) | (pb[1] << 16) | (pb[2] << 8) | (pb[3]));

    return -1;

} // Ipp32s FindStartCode(Ipp8u * (&pb), size_t &nSize)

// NAL unit splitter class
class StartCodeIterator : public StartCodeIteratorBase
{
public:

    StartCodeIterator()
        : m_code(-1)
        , m_pts(-1)
    {
        Reset();
    }

    virtual void Reset()
    {
        m_code = -1;
        m_pts = -1;
        m_prev.clear();
    }

    // Initialize with bitstream buffer
    virtual Ipp32s Init(UMC::MediaData * pSource)
    {
        Reset();
        StartCodeIteratorBase::Init(pSource);
        Ipp32s iCode = UMC_HEVC_DECODER::FindStartCode(m_pSource, m_nSourceSize);
        return iCode;
    }

    // Returns first NAL unit ID in memory buffer
    virtual Ipp32s CheckNalUnitType(UMC::MediaData * pSource)
    {
        if (!pSource)
            return -1;

        Ipp8u * source = (Ipp8u *)pSource->GetDataPointer();
        size_t  size = pSource->GetDataSize();

        Ipp32s startCodeSize;
        return FindStartCode(source, size, startCodeSize);
    }

    // Set bitstream pointer to start code address
    virtual Ipp32s MoveToStartCode(UMC::MediaData * pSource)
    {
        if (!pSource)
            return -1;

        if (m_code == -1)
            m_prev.clear();

        Ipp8u * source = (Ipp8u *)pSource->GetDataPointer();
        size_t  size = pSource->GetDataSize();

        Ipp32s startCodeSize;
        Ipp32s iCodeNext = FindStartCode(source, size, startCodeSize);

        pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));
        if (iCodeNext != -1)
        {
             pSource->MoveDataPointer(-startCodeSize);
        }

        return iCodeNext;
    }

    // Set destination bitstream pointer and size to NAL unit
    virtual Ipp32s GetNALUnit(UMC::MediaData * pSource, UMC::MediaData * pDst)
    {
        if (!pSource)
            return EndOfStream(pDst);

        Ipp32s iCode = GetNALUnitInternal(pSource, pDst);
        if (iCode == -1)
        {
            bool endOfStream = pSource && ((pSource->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_END_OF_STREAM) != 0);
            if (endOfStream)
                iCode = EndOfStream(pDst);
        }

        return iCode;
    }

    // Set destination bitstream pointer and size to NAL unit
    Ipp32s GetNALUnitInternal(UMC::MediaData * pSource, UMC::MediaData * pDst)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "GetNALUnitInternal");
        static const Ipp8u start_code_prefix[] = {0, 0, 1};

        if (m_code == -1)
            m_prev.clear();

        Ipp8u * source = (Ipp8u *)pSource->GetDataPointer();
        size_t  size = pSource->GetDataSize();

        if (!size)
            return -1;

        Ipp32s startCodeSize;

        Ipp32s iCodeNext = FindStartCode(source, size, startCodeSize);

        // Use start code data which is saved from previous call because start code could be split between input buffers from application
        if (m_prev.size())
        {
            if (iCodeNext == -1)
            {
                size_t sz = source - (Ipp8u *)pSource->GetDataPointer();
                size_t szToMove = sz;
                if (m_prev.size() + sz >  m_suggestedSize)
                {
                    sz = IPP_MIN(0, m_suggestedSize - m_prev.size());
                }

                m_prev.insert(m_prev.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + sz);
                pSource->MoveDataPointer((Ipp32s)szToMove);
                return -1;
            }

            source -= startCodeSize;
            m_prev.insert(m_prev.end(), (Ipp8u *)pSource->GetDataPointer(), source);
            pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));

            pDst->SetFlags(UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME);
            pDst->SetBufferPointer(&(m_prev[3]), m_prev.size() - 3);
            pDst->SetDataSize(m_prev.size() - 3);
            pDst->SetTime(m_pts);
            Ipp32s code = m_code;
            m_code = -1;
            m_pts = -1;
            return code;
        }

        if (iCodeNext == -1)
        {
            pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));
            return -1;
        }

        m_pts = pSource->GetTime();
        m_code = iCodeNext;

        // move before start code
        pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer() - startCodeSize));

        Ipp32s startCodeSize1;
        iCodeNext = FindStartCode(source, size, startCodeSize1);

        pSource->MoveDataPointer(startCodeSize);

        Ipp32u flags = pSource->GetFlags();

        if (iCodeNext == -1 && !(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_UNIT))
        {
            iCodeNext = 1;
            startCodeSize1 = 0;
            source += size;
            size = 0;
        }

        if (iCodeNext == -1)
        {
            if (m_code == NAL_UT_SPS)
            {
                pSource->MoveDataPointer(-startCodeSize); // leave start code for SPS
                return -1;
            }

            VM_ASSERT(!m_prev.size());

            size_t sz = source - (Ipp8u *)pSource->GetDataPointer();
            size_t szToMove = sz;
            if (sz >  m_suggestedSize)
            {
                sz = m_suggestedSize;
            }

            if (!m_prev.size())
                m_prev.insert(m_prev.end(), start_code_prefix, start_code_prefix + sizeof(start_code_prefix));
            m_prev.insert(m_prev.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + sz);
            pSource->MoveDataPointer((Ipp32s)szToMove);
            return -1;
        }

        // fill
        size_t nal_size = source - (Ipp8u *)pSource->GetDataPointer() - startCodeSize1;
        pDst->SetBufferPointer((Ipp8u*)pSource->GetDataPointer(), nal_size);
        pDst->SetDataSize(nal_size);
        pDst->SetFlags(pSource->GetFlags());
        pSource->MoveDataPointer((Ipp32s)nal_size);

        Ipp32s code = m_code;
        m_code = -1;

        pDst->SetTime(m_pts);
        m_pts = -1;
        return code;
    }

    // Reset state because stream is finished
    Ipp32s EndOfStream(UMC::MediaData * pDst)
    {
        if (m_code == -1)
        {
            m_prev.clear();
            return -1;
        }

        if (m_prev.size())
        {
            pDst->SetBufferPointer(&(m_prev[3]), m_prev.size() - 3);
            pDst->SetDataSize(m_prev.size() - 3);
            pDst->SetTime(m_pts);
            Ipp32s code = m_code;
            m_code = -1;
            m_pts = -1;
            return code;
        }

        m_code = -1;
        return -1;
    }

private:
    std::vector<Ipp8u>  m_prev;
    Ipp32s   m_code;
    Ipp64f   m_pts;

    // Searches NAL unit start code, places input pointer to it and fills up size paramters
    // ML: OPT: TODO: Replace with MaxL's fast start code search
    Ipp32s FindStartCode(Ipp8u * (&pb), size_t & size, Ipp32s & startCodeSize)
    {
        Ipp32u zeroCount = 0;

        Ipp32s i = 0;
        for (; i < (Ipp32s)size - 2; )
        {
            if (pb[1])
            {
                pb += 2;
                i += 2;
                continue;
            }

            zeroCount = 0;
            if (!pb[0])
                zeroCount++;

            Ipp32u j;
            for (j = 1; j < (Ipp32u)size - i; j++)
            {
                if (pb[j])
                    break;
            }

            zeroCount = zeroCount ? j: j - 1;

            pb += j;
            i += j;

            if (i >= (Ipp32s)size)
            {
                break;
            }

            if (zeroCount >= 2 && pb[0] == 1)
            {
                startCodeSize = IPP_MIN(zeroCount + 1, 4);
                size -= i + 1;
                pb++; // remove 0x01 symbol
                if (size >= 1)
                {
                    return (pb[0] & NAL_UNITTYPE_BITS_H265) >> NAL_UNITTYPE_SHIFT_H265;
                }
                else
                {
                    pb -= startCodeSize;
                    size += startCodeSize;
                    startCodeSize = 0;
                    return -1;
                }
            }

            zeroCount = 0;
        }

        if (!zeroCount)
        {
            for (Ipp32u k = 0; k < size - i; k++, pb++)
            {
                if (pb[0])
                {
                    zeroCount = 0;
                    continue;
                }

                zeroCount++;
            }
        }

        zeroCount = IPP_MIN(zeroCount, 3);
        pb -= zeroCount;
        size = zeroCount;
        startCodeSize = zeroCount;
        return -1;
    }
};

// Memory big-to-little endian converter implementation
class Swapper : public SwapperBase
{
public:

    virtual void SwapMemory(Ipp8u *pDestination, size_t &nDstSize, Ipp8u *pSource, size_t nSrcSize, std::vector<Ipp32u> *pRemovedOffsets)
    {
        SwapMemoryAndRemovePreventingBytes_H265(pDestination, nDstSize, pSource, nSrcSize, pRemovedOffsets);
    }

    virtual void SwapMemory(MemoryPiece * pMemDst, MemoryPiece * pMemSrc, std::vector<Ipp32u> *pRemovedOffsets)
    {
        size_t dstSize = pMemSrc->GetDataSize();
        SwapMemory(pMemDst->GetPointer(),
                    dstSize,
                    pMemSrc->GetPointer(),
                    pMemSrc->GetDataSize(),
                    pRemovedOffsets);

        VM_ASSERT(pMemDst->GetSize() >= dstSize);
        size_t tail_size = IPP_MIN(pMemDst->GetSize() - dstSize, DEFAULT_NU_TAIL_SIZE);
        memset(pMemDst->GetPointer() + dstSize, DEFAULT_NU_TAIL_VALUE, tail_size);
        pMemDst->SetDataSize(dstSize);
        pMemDst->SetTime(pMemSrc->GetTime());
    }
};

NALUnitSplitter_H265::NALUnitSplitter_H265()
    : m_pSwapper(0)
    , m_pStartCodeIter(0)
{
    m_MediaData.SetExData(&m_MediaDataEx);
}

NALUnitSplitter_H265::~NALUnitSplitter_H265()
{
    Release();
}

// Initialize splitter with default values
void NALUnitSplitter_H265::Init()
{
    Release();

    m_pSwapper = new Swapper();
    m_pStartCodeIter = new StartCodeIterator();
}

// Reset state
void NALUnitSplitter_H265::Reset()
{
    if (m_pStartCodeIter)
    {
        m_pStartCodeIter->Reset();
    }
}

// Free resources
void NALUnitSplitter_H265::Release()
{
    delete m_pSwapper;
    m_pSwapper = 0;
    delete m_pStartCodeIter;
    m_pStartCodeIter = 0;
}

// Returns first NAL unit ID in memory buffer
Ipp32s NALUnitSplitter_H265::CheckNalUnitType(UMC::MediaData * pSource)
{
    return m_pStartCodeIter->CheckNalUnitType(pSource); // find first start code
}

// Set bitstream pointer to start code address
Ipp32s NALUnitSplitter_H265::MoveToStartCode(UMC::MediaData * pSource)
{
    return m_pStartCodeIter->MoveToStartCode(pSource); // find first start code
}

// Set destination bitstream pointer and size to NAL unit
UMC::MediaDataEx * NALUnitSplitter_H265::GetNalUnits(UMC::MediaData * pSource)
{
    UMC::MediaDataEx * out = &m_MediaData;
    UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = &m_MediaDataEx;

    Ipp32s iCode = m_pStartCodeIter->GetNALUnit(pSource, out);

    if (iCode == -1)
    {
        pMediaDataEx->count = 0;
        return 0;
    }

    pMediaDataEx->values[0] = iCode;

    pMediaDataEx->offsets[0] = 0;
    pMediaDataEx->offsets[1] = (Ipp32s)out->GetDataSize();
    pMediaDataEx->count = 1;
    pMediaDataEx->index = 0;
    return out;
}

// Utility class for writing 32-bit little endian integers
class H265DwordPointer_
{
public:
    // Default constructor
    H265DwordPointer_(void)
    {
        m_pDest = NULL;
        m_nByteNum = 0;
    }

    H265DwordPointer_ operator = (void *pDest)
    {
        m_pDest = (Ipp32u *) pDest;
        m_nByteNum = 0;
        m_iCur = 0;

        return *this;
    }

    // Increment operator
    H265DwordPointer_ &operator ++ (void)
    {
        if (4 == ++m_nByteNum)
        {
            *m_pDest = m_iCur;
            m_pDest += 1;
            m_nByteNum = 0;
            m_iCur = 0;
        }
        else
            m_iCur <<= 8;

        return *this;
    }

    Ipp8u operator = (Ipp8u nByte)
    {
        m_iCur = (m_iCur & ~0x0ff) | ((Ipp32u) nByte);

        return nByte;
    }

protected:
    Ipp32u *m_pDest;                                            // (Ipp32u *) pointer to destination buffer
    Ipp32u m_nByteNum;                                          // (Ipp32u) number of current byte in dword
    Ipp32u m_iCur;                                              // (Ipp32u) current dword
};

// Utility class for reading big endian bitstream
class H265SourcePointer_
{
public:
    // Default constructor
    H265SourcePointer_(void)
    {
        m_pSource = NULL;
    }

    H265SourcePointer_ &operator = (void *pSource)
    {
        m_pSource = (Ipp8u *) pSource;

        m_nZeros = 0;
        m_nRemovedBytes = 0;

        return *this;
    }

    H265SourcePointer_ &operator ++ (void)
    {
        Ipp8u bCurByte = m_pSource[0];

        if (0 == bCurByte)
            m_nZeros += 1;
        else
        {
            if ((3 == bCurByte) && (2 <= m_nZeros))
                m_nRemovedBytes += 1;
            m_nZeros = 0;
        }

        m_pSource += 1;

        return *this;
    }

    bool IsPrevent(void)
    {
        if ((3 == m_pSource[0]) && (2 <= m_nZeros))
            return true;
        else
            return false;
    }

    operator Ipp8u (void)
    {
        return m_pSource[0];
    }

    Ipp32u GetRemovedBytes(void)
    {
        return m_nRemovedBytes;
    }

protected:
    Ipp8u *m_pSource;                                           // (Ipp8u *) pointer to destination buffer
    Ipp32u m_nZeros;                                            // (Ipp32u) number of preceding zeros
    Ipp32u m_nRemovedBytes;                                     // (Ipp32u) number of removed bytes
};

// Change memory region to little endian for reading with 32-bit DWORDs and remove start code emulation prevention byteps
void SwapMemoryAndRemovePreventingBytes_H265(void *pDestination, size_t &nDstSize, void *pSource, size_t nSrcSize, std::vector<Ipp32u> *pRemovedOffsets)
{
    H265DwordPointer_ pDst;
    H265SourcePointer_ pSrc;
    size_t i;

    // DwordPointer object is swapping written bytes
    // H265SourcePointer_ removes preventing start-code bytes

    // reset pointer(s)
    pSrc = pSource;
    pDst = pDestination;

    // first two bytes
    i = 0;
    while (i < (Ipp32u) IPP_MIN(2, nSrcSize))
    {
        pDst = (Ipp8u) pSrc;
        ++pDst;
        ++pSrc;
        ++i;
    }

    // do swapping
    if (NULL != pRemovedOffsets)
    {
        while (i < (Ipp32u) nSrcSize)
        {
            if (false == pSrc.IsPrevent())
            {
                pDst = (Ipp8u) pSrc;
                ++pDst;
            }
            else
                pRemovedOffsets->push_back(Ipp32u(i));

            ++pSrc;
            ++i;
        }
    }
    else
    {
        while (i < (Ipp32u) nSrcSize)
        {
            if (false == pSrc.IsPrevent())
            {
                pDst = (Ipp8u) pSrc;
                ++pDst;
            }

            ++pSrc;
            ++i;
        }
    }

    // write padding bytes
    nDstSize = nSrcSize - pSrc.GetRemovedBytes();
    while (nDstSize & 3)
    {
        pDst = (Ipp8u) (0);
        ++nDstSize;
        ++pDst;
    }

} // void SwapMemoryAndRemovePreventingBytes_H265(void *pDst, size_t &nDstSize, void *pSrc, size_t nSrcSize, , std::vector<Ipp32u> *pRemovedOffsets)

} // namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
