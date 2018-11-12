// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
