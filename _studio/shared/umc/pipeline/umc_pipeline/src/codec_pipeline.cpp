//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "codec_pipeline.h"
#include "vm_debug.h"
#include "umc_cyclic_buffer.h"
#include "umc_self_destruction_pointer.h"
#include "umc_readers.h"
#include "umc_audio_decoders.h"
#include "umc_audio_renders.h"
#include "umc_audio_codec.h"
#include "umc_splitters.h"
#define UMC_DECODER_CREATE_FUNCTION
#include "umc_video_decoders.h"
#include "umc_video_renders.h"
#include "umc_video_processing.h"
#include "directsound_render.h"

#include "umc_splitter_ex.h"

#undef USE_MPEG2SPLITTER

//#define VIDEO_CONF

using namespace UMC;

UMC::Status
CodecPipeline::SelectDataReader(UMC::ModuleContext& rContext,
                                UMC::DataReader*&   rpDataReader,
                                Ipp32u            ulPrefferedReader)
{
    UMC::Status umcRes = UMC::UMC_OK;

    delete rpDataReader;
    rpDataReader = NULL;

    if (DEF_DATA_READER == ulPrefferedReader)
    {   ulPrefferedReader = FILE_DATA_READER;   }

    UMC::RemoteReaderContext* pRemoteReaderContext = NULL;
    if (UMC::UMC_OK == umcRes && NULL == rpDataReader)
    {
        pRemoteReaderContext =
            DynamicCast<UMC::RemoteReaderContext,UMC::ModuleContext>(&rContext);
    }

#if defined(UMC_ENABLE_SOCKET_READER)
    if (UMC::UMC_OK == umcRes &&
        NULL        == rpDataReader &&
        SOCKET_DATA_READER == ulPrefferedReader)
    {
        UMC::SocketReader* pSocketReader = new UMC::SocketReader();

        if (NULL != pSocketReader)
        {
            UMC::SocketReaderParams SocketParams;
            SocketParams.port_number       = pRemoteReaderContext->m_uiPortNumber; //8200;
            SocketParams.transmission_mode = pRemoteReaderContext->m_transmissionMode; //UMC::MULTIPLE_CLIENTS;
            vm_string_strcpy(SocketParams.server_name, pRemoteReaderContext->m_szServerName);

            umcRes = pSocketReader->Init(&SocketParams);

            if (UMC::UMC_OK == umcRes)
            {   rpDataReader = pSocketReader;   }
            else
            {
                vm_debug_trace((UMC::UMC_OK != umcRes) ? VM_DEBUG_ALL : VM_DEBUG_NONE,
                             VM_STRING("Fail to reach server\n"));
                delete pSocketReader;
                return umcRes;
            }
        }
        else
        {   umcRes = UMC::UMC_ERR_ALLOC;    }
    }
#endif // defined(UMC_ENABLE_SOCKET_READER)

#if defined(UMC_ENABLE_HTTP_READER)
    if (UMC::UMC_OK      == umcRes &&
        NULL             == rpDataReader &&
        HTTP_DATA_READER == ulPrefferedReader)
    {
        UMC::HttpReader* pHttpReader = new UMC::HttpReader();

        if (NULL != pHttpReader)
        {
            UMC::HttpReaderParams HttpParams;
            vm_string_strcpy(HttpParams.uri,
                             pRemoteReaderContext->m_szServerName);
            HttpParams.portion_size = 10*1024;
            HttpParams.buffer_size  = 100*1024;

            umcRes = pHttpReader->Init(&HttpParams);

            if (UMC::UMC_OK == umcRes)
            {   rpDataReader = pHttpReader;   }
            else
            {
                vm_debug_trace((UMC::UMC_OK != umcRes) ? VM_DEBUG_ALL : VM_DEBUG_NONE,
                             VM_STRING("Fail to reach server\n"));
                delete pHttpReader;
                return umcRes;
            }
        }
        else
        {   umcRes = UMC::UMC_ERR_ALLOC;    }
    }
#endif // defined(UMC_ENABLE_HTTP_READER)

#if defined(UMC_ENABLE_MFC_HTTP_READER)
    if (UMC::UMC_OK      == umcRes &&
        NULL             == rpDataReader &&
        HTTP_DATA_READER == ulPrefferedReader)
    {
        UMC::MFCHttpReader* pHttpReader = new UMC::MFCHttpReader();

        if (NULL != pHttpReader)
        {
            UMC::MFCHttpReaderParams HttpParams;
            vm_string_strcpy(HttpParams.m_url,
                             pRemoteReaderContext->m_szServerName);
            HttpParams.m_portion_size = 10*1024;
            HttpParams.m_buffer_size  = 100*1024;

            umcRes = pHttpReader->Init(&HttpParams);

            if (UMC::UMC_OK == umcRes)
            {   rpDataReader = pHttpReader;   }
            else
            {
                vm_debug_trace((UMC::UMC_OK != umcRes) ? VM_DEBUG_ALL : VM_DEBUG_NONE,
                             VM_STRING("Fail to reach server\n"));
                delete pHttpReader;
                return umcRes;
            }
        }
        else
        {   umcRes = UMC::UMC_ERR_ALLOC;    }
    }
#endif // defined(UMC_ENABLE_HTTP_READER)

    UMC::LocalReaderContext* pLockalReaderContext = NULL;
    if (UMC::UMC_OK == umcRes && NULL == rpDataReader)
    {
        pLockalReaderContext =
            DynamicCast<UMC::LocalReaderContext,UMC::ModuleContext>(&rContext);

        if (NULL == pLockalReaderContext)
        { umcRes = UMC::UMC_ERR_INIT;    }
    }

#if defined(UMC_ENABLE_FILE_READER)
    if (UMC::UMC_OK       == umcRes &&
        NULL              == rpDataReader &&
        FILE_DATA_READER  == ulPrefferedReader)
    {
        UMC::FileReader* pFileReader = new UMC::FileReader();

        if (NULL != pFileReader)
        {
            UMC::FileReaderParams FileParams;
            FileParams.m_portion_size = 0;
            vm_string_strcpy(FileParams.m_file_name,
                             pLockalReaderContext->m_szFileName);

            umcRes = pFileReader->Init(&FileParams);

            if (UMC::UMC_OK == umcRes)
            {   rpDataReader = pFileReader; }
            else
            {
                vm_debug_message(VM_STRING("Can't open file: %s\n"),FileParams.m_file_name);
                delete pFileReader;
                return umcRes;
            }
        }
        else
        {   umcRes = UMC::UMC_ERR_ALLOC;    }
    }
#endif // defined(UMC_ENABLE_FILE_READER)

#if defined(UMC_ENABLE_FIO_READER)
    if (UMC::UMC_OK       == umcRes &&
        NULL              == rpDataReader &&
        FILE_DATA_READER  == ulPrefferedReader)
    {
        UMC::FIOReader* pFileReader = new UMC::FIOReader();

        if (NULL != pFileReader)
        {
            UMC::FileReaderParams FileParams;
            FileParams.m_portion_size = 0;
            vm_string_strcpy(FileParams.m_file_name,
                             pLockalReaderContext->m_szFileName);

            umcRes = pFileReader->Init(&FileParams);

            if (UMC::UMC_OK == umcRes)
            {   rpDataReader = pFileReader; }
            else
            {
                vm_debug_message(VM_STRING("Can't open file: %s\n"),FileParams.m_file_name);
                delete pFileReader;
                return umcRes;
            }
        }
        else
        {   umcRes = UMC::UMC_ERR_ALLOC;    }
    }
#endif // defined(UMC_ENABLE_FIO_READER)

    return umcRes;
}

Status
CodecPipeline::SelectAudioRender(AudioRenderParams& rInit,
                                 AudioRender*& rpRender,
                                 Ipp32u ulPrefferedRender)
{
    Status umcRes = UMC_OK;

    if (DEF_AUDIO_RENDER == ulPrefferedRender) {
#if defined(UMC_ENABLE_DSOUND_AUDIO_RENDER)
        ulPrefferedRender = DSOUND_AUDIO_RENDER;
#elif defined(UMC_ENABLE_OSS_AUDIO_RENDER)
        ulPrefferedRender = OSS_AUDIO_RENDER;
#elif defined(UMC_ENABLE_FW_AUDIO_RENDER)
        ulPrefferedRender = FW_AUDIO_RENDER;
#else
        ulPrefferedRender = NULL_AUDIO_RENDER;
#endif
    }

#if defined(UMC_ENABLE_DSOUND_AUDIO_RENDER)
    if (NULL == rpRender &&
        DSOUND_AUDIO_RENDER == ulPrefferedRender)
    {
        rpRender = new DSoundAudioRender;
        if (NULL == rpRender) { umcRes = UMC_ERR_ALLOC; }
        else {
            umcRes = rpRender->Init(&rInit);
            if (UMC_OK != umcRes) {
                delete rpRender;
                rpRender = NULL;
                umcRes = UMC_OK;
            }
        }
    }
#endif  // UMC_ENABLE_DSOUND_AUDIO_RENDER

#ifdef UMC_ENABLE_OSS_AUDIO_RENDER
    if (NULL == rpRender &&
        OSS_AUDIO_RENDER == ulPrefferedRender)
    {
        rpRender = new OSSAudioRender;
        if (NULL == rpRender)
        {   umcRes = UMC_ERR_ALLOC; }
        else {
            umcRes = rpRender->Init(&rInit);
            if (UMC_OK != umcRes) {
                delete rpRender;
                rpRender = NULL;
                umcRes = UMC_OK;
            }
        }
    }
#endif  // UMC_ENABLE_OSS_AUDIO_RENDER

#ifdef UMC_ENABLE_FW_AUDIO_RENDER
        if (NULL == rpRender &&
            FW_AUDIO_RENDER == ulPrefferedRender)
        {
            rpRender = new FWAudioRender;
            if (NULL == rpRender)
            {   umcRes = UMC_ERR_ALLOC; }
            else {
                umcRes = rpRender->Init(&rInit);
                if (UMC_OK != umcRes) {
                    delete rpRender;
                    rpRender = NULL;
                    umcRes = UMC_OK;
                }
            }
        }
#endif  // UMC_ENABLE_FW_AUDIO_RENDER

    if (NULL == rpRender)
    {
        ulPrefferedRender = NULL_AUDIO_RENDER;
        rpRender = new NULLAudioRender;
        if (NULL == rpRender)
        {   umcRes = UMC_ERR_ALLOC; }
        else {
            umcRes = rpRender->Init(&rInit);
            if (UMC_OK != umcRes) {
                delete rpRender;
                rpRender = NULL;
            }
        }
    }

    if (NULL != rpRender)
        vm_string_printf(VM_STRING("Audio Render :\t\t%s\n"), UMC::GetAudioRenderTypeString((UMC::AudioRenderType)ulPrefferedRender));

    return umcRes;
}

Status
CodecPipeline::SelectAudioRender(sAudioStreamInfo& rAudioInfo,
                                 ModuleContext& rContext,
                                 AudioRender*& rpAudioRender,
                                 Ipp32u ulPrefferedRender,
                                 vm_char* szOutputAudio)
{
    Status umcRes = UMC_OK;

    delete rpAudioRender;

    FWAudioRenderParams AudioRenderInit;
    AudioRenderInit.info = rAudioInfo;
    AudioRenderInit.pModuleContext = &rContext;
    AudioRenderInit.pOutFile = (vm_string_strlen(szOutputAudio))? szOutputAudio:NULL;

    if (UMC_OK == umcRes) {
        umcRes = SelectAudioRender(AudioRenderInit, rpAudioRender,ulPrefferedRender);
        if (UMC_OK != umcRes) {
            vm_debug_message(__VM_STRING("CodecPipeline::SelectAudioRender Failed to init"));
        }
    }

    return umcRes;
}

Status
CodecPipeline::SelectAudioDecoder(sAudioStreamInfo& rAudioInfo,
                                  BaseCodec*& rpAudioDecoder)
{
    Status umcRes = UMC_OK;

    delete rpAudioDecoder;

    if (UMC_OK == umcRes)
    {
        switch(rAudioInfo.stream_type)
        {
        case PCM_AUDIO:
        case LPCM_AUDIO:
            rpAudioDecoder = NULL;
            break;
#if defined(UMC_ENABLE_MP3_AUDIO_DECODER)
        case MP1L1_AUDIO:
        case MP1L2_AUDIO:
        case MP1L3_AUDIO:
        case MP2L1_AUDIO:
        case MP2L2_AUDIO:
        case MP2L3_AUDIO:
        case MPEG1_AUDIO:
        case MPEG2_AUDIO:
            rpAudioDecoder = DynamicCast<BaseCodec>(new MP3Decoder);
            if (NULL == rpAudioDecoder)
            { umcRes = UMC_ERR_ALLOC; }
            break;
#elif defined(UMC_ENABLE_MP3_INT_AUDIO_DECODER)
        case MP1L2_AUDIO:
        case MP1L3_AUDIO:
        case MP2L2_AUDIO:
        case MP2L3_AUDIO:
        case MPEG1_AUDIO:
        case MPEG2_AUDIO:
            rpAudioDecoder = DynamicCast<BaseCodec>(new MP3DecoderInt);
            if (NULL == rpAudioDecoder)
            { umcRes = UMC_ERR_ALLOC; }
            break;
#endif // defined(UMC_ENABLE_MP3_AUDIO_DECODER)

#if defined(UMC_ENABLE_AAC_AUDIO_DECODER)
        case AAC_AUDIO:
        case AAC_MPEG4_STREAM:
            rpAudioDecoder = DynamicCast<BaseCodec>(new AACDecoder);
            if (NULL == rpAudioDecoder)
            { umcRes = UMC_ERR_ALLOC; }
            break;
#elif defined(UMC_ENABLE_AAC_INT_AUDIO_DECODER)
        case AAC_AUDIO:
        case AAC_MPEG4_STREAM:
            rpAudioDecoder = DynamicCast<BaseCodec>(new AACDecoderInt);
            if (NULL == rpAudioDecoder)
            { umcRes = UMC_ERR_ALLOC; }
            break;
#endif // defined(UMC_ENABLE_AAC_INT_AUDIO_DECODER)

#if defined(UMC_ENABLE_AC3_AUDIO_DECODER)
        case AC3_AUDIO:
            rpAudioDecoder = DynamicCast<BaseCodec>(new AC3Decoder);
            if (NULL == rpAudioDecoder)
            { umcRes = UMC_ERR_ALLOC; }
            break;
#endif // defined(UMC_ENABLE_AC3_AUDIO_DECODER)

        case UNDEF_AUDIO:
        case TWINVQ_AUDIO:
        default:
            vm_debug_message(VM_STRING("Unsupported audio format!\n"));
            rpAudioDecoder = NULL;
            umcRes = UMC_ERR_INVALID_STREAM;
            break;
        }
    }

    if (UMC_OK != umcRes)
    {   vm_debug_trace(VM_DEBUG_INFO,VM_STRING("BaseCodec::SelectAudioDecoder failed!\n"));   }

    return umcRes;
}

Status
CodecPipeline::SelectDTAudioDecoder(sAudioStreamInfo& rAudioInfo,
                                    BaseCodec*& rpAudioDecoder,
                                    DualThreadedCodec*& rpDSAudioCodec,
                                    MediaBuffer*& rpMediaBuffer,
                                    MediaData* pDecSpecInfo)
{
    Status umcRes = UMC_OK;

    if (UMC_OK == umcRes)
    {   umcRes = SelectAudioDecoder(rAudioInfo, rpAudioDecoder);    }

    //  Init from splitter
    if (UMC_OK == umcRes &&
        UNDEF_AUDIO != rAudioInfo.stream_type &&
        NULL != rpAudioDecoder)
    {
        AudioCodecParams audioParams;
        MediaBufferParams CyclicBufferPrm;
        DualThreadCodecParams CodecPrm;

        if (NULL != rpDSAudioCodec) {   delete rpDSAudioCodec;  }
        if (NULL != rpMediaBuffer) {   delete rpMediaBuffer;    }

        rpDSAudioCodec = new DualThreadedCodec;
        if (NULL == rpAudioDecoder) {  umcRes = UMC_ERR_ALLOC; }

        if (UMC_OK == umcRes) {
            if (rAudioInfo.stream_type != UMC::AAC_MPEG4_STREAM)
                rpMediaBuffer = DynamicCast<MediaBuffer>(new LinearBuffer);
            else {
                // due to nature of AAC bitstream wrapped into MP4 files
                // sample buffer is required
                rpMediaBuffer = DynamicCast<MediaBuffer>(new SampleBuffer);
            }

            if (NULL == rpMediaBuffer) {  umcRes = UMC_ERR_ALLOC; }
        }

        if (UMC_OK == umcRes) {
            audioParams.m_info_in         = rAudioInfo;
            audioParams.m_info_out        = rAudioInfo;
            audioParams.m_pData           = pDecSpecInfo;

//            audioParams.m_info_in.channels = audioParams.m_info.channels;
//            audioParams.m_info_out.channels = audioParams.m_info.channels;
            CyclicBufferPrm.m_numberOfFrames   = 100;
            if(pDecSpecInfo)
            {
                Ipp32s size = (Ipp32s) pDecSpecInfo->GetDataSize();
                CyclicBufferPrm.m_prefInputBufferSize = (size < 188) ? 188: size;
            }
            else
                CyclicBufferPrm.m_prefInputBufferSize = 2304;

            CodecPrm.m_pCodec             = rpAudioDecoder;
            CodecPrm.m_pMediaBuffer       = rpMediaBuffer;
            CodecPrm.m_pCodecInitParams   = &audioParams;
            CodecPrm.m_pMediaBufferParams = &CyclicBufferPrm;

            umcRes = rpDSAudioCodec->InitCodec(&CodecPrm);
        }

        if (UMC_OK != umcRes) {
            if (NULL != rpAudioDecoder) {
                delete rpAudioDecoder;
                rpAudioDecoder = NULL;
            }
            if (NULL != rpDSAudioCodec) {
                delete rpDSAudioCodec;
                rpDSAudioCodec = NULL;
            }

            if (NULL != rpMediaBuffer) {
                delete rpMediaBuffer;
                rpMediaBuffer = NULL;
            }
        }
    }

    if (UMC_OK != umcRes)
    {   vm_debug_trace(VM_DEBUG_INFO,VM_STRING("DualThreadedCodec::SelectDualThreadedCodec failed!\n"));   }

    return umcRes;
}

Status CodecPipeline::SelectSplitter(SourceInfo *lpSourceInfo,
                                     Ipp32u uiSplitterFlags,
                                     Splitter*& rpSplitter,
                                     Ipp32u uiSelectedVideoPID,
                                     Ipp32u uiSelectedAudioPID)
{
    Status umcRes = UMC_OK;
    SourceInfoCam *lpCamInfo = NULL;
    SourceInfoNet *lpNetInfo = NULL;
    SourceInfoFile *lpFileInfo = NULL;
    SelfDestructionObject<SplitterParams> lpSplParams;

    // delete existing splitter
    if (rpSplitter)
        delete rpSplitter;
    rpSplitter = NULL;

    // try to create cam splitter
    if (NULL != (lpCamInfo = DynamicCast<SourceInfoCam> (lpSourceInfo)))
    {

#if defined(WIN32) && !defined(_WIN32_WCE) && defined (UMC_ENABLE_TRANSCODING)
        lpSplParams = new SplitterParams();
        if (NULL == lpSplParams)
            umcRes = UMC::UMC_ERR_FAILED;

        rpSplitter = new CaptureEx();
#endif // defined(WIN32) && !defined(_WIN32_WCE)

        if (NULL == rpSplitter)
            umcRes = UMC::UMC_ERR_FAILED;
    }
    // try to create net splitter
    else if (NULL != (lpNetInfo = DynamicCast<SourceInfoNet> (lpSourceInfo)))
    {
        umcRes = UMC::UMC_ERR_NOT_IMPLEMENTED;
    }
    // try to create file splitter
    else if (NULL != (lpFileInfo = DynamicCast<SourceInfoFile> (lpSourceInfo)))
    {
        return SelectSplitter(lpFileInfo->m_lpDataReader,
                              uiSplitterFlags,
                              rpSplitter,
                              uiSelectedVideoPID,
                              uiSelectedAudioPID);
    }
    // try to create unknown splitter
    else
    {
        umcRes = UMC::UMC_ERR_UNSUPPORTED;
    }

    // initialize created splitter
    if (UMC_OK == umcRes)
    {
        lpSplParams->m_lFlags = uiSplitterFlags;

        umcRes = rpSplitter->Init(*lpSplParams);
        if (UMC_OK != umcRes)
            vm_debug_message(__VM_STRING("Failed to initialize splitter "));
    }
    return umcRes;

} //Status CodecPipeline::SelectSplitter(SourceInfo *lpSourceInfo,

Status
CodecPipeline::SelectSplitter(DataReader* pDataReader,
                              Ipp32u uiSplitterFlags,
                              Splitter*& rpSplitter,
                              Ipp32u uiSelectedVideoPID,
                              Ipp32u uiSelectedAudioPID)
{
    Status umcRes = UMC_OK;
    SelfDestructionObject<SplitterParams> lpSplParams;

    if(rpSplitter)
      delete rpSplitter;
    rpSplitter = 0;

    lpSplParams = new SplitterParams();
    if (0 == lpSplParams)
    {  umcRes = UMC_ERR_ALLOC;}


    if (UMC_OK == umcRes) {
        switch(Splitter::GetStreamType(pDataReader))
        {
#if defined(UMC_ENABLE_MPEG2_SPLITTER)
        case MPEGx_SYSTEM_STREAM:
        rpSplitter = new ThreadedDemuxer();
        break;
#endif // UMC_ENABLE_MPEG2_SPLITTER

#if defined(UMC_ENABLE_MP4_SPLITTER)
        case MP4_ATOM_STREAM:
            rpSplitter = new MP4Splitter();
            break;
#endif // defined(UMC_ENABLE_ATOM_SPLITTER)

#if defined(UMC_ENABLE_AVI_SPLITTER)
        case AVI_STREAM:
            rpSplitter  = (Splitter*)new AVISplitter();
            break;
#endif // defined(UMC_ENABLE_AVI_SPLITTER)

#if defined(UMC_ENABLE_VC1_SPLITTER)
        case VC1_PURE_VIDEO_STREAM:
            rpSplitter  = (Splitter*)new VC1Splitter();
            break;
#endif // defined(UMC_ENABLE_VC1_SPLITTER)

#if defined(UMC_ENABLE_AVS_SPLITTER)
        case AVS_PURE_VIDEO_STREAM:
            rpSplitter  = (Splitter*)new AVSSplitter();
            break;
#endif // defined(UMC_ENABLE_AVS_SPLITTER)

        default:
            umcRes = UMC_ERR_UNSUPPORTED;

        }
    }

    if (UMC_OK == umcRes && NULL == rpSplitter)
    {   umcRes = UMC_ERR_ALLOC; }

    if (UMC_OK == umcRes) {
        lpSplParams->m_lFlags       = uiSplitterFlags;
        lpSplParams->m_pDataReader  = pDataReader;
        lpSplParams->m_uiSelectedAudioPID = uiSelectedAudioPID;
        lpSplParams->m_uiSelectedVideoPID = uiSelectedVideoPID;

        umcRes = rpSplitter->Init(*lpSplParams);
        if ((UMC_OK == umcRes) || (UMC_WRN_INVALID_STREAM == umcRes))
            umcRes = rpSplitter->Run();
        if (UMC_OK != umcRes) {
            vm_debug_message(VM_STRING("Failed to initialize splitter\n"));
        }
    }
    return umcRes;
}

Status
CodecPipeline::SelectVideoDecoder(sVideoStreamInfo& rVideoInfo,
                                  MediaData*        pDecSpecInfo,
                                  Ipp32u            lInterpolation,
                                  Ipp32u            lDeinterlacing,
                                  Ipp32u            numThreads,
                                  Ipp32u            ulVideoDecoderFlags,
                                  BaseCodec&        rColorConverter,
                                  VideoDecoder*&    rpVideoDecoder)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams VDecParams;

    if (rpVideoDecoder) {
      delete rpVideoDecoder;
      rpVideoDecoder = NULL;
    }

    rpVideoDecoder = CreateVideoDecoder(rVideoInfo.stream_type);
    if (!rpVideoDecoder) {
      return UMC_ERR_UNSUPPORTED;
    }

    VDecParams.info.stream_type = rVideoInfo.stream_type;
    VDecParams.info.stream_subtype = rVideoInfo.stream_subtype;
    if (pDecSpecInfo && pDecSpecInfo->GetDataSize()) {
      VDecParams.m_pData = pDecSpecInfo;
    }

    if (UMC_OK == umcRes) {
        VideoProcessingParams postProcessingParams;
        postProcessingParams.m_DeinterlacingMethod = (DeinterlacingMethod)lDeinterlacing;
        postProcessingParams.InterpolationMethod = lInterpolation;
        rColorConverter.SetParams(&postProcessingParams);

        VDecParams.info                 = rVideoInfo;
        VDecParams.lFlags               = ulVideoDecoderFlags;//FLAG_VDEC_CONVERT | FLAG_VDEC_REORDER;
        VDecParams.numThreads           = numThreads;
        VDecParams.pPostProcessing      = &rColorConverter;

        umcRes = rpVideoDecoder->Init(&VDecParams);
    }

    if (UMC_OK != umcRes)
    {   vm_debug_message(VM_STRING("VideoDecoder::SelectVideoDecoder Failed\n"));   }

    return umcRes;
}

Status
CodecPipeline::SelectVideoRender(ModuleContext& rContext,
                                 sClipInfo ClipInfo,
                                 UMC::sRECT rectDisp,
                                 UMC::sRECT rectRange,
                                 ColorFormat RenderCFormat,
                                 Ipp32u ulVideoRenderFlags,
                                 VideoRender*& rpVideoRender,
                                 Ipp32u ulPrefferedRender, //= DEF_VIDEO_RENDER
                                 vm_char* szOutputVideo)
{
    Status umcRes = UMC_OK;

    delete rpVideoRender;

    if (UMC_OK == umcRes) {
        //  Init Render
        FWVideoRenderParams Params;
        Params.out_data_template.Init(ClipInfo.width, ClipInfo.height, RenderCFormat);
        Params.disp         = rectDisp;
        Params.range        = rectRange;
        Params.pOutFile     = (vm_string_strlen(szOutputVideo))?szOutputVideo:NULL;

        Params.lFlags = ulVideoRenderFlags;

        umcRes = SelectVideoRender(Params,
                                   rContext,
                                   rpVideoRender,
                                   ulPrefferedRender);
        Params.out_data_template.Close();
    }
    return umcRes;
}

void SetNextPreferredVideoRender(Ipp32u& ulPrefferedRender)
{
    ulPrefferedRender = DEF_VIDEO_RENDER;

}

Status
CodecPipeline::SelectVideoRender(VideoRenderParams& rVideoRenderParams,
                                 ModuleContext& rContext,
                                 VideoRender*& rpRender,
                                 Ipp32u ulPrefferedRender) //= DEF_VIDEO_RENDER
{
    Status umcRes = UMC_OK;
    rpRender = NULL;

    if (DEF_VIDEO_RENDER == ulPrefferedRender)
    {
#if defined(UMC_ENABLE_SDL_VIDEO_RENDER)
        ulPrefferedRender = SDL_VIDEO_RENDER;
#elif defined(UMC_ENABLE_BLT_VIDEO_RENDER)
        ulPrefferedRender = BLT_VIDEO_RENDER;
#elif defined(UMC_ENABLE_DX_VIDEO_RENDER)
        ulPrefferedRender = DX_VIDEO_RENDER;
#else
        ulPrefferedRender = NULL_VIDEO_RENDER; //  Unknown architecture
#endif
    }

#if defined(UMC_ENABLE_SDL_VIDEO_RENDER)
    if (NULL == rpRender &&
        SDL_VIDEO_RENDER == ulPrefferedRender)
    {
        SDLVideoRender* pSDLRender = new SDLVideoRender;
        if (NULL != pSDLRender)
        {
            umcRes = pSDLRender->Init(rVideoRenderParams);
            if (UMC_OK == umcRes)
            {   rpRender = pSDLRender;  }
            else {
                delete pSDLRender;
                SetNextPreferredVideoRender(ulPrefferedRender);
            }
        }
    }
#endif  //  defined(SDL_ON)

#if defined(WIN32)
    HWNDModuleContext* pHWNDContext = NULL;
    if (NULL == rpRender) {
        pHWNDContext =
            DynamicCast<HWNDModuleContext,ModuleContext>(&rContext);
        if (NULL == pHWNDContext) { umcRes = UMC_ERR_INIT;    }
    }
#endif // defined(WIN32)

#if defined(UMC_ENABLE_DX_VIDEO_RENDER)
    if (NULL == rpRender &&
        DX_VIDEO_RENDER == ulPrefferedRender)
    {
        DXVideoRender* pDXRender = new DXVideoRender;
        if (NULL != pDXRender) {
            DXVideoRenderParams Init;
            static_cast<VideoRenderParams&>(Init) = rVideoRenderParams;
            Init.m_hWnd = pHWNDContext->m_hWnd;
            Init.m_ColorKey = pHWNDContext->m_ColorKey;
            umcRes = pDXRender->Init(Init);
            if (UMC_OK == umcRes)
            {   rpRender = pDXRender;   }
            else {
                delete pDXRender;
                SetNextPreferredVideoRender(ulPrefferedRender);
            }
        }
    }
#endif // defined(UMC_ENABLE_DX_VIDEO_RENDER)


#if defined(UMC_ENABLE_BLT_VIDEO_RENDER)
    if (NULL == rpRender &&
        BLT_VIDEO_RENDER == ulPrefferedRender)
    {
        BLTVideoRender* pBLTRender = new BLTVideoRender;
        if (NULL != pBLTRender) {
            DXVideoRenderParams Init;
            static_cast<VideoRenderParams&>(Init) = rVideoRenderParams;
            Init.m_hWnd = pHWNDContext->m_hWnd;
            Init.m_ColorKey = pHWNDContext->m_ColorKey;
            umcRes = pBLTRender->Init(Init);
            if (UMC_OK == umcRes)
            {   rpRender = pBLTRender;  }
            else {
                delete pBLTRender;
                SetNextPreferredVideoRender(ulPrefferedRender);
            }
        }
    }
#endif // defined(UMC_ENABLE_BLT_VIDEO_RENDER)


    if (NULL == rpRender &&
        NULL_VIDEO_RENDER == ulPrefferedRender)
    {
        NULLVideoRender* pNULLRender = new NULLVideoRender;
        if (NULL != pNULLRender) {
            umcRes = pNULLRender->Init(rVideoRenderParams);
            if (UMC_OK == umcRes)
            {   rpRender = pNULLRender; }
            else {
                delete pNULLRender;
                umcRes = UMC_ERR_INIT;
            }
        }
    }
#if defined (UMC_ENABLE_FILE_WRITER)
    if (NULL == rpRender &&
        FW_VIDEO_RENDER == ulPrefferedRender)
    {
        FWVideoRender* pFWRender = new FWVideoRender;
        if (NULL != pFWRender) {
            umcRes = pFWRender->Init(&rVideoRenderParams);
            if (UMC_OK == umcRes)
            {   rpRender = pFWRender;   }
            else {
                delete pFWRender;
                umcRes = UMC_ERR_INIT;
            }
        }
    }
#endif //UMC_ENABLE_FILE_WRITER
    if (NULL == rpRender)
    {
        ulPrefferedRender = NULL_VIDEO_RENDER;
        NULLVideoRender* pNULLRender = new NULLVideoRender;
        if (NULL != pNULLRender) {
            umcRes = pNULLRender->Init(rVideoRenderParams);
            if (UMC_OK == umcRes)
            {   rpRender = pNULLRender; }
            else {
                delete pNULLRender;
                umcRes = UMC_ERR_INIT;
            }
        }
    }

    if (NULL != rpRender)
        vm_string_printf(VM_STRING("Video Render :\t\t%s\n"), UMC::GetVideoRenderTypeString((UMC::VideoRenderType)ulPrefferedRender));
      vm_string_printf(VM_STRING("-RenderFormat:\t\t%s\n"), UMC::GetFormatTypeString(rVideoRenderParams.out_data_template.GetColorFormat()));
      return umcRes;
}
