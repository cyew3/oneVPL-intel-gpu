/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_H__
#define __UMC_H264_DEC_H__

#include <memory>

#include "umc_video_decoder.h"
#include "umc_media_buffer.h"

namespace UMC
{

class H264VideoDecoderParams : public VideoDecoderParams
{
public:
    DYNAMIC_CAST_DECL(H264VideoDecoderParams, VideoDecoderParams);

    enum
    {
        ENTROPY_CODING_CAVLC = 0,
        ENTROPY_CODING_CABAC = 1
    };

    enum
    {
        H264_PROFILE_UNKNOWN            = 0,
        H264_PROFILE_BASELINE           = 66,
        H264_PROFILE_MAIN               = 77,
        H264_PROFILE_SCALABLE_BASELINE  = 83,
        H264_PROFILE_SCALABLE_HIGH      = 86,
        H264_PROFILE_EXTENDED           = 88,
        H264_PROFILE_HIGH               = 100,
        H264_PROFILE_HIGH10             = 110,
        H264_PROFILE_MULTIVIEW_HIGH     = 118,
        H264_PROFILE_HIGH422            = 122,
        H264_PROFILE_STEREO_HIGH        = 128,
        H264_PROFILE_HIGH444            = 144,
        H264_PROFILE_ADVANCED444_INTRA  = 166,
        H264_PROFILE_ADVANCED444        = 188,
        H264_PROFILE_HIGH444_PRED       = 244,
        H264_PROFILE_CAVLC444_INTRA     = 44
    };

    enum
    {
        H264_LEVEL_1    = 10,
        H264_LEVEL_11   = 11,
        H264_LEVEL_1b   = 11,
        H264_LEVEL_12   = 12,
        H264_LEVEL_13   = 13,

        H264_LEVEL_2    = 20,
        H264_LEVEL_21   = 21,
        H264_LEVEL_22   = 22,

        H264_LEVEL_3    = 30,
        H264_LEVEL_31   = 31,
        H264_LEVEL_32   = 32,

        H264_LEVEL_4    = 40,
        H264_LEVEL_41   = 41,
        H264_LEVEL_42   = 42,

        H264_LEVEL_5    = 50,
        H264_LEVEL_51   = 51,
        H264_LEVEL_52   = 52,
        H264_LEVEL_MAX  = 52,

        H264_LEVEL_9    = 9  // for SVC profiles
    };

    H264VideoDecoderParams()
        : m_entropy_coding_type(ENTROPY_CODING_CAVLC)
        , m_DPBSize(16)
        , m_auxiliary_format_idc(0)
        , m_bufferedFrames(0)
    {
        m_fullSize.width = 0;
        m_fullSize.height = 0;

        m_cropArea.top = 0;
        m_cropArea.bottom = 0;
        m_cropArea.left = 0;
        m_cropArea.right = 0;
    }

    Ipp32s m_entropy_coding_type;
    Ipp32s m_DPBSize;
    Ipp32s m_auxiliary_format_idc;
    IppiSize m_fullSize;
    UMC::sRECT m_cropArea;

    Ipp32s m_bufferedFrames;
};

// forward declaration of internal types
class TaskSupplier;
class FrameAllocator;

class H264VideoDecoder : public VideoDecoder
{
    DYNAMIC_CAST_DECL(H264VideoDecoder, VideoDecoder);

public:

    H264VideoDecoder();

    virtual ~H264VideoDecoder();

//////////////////////
// interface methods
/////////////////////
    virtual Status Init(BaseCodecParams *params);

    // Decode & get decoded frame
    virtual Status GetFrame(MediaData* in, MediaData* out);

#ifdef UMC_RESTRICTED_CODE_RA
    Status GetRandomFrame(MediaData *pSource, MediaData *pDst, ReferenceList* reflist);
#endif

    virtual Status  SetParams(BaseCodecParams* params);

    virtual Status Close();

    virtual Status Reset();

    virtual Status GetInfo(BaseCodecParams* params);

    // reset skip frame counter
    virtual Status ResetSkipCount() // deprecated
    {
        return UMC_OK;
    };

    virtual Status ChangeVideoDecodingSpeed(Ipp32s& num);

    struct SkipInfo
    {
        bool isDeblockingTurnedOff;
        Ipp32s numberOfSkippedFrames;
    };

    virtual SkipInfo GetSkipInfo() const;

    virtual Status SkipVideoFrame(Ipp32s /*num*/) // deprecated
    {
        return UMC_ERR_NOT_IMPLEMENTED;
    };

    // get skip frame counter statistic
    virtual Ipp32u GetNumOfSkippedFrames() // deprecated
    {
        return 0;
    };

    virtual Status GetUserData(MediaData * pUD);

private:

    Status GetFrameInternal(MediaData* in, MediaData* out);

    std::auto_ptr<TaskSupplier>    m_pTaskSupplier;
    std::auto_ptr<FrameAllocator>  m_pFrameAllocator;

    bool m_IsOwnPostProcessing;

    bool m_IsInitialized;
}; // H264VideoDecoder

} // namespace UMC

#endif // __UMC_H264_DEC_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
