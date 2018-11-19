// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_H_
#define _ENCODER_VC1_H_

#include "umc_video_encoder.h"
#include "umc_dynamic_cast.h"
#include "umc_video_data.h"

namespace UMC
{
class VideoDataUD : public VideoData
{
    DYNAMIC_CAST_DECL(VideoDataUD, VideoData)
public:
    VideoDataUD ()
    {
        pUserData    = 0;
        userDataSize = 0;
    }
    ~VideoDataUD () {};
    uint8_t* pUserData;
    uint32_t  userDataSize;
};

class MediaDataUD : public MediaData
{
    DYNAMIC_CAST_DECL(MediaDataUD, MediaData)
public:
    MediaDataUD ()
    {
        pictureCode = 0;
    }
    ~MediaDataUD () {};
    uint8_t pictureCode;
};

class VC1EncoderParams : public VideoEncoderParams
{
    DYNAMIC_CAST_DECL(VC1EncoderParams, VideoEncoderParams)
public:
    VC1EncoderParams():
        m_uiGOPLength(0),
        m_uiBFrmLength(0),
        m_bVSTransform(false),
        m_bDeblocking(false),
        m_iConstQuant(0),
        m_uiNumFrames(0),
        m_pStreamName(0),
        m_uiMESearchSpeed(66),
        m_uiHRDBufferSize(0),
        m_uiHRDBufferInitFullness(0),
        m_bFrameRecoding(false),
        m_bMixed(false),
        m_bOrigFramePred(false),
        m_bInterlace(false),
        m_uiReferenceFieldType(0),
        m_bSceneAnalyzer(false),
        m_bUseMeFeedback(true),
        m_bUseUpdateMeFeedback(false),
        m_bUseFastMeFeedback(false),
        m_bFastUVMC(true),
        m_uiQuantType(3),
        m_bSelectVLCTables(0),
        m_bChangeInterpPixelType(0),
        m_bUseTreillisQuantization(0),
        m_uiNumSlices(0),
        m_uiOverlapSmoothing(0),
        m_bIntensityCompensation (false),
        m_bNV12(false)
    {};
    virtual ~VC1EncoderParams() {};
    virtual Status ReadParamFile(const vm_char * ParFileName);

    uint32_t              m_uiGOPLength;
    uint32_t              m_uiBFrmLength;
    bool                m_bVSTransform;
    bool                m_bDeblocking;
    uint8_t               m_iConstQuant;
    uint32_t              m_uiNumFrames;
    uint32_t              m_uiMESearchSpeed;
    uint32_t              m_uiHRDBufferSize;
    int32_t              m_uiHRDBufferInitFullness;
    vm_char*            m_pStreamName;
    bool                m_bFrameRecoding;
    bool                m_bMixed;
    bool                m_bOrigFramePred;
    bool                m_bInterlace;
    uint32_t              m_uiReferenceFieldType;
    bool                m_bSceneAnalyzer;
    bool                m_bUseMeFeedback;
    bool                m_bUseUpdateMeFeedback;
    bool                m_bUseFastMeFeedback;
    bool                m_bFastUVMC;
    uint32_t              m_uiQuantType;
    bool                m_bSelectVLCTables;
    bool                m_bChangeInterpPixelType;
    bool                m_bUseTreillisQuantization;
    uint32_t              m_uiNumSlices;
    uint32_t              m_uiOverlapSmoothing;
    bool                m_bIntensityCompensation;
    bool                m_bNV12;
};


class VC1VideoEncoder: public VideoEncoder
{
public:

    VC1VideoEncoder();
    ~VC1VideoEncoder();

    virtual Status Init     (BaseCodecParams *init);
    virtual Status SetParams(BaseCodecParams* params);
    virtual Status GetInfo  (BaseCodecParams *info);

    virtual Status Close    ();
    virtual Status Reset    ();

    virtual Status GetFrame (MediaData *in, MediaData *out);

protected:

    virtual Status CheckParameters() {return UMC_OK;}

private:

    void*     m_pEncoderSM;
    void*     m_pEncoderADV;
    bool      m_bAdvance;

};

}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
