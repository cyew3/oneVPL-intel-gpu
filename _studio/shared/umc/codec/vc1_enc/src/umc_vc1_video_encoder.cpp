//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "vm_debug.h"
#include "vm_time.h"
#include "umc_par_file_util.h"
#include "umc_video_data.h"
#include "umc_vc1_video_encoder.h"
#include "umc_vc1_enc_sm.h"
#include "umc_vc1_enc_adv.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_statistic.h"

using namespace UMC;

VC1VideoEncoder::VC1VideoEncoder()
{
    m_pEncoderSM = 0;
    m_pEncoderADV = 0;
    m_bAdvance = false;
}


VC1VideoEncoder::~VC1VideoEncoder()
{
    Close();
}


UMC::Status VC1VideoEncoder::Reset()
{
    Status Sts = UMC_OK;

    if (m_pEncoderSM)
    {
        Sts = ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->Reset();
    }

    if (m_pEncoderADV)
    {
        Sts = ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->Reset();
    }

    return UMC_ERR_NOT_IMPLEMENTED;
}


UMC::Status VC1VideoEncoder::SetParams(BaseCodecParams* params)
{
    UMC::Status  err = UMC_OK;

    Init(params);

    return err;

}


UMC::Status VC1VideoEncoder::Init(BaseCodecParams* init)
{
    UMC::Status             err     =    UMC_OK;

    VC1EncoderParams   vc1Params;
    VC1EncoderParams*  params  =    DynamicCast<VC1EncoderParams> (init);

    if (!params)
    {
        VideoEncoderParams*  videoParams  =    DynamicCast<VideoEncoderParams> (init);
        if (!videoParams)
        {
            return UMC::UMC_ERR_NULL_PTR;
        }

        params = & vc1Params;
        params->info.clip_info.height      = videoParams->info.clip_info.height;
        params->info.clip_info.width       = videoParams->info.clip_info.width;
        params->info.framerate             = videoParams->info.framerate;
        params->info.bitrate               = videoParams->info.bitrate;
        params->numThreads                 = videoParams->numThreads;
        params->m_bInterlace               = false;
        params->profile                    = UMC_VC1_ENCODER::VC1_ENC_PROFILE_A;
        params->m_uiGOPLength              = 200;
        params->m_uiBFrmLength             = 0;
        params->m_bMixed                   = false;
        params->m_uiReferenceFieldType     = UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_BOTH;
        params->m_uiOverlapSmoothing       = 0;
        params->m_bIntensityCompensation   = false;
        params->m_bUseMeFeedback           = true;
        params->m_bVSTransform             = true;
        params->m_bUseUpdateMeFeedback     = false;
        params->m_bUseFastMeFeedback       = false;
        params->m_bSelectVLCTables         = true;
        params->m_uiMESearchSpeed          = 25;
        params->m_bIntensityCompensation   = false;
        params->m_bChangeInterpPixelType   = true;
        params->m_bUseTreillisQuantization = true;
        params->m_bNV12                    = false;
    }

    Close();

    //MEMORY ALLOCATOR
    if (UMC_OK != BaseCodec::Init(init) )
    {
        Close();
        return UMC_ERR_INIT;
    }

    m_bAdvance = (params->profile == UMC_VC1_ENCODER::VC1_ENC_PROFILE_A);


    if (!m_bAdvance)
    {
        m_pEncoderSM = new UMC_VC1_ENCODER::VC1EncoderSM;
        if (!m_pEncoderSM)
            return UMC_ERR_ALLOC;
        ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->SetMemoryAllocator(m_pMemoryAllocator, m_bOwnAllocator);
        err = ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->Init(params);
    }
    else
    {
        m_pEncoderADV = new UMC_VC1_ENCODER::VC1EncoderADV;
        if (!m_pEncoderADV)
            return UMC_ERR_ALLOC;
        ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->SetMemoryAllocator(m_pMemoryAllocator, m_bOwnAllocator);
        err = ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->Init(params);
    }

    if (err != UMC_OK)
    {
        Close();
        return err;
    }

#ifdef _PROJECT_STATISTICS_
    TimeStatisticsStructureInitialization();
    if(params->m_pStreamName)
        vm_string_strcpy(m_TStat->streamName,params->m_pStreamName);
    m_TStat->bitrate = params->info.bitrate;
    m_TStat->GOPLen = params->m_uiGOPLength;
    m_TStat->BLen   = params->m_uiBFrmLength;
    m_TStat->meSpeedSearch = params->m_uiMESearchSpeed;
#endif

#ifdef _VC1_IPP_STATISTICS_
IppStatisticsStructureInitialization();
    if(params->m_pStreamName)
        vm_string_strcpy(m_IppStat->streamName,params->m_pStreamName);
    m_IppStat->bitrate = params->info.bitrate;
    m_IppStat->GOPLen = params->m_uiGOPLength;
    m_IppStat->BLen   = params->m_uiBFrmLength;
    m_IppStat->meSpeedSearch = params->m_uiMESearchSpeed;
#endif
    return UMC_OK;
}


UMC::Status VC1VideoEncoder::GetInfo(BaseCodecParams* info)
{
    UMC::Status err = UMC_OK;

    VC1EncoderParams*  params  =    DynamicCast<VC1EncoderParams> (info);

    if(params)
    {
        if (m_pEncoderSM)
        {
            err = ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->GetInfo(params);
        }

        if (m_pEncoderADV)
        {
            err = ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->GetInfo(params);
        }
    }

    else
    {
        VC1EncoderParams   vc1Params;
        if (m_pEncoderSM)
        {
            err = ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->GetInfo(&vc1Params);
        }

        if (m_pEncoderADV)
        {
            err = ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->GetInfo(&vc1Params);
        }

        info->profile = vc1Params.profile;
        info->numThreads = vc1Params.numThreads;
        info->m_SuggestedOutputSize = vc1Params.m_SuggestedOutputSize;
        info->m_SuggestedInputSize  = vc1Params.m_SuggestedInputSize;
        info->m_pData = vc1Params.m_pData;
        info->lpMemoryAllocator = vc1Params.lpMemoryAllocator;
        info->level = vc1Params.level;
    }

    return err;
}


UMC::Status VC1VideoEncoder::Close()
{
    if (m_pEncoderSM)
    {
      ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->Close();
       delete ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM);
        m_pEncoderSM = 0;
    }

    if (m_pEncoderADV)
    {
      ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->Close();
       delete ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV);
       m_pEncoderADV = 0;
    }

    BaseCodec::Close(); // delete internal allocator if exists

#ifdef _PROJECT_STATISTICS_
    DeleteStatistics();
#endif

#ifdef _VC1_IPP_STATISTICS_
    DeleteIppStatistics();
#endif

    return UMC_OK;
}


UMC::Status VC1VideoEncoder::GetFrame(UMC::MediaData* pIn, UMC::MediaData* pOut)
{
    static int i = -1;
    i++;
    Status Sts = UMC_ERR_NOT_INITIALIZED;

STATISTICS_START_TIME(m_TStat->startTime);
IPP_STAT_START_TIME(m_IppStat->startTime);
    if (m_pEncoderSM)
        Sts = ((UMC_VC1_ENCODER::VC1EncoderSM*)m_pEncoderSM)->GetFrame(pIn,pOut);
    else if (m_pEncoderADV)
        Sts = ((UMC_VC1_ENCODER::VC1EncoderADV*)m_pEncoderADV)->GetFrame(pIn,pOut);
STATISTICS_END_TIME(m_TStat->startTime, m_TStat->endTime, m_TStat->totalTime);
IPP_STAT_END_TIME(m_IppStat->startTime, m_IppStat->endTime, m_IppStat->totalTime);

#ifdef _PROJECT_STATISTICS_
if(Sts == UMC_ERR_END_OF_STREAM)
    WriteStatisticResults();
else
    m_TStat->frameCount++;
#endif

#ifdef _VC1_IPP_STATISTICS_
if(Sts == UMC_ERR_END_OF_STREAM)
    WriteIppStatisticResults();
else
    m_IppStat->frameCount++;
#endif

   return Sts;
}
#define PAR_STRLEN 512
UMC::Status VC1EncoderParams::ReadParamFile(const vm_char *ParFileName)
{
  vm_file           *InputFile = 0;
  vm_char           line[PAR_STRLEN];
  vm_char           temp[PAR_STRLEN];
  vm_char*          param=0;
  vm_char*          end_of_line = 0;

  size_t            length = 0;

  const vm_char     comment = '#';

  if (0 == (InputFile = vm_file_open(ParFileName, VM_STRING("rt"))))
  {
    vm_debug_trace1(VM_DEBUG_ERROR, VM_STRING("Can't open parameter file %s\n"), ParFileName);
    return UMC_ERR_OPEN_FAILED;
  }

  while (umc_file_fgets_ascii(line,PAR_STRLEN,InputFile))
  {
      end_of_line   = vm_string_strchr(line,comment);
      length        = (end_of_line)? end_of_line - line: vm_string_strlen(line);
      length        = (length<PAR_STRLEN)? length:PAR_STRLEN-1;

      if (!length)
          continue;
      memset (temp,0,sizeof(vm_char)*PAR_STRLEN);
      vm_string_strncpy(temp, line,length);
      if (vm_string_strstr(temp,VM_STRING("Profile:")))
      {
          param = temp + vm_string_strlen(VM_STRING("Profile:"));
          if (vm_string_strstr(param,VM_STRING("Simple")))
          {
            profile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_S;
            continue;
          }
          else if (vm_string_strstr(param,VM_STRING("Main")))
          {
            profile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_M;
            continue;
          }
          else if (vm_string_strstr(param,VM_STRING("Advance")))
          {
            profile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_A;
            continue;
          }
          else
          {
              vm_file_close(InputFile);
              return UMC::UMC_ERR_INIT;
          }
      }//profile
      else if (vm_string_strstr(temp,VM_STRING("GOPLength:")))
      {
          param = temp + vm_string_strlen(VM_STRING("GOPLength:"));
          vm_string_sscanf(param, VM_STRING("%d"),&m_uiGOPLength);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("BFramesLength:")))
      {
          param = temp + vm_string_strlen(VM_STRING("BFramesLength:"));
          vm_string_sscanf(param, VM_STRING("%d"),&m_uiBFrmLength);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("Bitrate:")))
      {
          param = temp + vm_string_strlen(VM_STRING("Bitrate:"));
          vm_string_sscanf(param, VM_STRING("%d"),&info.bitrate);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("Width:")))
      {
          param = temp + vm_string_strlen(VM_STRING("Width:"));
          vm_string_sscanf(param, VM_STRING("%d"),&info.clip_info.width);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("Height:")))
      {
          param = temp + vm_string_strlen(VM_STRING("Height:"));
          vm_string_sscanf(param, VM_STRING("%d"),&info.clip_info.height);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("VSTtransform:")))
      {
          Ipp32s t;
          param = temp + vm_string_strlen(VM_STRING("VSTtransform:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bVSTransform = (t!=0);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("Deblocking:")))
      {
          Ipp32s t;
          param = temp + vm_string_strlen(VM_STRING("Deblocking:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bDeblocking = (t!=0);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("ConstQuantization:")))
      {
          Ipp32s t;
          param = temp + vm_string_strlen(VM_STRING("ConstQuantization:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_iConstQuant = (Ipp8u)t;
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("NumberOfFrames:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("NumberOfFrames:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiNumFrames = t;
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("MESearchSpeed:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("MESearchSpeed:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiMESearchSpeed = t;
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("HRDBufferSize:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("HRDBufferSize:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiHRDBufferSize = t;
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("HRDInitFullness:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("HRDInitFullness:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiHRDBufferInitFullness = t;
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("FrameRecoding:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("FrameRecoding:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bFrameRecoding = (t!=0);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("MixedMV:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("MixedMV:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bMixed = (t!=0);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("OriginalPredictedFrame:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("OriginalPredictedFrame:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bOrigFramePred = (t!=0);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("InterlaceField:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("InterlaceField:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bInterlace = (t!=0);
          continue;
      }
      else if (vm_string_strstr(temp,VM_STRING("RefField:")))
      {
          Ipp32u t;
          param = temp + vm_string_strlen(VM_STRING("RefField:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiReferenceFieldType = t;

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("SceneAnalyzer:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("SceneAnalyzer:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bSceneAnalyzer = (t!=0);

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("UseFeedback:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("UseFeedback:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bUseMeFeedback = (t!=0);

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("UpdateFeedback:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("UpdateFeedback:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bUseUpdateMeFeedback = (t!=0);

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("UseFastFeedback:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("UseFastFeedback:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bUseFastMeFeedback = (t!=0);

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("QuantType:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("QuantType:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiQuantType = t;

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("NumberOfSlices:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("NumberOfSlices:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiNumSlices = t;

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("SelectVLCTables:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("SelectVLCTables:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bSelectVLCTables = (t!=0);

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("ChangeInterpPixelType:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("ChangeInterpPixelType:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bChangeInterpPixelType = (t!=0);

          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("UseTreillisQuantization:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("UseTreillisQuantization:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bUseTreillisQuantization = (t!=0);
      }
      else if  (vm_string_strstr(temp,VM_STRING("OverlapSmoothing:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("OverlapSmoothing:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_uiOverlapSmoothing = t;
          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("IntensityCompensation:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("IntensityCompensation:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bIntensityCompensation = (t!=0);
          continue;
      }
      else if  (vm_string_strstr(temp,VM_STRING("NV12:")))
      {
          Ipp32u t = 0;
          param = temp + vm_string_strlen(VM_STRING("NV12:"));
          vm_string_sscanf(param, VM_STRING("%d"),&t);
          m_bNV12 = (t!=0);
          continue;
      }
  } // while
  vm_file_close(InputFile);
  return UMC::UMC_OK;
}
#undef PAR_STRLEN

VideoEncoder* CreateVC1Encoder()
{
  VideoEncoder* ptr = new VC1VideoEncoder;
  return ptr;
}

#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

