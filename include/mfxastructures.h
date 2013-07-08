/*******************************************************************************

Copyright (C) 2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxastructures.h

*******************************************************************************/
#ifndef __MFXASTRUCTURES_H__
#define __MFXASTRUCTURES_H__
#include "mfxcommon.h"

#pragma warning(disable: 4201)

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* CodecId */
enum {
    MFX_CODEC_AAC         =MFX_MAKEFOURCC('A','A','C',' '),
    MFX_CODEC_MP3         =MFX_MAKEFOURCC('M','P','3',' ')
};

/* CodecProfile, CodecLevel */
enum {
    /* AAC Profiles & Levels */
    MFX_PROFILE_AAC_LC          =2,
    MFX_PROFILE_AAC_LTP         =4,
    MFX_PROFILE_AAC_MAIN        =1,
    MFX_PROFILE_AAC_SSR         =3,
    MFX_PROFILE_AAC_HE          =5,
    MFX_PROFILE_AAC_ALS         =0x00000020,
    MFX_PROFILE_AAC_BSAC        =22,
    MFX_PROFILE_AAC_PS          =29,

    /*MPEG AUDIO*/
    MFX_MPEG1_LAYER1_AUDIO      =0x00000110, 
    MFX_MPEG1_LAYER2_AUDIO      =0x00000120,
    MFX_MPEG1_LAYER3_AUDIO      =0x00000140,
    MFX_MPEG2_LAYER1_AUDIO      =0x00000210,
    MFX_MPEG2_LAYER2_AUDIO      =0x00000220,
    MFX_MPEG2_LAYER3_AUDIO      =0x00000240
};

/* Frame Info */
typedef struct  
{
    mfxU32                Bitrate;         // bitstream in bps
    mfxU16                Channels;        // number of audio channels
    mfxU32                SampleFrequency; // sample rate in Hz
    mfxU16                BitPerSample;    // 0 if compressed
    mfxU32                reserved[5]; 
} mfxAudioStreamInfo;


/*AAC HE decoder modes*/
enum {
    MFX_AUDIO_AAC_HE_HQ_MODE=    1,  /* high quality mode */
    MFX_AUDIO_AAC_HE_LP_MODE=    2   /* low process mode */
} ;
/*AAC HE decoder down sampling*/
enum {
    MFX_AUDIO_AAC_HE_DWNSMPL_OFF=1,       /* downsampling mode is disabled */
    MFX_AUDIO_AAC_HE_DWNSMPL_ON= 2        /* downsampling mode is enabled */
};

/* AAC decoder support of PS */
enum {
    MFX_AUDIO_AAC_PS_DISABLE=   0,
    MFX_AUDIO_AAC_PS_PARSER=    1,
    MFX_AUDIO_AAC_PS_ENABLE_BL= 111,
    MFX_AUDIO_AAC_PS_ENABLE_UR= 411
};

/*AAC decoder SBR support*/
enum {
    MFX_AUDIO_AAC_SBR_DISABLE =  0,
    MFX_AUDIO_AAC_SBR_ENABLE=    1,
    MFX_AUDIO_AAC_SBR_UNDEF=     2
};

/*MP3 decoder LFE-channel (Low Frequency Enhancement channel) */
enum
{
    MFX_AUDIO_MP3_LFE_FILTER_OFF= 1,
    MFX_AUDIO_MP3_LFE_FILTER_ON=  2
};

enum
{
    MFX_AUDIO_MP3_SYNC_MODE_BASE     = 1,
    MFX_AUDIO_MP3_SYNC_MODE_ADVANCED=  2
};

/*AAC header type*/
enum{
    MFX_AUDIO_AAC_ADTS=            1,
    MFX_AUDIO_AAC_ADIF=            2,
    MFX_AUDIO_AAC_RAW=             3,
};
/*AAC encoder stereo mode*/
enum 
{
    MFX_AUDIO_AAC_MONO=            0,
    MFX_AUDIO_AAC_LR_STEREO=       1,
    MFX_AUDIO_AAC_MS_STEREO=       2,
    MFX_AUDIO_AAC_JOINT_STEREO=    3
};
/*AAC encoder noise shaping*/
enum
{
    MFX_AUDIO_AAC_NOISE_MODEL_SIMPLE=  0,   // to learn more
    MFX_AUDIO_AAC_NOISE_MODEL_ADVANCED=1 
};



/* Audio Info */
typedef struct {
    mfxAudioStreamInfo m_info;   // original audio stream info

    mfxU32                CodecId;
    mfxU16                CodecProfile;
    mfxU16                CodecLevel;

    union {    
        struct {   /* AAC Decoding Options */
            mfxU16       ModeDecodeHEAACprofile;
            mfxU16       ModeDwnsmplHEAACprofile;
            mfxU16       FlagSBRSupportLev;
            mfxU16       FlagPSSupportLev;
            mfxU16       Layer;                                //  used in bit slicing arithmetic coding. layer show decoding level
            mfxU8        AACHeaderData[64];
            mfxU16       AACHeaderDataSize;
        };
        struct {   /* MP3 Decoding Options */
            mfxU16 LFEFilter; /// Low Frequency Effect(LFE) filter. LFE-channel (Low Frequency Enhancement channel) for multi channel mode
            mfxU16 SynchroMode; // set an algorithm of searching mp3 headers
        };
        struct {   /* AAC Encoding Options */
            mfxU16       OutputFormat;
            mfxU16       StereoMode;
            mfxU16       NoiseShapingModel;
        };
    };
} mfxInfoAudioMFX;


typedef struct {
    mfxU32  reserved[5];
    mfxU16  AsyncDepth;

    union {
        mfxInfoAudioMFX  mfx;
    };
    mfxU16            Protected;
    mfxExtBuffer**    ExtParam;
    mfxU16            NumExtParam;
} mfxAudioParam;


//Memory allocation information
typedef struct {
    mfxU32  SuggestedInputSize;     // max suggested frame size of input stream
    mfxU32  SuggestedOutputSize;    // max suggested frame size of output stream
    mfxU32  reserved[6];
} mfxAudioAllocRequest;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif


