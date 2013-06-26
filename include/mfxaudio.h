/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfxaudio.h

\* ****************************************************************************** */

#ifndef __MFXAUDIO_H__
#define __MFXAUDIO_H__

#include "mfxvideo.h"


/* This is the external include file for the Intel(R) Media Software Development Kit product */
#define MFX_AUDIO_VERSION_MAJOR 1
#define MFX_AUDIO_VERSION_MINOR 0

#pragma warning(disable: 4201)


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

// audio core

typedef struct _mfxSyncPoint *mfxSyncPoint;
mfxStatus MFX_CDECL MFXAudioCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);
mfxStatus MFX_CDECL MFXInitAudio(mfxIMPL implParam,mfxVersion *ver, mfxSession *session);
mfxStatus MFX_CDECL MFXCloseAudio(mfxSession session);

mfxStatus MFX_CDECL MFXAudioQueryIMPL(mfxSession session, mfxIMPL *impl);
mfxStatus MFX_CDECL MFXAudioQueryVersion(mfxSession session, mfxVersion *version);

/* AudioENCODE */
mfxStatus MFX_CDECL MFXAudioENCODE_Query(mfxSession session, mfxAudioParam *in, mfxAudioParam *out);
mfxStatus MFX_CDECL MFXAudioENCODE_QueryIOSize(mfxSession session, mfxAudioParam *par, mfxAudioAllocRequest *request);
mfxStatus MFX_CDECL MFXAudioENCODE_Init(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioENCODE_Reset(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioENCODE_Close(mfxSession session);
mfxStatus MFX_CDECL MFXAudioENCODE_GetAudioParam(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioENCODE_EncodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxBitstream *buffer_out, mfxSyncPoint *syncp);


/*AudioDecode*/
mfxStatus MFX_CDECL MFXAudioDECODE_Query(mfxSession session, mfxAudioParam *in, mfxAudioParam *out);
mfxStatus MFX_CDECL MFXAudioDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxAudioParam* par);
mfxStatus MFX_CDECL MFXAudioDECODE_Init(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioDECODE_Reset(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioDECODE_Close(mfxSession session);
mfxStatus MFX_CDECL MFXAudioDECODE_QueryIOSize(mfxSession session, mfxAudioParam *par, mfxAudioAllocRequest *request);
mfxStatus MFX_CDECL MFXAudioDECODE_GetAudioParam(mfxSession session, mfxAudioParam *par);
mfxStatus MFX_CDECL MFXAudioDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs /*mfxRAWAudio *buffer_out*/,mfxBitstream *buffer_out, mfxSyncPoint *syncp);

#endif