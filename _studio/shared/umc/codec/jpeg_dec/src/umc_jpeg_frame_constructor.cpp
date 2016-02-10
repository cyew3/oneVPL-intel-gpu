/* ///////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include "umc_jpeg_frame_constructor.h"
#include "jpegdec.h"

namespace UMC
{

//////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////
JpegFrameConstructor::JpegFrameConstructor()
    : m_prevLengthOfSegment(0)
    , m_code(0)
    , m_pts(-1)
    , m_suggestedSize(0)
{
    m_mediaData.SetExData(&m_mediaDataEx);
    Reset();
}

JpegFrameConstructor::~JpegFrameConstructor()
{
}

void JpegFrameConstructor::Init()
{
}

void JpegFrameConstructor::Reset()
{
    m_code = 0;
    m_pts = -1;
    m_suggestedSize = 0;
    m_prevLengthOfSegment = 0;
    m_RestartCount = 0;

    m_prev.clear();
    m_frame.clear();
    m_mediaDataEx.count = 0;
    m_mediaData.Close();

    m_flags.isSOI = 0;
    m_flags.isEOI = 0;
    m_flags.isSOS = 0;
}

void JpegFrameConstructor::Close()
{
    Reset();
}

Ipp32s JpegFrameConstructor::FindMarkerCode(Ipp8u * (&source), size_t & size, Ipp32s & startCodeSize)
{
    Ipp32u ffCount = 0;

    if (m_prevLengthOfSegment)
    {
        size_t copySize = IPP_MIN(m_prevLengthOfSegment, size);
        m_prevLengthOfSegment -= copySize;
        source += copySize;
        size -= copySize;

        if (m_prevLengthOfSegment)
            return 0;
    }

    for (;;)
    {
        Ipp32u i = 0;
        ffCount = 0;
        for (; i < (Ipp32u)size; i++)
        {
            if (source[i] == 0xff)
            {
                ffCount = 1;
                i++;
                break;
            }
        }

        for (; i < (Ipp32u)size; i++, ffCount++)
        {
            if (source[i] != 0xff)
            {
                break;
            }
        }

        source += i;
        size -= i;

        if (size <= 0)
        {
            break;
        }

        if (!source[0])
        {
            ffCount = 0;
        }
        else
        {
            ffCount = 1;
            startCodeSize = ffCount + 1;
            size -= 1;
            Ipp32s code = source[0];
            source++;
            return code;
        }
    }

    source -= ffCount;
    size = ffCount;
    ffCount = 0;
    startCodeSize = 0;
    return 0;
}

Ipp32s JpegFrameConstructor::CheckMarker(MediaData * pSource)
{
    if (!pSource)
        return 0;

    if (!m_code)
        m_prev.clear();

    Ipp8u * source = (Ipp8u *)pSource->GetDataPointer();
    size_t  size = pSource->GetDataSize();

    Ipp32s startCodeSize;
    Ipp32s iCodeNext = FindMarkerCode(source, size, startCodeSize);

    pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));
    if (iCodeNext)
    {
         pSource->MoveDataPointer(-startCodeSize);
    }

    return iCodeNext;
}

Ipp32s JpegFrameConstructor::GetMarker(MediaData * pSource, MediaData * pDst)
{
    if (!pSource)
        return EndOfStream(pDst);

    if (!m_code)
        m_prev.clear();

    Ipp8u * source = (Ipp8u *)pSource->GetDataPointer();
    size_t  size = pSource->GetDataSize();

    if (!size)
        return 0;

    Ipp32s startCodeSize;

    Ipp32s iCodeNext = FindMarkerCode(source, size, startCodeSize);

    if (m_prev.size())
    {
        if (!iCodeNext)
        {
            size_t sz = source - (Ipp8u *)pSource->GetDataPointer();
            if (m_suggestedSize && m_prev.size() + sz >  m_suggestedSize)
            {
                m_prev.clear();
                sz = IPP_MIN(sz, m_suggestedSize);
            }

            m_prev.insert(m_prev.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + sz);
            pSource->MoveDataPointer((Ipp32s)sz);
            return 0;
        }

        source -= startCodeSize;
        m_prev.insert(m_prev.end(), (Ipp8u *)pSource->GetDataPointer(), source);
        pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));

        pDst->SetBufferPointer(&(m_prev[0]), m_prev.size());
        pDst->SetDataSize(m_prev.size());
        pDst->SetTime(m_pts);
        Ipp32s code = m_code;
        m_code = 0;
        m_pts = -1;

        return code;
    }

    if (!iCodeNext)
    {
        pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer()));
        return 0;
    }

    m_pts = pSource->GetTime();
    m_code = iCodeNext;

    // move before start code
    pSource->MoveDataPointer((Ipp32s)(source - (Ipp8u *)pSource->GetDataPointer() - startCodeSize));

    Ipp32u flags = pSource->GetFlags();

    bool wasNextMarkerFound = true;

    switch(iCodeNext)
    {
    case JM_RST0:  // standalone markers
    case JM_RST1:
    case JM_RST2:
    case JM_RST3:
    case JM_RST4:
    case JM_RST5:
    case JM_RST6:
    case JM_RST7:
    case JM_SOI:
    case JM_EOI:
    case JM_TEM:
        break;
    case JM_SOS:
        break;
    default:   // marker's segment.
        {
            if (size >= 2)
            {
                size_t length = (source[0] << 8) | source[1];

                m_prevLengthOfSegment = length;
                wasNextMarkerFound = true;
            }
            else
            {
                return 0;
            }
        }
        break;
    }

    if(m_code != JM_EOI)
    {
        Ipp32s startCodeSize1 = 0;

        iCodeNext = wasNextMarkerFound ? FindMarkerCode(source, size, startCodeSize1) : 0;

        //pSource->MoveDataPointer(startCodeSize);

        if (!iCodeNext && !(flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_UNIT))
        {
            iCodeNext = 1;
            startCodeSize1 = 0;
        }

        if (!iCodeNext)
        {
            VM_ASSERT(!m_prev.size());
            size_t sz = source - (Ipp8u *)pSource->GetDataPointer();
            if (m_suggestedSize && sz >  m_suggestedSize)
            {
                sz = m_suggestedSize;
            }

            m_prev.insert(m_prev.end(), (Ipp8u *)pSource->GetDataPointer(), (Ipp8u *)pSource->GetDataPointer() + sz);
            pSource->MoveDataPointer((Ipp32s)sz);
            return 0;
        }

        // fill
        size_t nal_size = source - (Ipp8u *)pSource->GetDataPointer() - startCodeSize1;
        pDst->SetBufferPointer((Ipp8u*)pSource->GetDataPointer(), nal_size);
        pDst->SetDataSize(nal_size);
        pSource->MoveDataPointer((Ipp32s)nal_size);
    }
    else
    {
        pSource->MoveDataPointer(2);
    }

    Ipp32s code = m_code;
    m_code = 0;

    pDst->SetTime(m_pts);
    m_pts = -1;
    return code;
}

Ipp32s JpegFrameConstructor::EndOfStream(MediaData * pDst)
{
    if (!m_code)
    {
        m_prev.clear();
        return 0;
    }

    if (m_prev.size())
    {
        pDst->SetBufferPointer(&(m_prev[0]), m_prev.size());
        pDst->SetDataSize(m_prev.size());
        pDst->SetTime(m_pts);
        Ipp32s code = m_code;
        m_code = 0;
        m_pts = -1;
        return code;
    }

    m_code = 0;
    return 0;
}

Status JpegFrameConstructor::AddMarker(Ipp32u marker, MediaDataEx::_MediaDataEx* pMediaDataEx, size_t nBufferSize, MediaData *dst)
{
    if (!dst->GetDataSize())
        return UMC_OK;

    Ipp32u element = pMediaDataEx->count;

    if (element >= pMediaDataEx->limit - 1)
        return UMC_ERR_FAILED;

    m_frame.insert(m_frame.end(), (Ipp8u*)dst->GetDataPointer(), (Ipp8u*)dst->GetDataPointer() + dst->GetDataSize() );
    
    pMediaDataEx->offsets[0] = 0;
    // process RST marker
    if((JM_RST0 <= marker) && (JM_RST7 >= marker))
    {
        // add marker to MediaDataEx
        if((Ipp64u)m_frame.size() > ((Ipp64u)nBufferSize * element) / (pMediaDataEx->limit - 2))
        {
            pMediaDataEx->values[element] = marker | (m_RestartCount << 8);
            pMediaDataEx->offsets[element + 1] = (Ipp32u)m_frame.size();
            pMediaDataEx->count++;
        }
        // the end of previous interval would be the start of next
        else
        {
            pMediaDataEx->offsets[element] = (Ipp32u)m_frame.size();
        }
        m_RestartCount++;
    }
    // process SOS marker (add it anyway)
    else if(JM_SOS == marker)
    {
        pMediaDataEx->values[element] = marker | (m_RestartCount << 8);
        pMediaDataEx->offsets[element + 1] = (Ipp32u)m_frame.size();
        pMediaDataEx->count++;
        m_RestartCount++;
    }
    else
    {
        pMediaDataEx->values[element] = marker | (m_RestartCount << 8);
        pMediaDataEx->offsets[element + 1] = (Ipp32u)m_frame.size();
        pMediaDataEx->count++;
    }

    return UMC_OK;
}

void JpegFrameConstructor::ResetForNewFrame()
{
    m_frame.clear();
    m_mediaDataEx.count = 0;
    m_flags.isSOI = 0;
    m_flags.isEOI = 0;
    m_flags.isSOS = 0;
    m_RestartCount = 0;
}

MediaDataEx * JpegFrameConstructor::GetFrame(MediaData * in, Ipp32u maxBitstreamSize)
{
    for (;;)
    {
        MediaData dst;
        Ipp32u marker = GetMarker(in, &dst);
        
        if (marker == JM_NONE)
        {
            if(!in)
                break;

            // frame is not declared as "complete", and next marker is not found
            if(in->GetFlags() & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
                break;

            // frame is declared as "complete", but it does not contain SOS marker
            if(!(in->GetFlags() & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME) && m_flags.isSOS == 0)
                break;
        }      

        switch(marker)
        {
        case JM_SOI:
            if (m_flags.isSOI && m_flags.isSOS && m_frame.size() && !m_flags.isEOI) // EOI wasn't received
            {
                m_flags.isEOI = 1; // emulate EOI
                m_mediaData.SetBufferPointer(&m_frame[0], m_frame.size());
                m_mediaData.SetDataSize(m_frame.size());
                m_mediaData.SetFlags(in ? in->GetFlags() : 0);
                return &m_mediaData;
            }

            m_mediaData.SetTime(in->GetTime());
            m_frame.clear();
            m_mediaDataEx.count = 0;
            m_flags.isSOI = 1;
            m_flags.isEOI = 0;
            m_flags.isSOS = 0;
            m_RestartCount = 0;
            break;

            // JPEG picture data
        case JM_RST0:
        case JM_RST1:
        case JM_RST2:
        case JM_RST3:
        case JM_RST4:
        case JM_RST5:
        case JM_RST6:
        case JM_RST7:
        case JM_SOS:
            if (m_flags.isSOI)
            {
                m_flags.isSOS = 1;
            }
            else
            {
                continue;
            }
            break;

        case JM_EOI:
        default:
            break;
        }

        if (AddMarker(marker, &m_mediaDataEx, IPP_MAX(in ? in->GetBufferSize() : 0, maxBitstreamSize), &dst) != UMC_OK)
        {
            ResetForNewFrame();
            continue;
        }

        if (marker == JM_EOI)
        {
            if (m_flags.isEOI || !m_frame.size() || !m_flags.isSOS)
            {
                ResetForNewFrame();
                continue;
            }

            m_flags.isEOI = 1;

            if (m_frame.size())
            {
                m_flags.isEOI = 0;
                m_flags.isSOS = 0;

                m_mediaData.SetBufferPointer(&m_frame[0], m_frame.size());
                m_mediaData.SetDataSize(m_frame.size());
                m_mediaData.SetFlags(in ? in->GetFlags() : 0);
                return &m_mediaData;
            }
        }

        if(marker == JM_NONE && !(in->GetFlags() & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME) && m_flags.isSOS == 1)
        {
            if (m_frame.size())
            {
                m_flags.isEOI = 0;
                m_flags.isSOS = 0;

                m_mediaData.SetBufferPointer(&m_frame[0], m_frame.size());
                m_mediaData.SetDataSize(m_frame.size());
                m_mediaData.SetFlags(in ? in->GetFlags() : 0);
                return &m_mediaData;
            }
        }
    }

    return 0;
}

} // end namespace UMC


#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
