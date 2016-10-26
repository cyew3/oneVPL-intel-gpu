//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __AVSYNC_H__
#define __AVSYNC_H__

#include "umc_structures.h"
#include "umc_module_context.h"
#include "umc_thread.h"
#include "umc_event.h"
#include "umc_mutex.h"
#include "umc_data_reader.h"
#include "umc_splitter.h"
#include "umc_base_codec.h"
#include "umc_audio_render.h"
#include "umc_video_decoder.h"
#include "umc_video_render.h"
#include "umc_dual_thread_codec.h"

#include "timed_color_converter.h"
#include "umc_data_pointers_copy.h"
#include "umc_module_context.h"
#include "umc_video_data.h"
#include "vm_time.h"
#include "umc_event.h"

#include "umc_structures.h"

class AVSync
{
public:
    AVSync();

    class Stat {
    public:
        Ipp64f    dfDuration;             // stream duration
        Ipp64f    dfFrameRate;            // frame rate
        Ipp64f    dfFrameTime;            // frame presentation time
        Ipp64f    dfRenderTime;           // total time spent for rendering
        Ipp64f    dfRenderRate;           // estimated rendering rate (including decoding)
        Ipp32u    uiFramesDecoded;        // number decoded video frames
        Ipp32u    uiFramesRendered;       // number rendered video frames
        Ipp64f    dfDecodeTime;           // total time spent for decoding video
        Ipp64f    dfConversionTime;       // total color conversion time
        Ipp64f    dfDecodeRate;           // estimated decoding rate
        Ipp64f    dfConversionRate;       // estimated color conversion rate
        Ipp64f    dfAudioDecodeTime;
        Ipp64f    dfAudioPlayTime;
        Ipp64f    dfAudioDecodeRate;
        Ipp32u    uiFrameNum;             // current frame number at rendering
        Ipp32u    uiSkippedNum;           // skipped frames
        Stat() { Reset(); }

        void Reset()
        {
            dfFrameTime = 0.0;
            dfRenderTime = 0.0;
            dfRenderRate = 0.0;
            uiFramesDecoded = 0;
            dfDecodeTime = 0.0;
            dfConversionTime = 0.0;

            dfDecodeRate = 0.0;
            dfAudioDecodeTime = 0.0;
            dfAudioPlayTime = 0.0;
            dfAudioDecodeRate = 0.0;
            uiFrameNum = 0;
            uiSkippedNum = 0;
        }
    };

    struct ExternalInfo
    {
        ExternalInfo()
        {
            memset(this, 0, sizeof(ExternalInfo));
        }
        Ipp32u uiDataReaderFlags;
        Ipp32u ulSplitterFlags;
        Ipp32u ulVideoDecoderFlags;
        Ipp32u ulAudioDecoderFlags;
        Ipp32u ulVideoRenderFlags;
        Ipp32u ulAudioRenderFlags;
        UMC::DataReader  *m_pDataReader;
        UMC::Splitter    *m_pSplitter;
        UMC::AudioRender *m_pAudioRender;
        UMC::VideoDecoder*m_pVideoDecoder;
        UMC::VideoRender *m_pVideoRender;
        UMC::BaseCodec   *m_pAudioDecoder;
        UMC::DualThreadedCodec* m_pDSAudioCodec;
    };

    struct CommonCtl // control for all windows
    {
        vm_char    file_list[UMC::MAXIMUM_PATH]; // a list of playback files, max 11
        UMC::ColorFormat cformat;          // YUV color format
        Ipp32s     iBitDepth;
        Ipp32u     ulReduceCoeff;          // Reduce coefficient
        Ipp32u     ulSplitterFlags;        // Splitter Flags
        Ipp32u     ulVideoDecoderFlags;    // Video Decoder Flags
        Ipp32u     ulAudioDecoderFlags;    // Audio Decoder Flags
        Ipp32u     ulVideoRenderFlags;     // Video Render Flags
        Ipp32u     ulAudioRenderFlags;     // Audio Render Flags
        UMC::ModuleContext* pRenContext;   // Module context (HWND or some other)
        UMC::ModuleContext* pReadContext;  // Module context (local or remote)
        UMC::sRECT  rectDisp;               // Display size
        UMC::sRECT  rectRange;              // Screen size
        Ipp32u     lInterpolation;         // Interpolation Flags
        Ipp32u     uiPrefVideoRender;      // Prefered video render
        Ipp32u     uiPrefAudioRender;      // Prefered audio render
        Ipp32u     uiPrefDataReader;       // Prefered data reader
        Ipp32u     lDeinterlacing;         // Deinterlacing Flags
        Ipp32u     uiLimitVideoDecodeThreads;//up limit of number video
                                             //decoding threads
        Ipp32u     uiTrickMode;

        Ipp32u     uiSelectedVideoID;      //up limit of number video
        Ipp32u     uiSelectedAudioID;      //up limit of number video
        //allocate this one and pass any external info to AVSync
        // external info can be external splitter, dataReader and etc
        ExternalInfo *  pExternalInfo;
        bool       terminate;              // terminate the program after playback
        bool       performance;            // Performance statistic
        bool       repeat;                 // repeatedly playback
        bool       fullscr;                // turn on full screen
        bool       stick;                  // stick to the window size
        bool       debug;                  // enable step & fast forward, sound disabled
        bool       step;                   // enable step & fast forward, sound disabled
        bool       bSync;                  //  Play synchronously even if no sound
        vm_char    szOutputAudio[UMC::MAXIMUM_PATH];
        vm_char    szOutputVideo[UMC::MAXIMUM_PATH];
        CommonCtl();
    };

    // Initialize movie play back
    virtual UMC::Status Init(CommonCtl& rControlParams);

    // Finalize playback, wait until end if wait==true
    void Close();

    // Playback start
    virtual UMC::Status Start();

    // Stop playback
    virtual UMC::Status Stop();

    // Get playback statistics
    virtual UMC::Status GetStat(Stat& rStat);

    // Step playback
    virtual UMC::Status Step();

    // Resume playback
    virtual UMC::Status Resume();

    // Pause playback
    virtual UMC::Status Pause();

    // Pause playback
    virtual bool IsPlaying();

    void WaitForStop();

    void SetFullScreen(UMC::ModuleContext& ModContext, bool bFoolScreen);

    void HideSurface();
    void ShowSurface();

    //  Stream position control
    Ipp64u GetStreamSize();
    void GetPosition(Ipp64f& rdfPos);
    void SetPosition(Ipp64f dfPos);

    void ResizeDisplay(UMC::sRECT& rDispRect, UMC::sRECT& rRangeRect);

    Ipp32f SetVolume(Ipp32f volume)
    {
        if ( m_pAudioRender )
        {
            return m_pAudioRender->SetVolume(volume);
        }
        return 0;
    }
    Ipp32f GetVolume( void )
    {
        if(m_pAudioRender) return m_pAudioRender->GetVolume();

        return 0;
    }

    UMC::SystemStreamType GetSystemStreamType()
    {   return m_SystemType;   }

    UMC::VideoStreamType GetVideoStreamType()
    {
        return m_pVideoInfo ? m_pVideoInfo->stream_type : UMC::UNDEF_VIDEO;
    }

    UMC::ColorFormat GetVideoFormatType()
    {
        return m_pVideoInfo ? m_pVideoInfo->color_format : UMC::NONE;
    }

    UMC::AudioStreamType GetAudioStreamType()
    {
        return m_pAudioInfo ? m_pAudioInfo->stream_type : UMC::UNDEF_AUDIO;
    }

    Ipp32s GetDstFrmWidth()
    {   return m_DecodedFrameSize.width;   }

    Ipp32s GetDstFrmHeight()
    {   return m_DecodedFrameSize.height;  }

    Ipp32s GetSrcFrmWidth()
    {
        return m_pVideoInfo ? m_pVideoInfo->clip_info.width : 0;
    }

    Ipp32s GetSrcFrmHeight()
    {
        return m_pVideoInfo ? m_pVideoInfo->clip_info.height : 0;
    }

    Ipp64f GetSrcFrmRate()
    {
        return m_pVideoInfo ? m_pVideoInfo->framerate : 0;
    }

    Ipp32s GetSrcFrmBitRate()
    {
        return m_pVideoInfo ? m_pVideoInfo->bitrate : 0;
    }

    Ipp32s GetAudioSmplFreq()
    {
        return m_pAudioInfo ? m_pAudioInfo->sample_frequency : 0;
    }

    Ipp32s GetAudioBitRate()
    {
        return m_pAudioInfo ? m_pAudioInfo->bitrate : 0;
    }

    Ipp32s GetAudioBitPerSmpl()
    {
        return m_pAudioInfo ? m_pAudioInfo->bitPerSample : 0;
    }

    Ipp32s GetAudioNChannel()
    {
        return m_pAudioInfo ? m_pAudioInfo->channels : 0;
    }

    const UMC::SplitterInfo* GetProgramsInfo()
    {   return 0;    }

    UMC::Status SetTrickModeSpeed(CommonCtl& rControlParams,
                                  Ipp32u trickFlag, Ipp64f offset);

protected:

    UMC::Status GetFrameStub(UMC::MediaData* pInData,
                             UMC::VideoData& rOutData,
                             Ipp64f& rdfDecTime);


    static Ipp32u VM_THREAD_CALLCONVENTION SyncThreadProc(void* pvParam);
    static Ipp32u VM_THREAD_CALLCONVENTION AudioThreadProc(void* pvParam);
    static Ipp32u VM_THREAD_CALLCONVENTION VideoThreadProc(void* pvParam);

    void GetAVInfo (UMC::SplitterInfo*  pSplitterInfo);

    void SyncProc();
    void AudioProc();
    void VideoProc();

    UMC::Event m_StepEvent;
    UMC::Mutex m_MutAccess;

    UMC::Thread m_SyncThread;
    UMC::Thread m_AudioThread;
    UMC::Thread m_VideoThread;

    UMC::DataReader*   m_pDataReader;
    UMC::Splitter*     m_pSplitter;
    UMC::AudioRender*  m_pAudioRender;
    UMC::VideoDecoder* m_pVideoDecoder;
    UMC::VideoRender*  m_pVideoRender;

    //  Merge it to single class!!!
    UMC::BaseCodec*    m_pAudioDecoder;
    UMC::DualThreadedCodec* m_pDSAudioCodec;
    UMC::MediaBuffer*  m_pMediaBuffer;

    TimedColorConverter m_ColorConverter;
    UMC::DataPointersCopy m_DataPointersCopy;

    UMC::sClipInfo     m_DecodedFrameSize;

    volatile bool      m_bStopFlag;
    volatile bool      m_bAudioPlaying;
    volatile bool      m_bVideoPlaying;
    volatile bool      m_bPaused;

    bool               m_bSync;
    vm_tick            m_lliFreq;
    Ipp64f             m_dfFrameRate;
    Stat               m_Stat;
    UMC::ColorFormat   m_cFormat;
    CommonCtl          m_ccParams;

    UMC::AudioStreamInfo   *m_pAudioInfo;
    UMC::VideoStreamInfo   *m_pVideoInfo;
    UMC::MediaData         *m_pAudioDecSpecInfo;
    UMC::MediaData         *m_pVideoDecSpecInfo;
    Ipp32u                  m_nAudioTrack;
    Ipp32u                  m_nVideoTrack;

    UMC::SystemStreamType   m_SystemType;

};

#endif // __AVSYNC_H__

