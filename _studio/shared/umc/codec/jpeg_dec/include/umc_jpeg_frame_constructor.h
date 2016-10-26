//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_JPEG_FRAME_CONSTRUCTOR_H
#define __UMC_JPEG_FRAME_CONSTRUCTOR_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include <vector>
#include "umc_media_data_ex.h"

namespace UMC
{

class JpegFrameConstructor
{
public:

    JpegFrameConstructor();

    virtual ~JpegFrameConstructor();

    virtual void Init();

    virtual void Reset();

    virtual void Close();

    virtual MediaDataEx * GetFrame(MediaData * in, Ipp32u maxBitstreamSize);

    Ipp32s CheckMarker(MediaData * pSource);

    Ipp32s GetMarker(MediaData * in, MediaData * pDst);

protected:

    Status AddMarker(Ipp32u marker, MediaDataEx::_MediaDataEx* pMediaDataEx, size_t nBufferSize, MediaData *dst);
    Ipp32s EndOfStream(MediaData * pDst);

    Ipp32s FindMarkerCode(Ipp8u * (&source), size_t & size, Ipp32s & startCodeSize);

    void ResetForNewFrame();

    size_t              m_prevLengthOfSegment;
    std::vector<Ipp8u>  m_prev;
    std::vector<Ipp8u>  m_frame;
    MediaDataEx         m_mediaData;
    MediaDataEx::_MediaDataEx m_mediaDataEx;

    Ipp32s              m_code; // marker code
    Ipp64f              m_pts;
    size_t              m_suggestedSize;
    Ipp32s              m_RestartCount;

    struct JpegFCFlags
    {
        Ipp8u isSOI : 1;
        Ipp8u isEOI : 1;
        Ipp8u isSOS : 1;
    };

    JpegFCFlags       m_flags;
};

} // end namespace UMC

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif //__UMC_JPEG_FRAME_CONSTRUCTOR_H
