//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#include <math.h>
#include "avsync.h"
#include "codec_pipeline.h"
#include "umc_media_data_ex.h"
#include "null_audio_render.h"
#include "umc_module_context.h"
#include "umc_audio_codec.h"

#if !defined(ARM) && !defined(_ARM_)
#define TASK_SWITCH()   vm_time_sleep(0);
#else
#define TASK_SWITCH()
#endif

#undef CLASSNAME
#define CLASSNAME VM_STRING("AVSync")

#define REPOSITION_AVSYNC_PRECISION 0.99

enum
{
    DEF_TIME_TO_SLEEP           = 5
};

AVSync::CommonCtl::CommonCtl():
    cformat(UMC::NONE),                     // YUV color format
    ulSplitterFlags(UMC::UNDEF_SPLITTER),   // Splitter Flags
    ulVideoDecoderFlags(0),                 // Video Decoder Flags
    ulAudioDecoderFlags(0),                 // Audio Decoder Flags
    ulVideoRenderFlags(0),                  // Video Render Flags
    ulAudioRenderFlags(0),                  // Audio Render Flags
    pRenContext(NULL),                      // Module context (HWND or some other)
    pReadContext(NULL),                     // Module context (local or remote)
    lInterpolation(0),                      // Interpolation Flags
    uiPrefVideoRender(UMC::DEF_VIDEO_RENDER),// Prefered video render
    uiPrefDataReader(DEF_DATA_READER),      // Prefered data reader
    lDeinterlacing(0),                      // Deinterlacing Flags
    pExternalInfo(NULL),
    terminate(false),                       // terminate the program after playback
    performance(false),                     // Performance statistic
    repeat(false),                          // repeatedly playback
    fullscr(false),                         // turn on full screen
    stick(true),                            // stick to the window size
    debug(true),                            // enable step & fast forward, sound disabled
    step(false),                            // enable step & fast forward, sound disabled
    bSync(true),                            // Play synchronously even if no sound
    uiLimitVideoDecodeThreads(1),
    uiSelectedVideoID(0),                   //zero is default value to select any video stream
    uiSelectedAudioID(0),                   //zero is default value to select any audio stream
    uiTrickMode(UMC::UMC_TRICK_MODES_NO),
    iBitDepth(8)


{
    memset(szOutputVideo, 0, sizeof(szOutputVideo));
    memset(szOutputAudio, 0, sizeof(szOutputAudio));
}

AVSync::AVSync():
    m_pDataReader(NULL),
    m_pSplitter(NULL),
    m_pAudioDecoder(NULL),
    m_pDSAudioCodec(NULL),
    m_pMediaBuffer(NULL),
    m_pAudioRender(NULL),
    m_pVideoDecoder(NULL),
    m_pVideoRender(NULL),
    m_bStopFlag(false),
    m_bAudioPlaying(false),
    m_bVideoPlaying(false),
    m_bPaused(false),
    m_bSync(false),
    m_dfFrameRate(1),
    m_SystemType(UMC::UNDEF_STREAM),
    m_pAudioInfo(0),
    m_pVideoInfo(0),
    m_nAudioTrack(0),
    m_nVideoTrack(0),
    m_pVideoDecSpecInfo(0),
    m_pAudioDecSpecInfo(0)
{
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::AVSync()"), VM_STRING("AVSync,+"));
    m_DecodedFrameSize.width = 0;
    m_DecodedFrameSize.height = 0;
    m_lliFreq = vm_time_get_frequency();
    m_cFormat = UMC::YV12;
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::AVSync()"), VM_STRING("AVSync,-"));
}
void AVSync::GetAVInfo (UMC::SplitterInfo*  pSplitterInfo)
{
    Ipp32u i;

    m_pVideoInfo = 0;
    m_pAudioInfo = 0;
    m_pAudioDecSpecInfo = 0;
    m_pVideoDecSpecInfo = 0;

    for (i=0; i< pSplitterInfo->m_nOfTracks; i++)
    {
        if (pSplitterInfo->m_ppTrackInfo[i]->m_isSelected)
        {
            if (pSplitterInfo->m_ppTrackInfo[i]->m_Type & UMC::TRACK_ANY_VIDEO)
            {
                VM_ASSERT(!m_pVideoInfo);
                m_nVideoTrack = i;
                m_pVideoInfo = (UMC::VideoStreamInfo*)pSplitterInfo->m_ppTrackInfo[i]->m_pStreamInfo;
                m_pVideoDecSpecInfo = pSplitterInfo->m_ppTrackInfo[i]->m_pDecSpecInfo;
            }
            else if (pSplitterInfo->m_ppTrackInfo[i]->m_Type & UMC::TRACK_ANY_AUDIO)
            {
                VM_ASSERT(!m_pAudioInfo);
                m_nAudioTrack = i;
                m_pAudioInfo = (UMC::AudioStreamInfo*)pSplitterInfo->m_ppTrackInfo[i]->m_pStreamInfo;
                m_pAudioDecSpecInfo = pSplitterInfo->m_ppTrackInfo[i]->m_pDecSpecInfo;
            }
        }//if
    }//for
}

UMC::Status
AVSync::Init(CommonCtl& rControlParams)
{
    UMC::Status umcRes = UMC::UMC_OK;
    UMC::MediaData   FirstFrameA;
    ExternalInfo* pExtInf = rControlParams.pExternalInfo;
    UMC::SplitterInfo*  pSplitterInfo=0;

    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Init"), VM_STRING("Init,+"));

    m_MutAccess.Lock();

    m_ccParams = rControlParams;

    m_cFormat = rControlParams.cformat;

    if (UMC::UMC_OK == umcRes)
    {
        if(pExtInf && pExtInf->m_pDataReader)
        {
            m_pDataReader = pExtInf->m_pDataReader;
        }
        else
        {
            umcRes = CodecPipeline::SelectDataReader(*rControlParams.pReadContext,
                                                     m_pDataReader,
                                                     rControlParams.uiPrefDataReader);
        }
    }

    if (UMC::UMC_OK == umcRes)
    {
        if(pExtInf && pExtInf->m_pSplitter)
        {
            m_pSplitter = pExtInf->m_pSplitter;
        }
        else
        {
            umcRes = CodecPipeline::SelectSplitter(m_pDataReader,
                                                   rControlParams.ulSplitterFlags,
                                                   m_pSplitter,
                                                   rControlParams.uiSelectedVideoID,
                                                   rControlParams.uiSelectedAudioID);

        }
    }

    if (UMC::UMC_OK == umcRes)
    {
        umcRes = m_pSplitter->GetInfo(&pSplitterInfo);

        if (UMC::UMC_OK != umcRes)
        {
            vm_debug_trace(VM_DEBUG_NONE, VM_STRING("Failed to get splitter info\n"));
        }
        else
        {
            GetAVInfo (pSplitterInfo);
            m_SystemType = pSplitterInfo->m_SystemType;
        }
    }

    if (UMC::UMC_OK == umcRes && m_pAudioInfo)
    {
        // Get splitter some time to read out at least one audio frame
        vm_time_sleep(0);

        if (UMC::UMC_OK == umcRes)
        {
            umcRes = CodecPipeline::SelectDTAudioDecoder(*m_pAudioInfo,
                                                         m_pAudioDecoder,
                                                         m_pDSAudioCodec,
                                                         m_pMediaBuffer,
                                                         m_pAudioDecSpecInfo);
        }

        if (UMC::UMC_OK == umcRes && m_pAudioInfo)
        {
            if (UMC::FLAG_AREN_VOID & rControlParams.ulAudioRenderFlags)
            {
                rControlParams.uiPrefAudioRender = UMC::NULL_AUDIO_RENDER;
            }
            umcRes = CodecPipeline::SelectAudioRender(*m_pAudioInfo,
                                                      *rControlParams.pRenContext,
                                                      m_pAudioRender,
                                                      rControlParams.uiPrefAudioRender,
                                                      rControlParams.szOutputAudio);
        }
        else if (UMC::UMC_ERR_INVALID_STREAM == umcRes)
        {
            m_pSplitter->EnableTrack(m_nAudioTrack, 0); // disable unsupported track
            m_pAudioInfo = 0;
            umcRes = UMC::UMC_OK;
        }
    }

    if (UMC::UMC_OK == umcRes && m_pVideoInfo)
    {
        UMC::BaseCodec *pColorConverter = ((UMC::NONE == rControlParams.cformat) ?
                                           ((UMC::BaseCodec *) &m_DataPointersCopy) :
                                           ((UMC::BaseCodec *) &m_ColorConverter));

         umcRes = CodecPipeline::SelectVideoDecoder(*m_pVideoInfo,
                                                   m_pVideoDecSpecInfo,
                                                   rControlParams.lInterpolation,
                                                   rControlParams.lDeinterlacing,
                                                   rControlParams.uiLimitVideoDecodeThreads,
                                                   rControlParams.ulVideoDecoderFlags,
                                                   *pColorConverter,
                                                   m_pVideoDecoder);

        if (UMC::UMC_OK == umcRes)
        {
            UMC::VideoDecoderParams vParams;
            m_pVideoDecoder->GetInfo(&vParams);

            if(!(Ipp32s)m_pVideoInfo->framerate)
                m_pVideoInfo->framerate = vParams.info.framerate;
            if(!m_pVideoInfo->clip_info.width)
                m_pVideoInfo->clip_info.width = vParams.info.clip_info.width;
            if(!m_pVideoInfo->clip_info.height)
                m_pVideoInfo->clip_info.height = vParams.info.clip_info.height;

            m_DecodedFrameSize = m_pVideoInfo->clip_info;
        }

        if (UMC::UMC_OK == umcRes)
        {
            if(pExtInf && pExtInf->m_pVideoRender)
            {
                m_pVideoRender = pExtInf->m_pVideoRender;
            }
            else
            {
                if (UMC::DEF_VIDEO_RENDER == rControlParams.uiPrefVideoRender)
                {
                    rControlParams.uiPrefVideoRender =
#if defined(USE_FW_RENDERER)
                        UMC::FW_VIDEO_RENDERER;
#elif defined(USE_NULL_RENDERER)
                        UMC::NULL_VIDEO_RENDERER;
#elif defined(_WIN32_WINNT)
                        UMC::BLT_VIDEO_RENDER;
#else
                        UMC::DEF_VIDEO_RENDER;
#endif
                }
                umcRes = CodecPipeline::SelectVideoRender(*rControlParams.pRenContext,
                                                          m_DecodedFrameSize,
                                                          rControlParams.rectDisp,
                                                          rControlParams.rectDisp,
                                                          rControlParams.cformat,
                                                          rControlParams.ulVideoRenderFlags,
                                                          m_pVideoRender,
                                                          rControlParams.uiPrefVideoRender,
                                                          rControlParams.szOutputVideo);
            }

            if (UMC::UMC_OK != umcRes)
            {
                vm_debug_message(VM_STRING("Failed to intialize video render\n"));
            }
        }
        if (UMC::UMC_OK != umcRes) {
          m_pSplitter->EnableTrack(m_nVideoTrack, 0); // disable unsupported track
        }

        if (UMC::UMC_OK == umcRes)
        {   umcRes = m_StepEvent.Init(1,1); }

        if (UMC::UMC_OK == umcRes &&
            m_pVideoInfo)
        {
            if(rControlParams.step)
                umcRes = m_StepEvent.Reset();
            else
                umcRes = m_StepEvent.Set();
        }

        if (UMC::UMC_OK == umcRes)
        {
            m_bSync = rControlParams.bSync;
            m_dfFrameRate = m_pVideoInfo->framerate;
            m_Stat.Reset();
            m_bStopFlag = false;
            m_Stat.dfFrameRate = m_dfFrameRate;
            if (m_pAudioInfo)
            {
                m_Stat.dfDuration = (m_pVideoInfo->duration < m_pAudioInfo->duration) ?
                                  m_pAudioInfo->duration: m_pVideoInfo->duration;
            }
            else
            {
                 m_Stat.dfDuration = m_pVideoInfo->duration;
            }

        }
    }

    if (UMC::UMC_OK == umcRes)
    {
        vm_string_printf(VM_STRING("\n"));
        if (pSplitterInfo)
        {
            vm_string_printf(VM_STRING("Stream Type  :\t\t%s\n"), UMC::GetStreamTypeString(m_SystemType));
        }
        if (m_pVideoInfo)
        {
            vm_string_printf(VM_STRING("Video Info   :\n"));
            vm_string_printf(VM_STRING("-Video Type  :\t\t%s\n"), UMC::GetVideoTypeString((UMC::VideoStreamType)m_pVideoInfo->stream_type));
            vm_string_printf(VM_STRING("-Resolution  :\t\t%dx%d\n"), m_pVideoInfo->clip_info.width,m_pVideoInfo->clip_info.height);
            vm_string_printf(VM_STRING("-Frame Rate  :\t\t%.2lf\n"), m_pVideoInfo->framerate);
        }
        if (m_pAudioInfo)
        {
            vm_string_printf(VM_STRING("Audio Info   :\n"));
            vm_string_printf(VM_STRING("-Audio Type  :\t\t%s\n"), UMC::GetAudioTypeString((UMC::AudioStreamType)m_pAudioInfo->stream_type));
            vm_string_printf(VM_STRING("-S.Frequency :\t\t%d\n"), m_pAudioInfo->sample_frequency);
            vm_string_printf(VM_STRING("-Num.Channel :\t\t%d\n"), m_pAudioInfo->channels);
            vm_string_printf(VM_STRING("-BitPerSample:\t\t%d\n"), m_pAudioInfo->bitPerSample);
            vm_string_printf(VM_STRING("\n"));
        }
    }

    m_MutAccess.Unlock();

    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Init"), VM_STRING("Init,-"));

    return umcRes;
}

UMC::Status
AVSync::Start()
{
    UMC::Status umcRes = UMC::UMC_OK;

    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Start"), VM_STRING("Start,+"));

    m_MutAccess.Lock();

    m_bStopFlag = false;
    m_bAudioPlaying = false;
    m_bVideoPlaying = false;
    m_Stat.Reset();

    if (UMC::UMC_OK == umcRes && m_pVideoInfo)
    {
        VM_ASSERT(NULL != m_pVideoRender);
        vm_debug_trace(VM_DEBUG_INFO, VM_STRING("Start: VideoProc start:\n"));
        umcRes = m_VideoThread.Create(VideoThreadProc,
                                      static_cast<AVSync*>(this));
    }

    if (UMC::UMC_OK == umcRes &&        m_pAudioInfo)
    {
        VM_ASSERT(NULL != m_pAudioRender);
        VM_ASSERT(m_pAudioInfo);
        vm_debug_trace(VM_DEBUG_INFO, VM_STRING("Start: AudioThread start:\n"));
        umcRes = m_AudioThread.Create(AudioThreadProc,
                                      static_cast<AVSync*>(this));
    }

    if (UMC::UMC_OK == umcRes &&
        (NULL != m_pVideoRender))
//        (NULL != m_pVideoRender || NULL != m_pAudioRender))
    {
        vm_debug_trace(VM_DEBUG_INFO, VM_STRING("Start: SyncThread start\n"));
        umcRes = m_SyncThread.Create(SyncThreadProc,
                                     static_cast<AVSync*>(this));
    }

    if (UMC::UMC_OK == umcRes && NULL != m_pVideoRender)
    {
        vm_debug_trace(VM_DEBUG_INFO, VM_STRING("Start: Video Render ShowSurface:\n"));
        m_pVideoRender->ShowSurface();
    }

    if (UMC::UMC_OK == umcRes)
    {
        vm_time_sleep(1);

#if defined (ARM) || defined (_ARM_)
        m_SyncThread.SetPriority(VM_THREAD_PRIORITY_NORMAL);
        m_AudioThread.SetPriority(VM_THREAD_PRIORITY_NORMAL);
        m_VideoThread.SetPriority(VM_THREAD_PRIORITY_NORMAL);
#else
        m_SyncThread.SetPriority(VM_THREAD_PRIORITY_HIGHEST);
        //m_AudioThread.SetPriority(VM_THREAD_PRIORITY_HIGHEST);
        m_AudioThread.SetPriority(VM_THREAD_PRIORITY_HIGH);
        m_VideoThread.SetPriority(VM_THREAD_PRIORITY_NORMAL);
#endif
    }

    if (UMC::UMC_OK != umcRes)
    {
        Stop();
    }

    m_MutAccess.Unlock();

    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Start"), VM_STRING("Start,-,1"));
    return umcRes;
}

UMC::Status AVSync::Stop()
{
    Close();
    return UMC::UMC_OK;
}

void AVSync::Close()
{
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,+"));

    m_MutAccess.Lock();

    m_bStopFlag = true;
    m_StepEvent.Set();

    if (m_VideoThread.IsValid()) {
        vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,before wait video_thread"));
        m_VideoThread.Wait();
        vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,after wait video_thread"));
    }

    //Splitter can be stopped only after the video thread has finished
    if (NULL != m_pSplitter)
    {
        m_pSplitter->Stop();
    }

    if (m_SyncThread.IsValid())  {
        vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,before wait sync_thread"));
        m_SyncThread.Wait();
        vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,after wait sync_thread"));
    }
    if (m_AudioThread.IsValid()) {
        vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,before wait audio_thread"));
        m_AudioThread.Wait();
        vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,after wait audio_thread"));
    }
    if (NULL != m_pVideoRender) {
        delete m_pVideoRender;
        m_pVideoRender = NULL;
    }
    if (NULL != m_pAudioRender) {
        m_pAudioRender->Stop();
        delete m_pAudioRender;
        m_pAudioRender = NULL;
    }
    if ( NULL != m_pVideoDecoder )
    {
        delete m_pVideoDecoder;
        m_pVideoDecoder = NULL;
    }
    if ( NULL != m_pDSAudioCodec )
    {
        delete m_pDSAudioCodec;
        m_pDSAudioCodec = NULL;
    }
    if ( NULL != m_pMediaBuffer )
    {
        delete m_pMediaBuffer;
        m_pMediaBuffer = NULL;
    }
    if ( NULL != m_pAudioDecoder )
    {
        delete m_pAudioDecoder;
        m_pAudioDecoder = NULL;
    }
    if (NULL != m_pSplitter)
    {
        if(!(m_ccParams.pExternalInfo && m_ccParams.pExternalInfo->m_pSplitter))
        {
            delete m_pSplitter;
            m_pSplitter = NULL;
        }
    }
    if (NULL != m_pDataReader)
    {
        if(!(m_ccParams.pExternalInfo && m_ccParams.pExternalInfo->m_pDataReader))
        {
            delete m_pDataReader;
            m_pDataReader = NULL;
        }
    }
    m_MutAccess.Unlock();

    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Close"), VM_STRING("Close,-"));
}

UMC::Status AVSync::GetStat(Stat& rStat)
{
    UMC::Status umcRes = UMC::UMC_OK;
    m_MutAccess.Lock();
    rStat = m_Stat;
    m_MutAccess.Unlock();
    return umcRes;
}

UMC::Status AVSync::Step()
{
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Step"), VM_STRING("Step,+"));
    UMC::Status umcRes = m_StepEvent.Pulse();
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Step"), VM_STRING("Step,-"));
    return umcRes;
}

UMC::Status AVSync::Resume()
{
    UMC::Status umcRes = UMC::UMC_OK;
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Resume"), VM_STRING("Resume,+"));
    m_MutAccess.Lock();

    if (NULL != m_pAudioRender)
    {   umcRes = m_pAudioRender->Pause(false);  }

    if (UMC::UMC_OK == umcRes)
    {
        m_bPaused = false;
        umcRes = m_StepEvent.Set();
    }

    m_MutAccess.Unlock();
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Resume"), VM_STRING("Resume,-"));
    return umcRes;
}

UMC::Status AVSync::Pause()
{
    UMC::Status umcRes = UMC::UMC_OK;
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Pause"), VM_STRING("Pause,+"));
    m_MutAccess.Lock();

    m_bPaused = true;

    if (!m_AudioThread.IsValid())
    {   umcRes = m_StepEvent.Pulse();   }
    else
    {   umcRes = m_pAudioRender->Pause(true);   }

    m_MutAccess.Unlock();
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::Pause"), VM_STRING("Pause,-"));
    return umcRes;
}

bool AVSync::IsPlaying()
{
    return m_SyncThread.IsValid() ||
           m_AudioThread.IsValid() ||
           m_VideoThread.IsValid();
}

void AVSync::WaitForStop()
{
    //always synchronized stream finishing with SyncProc
    if(m_SyncThread.IsValid())
    {
        vm_debug_trace_withfunc(VM_DEBUG_FILE,
                                VM_STRING("AVSync::WaitForStop"),
                                VM_STRING("PlayFileList,before wait sync_thread"));
        m_SyncThread.Wait();
        vm_debug_trace_withfunc(VM_DEBUG_FILE,
                                VM_STRING("AVSync::WaitForStop"),
                                VM_STRING("PlayFileList,after wait sync_thread"));
    }
    else
    {
        //there is an exception: pure audio files
        //SyncProc is absent in this case
        if(m_AudioThread.IsValid())
        {
            vm_debug_trace_withfunc(VM_DEBUG_FILE,
                                    VM_STRING("AVSync::WaitForStop"),
                                    VM_STRING("PlayFileList,before wait audio_thread"));
            m_AudioThread.Wait();
            vm_debug_trace_withfunc(VM_DEBUG_FILE,
                                    VM_STRING("AVSync::WaitForStop"),
                                    VM_STRING("PlayFileList,after wait audio_thread"));
        }
    }
}

void AVSync::SetFullScreen(UMC::ModuleContext& ModContext,
                           bool bFoolScreen)
{
    if (NULL != m_pVideoRender)
    {   m_pVideoRender->SetFullScreen(ModContext, bFoolScreen); }
}

Ipp64u AVSync::GetStreamSize()
{
    Ipp64u stRes = 0;
    if (NULL != m_pDataReader)
    {   stRes = m_pDataReader->GetSize();   }
    return stRes;
}

void AVSync::GetPosition(Ipp64f& rdfPos)
{
    rdfPos = 0;
    if (NULL != m_pSplitter)
    {
        /*m_pSplitter->GetPosition(rdfPos);*/
        // commented
        // since method Splitter::GetPosition
        // can not really be implemented to do what user expects, it was removed from API
        // If AVSync is required to provide such functionality
        // it needs to implement it by itself
    }
}

void AVSync::HideSurface()
{
    if (NULL != m_pVideoRender)
    {   m_pVideoRender->HideSurface();  }
}

void AVSync::ShowSurface()
{
    if (NULL != m_pVideoRender)
    {   m_pVideoRender->ShowSurface();  }
}

void AVSync::SetPosition(Ipp64f /*dfPos*/)
{

}

UMC::Status AVSync::GetFrameStub(UMC::MediaData* pInData,
                                 UMC::VideoData& rOutData,
                                 Ipp64f& rdfDecTime)
{
    UMC::Status umcRes = UMC::UMC_OK;
    vm_tick DecStartTime, DecStopTime;
    Ipp32s iInDataSize = 0;

    // If any input data is present
    if (NULL != pInData)
    {   iInDataSize = (Ipp32s) pInData->GetDataSize();   }

    DecStartTime = vm_time_get_tick();
    umcRes = m_pVideoDecoder->GetFrame(pInData, &rOutData);
    DecStopTime = vm_time_get_tick();

    rdfDecTime += (Ipp64f)(Ipp64s)(DecStopTime - DecStartTime) /
                                  (Ipp64f)(Ipp64s)m_lliFreq;
    vm_debug_trace1(VM_DEBUG_NONE,
                    VM_STRING("GetFrame: %lf\n"), rOutData.GetTime());

    return umcRes;
}

#define SKIP_FRAME_TOLERENCE 7

void AVSync::SyncProc()
{
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::SyncProc"), VM_STRING("SyncProc,+"));
    vm_tick t1 = 0, t2 = 0, t2_prev = 0;
    Ipp64f flip_time = 0.001;
    Ipp64f flip_times[3] = {0.001,0.001,0.001};
    UMC::Status umcRes = UMC::UMC_OK;
    bool bNoAudioAnyMore = (NULL == m_pAudioRender);
    bool bNoVideoAnyMore = (NULL == m_pVideoRender);
    bool bNullAudioRender = false;
    UMC::AudioRender* pStoppedAudioRender = NULL;
    Ipp64f prevVideoPts = 0;
    UMC::NULLAudioRenderParams renderParams;

    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::SyncProc start\n"));

    // Wait until video and audio decoding threads will pass some data to
    // the renders - we can't start synchronization without it
    while (!m_bStopFlag &&
          ((NULL != m_pVideoRender && !m_bVideoPlaying) ||
           (NULL != m_pAudioRender && !m_bAudioPlaying)))
    {   vm_time_sleep(1);   }

    m_Stat.uiFramesRendered = 0;
    // Continue video data rendering until no more data in audio and video
    // render left
    for (Ipp32s frame_num = 0, skip_window = 0;
         UMC::UMC_OK == umcRes && (!bNoAudioAnyMore || !bNoVideoAnyMore) && !m_bStopFlag;
         frame_num++)
    {
        m_Stat.uiFrameNum = frame_num + m_Stat.uiSkippedNum;

        // Check next frame PTS if any
        if (!bNoVideoAnyMore)
        {
            UMC::Status get_fr_sts = UMC::UMC_OK;
            do
            {
                VM_ASSERT(NULL != m_pVideoRender);
                get_fr_sts = m_pVideoRender->GetRenderFrame(&(m_Stat.dfFrameTime));
                if (UMC::UMC_OK != get_fr_sts)
                    vm_time_sleep(DEF_TIME_TO_SLEEP);
            } while (get_fr_sts == UMC::UMC_ERR_TIMEOUT && !m_bStopFlag);

            vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("next frame: %f"), m_Stat.dfFrameTime);

            if (m_Stat.dfFrameTime == -1.0 || get_fr_sts != UMC::UMC_OK)
            {
               bNoVideoAnyMore = true;
            }
        }
        if(frame_num == 0)
        {
            prevVideoPts = m_Stat.dfFrameTime;
        }

        // If we have no more audio but some video or if we have no audio at all
        if (m_bSync && ((NULL == m_pAudioRender ) ||
                        (NULL == pStoppedAudioRender &&
                         m_bAudioPlaying &&
                         bNoAudioAnyMore &&
                        !bNoVideoAnyMore)))
        {
            pStoppedAudioRender = m_pAudioRender;
            m_pAudioRender = new UMC::NULLAudioRender(m_Stat.dfFrameTime);
            if (NULL == m_pAudioRender)
            {
                //  signal error, stop everything
                m_bStopFlag = true;
                umcRes = UMC::UMC_ERR_ALLOC;
            }
            else
            {
                // Start time counting
                m_pAudioRender->Pause(false);
                m_bAudioPlaying = true;
                bNullAudioRender = true;
            }
        }


        TASK_SWITCH();

        Ipp64f ft = m_Stat.dfFrameTime - flip_time;

        vm_debug_trace2(VM_DEBUG_NONE, VM_STRING("#%d, video time: %f\n"),
                       frame_num, m_Stat.dfFrameTime);

        // Let's synchronize video to audio if there is some data in the audio
        // render or NULLAudioRender is used
        if (!bNoAudioAnyMore || bNullAudioRender)
        {
            VM_ASSERT(NULL != m_pAudioRender);
            VM_ASSERT(NULL != m_pVideoDecoder);

            Ipp64f dfAudioTime = m_pAudioRender->GetTime();
            dfAudioTime += m_pAudioRender->GetDelay();

            vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("\t\taudio time: %f\n"), dfAudioTime);
            vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("video time: %f\n"), ft);

            // Wait for the next video frame display time if we have one
            if (!bNoVideoAnyMore && m_bSync)
            {
                if(prevVideoPts > ft + 1.0 || prevVideoPts + 1.0 < ft) //PTS jump
                {
                    if(abs(dfAudioTime - ft) > 1.0)
                    {
                        // try to syncronize video and audio after PTS jump
                        if (!bNullAudioRender)
                        {
                            volatile Ipp64f st1;
                            Ipp32u   n = 0;
                            for (st1 = dfAudioTime;
                                st1 > 0 && (abs(st1 - ft) > 1.0) && !m_bStopFlag && n < 100; n++)
                            {
                                vm_time_sleep(DEF_TIME_TO_SLEEP);
                                st1 = m_pAudioRender->GetTime();
                                vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("\t\t\trestore time: %f\n"), st1);
                                dfAudioTime = st1;
                            }
                        }
                        else
                        {

                            renderParams.m_InitPTS = m_Stat.dfFrameTime;
                            m_pAudioRender->SetParams(&renderParams);
                            m_pAudioRender->Pause(false);
                        }
                     }
                }

                if (ft > dfAudioTime)
                {
                    skip_window = 0;
                    umcRes = m_pVideoDecoder->ResetSkipCount();
                    if (UMC::UMC_ERR_NOT_IMPLEMENTED == umcRes)
                    {    umcRes = UMC::UMC_OK;    }

                    volatile Ipp64f st1;
                    Ipp32u   n = 0;

                    for (st1 = dfAudioTime;
                         st1 >= dfAudioTime && ft > st1 && !m_bStopFlag && n < 100 ;n++)
                    {
                        Ipp32f a=0;
                        vm_time_sleep(IPP_MAX(0,IPP_MIN(1,(Ipp32s)((ft-st1)*1000))));
                        st1 = m_pAudioRender->GetTime();
                        a = (Ipp32f) m_pAudioRender->GetDelay();
                        st1 += a;
                        vm_debug_trace2(VM_DEBUG_NONE, VM_STRING("wait:\t\taudio time: %f+%f\n"),
                                        st1,a);
                        if(m_bPaused)
                        {
                           m_pVideoRender->ShowLastFrame();
                        }
                    }
                }
                else if (ft < dfAudioTime &&
                         (dfAudioTime - ft > (0.7/m_dfFrameRate)))
                {
                    if (++skip_window >= SKIP_FRAME_TOLERENCE)
                    {
                        skip_window = 0;
                        umcRes = m_pVideoDecoder->SkipVideoFrame(1);
                        if (UMC::UMC_ERR_NOT_IMPLEMENTED == umcRes)
                        {   umcRes = UMC::UMC_OK;   }
                        vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("skip: %d\n"), 1);
                    }
                }
            }

            // Stop synchronization efforts and play the rest of the video on
            // maximum speed if we have no more audo samples
            if (-1.0 == dfAudioTime)
            {   bNoAudioAnyMore = true; }
        }
        prevVideoPts = m_Stat.dfFrameTime;

        // It's time to display next video frame
        if (UMC::UMC_OK == umcRes && !bNoVideoAnyMore)
        {
            t1 = vm_time_get_tick();
            umcRes = m_pVideoRender->RenderFrame();
            t2 = vm_time_get_tick();
            m_Stat.uiFramesRendered++;
        }

        // Update Statistic structure and frame display statistic
        if (UMC::UMC_OK == umcRes)
        {
            m_Stat.uiSkippedNum = m_pVideoDecoder->GetNumOfSkippedFrames();

            Ipp64f this_flip_time =
                    (Ipp64f)(Ipp64s)(t2-t1)/(Ipp64f)(Ipp64s)m_lliFreq;

            flip_times[0] = flip_times[1];
            flip_times[1] = flip_times[2];
            flip_times[2] = this_flip_time;

            flip_time = (flip_times[0] + flip_times[1] + flip_times[2]) / 3;

            while (VM_TIMEOUT == m_StepEvent.Wait(500))
            {
                m_pVideoRender->ShowLastFrame();
                if (m_bStopFlag)
                {   break;  }
            }

            // ignore the first frame (might be a long wait to be synchronized
            if (1 < m_Stat.uiFrameNum)
            {
                m_Stat.dfRenderTime += (Ipp64f)(Ipp64s)(t2-t2_prev) /
                                               (Ipp64f)(Ipp64s)m_lliFreq;
                m_Stat.dfRenderRate =
                    (Ipp64f)(m_Stat.uiFrameNum - 1) / m_Stat.dfRenderTime;
            }
            t2_prev = t2;

            TASK_SWITCH();
        }

    }

    if (NULL != pStoppedAudioRender)
    {   delete pStoppedAudioRender; }

    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::SyncProc exit\n"));
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::SyncProc"), VM_STRING("SyncProc,-"));
}

void AVSync::VideoProc()
{
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::VideoProc"), VM_STRING("VideoProc,+"));
    UMC::Status umcRes = UMC::UMC_OK;
    UMC::Status umcSplitRes = UMC::UMC_OK;
    UMC::MediaDataEx data;
    UMC::VideoData out_data;
    bool bVideoRndrIsLocked = false;

    out_data.Init(m_DecodedFrameSize.width,
                  m_DecodedFrameSize.height,
                  m_cFormat,
                  m_ccParams.iBitDepth);

    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::VideoProc start\n"));
    // Main decoding cycle
    for (Ipp32s iFrameNum = 0; !m_bStopFlag && UMC::UMC_OK == umcRes; iFrameNum++)
    {
        // Wait for the free buffer in the video render
        do
        {
            umcRes = m_pVideoRender->LockInputBuffer(&out_data);

            // Be aware that video render surface was locked and not released yet
            if (UMC::UMC_OK == umcRes)
            {
                bVideoRndrIsLocked = true;
                break;
            }
            // there is only one legal error return value,
            // all other are incorrect.
            else if ((!m_bStopFlag) &&
                (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes ||UMC::UMC_ERR_TIMEOUT == umcRes ))
            {
                vm_time_sleep(DEF_TIME_TO_SLEEP);
            }
            else
            {
                vm_string_printf(VM_STRING("Error in video render\n"));
                break;
            }

        } while (!m_bStopFlag );




        // Repeat decode procedure until the decoder will agree to decompress
        // at least one frame
        Ipp64f dfDecTime = 0;
        do {
            // Get some more data from the the splitter if we've decoded
            // all data from the previous buffer
            if ((4 >= data.GetDataSize() || umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
                && UMC::UMC_OK == umcSplitRes)
            {
                while ((UMC::UMC_ERR_NOT_ENOUGH_DATA==(umcSplitRes = umcRes = m_pSplitter->GetNextData(&data,m_nVideoTrack ))) &&  !m_bStopFlag)
                {
                    vm_time_sleep(DEF_TIME_TO_SLEEP);
                }
                vm_debug_trace1(VM_DEBUG_ALL, VM_STRING("VideoProc: data size from splitter is %f\n"),
                               data.GetTime());
                if(UMC::UMC_WRN_REPOSITION_INPROGRESS == umcSplitRes)
                {
                    umcSplitRes = UMC::UMC_OK;
                }
            }
            else
            {   umcRes = UMC::UMC_OK;   }

            vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("Splitter PTS: %lf\n"), data.GetTime());

            // Ok, here is no more data in the splitter. Let's extract the rest
            // of decoded data from the decoder
            if (UMC::UMC_OK == umcRes && NULL == data.GetDataPointer())
            {   umcSplitRes = UMC::UMC_ERR_END_OF_STREAM;   }

            if (UMC::UMC_ERR_END_OF_STREAM == umcSplitRes)
            {   umcRes = UMC::UMC_OK;   }

            out_data.SetTime(data.GetTime());

            if (UMC::UMC_OK == umcRes)
            {
                if (UMC::UMC_ERR_END_OF_STREAM != umcSplitRes)
                {   umcRes = GetFrameStub(&data, out_data, dfDecTime);  }
                else
                // take buffered reference frames from the decoder
                {   umcRes = GetFrameStub(NULL, out_data, dfDecTime);  }
            }

            vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("Decoder PTS: %lf\n"),
                           out_data.GetTime());
            //vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("%d\n"),
            //               out_data.m_FrameType);
        } while (((UMC::UMC_ERR_NOT_ENOUGH_DATA == umcRes && UMC::UMC_ERR_END_OF_STREAM != umcSplitRes) ||
                  UMC::UMC_ERR_SYNC == umcRes) && !m_bStopFlag);

        if (UMC::UMC_OK != umcRes || m_bStopFlag)
        {   break;  }

        // Update statistic structure
        {
            m_Stat.dfConversionTime = m_ColorConverter.GetConversionTime();
            m_Stat.dfDecodeTime += dfDecTime;
            m_Stat.uiFramesDecoded++;
            m_Stat.dfDecodeRate =
                (Ipp64f)(Ipp64s)(m_Stat.uiFramesDecoded) / m_Stat.dfDecodeTime;

            if (0.0 != m_Stat.dfConversionTime)
            {
                m_Stat.dfConversionRate =
                    (Ipp64f)(Ipp64s)(m_Stat.uiFramesDecoded) / m_Stat.dfConversionTime;
            }
            else
            {   m_Stat.dfConversionRate = 0.0;  }
        }

        // Unlock video render surface
        if (bVideoRndrIsLocked)
        {
            umcRes = m_pVideoRender->UnLockInputBuffer(&out_data);
            bVideoRndrIsLocked = false;
        }

        m_bVideoPlaying = true;
    }

    if(m_bVideoPlaying == false)
        m_bStopFlag = true;

    if (bVideoRndrIsLocked)
    {
        out_data.SetTime(-1.0);
        out_data.SetFrameType(UMC::NONE_PICTURE);
        umcRes = m_pVideoRender->UnLockInputBuffer(&out_data, UMC::UMC_ERR_END_OF_STREAM);
        //m_bVideoPlaying = true;
    }

    m_pVideoRender->Stop();
    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::VideoProc start exiting\n"));


//    m_bVideoPlaying = false;
    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::VideoProc exit\n"));
    vm_debug_trace_withfunc(VM_DEBUG_FILE, VM_STRING("AVSync::VideoProc"), VM_STRING("VideoProc,-"));
}

#define LockBuffer(pFunction, pMedium, n ,bStop) \
{ \
    do \
    { \
        umcRes = pFunction(pMedium,n); \
        if ((UMC::UMC_ERR_NOT_ENOUGH_DATA == umcRes) || \
            (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)) \
            vm_time_sleep(5); \
    } while ((false == bStop) && \
             ((UMC::UMC_ERR_NOT_ENOUGH_DATA == umcRes) || \
              (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes))); \
}

template<class typeSource, class typeMedium>
UMC::Status LockInputBuffer(typeSource *pSource, typeMedium *pMedium, bool *pbStop)
{
    UMC::Status umcRes;

    do
    {
        umcRes = pSource->LockInputBuffer(pMedium);
        if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
            vm_time_sleep(DEF_TIME_TO_SLEEP);

    } while ((false == *pbStop) &&
             (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes));

    return umcRes;

} // UMC::Status LockInputBuffer(typeSource *pSource, typeMedium *pMedium, bool *pbStop)

template<class typeDestination, class typeMedium>
UMC::Status LockOutputBuffer(typeDestination *pDestination, typeMedium *pMedium, bool *pbStop)
{
    UMC::Status umcRes;

    do
    {
        umcRes = pDestination->LockOutputBuffer(pMedium);
        if (UMC::UMC_ERR_NOT_ENOUGH_DATA == umcRes)
            vm_time_sleep(DEF_TIME_TO_SLEEP);

    } while ((false == *pbStop) &&
             (UMC::UMC_ERR_NOT_ENOUGH_DATA == umcRes));

    return umcRes;

} // UMC::Status LockOutputBuffer(typeDestination *pDestination, typeMedium *pMedium, bool *pbStop)

void AVSync::AudioProc()
{
    //LOG  (VM_STRING("AudioProc,+"));
    UMC::Status umcRes = UMC::UMC_OK;
    UMC::MediaData ComprData;
    UMC::AudioData  UncomprData;
    bool bSplitterIsEmpty = false;
    Ipp64f dfStartTime = 0;
    Ipp32u uiComprSize = 0;
    Ipp32u uiShift = 0;

    // check error(s)
    VM_ASSERT(NULL != m_pSplitter);

    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::AudioProc start\n"));

    // Continue passing data from the splitter to decoder and from
    // decoder to the render
    UncomprData.m_info  = *m_pAudioInfo;

    while ((false == m_bStopFlag) &&
           (false == bSplitterIsEmpty))
    {
        // 1st step: obtain data from splitter
        LockBuffer(m_pSplitter->GetNextData, &ComprData,m_nAudioTrack, m_bStopFlag);
        vm_debug_trace1(VM_DEBUG_ALL, VM_STRING("AudioProc: data size from splitter is %f\n"),
                               ComprData.GetTime());
        // check error(s) & end of stream
        if (UMC::UMC_ERR_END_OF_STREAM == umcRes)
        {
            bSplitterIsEmpty = true;
            ComprData.SetDataSize(0);
        }

        else if (UMC::UMC_OK != umcRes)
            break;
        // save data size and data time
        uiComprSize = (Ipp32u) ComprData.GetDataSize();
        dfStartTime = ComprData.GetTime();

        // decode data and pass them to renderer
        uiShift = 0;
        do
        {
            // 2nd step: compressed data should be passed to the decoder first
            if (m_pDSAudioCodec)
            {
                UMC::MediaData buff;

                // get decoder's internal buffer
                umcRes = m_pDSAudioCodec->LockInputBuffer(&buff);
                // check error(s)
                if (UMC::UMC_OK != umcRes)
                    break;

                // Copy compressed data to the decoder's buffer
                if (UMC::UMC_OK == umcRes)
                {
                    Ipp32u uiDataToCopy = IPP_MIN((Ipp32u) buff.GetBufferSize(), uiComprSize);

                    memcpy(buff.GetDataPointer(),
                           (Ipp8u*)ComprData.GetDataPointer() + uiShift,
                           uiDataToCopy);

                    buff.SetDataSize(uiDataToCopy);
                    buff.SetTime(dfStartTime);

                    umcRes = m_pDSAudioCodec->UnLockInputBuffer(&buff,
                                                                (bSplitterIsEmpty) ? (UMC::UMC_ERR_END_OF_STREAM) : (UMC::UMC_OK));
                    // check error(s)
                    if (UMC::UMC_OK != umcRes)
                        break;


                    uiShift += uiDataToCopy;
                    uiComprSize -= uiDataToCopy;
                }
            }

            do
            {
                // wait until audio renderer will free enough internal buffers
                umcRes = LockInputBuffer(m_pAudioRender, &UncomprData, (bool *) &m_bStopFlag);
                // check error(s)
                if (UMC::UMC_OK != umcRes)
                {
                   if (!m_bStopFlag)
                      vm_string_printf(VM_STRING("Error in audio render\n"));

                    TASK_SWITCH();
                    break;
                }

                // move decoded data to the renderer

                // brunch for compressed data
                if (m_pDSAudioCodec)
                {
                    Ipp64f dfStart, dfEnd;

                    UncomprData.SetDataSize(0);
                    vm_tick ullDecTime = vm_time_get_tick();
                    umcRes = m_pDSAudioCodec->GetFrame(&UncomprData);
                    ullDecTime = vm_time_get_tick() - ullDecTime;
                    // check error(s)
                    if (UMC::UMC_OK != umcRes)
                        break;

                    VM_ASSERT(UMC::UMC_ERR_UNSUPPORTED != umcRes);
                    // calculate decoding time
                    m_Stat.dfAudioDecodeTime += (Ipp64f)(Ipp64s)(ullDecTime) /
                                                (Ipp64f)(Ipp64s)m_lliFreq;
                    UncomprData.GetTime(dfStart, dfEnd);
                    m_Stat.dfAudioPlayTime += dfEnd - dfStart;
                }
                // branch for PCM data
                else
                {
                    Ipp64f dfStart = 0.0;
                    Ipp64f dfEnd = 0.0;

                    if (0 == uiComprSize)
                        break;

                    Ipp32u uiDataToCopy = (Ipp32u) IPP_MIN(uiComprSize, UncomprData.GetBufferSize());

                    memcpy(UncomprData.GetDataPointer(),
                           ((Ipp8u*)ComprData.GetDataPointer()) + uiShift,
                           uiDataToCopy);
                    UncomprData.SetDataSize(uiDataToCopy);

                    ComprData.GetTime(dfStart, dfEnd);
                    Ipp64f dfNorm = (dfEnd - dfStart) / (uiShift + uiComprSize);
                    dfStart += dfNorm * uiShift;
                    dfEnd = dfStart + dfNorm * uiDataToCopy;
                    UncomprData.SetTime(dfStart, dfEnd);

                    uiShift += uiDataToCopy;
                    uiComprSize -= uiDataToCopy;
                    vm_debug_trace2(VM_DEBUG_INFO, VM_STRING("AudioProc: start: %lf, stop %lf\n"),
                                    dfStart, dfEnd);
                }

                // call finalizing function
                if (UncomprData.GetDataSize())
                {
                    Ipp64f dfEndTime;
                    vm_debug_trace1(VM_DEBUG_NONE, VM_STRING("taudio time: %f\n"), UncomprData.GetTime());
                    UncomprData.GetTime(dfStartTime, dfEndTime);
                    m_Stat.dfAudioDecodeRate = (Ipp64f)(m_Stat.dfAudioDecodeTime / m_Stat.dfAudioPlayTime);
                    dfStartTime = dfEndTime;

                    umcRes = m_pAudioRender->UnLockInputBuffer(&UncomprData);
                    // check error(s)
                    TASK_SWITCH();
                    if (UMC::UMC_OK != umcRes)
                        break;
                    // open SyncProc() only after render starts
                    if (-1. != m_pAudioRender->GetTime())
                        m_bAudioPlaying = true;
                }
            } while (false == m_bStopFlag);
            // check after-cicle error(s)
            if ((UMC::UMC_OK != umcRes) &&
                (UMC::UMC_ERR_NOT_ENOUGH_DATA != umcRes) &&
                (UMC::UMC_ERR_SYNC != umcRes))
                break;

            umcRes = UMC::UMC_OK;




        } while ((false == m_bStopFlag) &&
                 (0 != uiComprSize));
        // check after-cicle error(s)
        if (UMC::UMC_OK != umcRes)
            break;

    }

    // send end of stream to renderer
    if (false == m_bStopFlag)
    {
        // wait until audio renderer will free enough intermal buffers
        do
        {
            umcRes = m_pAudioRender->LockInputBuffer(&UncomprData);
            if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
                vm_time_sleep(DEF_TIME_TO_SLEEP);

        } while ((false == m_bStopFlag) &&
                 (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes));
        // check error(s)
        if (UMC::UMC_OK == umcRes)
        {
            UncomprData.SetDataSize(0);
            UncomprData.SetTime(0.0);
            umcRes = m_pAudioRender->UnLockInputBuffer(&UncomprData, UMC::UMC_ERR_END_OF_STREAM);
        }
    }

    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("AVSync::AudioProc exit\n"));
    //LOG  (VM_STRING("AudioProc,-"));
}

Ipp32u VM_THREAD_CALLCONVENTION AVSync::SyncThreadProc(void* pvParam)
{
    VM_ASSERT(NULL != pvParam);
    reinterpret_cast<AVSync*>(pvParam)->SyncProc();
    return 0;
}

Ipp32u VM_THREAD_CALLCONVENTION AVSync::VideoThreadProc(void* pvParam)
{
    VM_ASSERT(NULL != pvParam);
    reinterpret_cast<AVSync*>(pvParam)->VideoProc();
    return 0;
}

Ipp32u VM_THREAD_CALLCONVENTION AVSync::AudioThreadProc(void* pvParam)
{
    VM_ASSERT(NULL != pvParam);
    reinterpret_cast<AVSync*>(pvParam)->AudioProc();
    return 0;
}

void AVSync::ResizeDisplay(UMC::sRECT& rDispRect, UMC::sRECT& rRangeRect)
{
    if (NULL != m_pVideoRender)
    {
        if (2 >= rDispRect.bottom - rDispRect.top ||
            2 >= rDispRect.right - rDispRect.left)
        // I guess application was minimized
        {   m_pVideoRender->HideSurface();  }
        else
        {
            m_pVideoRender->ShowSurface();
            m_pVideoRender->ResizeDisplay(rDispRect, rRangeRect);
        }
    }
}

#ifndef UNREFERENCED_PARAM
#define UNREFERENCED_PARAM(param) \
    param = param;
#endif // UNREFERENCED_PARAM

UMC::Status AVSync::SetTrickModeSpeed(CommonCtl& rControlParams, Ipp32u trickFlag, Ipp64f offset)
{
    UMC::Status umcRes = UMC::UMC_OK;

    UNREFERENCED_PARAM(offset)
    UNREFERENCED_PARAM(trickFlag)

    UMC::VideoDecoderParams VideoDecoderInit;
    UMC::AudioRenderParams AudioRenderInit;
    AudioRenderInit.info = *m_pAudioInfo;

    if(rControlParams.uiTrickMode == UMC::UMC_TRICK_MODES_FFW_FAST)
    {
        AudioRenderInit.info.sample_frequency *=2;
    }
    else if(rControlParams.uiTrickMode == UMC::UMC_TRICK_MODES_FFW_FASTER)
    {
        AudioRenderInit.info.sample_frequency *=4;
    }
    else if(rControlParams.uiTrickMode == UMC::UMC_TRICK_MODES_SFW_SLOW)
    {
        AudioRenderInit.info.sample_frequency /=2;
    }
    else if(rControlParams.uiTrickMode == UMC::UMC_TRICK_MODES_SFW_SLOWER)
    {
        AudioRenderInit.info.sample_frequency /=4;
    }

    VideoDecoderInit.lTrickModesFlag = rControlParams.uiTrickMode;
    if(m_pVideoDecoder)
        m_pVideoDecoder->SetParams(&VideoDecoderInit);
    if(m_pAudioRender)
    {
        m_pAudioRender->SetParams(&AudioRenderInit, rControlParams.uiTrickMode);
        m_pAudioRender->Pause(false);
    }

    return umcRes;
}
