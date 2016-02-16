/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __UMC_IVF_SPL_H__
#define __UMC_IVF_SPL_H__

#include "umc_splitter.h"

#define READ_BYTES(pBuf, size)\
{\
    nBytesToread = size;\
    sts = m_pRdr->ReadData(pBuf, &nBytesToread);\
    UMC_CHECK_STATUS(sts);\
}

//splitter in UMC interface since it can be included in umc structure
namespace UMC
{
    class IVFSplitter 
        : public Splitter
    {
        DYNAMIC_CAST_DECL(IVFSplitter, Splitter);
    public:
        IVFSplitter()
            : m_pRdr()
            , m_bParsed()
            , m_bLastFrameCached()
        {
        }
        virtual ~IVFSplitter()
        {
        }
        virtual Status  Init(SplitterParams& rInit)
        {
            UMC_CHECK(!m_bParsed, UMC_ERR_INIT);

            m_pRdr = rInit.m_pDataReader;
            UMC_CHECK_PTR(m_pRdr);

            //read header
            Ipp32u nBytesToread;
            Status sts;
            
            READ_BYTES(&m_hdr.dkif, sizeof(m_hdr.dkif));
            READ_BYTES(&m_hdr.version, sizeof(m_hdr.version));
            READ_BYTES(&m_hdr.header_len, sizeof(m_hdr.header_len));
            READ_BYTES(&m_hdr.codec_FourCC, sizeof(m_hdr.codec_FourCC));
            READ_BYTES(&m_hdr.width, sizeof(m_hdr.width));
            READ_BYTES(&m_hdr.height, sizeof(m_hdr.height));
            READ_BYTES(&m_hdr.frame_rate, sizeof(m_hdr.frame_rate));
            READ_BYTES(&m_hdr.time_scale, sizeof(m_hdr.time_scale));
            READ_BYTES(&m_hdr.num_frames, sizeof(m_hdr.num_frames));
            READ_BYTES(&m_hdr.unused, sizeof(m_hdr.unused));
            sts = m_pRdr->MovePosition(m_hdr.header_len - m_pRdr->GetPosition());
            UMC_CHECK_STATUS(sts);

            //check IVF format structure
            if (MFX_MAKEFOURCC('D','K','I','F') != m_hdr.dkif)
                return UMC_ERR_INVALID_STREAM;
            if ((MFX_MAKEFOURCC('V','P','8','0') != m_hdr.codec_FourCC) &&
                (MFX_MAKEFOURCC('V','P','9','0') != m_hdr.codec_FourCC))
                return UMC_ERR_INVALID_STREAM;

            if (0 == m_hdr.time_scale)
                return UMC_ERR_SYNC;

            sts = m_LastFrame.Alloc(m_hdr.width * m_hdr.height * 4);
            UMC_CHECK_STATUS(sts);

            m_bParsed = true;

            m_videoInfo.clip_info.width = m_hdr.width;
            m_videoInfo.clip_info.height = m_hdr.height;
            m_videoInfo.framerate = m_hdr.frame_rate / m_hdr.time_scale;
            if (MFX_MAKEFOURCC('V','P','8','0') == m_hdr.codec_FourCC)
                m_videoInfo.stream_type = VP8_VIDEO;
            if (MFX_MAKEFOURCC('V','P','9','0') == m_hdr.codec_FourCC)
                m_videoInfo.stream_type = VP9_VIDEO;

            if (0 != m_videoInfo.framerate)
            {
                m_videoInfo.duration = m_hdr.num_frames * 1 / m_videoInfo.framerate;
            }
            
            m_TrackInfo.m_Type = TRACK_VP8;
            m_TrackInfo.m_isSelected = true;
            m_TrackInfo.m_pStreamInfo = &m_videoInfo;

            m_trackArray[0] = &m_TrackInfo;

            m_splInfo.m_nOfTracks = 1;
            m_splInfo.m_ppTrackInfo = m_trackArray;
            m_splInfo.m_SystemType = IVF_STREAM;
            m_splInfo.m_dRate = 1.;
            m_splInfo.m_dDuration = m_videoInfo.duration;

            return UMC_OK;
        }
        virtual Status GetInfo(SplitterInfo** ppInfo)
        {
            UMC_CHECK(m_bParsed, UMC_ERR_INIT);
            UMC_CHECK_PTR(ppInfo);
            *ppInfo = &m_splInfo;
            return UMC_OK;
        }
        virtual Status  Close()
        {
            m_bParsed = false;
            return UMC_OK;
        }
        virtual Status CheckNextData(MediaData*   data, Ipp32u /*nTrack*/)
        {
            UMC_CHECK(m_bParsed, UMC_ERR_INIT);
            
            if (m_bLastFrameCached)
            {
                *data = m_LastFrame;
                return UMC_OK;
            }

            Status sts;
            sts = GetNextData(data, 0);
            UMC_CHECK_STATUS(sts);
            m_bLastFrameCached = true;

            return UMC_OK;
        }
        virtual Status GetNextData(MediaData*   data, Ipp32u /*nTrack*/)
        {
            UMC_CHECK(m_bParsed, UMC_ERR_INIT);

            if (m_bLastFrameCached)
            {
                *data = m_LastFrame;
                m_bLastFrameCached = false;
                return UMC_OK;
            }

            Ipp32u nBytesToread=4, nBytesInFrame;
            Status sts;
            READ_BYTES(&nBytesInFrame, sizeof(nBytesInFrame));
            READ_BYTES(&m_nPts, sizeof(m_nPts));
            if (m_LastFrame.GetBufferSize() < nBytesInFrame)
                m_LastFrame.Alloc(nBytesInFrame*2);
            UMC_CHECK(nBytesInFrame <= m_LastFrame.GetBufferSize(), UMC_ERR_NOT_ENOUGH_BUFFER);

            READ_BYTES((Ipp8u*)m_LastFrame.GetDataPointer(), nBytesInFrame);
            m_LastFrame.SetDataSize(nBytesInFrame);
            m_LastFrame.SetTime((double)m_nPts / m_hdr.time_scale);

            *data = m_LastFrame;

            return UMC_OK;
        }
        virtual Status Run ()
        {
            return UMC_OK;
        }
        virtual Status Stop ()
        {
            return UMC_OK;
        }
    
    protected:
        MediaData       m_LastFrame;
        DataReader     *m_pRdr;
        TrackInfo       m_TrackInfo;
        TrackInfo      *m_trackArray[1];
        SplitterInfo    m_splInfo;
        VideoStreamInfo m_videoInfo;
        bool            m_bParsed;
        bool            m_bLastFrameCached;
        Ipp64u          m_nPts;

      /*bytes 0-3    signature: 'DKIF'
        bytes 4-5    version (should be 0)
        bytes 6-7    length of header in bytes
        bytes 8-11   codec FourCC (e.g., 'VP80')
        bytes 12-13  width in pixels
        bytes 14-15  height in pixels
        bytes 16-19  frame rate
        bytes 20-23  time scale
        bytes 24-27  number of frames in file
        bytes 28-31  unused*/

        struct DKIFHrd
        {
            Ipp32u dkif;
            Ipp16u version;
            Ipp16u header_len;
            Ipp32u codec_FourCC;
            Ipp16u width;
            Ipp16u height;
            Ipp32u frame_rate;
            Ipp32u time_scale;
            Ipp32u num_frames;
            Ipp32u unused;
        }m_hdr;
    };
}

#endif //__UMC_IVF_SPL_H__
