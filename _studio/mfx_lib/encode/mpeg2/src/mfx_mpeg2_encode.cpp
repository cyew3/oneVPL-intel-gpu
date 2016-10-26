//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include "mfx_enc_common.h"
#include "mfx_mpeg2_encode.h"
#include "umc_mpeg2_video_encoder.h"
#include "umc_mpeg2_enc.h"
#include "umc_mpeg2_enc_defs.h"
#include "mfx_brc_common.h"
#include "umc_video_brc.h"
#include "mfx_tools.h"
#include "vm_sys_info.h"
#include "ipps.h"
#include "vm_time.h"
#include "mfx_task.h"
#include "umc_mpeg2_brc.h"
#include <stdio.h>

// temporary defines
#define RET_UMC_TO_MFX(umc_ret) ConvertStatusUmc2Mfx(umc_ret)

#define CHECK_VERSION(ver) /*ver.Major==0;*/
#define CHECK_CODEC_ID(id, myid) /*id==myid;*/

#define SP   5
#define MP   4
#define SNR  3
#define SPAT 2
#define HP   1

#define LL  10
#define ML   8
#define H14  6
#define HL   4

static mfxStatus ApplyTargetUsage (mfxVideoParam* par, UMC::MPEG2EncoderParams* umcpar)
{
  const mfxU16 MAXQ = 256;
  static Ipp32s MAX_RANGE_X [4]= {512, 1024, 2048, 2048};
  static Ipp32s MAX_RANGE_Y [4]= {64,  128,  128,  128};

  int t =    (umcpar->profile == SP)?  0 : 
            ((umcpar->profile == MP)?  1:
            ((umcpar->profile == H14)? 2: 3));

  mfxI32 qual = (par->mfx.TargetUsage - MFX_TARGETUSAGE_BEST_SPEED) * MAXQ / (MFX_TARGETUSAGE_BEST_QUALITY-MFX_TARGETUSAGE_BEST_SPEED);
  if  (qual<0 || qual>MAXQ) qual = MAXQ/2;

  umcpar->IPDistance = (umcpar->profile == SP)? 1 : par->mfx.GopRefDist;
  umcpar->gopSize = par->mfx.GopPicSize;

  mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);

  if (ext && ext->MVSearchWindow.x && ext->MVSearchWindow.y)
  {
    umcpar->rangeP[0]    = ext->MVSearchWindow.x;
    umcpar->rangeP[1]    = ext->MVSearchWindow.y;
    umcpar->rangeB[0][0] = ext->MVSearchWindow.x;
    umcpar->rangeB[0][1] = ext->MVSearchWindow.y;
    umcpar->rangeB[1][0] = ext->MVSearchWindow.x;
    umcpar->rangeB[1][1] = ext->MVSearchWindow.y;
  }
  else
  {
    umcpar->rangeP[0] = 1 + 48 * qual/MAXQ;
    umcpar->rangeP[1] = 1 + 40 * qual/MAXQ;
    umcpar->rangeB[0][0] = 1 + 14 * qual/MAXQ;
    umcpar->rangeB[0][1] = 1 + 9 * qual/MAXQ;
    umcpar->rangeB[1][0] = 1 + 31 * qual/MAXQ;
    umcpar->rangeB[1][1] = 1 + 22 * qual/MAXQ;
  }

  umcpar->rangeP[0]    = umcpar->rangeP[0]    < MAX_RANGE_X [t] ? umcpar->rangeP[0]   : MAX_RANGE_X [t];
  umcpar->rangeB[0][0] = umcpar->rangeB[0][0] < MAX_RANGE_X [t] ? umcpar->rangeB[0][0]: MAX_RANGE_X [t];
  umcpar->rangeB[1][0] = umcpar->rangeB[1][0] < MAX_RANGE_X [t] ? umcpar->rangeB[1][0]: MAX_RANGE_X [t];

  umcpar->rangeP[1]    = umcpar->rangeP[1]    < MAX_RANGE_Y [t] ? umcpar->rangeP[1]   : MAX_RANGE_Y [t];
  umcpar->rangeB[0][1] = umcpar->rangeB[0][1] < MAX_RANGE_Y [t] ? umcpar->rangeB[0][1]: MAX_RANGE_Y [t];
  umcpar->rangeB[1][1] = umcpar->rangeB[1][1] < MAX_RANGE_Y [t] ? umcpar->rangeB[1][1]: MAX_RANGE_Y [t];

  if (umcpar->IPDistance == 0  && umcpar->gopSize == 0)
  {
      if (qual > (MAXQ/4)*3)
      {
        // long GOP (P->I recoding are possible)
        umcpar->gopSize = 120;
        umcpar->IPDistance = 3;
      }
      else if (qual > MAXQ/4)
      {
        // short GOP
        umcpar->gopSize = 12;
        umcpar->IPDistance = 3;
      }
      else
      {
        // short GOP
        umcpar->gopSize = 12;
        umcpar->IPDistance = 1;
      }
  }
  else if (umcpar->IPDistance != 0  && umcpar->gopSize == 0)
  {
    if (qual > (MAXQ/4)*3)
      {
        // long GOP (P->I recoding are possible)
        umcpar->gopSize = umcpar->IPDistance*40;
      }
      else
      {
        // short GOP
        umcpar->gopSize = umcpar->IPDistance*4;
      }
  }
  else if (umcpar->IPDistance == 0  && umcpar->gopSize != 0)
  {
      if (qual <= MAXQ/4)
      {
        umcpar->IPDistance = 1;
      }
      else
      {
        umcpar->IPDistance = (umcpar->gopSize > 3)? 3:umcpar->gopSize;
      }
  }
  else
  {
      umcpar->IPDistance = (umcpar->gopSize >= umcpar->IPDistance)? umcpar->IPDistance : umcpar->gopSize;
  }
  umcpar->allow_prediction16x8 = (qual > MAXQ/2) ? 1 : 0;
  umcpar->frame_pred_frame_dct[2] = umcpar->frame_pred_frame_dct[1] = umcpar->frame_pred_frame_dct[0] =  (qual < MAXQ/4) ? 1 : 0;
  if(par->mfx.TargetUsage == 1)
    umcpar->enable_Dual_prime = true;
  return MFX_ERR_NONE;
}

static mfxStatus CheckExtendedBuffers (mfxVideoParam* par)
{
#define NUM_SUPPORTED_BUFFERS 4

    mfxU32 supported_buffers[NUM_SUPPORTED_BUFFERS] = {
        MFX_EXTBUFF_CODING_OPTION,
        MFX_EXTBUFF_CODING_OPTION_SPSPPS,
        MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION,
        MFX_EXTBUFF_VIDEO_SIGNAL_INFO
    };
    mfxU32 num_supported = 0;

    if (par->NumExtParam == 0 || par->ExtParam == 0)
    {
        return MFX_ERR_NONE;
    }
    for (mfxU32 n_buf=0; n_buf < NUM_SUPPORTED_BUFFERS; n_buf++)
    {
        mfxU32 num = 0;
        for (mfxU32 i=0; i < par->NumExtParam; i++)
        {
            if (par->ExtParam[i] == NULL)
            {
               return MFX_ERR_NULL_PTR;
            }
            if (par->ExtParam[i]->BufferId == supported_buffers[n_buf])
            {
                num ++;
            }
        }
        if (num > 1)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        num_supported += num;

    }
    return (num_supported == par->NumExtParam) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;

#undef NUM_SUPPORTED_BUFFERS
}

MFXVideoENCODEMPEG2::MFXVideoENCODEMPEG2(VideoCORE *core, mfxStatus *sts)
: VideoENCODE(),
  m_InputSurfaces(core),
  m_request(),
  m_response()
{
  m_core = core;

  m_pFramesStore = 0;
  m_nFrames = 0;

  m_brc = 0;
  m_pGOP = 0;
  m_pWaitingList = 0;

  memset(&frames, 0, sizeof(frames));
  m_bInitialized                = false;
  m_GopOptFlag                  = 0;
  m_InputFrameOrder             = -1;
  m_OutputFrameOrder            = -1;
  m_BitstreamLen                = 0;
  m_IdrInterval                 = 0;
  m_bAddEOS                     = false;
  m_picStruct                   = 0;
  m_BufferSizeInKB              = 0;
  m_AspectRatioW                = 0;
  m_AspectRatioH                = 0;
  m_Width                       = 0;
  m_Height                      = 0;
  m_nEncodeCalls                = 0;
  m_nFrameInGOP                 = 0;
  m_IOPattern                   = 0;
  m_TargetUsage                 = 0;
  m_bConstantQuant              = 0;
  pSHEx                         = 0;
  m_nThreads = 0;
  m_bAddDisplayExt = 0;
#ifdef _NEW_THREADING

  m_pExtTasks              = 0;

#endif


  *sts = (core ? MFX_ERR_NONE : MFX_ERR_NULL_PTR);
}



mfxStatus MFXVideoENCODEMPEG2::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);
    if(in==0)
    {
        memset(&out->mfx, 0, sizeof(mfxInfoMFX));
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;
        out->mfx.FrameInfo.CropX = 0;
        out->mfx.FrameInfo.CropY = 0;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
        out->mfx.RateControlMethod = 1; // not sure, it is BRC
        out->mfx.InitialDelayInKB = 1; // not sure, it is BRC
        out->mfx.BufferSizeInKB = 1; // not sure, it is BRC
        out->mfx.TargetKbps = 1;
        out->mfx.MaxKbps = 1; // not sure, it is BRC
        out->mfx.NumSlice = 1;
        out->mfx.NumThread = 1;
        out->mfx.TargetUsage = 1;
        out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        out->AsyncDepth = 0;
        MFX_CHECK_STS (CheckExtendedBuffers(out));


        mfxExtCodingOption* ext_out = GetExtCodingOptions(out->ExtParam,out->NumExtParam);
        if (ext_out)
        {
            mfxU32 bufOffset = sizeof(mfxExtBuffer);
            mfxU32 bufSize   = sizeof(mfxExtCodingOption) - bufOffset;

            memset ((mfxU8*)(ext_out) + bufOffset,0, bufSize);

            ext_out->EndOfSequence     = 1;
            ext_out->FramePicture      = 1;    
        }
        mfxExtCodingOptionSPSPPS* pSPSPPS_out = SHParametersEx::GetExtCodingOptionsSPSPPS (out->ExtParam, out->NumExtParam);
        if (pSPSPPS_out)
        {
            pSPSPPS_out->PPSBuffer  = NULL;
            pSPSPPS_out->PPSBufSize = 0;
            pSPSPPS_out->SPSBuffer  = (mfxU8*)1;
            pSPSPPS_out->SPSBufSize =  1;
        }

        mfxExtVideoSignalInfo* pVideoSignal_out = GetExtVideoSignalInfo(out->ExtParam, out->NumExtParam);
        if (pVideoSignal_out)
        {
            pVideoSignal_out->VideoFormat              = 1;
            pVideoSignal_out->ColourDescriptionPresent = 1;
            pVideoSignal_out->ColourPrimaries          = 1;
            pVideoSignal_out->TransferCharacteristics  = 1;
            pVideoSignal_out->MatrixCoefficients       = 1;
            pVideoSignal_out->VideoFullRange           = 0;
        }
    }
    else
    {
        bool bUnsupported = false;
        bool bWarning = false;

        out->mfx = in->mfx;
        out->IOPattern = in->IOPattern;
        out->Protected = in->Protected;
        out->AsyncDepth = in->AsyncDepth;

        MFX_CHECK_STS (CheckExtendedBuffers(in));
        MFX_CHECK_STS (CheckExtendedBuffers(out));

        mfxExtCodingOptionSPSPPS* pSPSPPS_out = SHParametersEx::GetExtCodingOptionsSPSPPS (out->ExtParam, out->NumExtParam);
        mfxExtCodingOptionSPSPPS* pSPSPPS_in  = SHParametersEx::GetExtCodingOptionsSPSPPS (in->ExtParam,  in->NumExtParam);

        if (pSPSPPS_out && pSPSPPS_in)
        {
            if (pSPSPPS_in->SPSBuffer && pSPSPPS_out->SPSBuffer && pSPSPPS_out->SPSBufSize && pSPSPPS_in->SPSBufSize)
            {
                mfxU32 real_size = 0;
                if (SHParametersEx::CheckSHParameters(pSPSPPS_in->SPSBuffer, pSPSPPS_in->SPSBufSize, real_size, out, 0))
                {
                    if (real_size <= pSPSPPS_out->SPSBufSize)
                    {
                        memcpy_s(pSPSPPS_out->SPSBuffer, real_size * sizeof(mfxU8), pSPSPPS_in->SPSBuffer, real_size * sizeof(mfxU8));  
                        memset(pSPSPPS_out->SPSBuffer + real_size, 0, pSPSPPS_out->SPSBufSize - real_size);
                    }
                    else
                    {
                        memset(pSPSPPS_out->SPSBuffer, 0, pSPSPPS_out->SPSBufSize);
                        bUnsupported   = true;
                    }

                }
                else
                {
                    memset(pSPSPPS_out->SPSBuffer, 0, pSPSPPS_out->SPSBufSize);
                    bUnsupported   = true;
                }        
            }
            else if (pSPSPPS_in->SPSBuffer || pSPSPPS_out->SPSBuffer || pSPSPPS_out->SPSBufSize || pSPSPPS_in->SPSBufSize)
            {
                bUnsupported   = true;                

            }
        }
        else if (!(pSPSPPS_in == 0 && pSPSPPS_out ==0))
        {
            bUnsupported   = true;  
        }

        if (out->Protected)
        {
            out->Protected = 0;
            bUnsupported   = true;
        }
        if (out->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 && out->mfx.FrameInfo.FourCC !=0)
        {
            out->mfx.FrameInfo.FourCC = 0;
            bUnsupported   = true;
        }

        mfxU16 ps = out->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE|MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF);

        if (ps != MFX_PICSTRUCT_PROGRESSIVE &&
            ps != MFX_PICSTRUCT_FIELD_TFF &&
            ps != MFX_PICSTRUCT_FIELD_BFF &&
            ps != MFX_PICSTRUCT_UNKNOWN)
        {
            ps = MFX_PICSTRUCT_UNKNOWN;
        }
        if (out->mfx.FrameInfo.PicStruct != ps)
        {
            out->mfx.FrameInfo.PicStruct = ps;
            bWarning = true;
        }

        mfxU16 t = (out->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0x0f:0x1f;

        if (out->mfx.FrameInfo.Width > 0x1fff ||(out->mfx.FrameInfo.Width & 0x0f))
        {
            out->mfx.FrameInfo.Width = 0;
            bUnsupported = true;
        }
        if (out->mfx.FrameInfo.Height > 0x1fff ||(out->mfx.FrameInfo.Height & t))
        {
            out->mfx.FrameInfo.Height = 0;
            bUnsupported = true;
        }


        mfxU32 temp = out->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY);

        if ((temp==0 && out->IOPattern)|| temp==(MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY))
        {
            out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            bWarning = true;
        }
        mfxExtCodingOption* ext_in  = GetExtCodingOptions(in->ExtParam, in->NumExtParam);
        mfxExtCodingOption* ext_out = GetExtCodingOptions(out->ExtParam,out->NumExtParam);

        MFX_CHECK ((ext_in !=0 && ext_out != 0) || (ext_in == 0 && ext_out == 0),  MFX_ERR_UNDEFINED_BEHAVIOR );

        if (ext_in && ext_out)
        {
                mfxExtCodingOption temp = {0};

                mfxU32 bufOffset = sizeof(mfxExtBuffer);
                mfxU32 bufSize   = sizeof(mfxExtCodingOption) - bufOffset;

                memcpy_s(&temp, sizeof(mfxExtCodingOption), ext_in, sizeof(mfxExtCodingOption));

                memset ((mfxU8*)(ext_out) + bufOffset,0, bufSize);

                ext_out->EndOfSequence     = temp.EndOfSequence;
                ext_out->FramePicture      = temp.FramePicture;  

                bWarning = bWarning || (memcmp((mfxU8*)(ext_out) + bufOffset,(mfxU8*)(&temp) + bufOffset, bufSize)!= 0);
                bUnsupported = bUnsupported || (temp.FieldOutput == MFX_CODINGOPTION_ON);
        }

        mfxExtVideoSignalInfo* pVideoSignal_out = GetExtVideoSignalInfo(out->ExtParam, out->NumExtParam);
        mfxExtVideoSignalInfo* pVideoSignal_in  = GetExtVideoSignalInfo(in->ExtParam,  in->NumExtParam);

        MFX_CHECK ((pVideoSignal_in == 0) == (pVideoSignal_out == 0),  MFX_ERR_UNDEFINED_BEHAVIOR );

        if (pVideoSignal_in && pVideoSignal_out)
        {
            *pVideoSignal_out = *pVideoSignal_in;

            if (CheckExtVideoSignalInfo(pVideoSignal_out) == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
            {
                bWarning = true;
            }
        }

        if ((out->mfx.FrameInfo.Width!= 0 && out->mfx.FrameInfo.CropW > out->mfx.FrameInfo.Width) ||
            (out->mfx.FrameInfo.CropW == 0 && out->mfx.FrameInfo.CropH != 0))
        {
            bWarning = true;
            out->mfx.FrameInfo.CropW = out->mfx.FrameInfo.Width;
        }

        if ((out->mfx.FrameInfo.Height != 0 && out->mfx.FrameInfo.CropH > out->mfx.FrameInfo.Height)||
            (out->mfx.FrameInfo.CropW != 0 && out->mfx.FrameInfo.CropH == 0))
        {
            bWarning = true;
            out->mfx.FrameInfo.CropH = out->mfx.FrameInfo.Height;
        }

        if (out->mfx.FrameInfo.CropX || out->mfx.FrameInfo.CropY)
        {
            out->mfx.FrameInfo.CropX = 0;
            out->mfx.FrameInfo.CropY = 0;
            bWarning = true;
        }
        mfxStatus sts = MFX_ERR_NONE;
        if (out->mfx.FrameInfo.FrameRateExtN !=0 && out->mfx.FrameInfo.FrameRateExtD != 0)
        {
            sts = CheckFrameRateMPEG2(out->mfx.FrameInfo.FrameRateExtD, out->mfx.FrameInfo.FrameRateExtN);
            if (sts != MFX_ERR_NONE)
            {
                bWarning = true;            
            }        
        }
        else
        {
            if (out->mfx.FrameInfo.FrameRateExtN !=0 || out->mfx.FrameInfo.FrameRateExtD != 0)
            {
                out->mfx.FrameInfo.FrameRateExtN = 0;
                out->mfx.FrameInfo.FrameRateExtD = 0;
                bUnsupported = true;
            }
        }

        if ((out->mfx.TargetUsage < MFX_TARGETUSAGE_BEST_QUALITY || out->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED)&&
            out->mfx.TargetUsage !=0)
        {
            out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
            bWarning = true;
        }



        if (out->mfx.FrameInfo.ChromaFormat != 0 &&
            out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        {
            out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            bWarning = true;
        }

        sts = CheckAspectRatioMPEG2 (out->mfx.FrameInfo.AspectRatioW,
            out->mfx.FrameInfo.AspectRatioH, 
            out->mfx.FrameInfo.Width, 
            out->mfx.FrameInfo.Height,
            out->mfx.FrameInfo.CropW,
            out->mfx.FrameInfo.CropH);

        if (sts != MFX_ERR_NONE)
        {
            bWarning = true;
            if (sts < MFX_ERR_NONE)
            {
                out->mfx.FrameInfo.AspectRatioW = 1;
                out->mfx.FrameInfo.AspectRatioH = 1; 
            }
        }

        if (out->mfx.CodecProfile != MFX_PROFILE_MPEG2_SIMPLE &&
            out->mfx.CodecProfile != MFX_PROFILE_MPEG2_MAIN   &&
            out->mfx.CodecProfile != MFX_PROFILE_MPEG2_HIGH &&
            out->mfx.CodecProfile != MFX_PROFILE_UNKNOWN)
        {
            out->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
            bWarning = true;
        }
        if (out->mfx.CodecLevel != MFX_LEVEL_MPEG2_LOW &&
            out->mfx.CodecLevel != MFX_LEVEL_MPEG2_MAIN &&
            out->mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH1440 &&
            out->mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH &&
            out->mfx.CodecLevel != MFX_LEVEL_UNKNOWN)
        {
            out->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
            bWarning = true;
        }
        if (out->mfx.RateControlMethod != MFX_RATECONTROL_CBR  &&
            out->mfx.RateControlMethod != MFX_RATECONTROL_VBR  &&
            out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
            out->mfx.RateControlMethod != MFX_RATECONTROL_CQP  &&
            out->mfx.RateControlMethod != 0)
        {
            out->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
            bWarning = true;
        }

        mfxU16 gof = out->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);
        if (out->mfx.GopOptFlag != gof)
        {
            out->mfx.GopOptFlag = gof;
            bWarning = true;
        }

        if (bUnsupported)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        if (bWarning)
        {
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMPEG2::QueryIOSurf(mfxVideoParam *par_input, mfxFrameAllocRequest *request)
{
    UMC::MPEG2EncoderParams umcpar;
    mfxVideoParam parameters;
    mfxVideoParam *par = & parameters;

    MFX_CHECK_NULL_PTR1(par_input);
    memcpy_s (par, sizeof(mfxVideoParam), par_input, sizeof(mfxVideoParam));

    MFX_CHECK_NULL_PTR1(request);
    CHECK_VERSION(par->Version);
    CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);
    MFX_CHECK (CheckExtendedBuffers(par) == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);

    mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);
    mfxExtCodingOptionSPSPPS* pSPSPPS = SHParametersEx::GetExtCodingOptionsSPSPPS (par->ExtParam, par->NumExtParam);

    if (pSPSPPS)
    {
        //mfxStatus sts = MFX_ERR_NONE;
        mfxU32 real_len = 0;
        MFX_CHECK(pSPSPPS->PPSBufSize == 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(!pSPSPPS->PPSBuffer, MFX_ERR_INVALID_VIDEO_PARAM);

        if  (!SHParametersEx::CheckSHParameters(pSPSPPS->SPSBuffer, pSPSPPS->SPSBufSize, real_len, par, 0))
            return MFX_ERR_INVALID_VIDEO_PARAM;   
    }

    if (par->mfx.FrameInfo.Width > 0x1fff || (par->mfx.FrameInfo.Width & 0x0f) != 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    mfxU32 mask = (par->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE)? 0x0f:0x1f;

    if (par->mfx.FrameInfo.Height > 0x1fff || (par->mfx.FrameInfo.Height & mask) != 0 )
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    ApplyTargetUsage (par, &umcpar);

    if ((par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY))== MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        request->Info              = par->mfx.FrameInfo;
        request->NumFrameMin       = (mfxU16)umcpar.IPDistance;
        request->NumFrameSuggested = (mfxU16)umcpar.IPDistance;
        request->Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }
    else if (((par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY))== MFX_IOPATTERN_IN_SYSTEM_MEMORY)||
        ((par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_IN_OPAQUE_MEMORY))== MFX_IOPATTERN_IN_OPAQUE_MEMORY))
    {
        request->Info              =  par->mfx.FrameInfo;
        request->NumFrameMin       = (mfxU16)umcpar.IPDistance;
        request->NumFrameSuggested = (mfxU16)umcpar.IPDistance;
#ifdef ME_REF_ORIGINAL
        request->NumFrameMin       += 2;
        request->NumFrameSuggested += 2;
#endif
        if (par->IOPattern  & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
        else
            request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (ext && (ext->EndOfSequence==MFX_CODINGOPTION_ON))
    {
        request->NumFrameMin       += 1;
        request->NumFrameSuggested += 1;
    }

    if (par->AsyncDepth > 0)
    {
        request->NumFrameMin       += par->AsyncDepth - 1;
        request->NumFrameSuggested += par->AsyncDepth - 1;
    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMPEG2::Init(mfxVideoParam *par)
{
    if (m_bInitialized)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxStatus sts = ResetImpl(par);
    if (sts < MFX_ERR_NONE)
        return sts;

    m_bInitialized = true;
    return sts;
}

static mfxStatus  UnlockFrames (MFXGOP* pGOP, MFXWaitingList* pWaitingList, VideoCORE* pcore)
{
 mfxStatus  sts = MFX_ERR_NONE;
 if (pGOP !=0)
  {
    pGOP->CloseGop(false);
    for (;;)
    {
        sFrameEx  fr = {};
        if (!pGOP->GetFrameExForDecoding(&fr,0,0,0))
        {
            break;
        }
        sts = pcore->DecreaseReference(&fr.m_pFrame->Data);
        MFX_CHECK_STS(sts);
        pGOP->ReleaseCurrentFrame();
    }
  }
  if (pWaitingList)
  {
    for (;;)
    {
        sFrameEx  fr = {};
        if (!pWaitingList->GetFrameEx(&fr))
        {
            break;
        }
        sts = pcore->DecreaseReference(&fr.m_pFrame->Data);
        MFX_CHECK_STS(sts);
        pWaitingList->MoveOnNextFrame();
    }
  }
  return sts;
}

//virtual mfxStatus Reset(mfxVideoParam *par);

mfxStatus MFXVideoENCODEMPEG2::Reset(mfxVideoParam *par_input)
{
    MFX_CHECK(is_initialized(), MFX_ERR_NOT_INITIALIZED);
    return ResetImpl(par_input);
}

mfxStatus MFXVideoENCODEMPEG2::ResetImpl(mfxVideoParam *par_input)
{
    UMC::MPEG2EncoderParams params;
    mfxStatus sts = MFX_ERR_NONE;
    UMC::Status ret;
    bool bProgressiveSequence = false;
    mfxVideoParam local_params;
    mfxVideoParam *par = &local_params;
    bool warning = false;
    bool invalid = false;


    MFX_CHECK_NULL_PTR1(par_input);
    memcpy_s(par, sizeof(mfxVideoParam), par_input, sizeof(mfxVideoParam));


    CHECK_VERSION(par->Version);
    CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);
    MFX_CHECK (CheckExtendedBuffers(par)== MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);

    mfxExtCodingOptionSPSPPS* pSPSPPS = SHParametersEx::GetExtCodingOptionsSPSPPS (par->ExtParam, par->NumExtParam);
    if (pSHEx)
    {
        delete pSHEx;
        pSHEx = 0;
    }
    pSHEx = new SHParametersEx;  

    if (pSPSPPS)
    {
        MFX_CHECK(pSPSPPS->PPSBufSize == 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(!pSPSPPS->PPSBuffer, MFX_ERR_INVALID_VIDEO_PARAM);

        sts = pSHEx->FillSHParameters(par, 0);
        MFX_CHECK_STS(sts);

    }

    sts = CheckVideoParamEncoders(par, m_core->IsExternalFrameAllocator(), MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    m_picStruct = par->mfx.FrameInfo.PicStruct;
    switch (m_picStruct)
    {
    case MFX_PICSTRUCT_PROGRESSIVE:
        bProgressiveSequence = true;
        params.info.interlace_type = UMC::PROGRESSIVE;
        break;
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_UNKNOWN:
        params.info.interlace_type = UMC::INTERLEAVED_TOP_FIELD_FIRST;
        break;
    case MFX_PICSTRUCT_FIELD_BFF:
        params.info.interlace_type = UMC::INTERLEAVED_BOTTOM_FIELD_FIRST;
        break;
    default:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (par->mfx.FrameInfo.CropX!=0 || par->mfx.FrameInfo.CropY!=0 ||
        par->mfx.FrameInfo.Width > 0x1fff || par->mfx.FrameInfo.Height > 0x1fff ||
        par->mfx.FrameInfo.CropW > par->mfx.FrameInfo.Width ||
        par->mfx.FrameInfo.CropH > par->mfx.FrameInfo.Height)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_Width  = par->mfx.FrameInfo.Width;
    m_Height = par->mfx.FrameInfo.Height;


    
    sts = CheckFrameRateMPEG2(par->mfx.FrameInfo.FrameRateExtD, par->mfx.FrameInfo.FrameRateExtN);
    
    if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
    {
        warning = true;        
    }
    else if (sts == MFX_ERR_INVALID_VIDEO_PARAM)
    {
        invalid = true;
    }
    
    if ((par->mfx.FrameInfo.Width  & 15) !=0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (bProgressiveSequence)
    {
        if ((par->mfx.FrameInfo.Height  & 15) !=0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    else
    {
        if ((par->mfx.FrameInfo.Height  & 31) !=0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);

    if (ext)
    {
        mfxExtCodingOption temp = {0};

        mfxU32 bufOffset = sizeof(mfxExtBuffer);
        mfxU32 bufSize   = sizeof(mfxExtCodingOption) - bufOffset;
        temp.EndOfSequence = ext->EndOfSequence;
        temp.FramePicture  = ext->FramePicture;  

        warning  = warning || (memcmp((mfxU8*)(ext) + bufOffset,(mfxU8*)(&temp) + bufOffset, bufSize)!= 0);
        invalid  = invalid || (ext->FieldOutput == MFX_CODINGOPTION_ON);   

    }

    if (par->mfx.RateControlMethod != MFX_RATECONTROL_CBR  &&
        par->mfx.RateControlMethod != MFX_RATECONTROL_VBR  &&
        par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
        par->mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        if (par->mfx.RateControlMethod != 0)
            warning = true;
        par->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    }

    m_bConstantQuant = (par->mfx.RateControlMethod == MFX_RATECONTROL_CQP)? 1:0;
    if (!m_bConstantQuant)
    {
        if (par->mfx.TargetKbps == 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }



    // conditions for input frames
    if (m_response.NumFrameActual>0)
    {
        if (m_request.Info.Width < par->mfx.FrameInfo.Width || m_request.Info.Height < par->mfx.FrameInfo.Height)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    }

    params.info.clip_info.width  = (par->mfx.FrameInfo.CropW != 0) ? par->mfx.FrameInfo.CropW : par->mfx.FrameInfo.Width;
    params.info.clip_info.height = (par->mfx.FrameInfo.CropH != 0) ? par->mfx.FrameInfo.CropH : par->mfx.FrameInfo.Height;

    params.FieldPicture = 0;
    if (!bProgressiveSequence)
    {
        params.FieldPicture = 1;
        if (ext && (ext->FramePicture == MFX_CODINGOPTION_ON))
            params.FieldPicture = 0;
    }

    if(par->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422)
        params.info.color_format = UMC::YUV422;
    else if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
        params.info.color_format = UMC::NV12;
    else
        params.info.color_format = UMC::YUV420;

    sts = CheckAspectRatioMPEG2(
        par->mfx.FrameInfo.AspectRatioW,
        par->mfx.FrameInfo.AspectRatioH, 
        params.info.clip_info.width, 
        params.info.clip_info.height,
        0, 
        0);

    if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
    {
        warning = true;  
        par->mfx.FrameInfo.AspectRatioW = 1;
        par->mfx.FrameInfo.AspectRatioH = 1;
    }
    else if (sts < 0)
    {
        par->mfx.FrameInfo.AspectRatioW = 0;
        par->mfx.FrameInfo.AspectRatioH = 0; 
        invalid = true;
    }

    params.info.aspect_ratio_width = (par->mfx.FrameInfo.AspectRatioW!=0) ?
        (par->mfx.FrameInfo.AspectRatioW*params.info.clip_info.width):params.info.clip_info.width;
    params.info.aspect_ratio_height = (par->mfx.FrameInfo.AspectRatioH!=0) ?
        (par->mfx.FrameInfo.AspectRatioH*params.info.clip_info.height):params.info.clip_info.height;


    m_AspectRatioW = par->mfx.FrameInfo.AspectRatioW;
    m_AspectRatioH = par->mfx.FrameInfo.AspectRatioH;

    Ipp64f fr = CalculateUMCFramerate(
        par->mfx.FrameInfo.FrameRateExtN,
        par->mfx.FrameInfo.FrameRateExtD);

    params.info.framerate = fr;

    m_BufferSizeInKB = par->mfx.BufferSizeInKB * IPP_MAX(1, par->mfx.BRCParamMultiplier); //(par->mfx.BufferSizeInKB!=0)? par->mfx.BufferSizeInKB:GetBufferSizeInKB (par->mfx.TargetKbps, fr);

    {
        if (CorrectProfileLevelMpeg2(par->mfx.CodecProfile, par->mfx.CodecLevel, par->mfx.FrameInfo.Width,par->mfx.FrameInfo.Height,fr, m_bConstantQuant ? 0 : (mfxU32)(par->mfx.TargetKbps * BRC_BITS_IN_KBIT), par->mfx.GopRefDist))
            warning = true;

        switch (par->mfx.CodecProfile)
        {
        case MFX_PROFILE_MPEG2_SIMPLE:
            params.profile = SP;
            break;
        case MFX_PROFILE_MPEG2_MAIN:
            params.profile = MP;
            break;
        case MFX_PROFILE_MPEG2_HIGH:
            params.profile = HP;
            break;
        default:
            params.profile = MP;
            break;
        }
        switch (par->mfx.CodecLevel)
        {
        case MFX_LEVEL_MPEG2_LOW:
            params.level = LL;
            break;
        case MFX_LEVEL_MPEG2_MAIN:
            params.level = ML;
            break;
        case MFX_LEVEL_MPEG2_HIGH:
            params.level = HL;
            break;
        case MFX_LEVEL_MPEG2_HIGH1440:
            params.level = H14;
            break;
        default:
            params.level = ML;
            break;
        }
    }

    ApplyTargetUsage(par, &params);
    m_TargetUsage = par->mfx.TargetUsage;
    params.numThreads = par->mfx.NumThread;

    // TargetUsage ignored
    // RateControlMethod - not found
    if (m_bConstantQuant)
    {
        UMC::Mpeg2_BrcParams brcParams;
        brcParams.frameWidth  = par->mfx.FrameInfo.Width;
        brcParams.frameHeight = par->mfx.FrameInfo.Height;
        brcParams.quant[0] = par->mfx.QPI;
        brcParams.quant[1] = par->mfx.QPP;
        brcParams.quant[2] = par->mfx.QPB;

        if (m_brc == NULL)
        {
            m_brc = new UMC::MPEG2BRC_CONST_QUNT;
        }
        ret = m_brc->Init(&brcParams);
        MFX_CHECK_UMC_STS (ret);

        ret = m_brc->GetParams(&brcParams);
        MFX_CHECK_UMC_STS (ret);

        m_BufferSizeInKB = (mfxU32)((brcParams.maxFrameSize + 999)/1000);
    }
    else
    {
        UMC::VideoBrcParams brcParams;
        params.VBV_BufferSize = (m_BufferSizeInKB * BRC_BYTES_IN_KBYTE * 8 + 16383) / 16384;
        params.info.bitrate = (Ipp32u)(par->mfx.TargetKbps * BRC_BITS_IN_KBIT);
        sts = ConvertVideoParam_Brc(par, &brcParams);
        if (brcParams.BRCMode == UMC::BRC_AVBR)
        {
            brcParams.maxBitrate           = brcParams.targetBitrate;
            brcParams.HRDBufferSizeBytes   = brcParams.targetBitrate; // 8 sec buffer
            brcParams.HRDInitialDelayBytes = brcParams.HRDBufferSizeBytes / 2;
        }
        if (brcParams.BRCMode == UMC::BRC_VBR && brcParams.HRDBufferSizeBytes == 0)
        {
            //Default HRD buffer must be limited by 300 frames.

            mfxF64 frameSize = brcParams.maxBitrate != 0 
                             ? brcParams.maxBitrate / brcParams.info.framerate / 8
                             : brcParams.targetBitrate / brcParams.info.framerate / 8;
            
            brcParams.HRDBufferSizeBytes = (mfxU32)(frameSize * 100);
        }
        brcParams.info.interlace_type = params.info.interlace_type;
        brcParams.GOPPicSize = params.gopSize;
        brcParams.GOPRefDist = params.IPDistance;
        brcParams.HRDBufferSizeBytes = m_BufferSizeInKB > 0 ?   m_BufferSizeInKB * BRC_BYTES_IN_KBYTE :
                                                                brcParams.HRDBufferSizeBytes;
        //  brcParams.info.framerate = fr;
        MFX_CHECK_STS(sts);

        if (m_brc == NULL)
        {
            m_brc = new UMC::MPEG2BRC;
            ret = m_brc->Init(&brcParams);
            MFX_CHECK_UMC_STS (ret);
        }
        else
        {
            m_brc->Close();
            ret = m_brc->Init(&brcParams);
            MFX_CHECK_UMC_STS (ret);
        }

        ret = m_brc->GetParams(&brcParams);
        MFX_CHECK_UMC_STS (ret);

        m_BufferSizeInKB = (mfxU32)((brcParams.HRDBufferSizeBytes + 999)/1000);

    }
    //m_codec.brc = NULL;

    params.lFlags = (par->mfx.EncodedOrder) ? 0 : UMC::FLAG_VENC_REORDER;
    // forget not inserted user data, if any

    m_InputFrameOrder     = -1;
    m_OutputFrameOrder    = -1;
    m_nEncodeCalls        = -1;
    m_BitstreamLen        = 0;
    m_nFrameInGOP         = 0;

    m_GopOptFlag  = par->mfx.GopOptFlag;
    m_IdrInterval = par->mfx.IdrInterval;
    m_bAddEOS     = (ext && (ext->EndOfSequence == MFX_CODINGOPTION_ON));

    params.strictGOP = (m_GopOptFlag & MFX_GOP_STRICT)? 2 : 1;

    m_bAddDisplayExt = false;
    if (mfxExtVideoSignalInfo * extSignalInfo = GetExtVideoSignalInfo(par->ExtParam, par->NumExtParam))
    {
        CheckExtVideoSignalInfo(extSignalInfo);
        params.video_format                 = extSignalInfo->VideoFormat;
        params.color_description            = extSignalInfo->ColourDescriptionPresent;
        params.color_primaries              = extSignalInfo->ColourPrimaries;
        params.transfer_characteristics     = extSignalInfo->TransferCharacteristics;
        params.matrix_coefficients          = extSignalInfo->MatrixCoefficients;

        m_bAddDisplayExt = true;
    }

    m_codec.brc = m_brc;
    ret = m_codec.Init(&params);
    sts = RET_UMC_TO_MFX(ret);
    MFX_CHECK_STS(sts);

    m_codec.closed_gop = (par->mfx.GopOptFlag & MFX_GOP_CLOSED) ? 1 : 0;

    if (!m_bConstantQuant)
    {
        UMC::VideoBrcParams brcParams;
        ret = m_brc->GetParams(&brcParams);
        MFX_CHECK_UMC_STS (ret);
        mfxU32 GOPLengthBits     = ((mfxU32)(params.info.bitrate/params.info.framerate))*brcParams.GOPPicSize;
        mfxI32 numPFrames        = (params.gopSize/params.IPDistance > 1) ? brcParams.GOPPicSize/params.IPDistance - 1:0;
        mfxI32 numBFrames        = (params.gopSize > numPFrames + 1) ? params.gopSize - numPFrames - 1:0;

        mfxU32 minGOPLengthBits  = m_codec.m_MinFrameSizeBits[0] + numPFrames * m_codec.m_MinFrameSizeBits[1] + numBFrames * m_codec.m_MinFrameSizeBits[2];

        if (GOPLengthBits < minGOPLengthBits)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if(m_codec.frames.frame_buffer != 0)
        m_codec.DeleteFrameBuffer();


    sts = UnlockFrames (m_pGOP, m_pWaitingList,m_core);
    MFX_CHECK_STS(sts);

    if (m_pGOP)
    {
        if (m_pGOP->GetMaxBFrames() < params.IPDistance - 1)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        m_pGOP->Reset(m_GopOptFlag&MFX_GOP_CLOSED, par->mfx.IdrInterval,ext && ext->EndOfSequence !=0,par->mfx.EncodedOrder!=0);
    }
    else
    {
        m_pGOP = new MFXGOP;
        if (!m_pGOP)
        {
            return MFX_ERR_NULL_PTR;
        }
        sts = m_pGOP->Init(params.IPDistance - 1, m_GopOptFlag&MFX_GOP_CLOSED, par->mfx.IdrInterval,ext && ext->EndOfSequence==MFX_CODINGOPTION_ON,par->mfx.EncodedOrder!=0);
        MFX_CHECK_STS(sts);
    }
    mfxI32 maxFramesInWaitingList  = 0;
    mfxI32 minFramesInWaitingList = 0;
    mfxI32 delayInWaitingList = 0;

    if (par->mfx.EncodedOrder)
    {
        maxFramesInWaitingList   = (m_bAddEOS)? 2 : 1;
        minFramesInWaitingList   = (m_bAddEOS)? 1 : 0;
        delayInWaitingList       = minFramesInWaitingList;
    }
    else
    {
        maxFramesInWaitingList = (params.IPDistance + 1)*3;
        minFramesInWaitingList = ((m_bAddEOS)? 1 : 0);
        delayInWaitingList     =  params.IPDistance - 1 + minFramesInWaitingList;
    }

    if (m_pWaitingList)
    {
        if (m_pWaitingList->GetMaxFrames() < maxFramesInWaitingList)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        m_pWaitingList->Reset(minFramesInWaitingList, delayInWaitingList);
    }
    else
    {
        m_pWaitingList = new MFXWaitingList;
        if (!m_pWaitingList)
        {
            return MFX_ERR_NULL_PTR;
        }
        sts = m_pWaitingList->Init(maxFramesInWaitingList, minFramesInWaitingList,delayInWaitingList);
        MFX_CHECK_STS(sts);
    }

#ifdef ME_REF_ORIGINAL
    for (mfxU32 i=0; i<2; i++) {
        if(frames.buf_org[i] != 0) {
            sts = m_core->DecreaseReference(&frames.buf_org[i]->Data);
            MFX_CHECK_STS(sts);

        }
    }
#endif

    {
        mfxFrameAllocRequest request = { 0 };
        sts = QueryIOSurf(par, &request);
        MFX_CHECK(sts >=0, sts);
        sts = m_InputSurfaces.Reset (par, request.NumFrameMin);
        MFX_CHECK_STS(sts);
    } 
    // Alloc reconstructed frames: 
    {        
        m_request.NumFrameMin       = m_InputSurfaces.isSysMemFrames() ? 2 : 3;
        m_request.NumFrameSuggested = m_request.NumFrameMin;
        m_request.Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
#ifdef ME_REF_ORIGINAL
        m_request.NumFrameMin       += 2;
        m_request.NumFrameSuggested += 2;
#endif
    }
    m_IOPattern = par->IOPattern;
    if (m_response.NumFrameActual>0)
    {        
        if (m_response.NumFrameActual < m_request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
    else
    {
        m_request.Info = par->mfx.FrameInfo;
        sts = m_core->AllocFrames(&m_request, &m_response);
        MFX_CHECK_STS(sts);

        if (m_response.NumFrameActual < m_request.NumFrameMin)
            return MFX_ERR_MEMORY_ALLOC;

        m_nFrames = m_response.NumFrameActual;

        if (m_pFramesStore)
            return MFX_ERR_MEMORY_ALLOC;

        m_pFramesStore = new mfxFrameSurface1 [m_nFrames];
        MFX_CHECK_NULL_PTR1(m_pFramesStore);

        memset (m_pFramesStore,0,sizeof(mfxFrameSurface1)*m_nFrames);

        for (mfxU32 i=0; i<m_nFrames; i++)
        {
            m_pFramesStore[i].Data.MemId = m_response.mids[i];
            m_pFramesStore[i].Info = m_request.Info;
            sts = m_core->LockFrame(m_pFramesStore[i].Data.MemId,&m_pFramesStore[i].Data);
            MFX_CHECK_STS(sts);
        }

        for (mfxU32 i=0; i<2; i++)
        {
            frames.buf_ref[i] = &m_pFramesStore[i];
            frames.FrameOrderRef[i] = 0;
        }

        if (!m_InputSurfaces.isSysMemFrames())
        {
            ConfigPolarSurface();
        }

    }


    frames.bFirstFrame = true;
    m_nThreads = par->mfx.NumThread;
#ifdef _NEW_THREADING
    CreateTaskInfo();
#endif

    if (invalid)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return warning ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

//virtual mfxStatus Close(void); // same name
mfxStatus MFXVideoENCODEMPEG2::Close(void)
{
    mfxI32 i;
    mfxStatus sts = MFX_ERR_NONE;
    if(!is_initialized())
        return MFX_ERR_NOT_INITIALIZED;

    m_nThreads = 0;

    if (pSHEx)
    {
        delete pSHEx;
        pSHEx = 0;  
    }
    for (i=0; i<2; i++)
    {
        frames.buf_ref[i] = 0;
#ifdef ME_REF_ORIGINAL
        if(frames.buf_org[i] != 0) {
            //frames.buf_org[i]->Locked--;
            sts = m_core->DecreaseReference(&frames.buf_org[i]->Data);
            MFX_CHECK_STS(sts);
            frames.buf_org[i] = 0;
        }
#endif
    }

    for (mfxU32 i=0; i<m_nFrames; i++)
    {
        sts = m_core->UnlockFrame(m_pFramesStore[i].Data.MemId,&m_pFramesStore[i].Data);
        MFX_CHECK_STS(sts);
    }
    delete [] m_pFramesStore;
    m_pFramesStore = 0;

    m_core->FreeFrames(&m_response);
    m_response.NumFrameActual = 0;

    m_nFrames = 0;

    m_codec.Close();
    if (m_brc)
    {
        m_brc->Close();
        delete m_brc;
        m_brc=0;
    }


    sts = UnlockFrames (m_pGOP, m_pWaitingList,m_core);
    MFX_CHECK_STS(sts);

    if (m_pGOP)
    {
        m_pGOP->Close();
        delete m_pGOP;
        m_pGOP = 0;
    }
    if (m_pWaitingList)
    {
        m_pWaitingList->Close();
        delete m_pWaitingList;
        m_pWaitingList = 0;
    }

    m_picStruct = 0;

#ifdef _NEW_THREADING
    DeleteTaskInfo();
#endif

   m_InputSurfaces.Close();


    m_bInitialized = false;

    return MFX_ERR_NONE;
}

void GetBRCParamsWithMultiplier(mfxInfoMFX & mfx, const UMC::VideoBrcParams & brcParams, mfxU32 bufferSizeInKB)
{
    mfxU32 initialDelayKB = brcParams.HRDInitialDelayBytes / 1000;
    mfxU32 targetKbps = brcParams.targetBitrate / 1000;
    mfxU32 maxKbps = brcParams.maxBitrate / 1000;

    mfxU32 maxVal = IPP_MAX(IPP_MAX(targetKbps, maxKbps), IPP_MAX(bufferSizeInKB, initialDelayKB));
    mfxU16 mult = IPP_MAX((mfxU16)((maxVal + 0xffff) / 0x10000), 1);

    mfx.BRCParamMultiplier = mult;
    mfx.InitialDelayInKB = (mfxU16)((initialDelayKB + mult - 1) / mult);
    mfx.TargetKbps       = (mfxU16)((targetKbps  + mult - 1) / mult);
    mfx.BufferSizeInKB   = (mfxU16)((bufferSizeInKB  + mult - 1) / mult);
    mfx.MaxKbps          = (mfxU16)((maxKbps  + mult - 1) / mult);
}

mfxStatus MFXVideoENCODEMPEG2::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    CHECK_VERSION(par->Version);

    MFX_CHECK (is_initialized(),MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK (CheckExtendedBuffers(par) == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    par->mfx.CodecId = MFX_CODEC_MPEG2;

    UMC::MPEG2EncoderParams   encodeInfo;

    UMC::Status               umcret = UMC::UMC_OK;

    umcret = m_codec.GetInfo(&encodeInfo);
    MFX_CHECK_UMC_STS (umcret);


    par->IOPattern = m_IOPattern;
    par->mfx.FrameInfo.CropW = (mfxU16)encodeInfo.info.clip_info.width;
    par->mfx.FrameInfo.CropH = (mfxU16)encodeInfo.info.clip_info.height;
    par->mfx.FrameInfo.Width =  m_Width;
    par->mfx.FrameInfo.Height = m_Height;

    if(encodeInfo.info.interlace_type == UMC::PROGRESSIVE)
        par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    else if(encodeInfo.info.interlace_type == UMC::INTERLEAVED_TOP_FIELD_FIRST)
        par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
    else if(encodeInfo.info.interlace_type == UMC::INTERLEAVED_BOTTOM_FIELD_FIRST)
        par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
    else
        return MFX_ERR_NULL_PTR; // lack of return codes;

    //  par->mfx.FramePicture= encodeInfo.FieldPicture ? 0 : 1;
    mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);
    if (ext)
    {
        ext->FramePicture  = (mfxU16)(encodeInfo.FieldPicture ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);
        ext->EndOfSequence = (mfxU16)(m_bAddEOS ?   MFX_CODINGOPTION_ON:MFX_CODINGOPTION_OFF);
    }

    if (mfxExtCodingOptionSPSPPS* ext = SHParametersEx::GetExtCodingOptionsSPSPPS(par->ExtParam, par->NumExtParam))
    {
        MFX_CHECK_NULL_PTR1(ext->SPSBuffer);

        // pps is n/a for mpeg2
        ext->PPSBufSize = 0;
        ext->PPSId = 0;

        const mfxU32 len = 128;
        mfxU32 off1 = 0;
        mfxU32 off2 = 0;
        mfxU32 off3 = 0;
        mfxU32 buf1[len / 4] = { 0, };
        mfxU32 buf2[len / 4] = { 0, };
        mfxU32 buf3[len / 4] = { 0, };

        off1 = m_codec.PutSequenceHeader2Mem(buf1, len);
        off2 = m_codec.PutSequenceExt2Mem(buf2, len);
        if (m_bAddDisplayExt)
            off3 = m_codec.PutSequenceDisplayExt2Mem(buf3, len);

        if (off1 + off2 + off3 > ext->SPSBufSize)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        memcpy_s(ext->SPSBuffer, off1,               buf1, off1);
        memcpy_s(ext->SPSBuffer + off1, off2,        buf2, off2);
        memcpy_s(ext->SPSBuffer + off1 + off2, off3, buf3, off3);
        ext->SPSBufSize = mfxU16(off1 + off2 + off3);
        ext->SPSId      = 0;
    }

    if (m_bConstantQuant)
    {
        UMC::Mpeg2_BrcParams brcParams;
        umcret = m_brc->GetParams(&brcParams);
        MFX_CHECK_UMC_STS (umcret);
        par->mfx.QPI = brcParams.quant[0];
        par->mfx.QPP = brcParams.quant[1] ;
        par->mfx.QPB = brcParams.quant[2];
    }

    if(encodeInfo.info.color_format == UMC::YUV420 || encodeInfo.info.color_format == UMC::NV12)
        par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    else if(encodeInfo.info.color_format == UMC::YUV422)
        par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
    else
        return MFX_ERR_NULL_PTR; // lack of return codes;

    if (encodeInfo.info.color_format == UMC::NV12)
        par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    else if (encodeInfo.info.color_format == UMC::YUV420)
        par->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
    else
        return MFX_ERR_UNDEFINED_BEHAVIOR;



    //par->mfx.FrameInfo.AspectRatio = (mfxU8)m_codec.aspectRatio_code;
    par->mfx.FrameInfo.AspectRatioW = m_AspectRatioW;
    par->mfx.FrameInfo.AspectRatioH = m_AspectRatioH;

    switch (m_codec.frame_rate_code)
    {
    case 1: par->mfx.FrameInfo.FrameRateExtN = 24000; par->mfx.FrameInfo.FrameRateExtD = 1001; break;
    case 2: par->mfx.FrameInfo.FrameRateExtN = 24; par->mfx.FrameInfo.FrameRateExtD = 1; break;
    case 3: par->mfx.FrameInfo.FrameRateExtN = 25; par->mfx.FrameInfo.FrameRateExtD = 1; break;
    case 4: par->mfx.FrameInfo.FrameRateExtN = 30000; par->mfx.FrameInfo.FrameRateExtD = 1001; break;
    case 5: par->mfx.FrameInfo.FrameRateExtN = 30; par->mfx.FrameInfo.FrameRateExtD = 1; break;
    case 6: par->mfx.FrameInfo.FrameRateExtN = 50; par->mfx.FrameInfo.FrameRateExtD = 1; break;
    case 7: par->mfx.FrameInfo.FrameRateExtN = 60000; par->mfx.FrameInfo.FrameRateExtD = 1001; break;
    case 8: par->mfx.FrameInfo.FrameRateExtN = 60; par->mfx.FrameInfo.FrameRateExtD = 1; break;
    default: par->mfx.FrameInfo.FrameRateExtN = 0; par->mfx.FrameInfo.FrameRateExtD = 0;
    }
    par->mfx.FrameInfo.FrameRateExtN *= m_codec.frame_rate_extension_n + 1;
    par->mfx.FrameInfo.FrameRateExtD *= m_codec.frame_rate_extension_d + 1;

    par->mfx.CodecProfile = (mfxU8)(encodeInfo.profile<<4);
    par->mfx.CodecLevel = (mfxU8)(encodeInfo.level);

    par->mfx.GopPicSize = (mfxU16)encodeInfo.gopSize;
    par->mfx.GopRefDist = (mfxU8)encodeInfo.IPDistance;
    par->mfx.GopOptFlag =  m_GopOptFlag;

    UMC::VideoBrcParams       brcParams;
    umcret = m_brc->GetParams(&brcParams);
    MFX_CHECK_UMC_STS (umcret);

    if (!m_bConstantQuant)
    {
        // Fill BRCParamMultiplier, InitialDelayInKB, TargetKbps, BufferSizeInKB, MaxKbps
        GetBRCParamsWithMultiplier(par->mfx, brcParams, m_BufferSizeInKB);

        switch (brcParams.BRCMode)
        {
        case UMC::BRC_CBR:  par->mfx.RateControlMethod = MFX_RATECONTROL_CBR;  break;
        case UMC::BRC_VBR:  par->mfx.RateControlMethod = MFX_RATECONTROL_VBR;  break;
        case UMC::BRC_AVBR: par->mfx.RateControlMethod = MFX_RATECONTROL_AVBR; break;
        default: par->mfx.RateControlMethod = 0; break;
        }
    }
    else
    {
        par->mfx.RateControlMethod = (mfxU16)MFX_RATECONTROL_CQP;
        mfxU16 mult = IPP_MAX((mfxU16)((m_BufferSizeInKB + 0xffff) / 0x10000), 1);
        par->mfx.BRCParamMultiplier = mult;
        par->mfx.BufferSizeInKB = (mfxU16)((m_BufferSizeInKB  + mult - 1) / mult);
    }    

    par->mfx.NumThread         = (mfxU8)encodeInfo.numThreads;

    par->mfx.EncodedOrder = (encodeInfo.lFlags & UMC::FLAG_VENC_REORDER) ? 0 : 1;
    par->mfx.TargetUsage = m_TargetUsage;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMPEG2::GetFrameParam(mfxFrameParam *par)
{
  MFX_CHECK_NULL_PTR1(par)
  if(!is_initialized())
    return MFX_ERR_NOT_INITIALIZED;

  par->MPEG2.CucId = MFX_CUC_MPEG2_FRAMEPARAM;
  par->MPEG2.CucSz = sizeof(mfxFrameParam);

  if(m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE) par->MPEG2.FrameType = MFX_FRAMETYPE_I; else
  if(m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE) par->MPEG2.FrameType = MFX_FRAMETYPE_P; else
  if(m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE) par->MPEG2.FrameType = MFX_FRAMETYPE_B; else {
    par->MPEG2.FrameType = MFX_FRAMETYPE_I;
    //return MFX_ERR_UNSUPPORTED; // no frame was encoded so far - undefined output
  }

  par->MPEG2.DecFrameLabel = 0; //?
  par->MPEG2.VppFrameLabel = 0; //?
  par->MPEG2.NumMb = (mfxU16)m_codec.MBcount /*>> encodeInfo.FieldPicture*/; // of full frame?
  par->MPEG2.FrameWinMbMinus1 = (mfxU16)(m_codec.MBcountH - 1);
  par->MPEG2.FrameHinMbMinus1 = (mfxU16)((m_codec.MBcountV /*>> encodeInfo.FieldPicture*/) - 1);
  //par->MPEG2.CurrFrameLabel = m_codec.encodeInfo.numEncodedFrames;
  par->MPEG2.NumRefFrame = (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)?2:((m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE)?1:0);

  par->MPEG2.FieldPicFlag = m_codec.encodeInfo.FieldPicture;
  par->MPEG2.SecondFieldFlag = m_codec.second_field;
  par->MPEG2.BottomFieldFlag = (m_codec.picture_structure == MPEG2_BOTTOM_FIELD) ? 1 : 0;
  switch(m_codec.encodeInfo.info.color_format) {
    case UMC::YUV420: par->MPEG2.ChromaFormatIdc = MFX_CHROMAFORMAT_YUV420; break;
    case UMC::NV12:   par->MPEG2.ChromaFormatIdc = MFX_CHROMAFORMAT_YUV420; break;
    case UMC::YUV422: par->MPEG2.ChromaFormatIdc = MFX_CHROMAFORMAT_YUV422; break;
    case UMC::YUV444: par->MPEG2.ChromaFormatIdc = MFX_CHROMAFORMAT_YUV444; break;
    case UMC::GRAY: par->MPEG2.ChromaFormatIdc = MFX_CHROMAFORMAT_MONOCHROME; break;
    default: return MFX_ERR_UNSUPPORTED;
  }
  par->MPEG2.RefPicFlag = (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE || m_codec.encodeInfo.IPDistance == 1)?0:1;
  par->MPEG2.BackwardPredFlag = (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)?1:0; //?
  par->MPEG2.NoResidDiffs = 1; // currently
  par->MPEG2.FrameMbsOnlyFlag = m_codec.encodeInfo.frame_pred_frame_dct[m_codec.picture_coding_type - UMC::MPEG2_I_PICTURE];
  par->MPEG2.BrokenLinkFlag = m_codec.closed_gop || (m_codec.encodeInfo.IPDistance==1); // valid only inside I
  par->MPEG2.CloseEntryFlag = 0 || ((m_codec.encodeInfo.IPDistance==1)); // unless reset happens or NO B
  par->MPEG2.IntraPicFlag = (m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE);

  par->MPEG2.MvGridAndChroma = 0;

  if(m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE || m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)
    par->MPEG2.BitStreamFcodes = (mfxU16)((m_codec.mp_f_code[0][0]<<12) | (m_codec.mp_f_code[0][1]<<8));
  if(m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)
    par->MPEG2.BitStreamFcodes |= (m_codec.mp_f_code[1][0]<<4) | m_codec.mp_f_code[1][1];

  //par->MPEG2.BitStreamPCEelement
  mfxU32 actual_tff;
  actual_tff = ((m_codec.picture_structure == MPEG2_FRAME_PICTURE && m_codec.encodeInfo.info.interlace_type != UMC::PROGRESSIVE) ||
    m_codec.repeat_first_field);

  par->MPEG2.ProgressiveFrame   = (m_codec.encodeInfo.info.interlace_type == UMC::PROGRESSIVE);
  par->MPEG2.Chroma420type      = ((par->MPEG2.ChromaFormatIdc == MFX_CHROMAFORMAT_YUV420) && par->MPEG2.ProgressiveFrame) ? 1 : 0;
  par->MPEG2.RepeatFirstField   = m_codec.repeat_first_field;
  par->MPEG2.AlternateScan      = (m_codec.curr_scan!=0);
  par->MPEG2.IntraVLCformat     = m_codec.curr_intra_vlc_format;
  par->MPEG2.QuantScaleType     = m_codec.q_scale_type;
  par->MPEG2.ConcealmentMVs     = 0;
  par->MPEG2.FrameDCTprediction = m_codec.curr_frame_pred;
  par->MPEG2.TopFieldFirst      = (actual_tff) ? m_codec.top_field_first : 0;
  par->MPEG2.PicStructure       = m_codec.picture_structure;
  par->MPEG2.IntraDCprecision   = m_codec.intra_dc_precision;

  par->MPEG2.BSConcealmentNeed = 0;
  par->MPEG2.BSConcealmentMethod = 0;

  // ?
  par->MPEG2.RefFrameListP[0] = 0;
  par->MPEG2.RefFrameListB[0][0] = 0;

  // check different for vbr!
  par->MPEG2.MinFrameSize = m_codec.rc_vbv_min>>3;
  par->MPEG2.MaxFrameSize = m_codec.rc_vbv_max>>3;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMPEG2::GetEncodeStat(mfxEncodeStat *stat)
{
  if(!is_initialized())
    return MFX_ERR_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(stat)

  stat->NumBit   = (mfxU64)(m_BitstreamLen*8);
  stat->NumFrame = m_OutputFrameOrder + 1;

  stat->NumCachedFrame = m_InputFrameOrder - m_OutputFrameOrder;
  return MFX_ERR_NONE;
}
mfxStatus MFXVideoENCODEMPEG2::ReorderFrame(mfxEncodeInternalParams *pInInternalParams, mfxFrameSurface1 *in,
                                            mfxEncodeInternalParams *pOutInternalParams, mfxFrameSurface1 **out)
{
  if (in)
  {
    if (!m_pWaitingList->AddFrame( in, pInInternalParams))
    {
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }
  }
  // Fill GOP structure using waiting list
  for(;;)
  {
    sFrameEx         CurFrame = {};

    if (!m_pWaitingList->GetFrameEx(&CurFrame, in == NULL))
    {
        break;
    }
    if (!m_pGOP->AddFrame(&CurFrame))
    {
        break;
    }
    m_pWaitingList->MoveOnNextFrame();
  }
  if (!in)
  {
     m_pGOP->CloseGop(0 != (m_GopOptFlag & MFX_GOP_STRICT));
  }

  sFrameEx  CurFrame = {};

  // Extract next frame from GOP structure
  if (!m_pGOP->GetFrameExForDecoding(&CurFrame,m_pWaitingList->isNextReferenceIntra(), m_pWaitingList->isNextBFrame(),m_pWaitingList->isLastFrame()))
  {
     return MFX_ERR_MORE_DATA;
  }
  mfxU16 frameType = CurFrame.m_sEncodeInternalParams.FrameType;

  //Correct InternalFlags
  CurFrame.m_sEncodeInternalParams.InternalFlags = (CurFrame.m_bAddHeader)? MFX_IFLAG_ADD_HEADER:0;
  if (CurFrame.m_bAddEOS)
  {
    CurFrame.m_sEncodeInternalParams.InternalFlags |= MFX_IFLAG_ADD_EOS;
  }
  if (CurFrame.m_bOnlyBwdPrediction && isBPredictedFrame(frameType))
  {
    CurFrame.m_sEncodeInternalParams.InternalFlags |= MFX_IFLAG_BWD_ONLY;
  }

  // Check frame order parameters
  if (isPredictedFrame(frameType))
  {
      sFrameEx refFrame = {0};
      if (m_pGOP->GetFrameExReference(&refFrame))
      {
        MFX_CHECK((CurFrame.m_FrameOrder > refFrame.m_FrameOrder) && ((Ipp32s)CurFrame.m_FrameOrder - (Ipp32s)refFrame.m_FrameOrder <= m_codec.encodeInfo.IPDistance) ,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
      }
  }
  else if (isBPredictedFrame(frameType))
  {
      if (CurFrame.m_bOnlyBwdPrediction)
      {
          sFrameEx refFrame = {0};
          if (m_pGOP->GetFrameExReference(&refFrame,true))
          {
            MFX_CHECK((CurFrame.m_FrameOrder < refFrame.m_FrameOrder) && ((Ipp32s)refFrame.m_FrameOrder - (Ipp32s)CurFrame.m_FrameOrder < m_codec.encodeInfo.IPDistance) ,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
          }
      }
      else
      {
          sFrameEx refFrameF = {0};
          sFrameEx refFrameB = {0};
          if (m_pGOP->GetFrameExReference(&refFrameF,false) && m_pGOP->GetFrameExReference(&refFrameB,true))
          {
            MFX_CHECK((CurFrame.m_FrameOrder < refFrameB.m_FrameOrder) && (CurFrame.m_FrameOrder > refFrameF.m_FrameOrder) ,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
          }
      }
  }

  *out = CurFrame.m_pFrame;
  *pOutInternalParams = CurFrame.m_sEncodeInternalParams;

  m_pGOP->ReleaseCurrentFrame();

  return MFX_ERR_NONE;
}


mfxStatus MFXVideoENCODEMPEG2::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)
{
    mfxStatus sts       = MFX_ERR_NONE;
    bool      bWarning  = false;

    MFX_CHECK(is_initialized(),MFX_ERR_NOT_INITIALIZED);
    CHECK_VERSION(bs->Version);
    MFX_CHECK_NULL_PTR2(bs, pInternalParams);
    MFX_CHECK(bs->DataOffset <= 32 , MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 startbspos = bs->DataOffset + bs->DataLength;
    mfxI32 output_buffer_size = (mfxI32)(bs->MaxLength - startbspos);

    MFX_CHECK(output_buffer_size >= (mfxI32)m_BufferSizeInKB*1000,MFX_ERR_NOT_ENOUGH_BUFFER);
    MFX_CHECK_NULL_PTR1(bs->Data);

    if (surface)
    {
        if (m_picStruct != MFX_PICSTRUCT_UNKNOWN)
        {
            if ((surface->Info.PicStruct&0x0f) != (m_picStruct) &&
                (surface->Info.PicStruct&0x0f) != MFX_PICSTRUCT_UNKNOWN &&
                (surface->Info.PicStruct&0x0f) != MFX_PICSTRUCT_PROGRESSIVE)
            {
                bWarning=true;
            }
        }
        else if ((surface->Info.PicStruct&0x0f) == MFX_PICSTRUCT_UNKNOWN)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        if (surface->Info.Width  != m_Width || surface->Info.Height != m_Height)
            bWarning = true;

        MFX_CHECK(surface->Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (surface->Data.Y)
        {
            MFX_CHECK(surface->Data.Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);        
            CHECK_VERSION(surface->Version);        
        }
        sts = m_core->IncreaseReference(&surface->Data);
        MFX_CHECK_STS(sts);
        m_InputFrameOrder++;

        mfxU16 frameType = (ctrl)? ctrl->FrameType : 0;
        if (!(m_codec.encodeInfo.lFlags & UMC::FLAG_VENC_REORDER))
        {
            mfxU16 type = frameType & (MFX_FRAMETYPE_I|MFX_FRAMETYPE_P|MFX_FRAMETYPE_B);
            MFX_CHECK ((type == MFX_FRAMETYPE_I || type == MFX_FRAMETYPE_P || type == MFX_FRAMETYPE_B), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }

        pInternalParams->FrameType    = frameType;
        pInternalParams->FrameOrder   = (m_codec.encodeInfo.lFlags & UMC::FLAG_VENC_REORDER)? m_InputFrameOrder:surface->Data.FrameOrder;
        if (ctrl)
        {
            pInternalParams->ExtParam    = ctrl->ExtParam;
            pInternalParams->NumExtParam = ctrl->NumExtParam;
            pInternalParams->NumPayload  = ctrl->NumPayload;
            pInternalParams->Payload     = ctrl->Payload;
            pInternalParams->QP          = ctrl->QP;

        }
        else
        {
            pInternalParams->ExtParam    = 0;
            pInternalParams->NumExtParam = 0;
            pInternalParams->NumPayload  = 0;
            pInternalParams->Payload     = 0;
            pInternalParams->QP          = 0;

        }

        *reordered_surface = m_InputSurfaces.GetOpaqSurface(surface);

        if (m_InputFrameOrder < m_pWaitingList->GetDelay())
        {
            return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
        }
        else
        {
            m_nEncodeCalls ++;
            return (bWarning? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:MFX_ERR_NONE);
        }
    }
    else
    {
        m_nEncodeCalls ++;
        *reordered_surface = 0;
        return (m_nEncodeCalls <= m_InputFrameOrder)? MFX_ERR_NONE: MFX_ERR_MORE_DATA;
    }
}

mfxStatus MFXVideoENCODEMPEG2::EncodeFrame(mfxEncodeCtrl* /*ctrl*/, mfxEncodeInternalParams *pInputInternalParams, mfxFrameSurface1 *input_surface, mfxBitstream *bs)
{
  mfxStatus ret = MFX_ERR_NONE;

       MFX::AutoTimer timer("MPEG2Enc_EncodeFrame");

  if(!is_initialized())
    return MFX_ERR_NOT_INITIALIZED;

  mfxEncodeInternalParams sInternalParams ;
  mfxEncodeInternalParams*pInternalParams = &sInternalParams;
  mfxFrameSurface1 *      surface = 0;

  mfxU32 dataLen = bs->DataLength;

  if (input_surface!=NULL)
  {
    mfxU16 type = pInputInternalParams->FrameType & (MFX_FRAMETYPE_I|MFX_FRAMETYPE_P|MFX_FRAMETYPE_B);
    if (m_codec.encodeInfo.lFlags & UMC::FLAG_VENC_REORDER)
    {      
      if (type != MFX_FRAMETYPE_I)
      {
          GetFrameTypeMpeg2 (m_nFrameInGOP,
              m_codec.encodeInfo.gopSize,
              m_codec.encodeInfo.IPDistance,
              false/*(m_GopOptFlag & MFX_GOP_CLOSED)!=0*/,
              &pInputInternalParams->FrameType);
      }
      m_nFrameInGOP = (pInputInternalParams->FrameType & MFX_FRAMETYPE_I) ? 1 : m_nFrameInGOP + 1;
    } 
    else
    {
      MFX_CHECK((type == MFX_FRAMETYPE_I || type == MFX_FRAMETYPE_P || type == MFX_FRAMETYPE_B), MFX_ERR_UNDEFINED_BEHAVIOR);
    }
 }

 ret = ReorderFrame(pInputInternalParams,GetOriginalSurface(input_surface),pInternalParams,&surface);
 if (ret == MFX_ERR_MORE_DATA)
 {
     return MFX_ERR_NONE;
 }
 else if (ret != MFX_ERR_NONE)
 {
    return MFX_ERR_UNDEFINED_BEHAVIOR;
 }

  // can set picture coding params here according to VideoData

 mfxU16 curPicStruct = (surface->Info.PicStruct != MFX_PICSTRUCT_UNKNOWN) ? surface->Info.PicStruct:(m_picStruct| (surface->Info.PicStruct&0xf0));

  if (m_picStruct != MFX_PICSTRUCT_UNKNOWN)
  {
    if (surface->Info.PicStruct != (m_picStruct&0x0f) &&
        surface->Info.PicStruct != MFX_PICSTRUCT_UNKNOWN &&
        surface->Info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        curPicStruct = m_picStruct| (surface->Info.PicStruct&0xf0);
    }
  }

 

  m_codec.repeat_first_field = 0;
  m_codec.top_field_first = 0;

  if(curPicStruct & MFX_PICSTRUCT_PROGRESSIVE)
  {
    m_codec.picture_structure  = MPEG2_FRAME_PICTURE;
    m_codec.progressive_frame  = true;

    if (m_picStruct != MFX_PICSTRUCT_PROGRESSIVE) /*progressive frame in not progressive sequence*/
    {
        m_codec.top_field_first    = (((m_picStruct == MFX_PICSTRUCT_UNKNOWN)? curPicStruct: m_picStruct)& MFX_PICSTRUCT_FIELD_BFF)? 0:1;
        m_codec.repeat_first_field = (curPicStruct & MFX_PICSTRUCT_FIELD_REPEATED)? 1:0;
    }
    else /*progressive frame in progressive sequence*/
    {
        if (surface->Info.PicStruct & MFX_PICSTRUCT_FRAME_DOUBLING)
        {
            m_codec.repeat_first_field = 1;
        }
        else if (surface->Info.PicStruct & MFX_PICSTRUCT_FRAME_TRIPLING)
        {
            m_codec.repeat_first_field = 1;
            m_codec.top_field_first = 1;
        }
    }
  }
  else if(curPicStruct & MFX_PICSTRUCT_FIELD_TFF)
  {
    m_codec.top_field_first   = true;
    m_codec.picture_structure = (m_codec.encodeInfo.FieldPicture)? MPEG2_TOP_FIELD:MPEG2_FRAME_PICTURE;
    m_codec.progressive_frame = false;
  }
  else if(curPicStruct & MFX_PICSTRUCT_FIELD_BFF)
  {
    m_codec.top_field_first   = false;
    m_codec.picture_structure = (m_codec.encodeInfo.FieldPicture)? MPEG2_BOTTOM_FIELD:MPEG2_FRAME_PICTURE;
    m_codec.progressive_frame = false;
  }
  else
  {
    m_codec.picture_structure = MPEG2_FRAME_PICTURE;
    m_codec.top_field_first   = true;
    m_codec.progressive_frame = true;
  }

  //printf("frame: %d ",pInternalParams->FrameOrder);
  /* Start debug

  if (pInternalParams->InternalFlags&MFX_IFLAG_ADD_HEADER)
  {
    printf("SH ");
  }

  if (pInternalParams->FrameType & MFX_FRAMETYPE_B)
  {
    printf("B picture");
  }
  else if (pInternalParams->FrameType & MFX_FRAMETYPE_P)
  {
    printf("P picture");
  }
  else if (pInternalParams->FrameType & MFX_FRAMETYPE_I)
  {
    printf("I picture");
  }
  else
  {
    printf("Error");
    return MFX_ERR_UNSUPPORTED;
  }
  if (pInternalParams->InternalFlags&MFX_IFLAG_BWD_ONLY)
  {
    printf(" Backward");
  }
  if (pInternalParams->InternalFlags&MFX_IFLAG_ADD_EOS)
  {
    printf(" EOS");
  }
  printf("\n");

   End debug*/

  m_codec.m_bSH     = (pInternalParams->InternalFlags&MFX_IFLAG_ADD_HEADER)? true:false;
  m_codec.m_bEOS    = (pInternalParams->InternalFlags&MFX_IFLAG_ADD_EOS)?    true:false;
  m_codec.m_bBwdRef = (pInternalParams->InternalFlags&MFX_IFLAG_BWD_ONLY)?   true:false;

  if (pInternalParams->FrameType & MFX_FRAMETYPE_B)
  {
    m_codec.picture_coding_type = UMC::MPEG2_B_PICTURE;
  }
  else if (pInternalParams->FrameType & MFX_FRAMETYPE_P)
  {
    m_codec.picture_coding_type = UMC::MPEG2_P_PICTURE;
  }
  else if (pInternalParams->FrameType & MFX_FRAMETYPE_I)
  {
    m_codec.picture_coding_type = UMC::MPEG2_I_PICTURE;
  }
  else
  {
    return MFX_ERR_UNSUPPORTED;
  }

  if (pInternalParams->FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P)) {
    m_codec.P_distance = pInternalParams->FrameOrder - frames.FrameOrderRef[1];
    m_codec.B_count = 0; // will be +1 next time
  } else {
    m_codec.B_count = pInternalParams->FrameOrder - frames.FrameOrderRef[0];
    if (m_codec.B_count >  m_codec.P_distance || m_codec.B_count<=0)
        return MFX_ERR_UNSUPPORTED;
  }
  if (pInternalParams->FrameType & MFX_FRAMETYPE_I)
    m_codec.GOP_count = 0; //?? OK ??
  else
    m_codec.GOP_count++;

  if (m_bConstantQuant)
  {
      //update quant values
      // need to do
  }

  ret = GetFrame(pInternalParams, surface, bs);
  
  if(ret != MFX_ERR_NONE)
    return ret;

  m_OutputFrameOrder ++;
  m_BitstreamLen = m_BitstreamLen + (bs->DataLength - dataLen);

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMPEG2::CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 *surface, mfxBitstream * /*bs*/)
{
  MFX_CHECK_NULL_PTR1(surface)
  m_OutputFrameOrder ++;
  return m_core->DecreaseReference(&surface->Data);
}
static void GetBlockOffsetFrm (Ipp32s *pBlockOffset, Ipp32s pitch)
{
  pBlockOffset[0]  = 0;
  pBlockOffset[1]  = 8;
  pBlockOffset[2]  = 8*pitch;
  pBlockOffset[3]  = 8*pitch + 8;
  pBlockOffset[4]  = 0;
  pBlockOffset[5]  = 0;
  pBlockOffset[6]  = 8*pitch;
  pBlockOffset[7]  = 8*pitch;
  pBlockOffset[8]  = 8;
  pBlockOffset[9]  = 8;
  pBlockOffset[10] = 8*pitch + 8;
  pBlockOffset[11] = 8*pitch + 8;
}
static void GetBlockOffsetFld (Ipp32s *pBlockOffset, Ipp32s pitch)
{
  pBlockOffset[0]  = 0;
  pBlockOffset[1]  = 8;
  pBlockOffset[2]  = pitch;
  pBlockOffset[3]  = pitch + 8;
  pBlockOffset[4]  = 0;
  pBlockOffset[5]  = 0;
  pBlockOffset[6]  = pitch;
  pBlockOffset[7]  = pitch;
  pBlockOffset[8]  = 8;
  pBlockOffset[9]  = 8;
  pBlockOffset[10] = pitch + 8;
  pBlockOffset[11] = pitch + 8;
}

mfxStatus MFXVideoENCODEMPEG2::GetFrame(mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *in, mfxBitstream* bs)
{
  mfxStatus   sts = MFX_ERR_NONE;
  bool bLockedInput = false;
#ifdef ME_REF_ORIGINAL
  bool bLockedOriginal [2] = {false, false};
#endif

  mfxFrameSurface1 *curenc = 0;

  MFX_CHECK_NULL_PTR2(in,bs)

  // check output size if we have a frame to encode
  mfxU32 startbspos = bs->DataOffset + bs->DataLength;
  m_codec.out_pointer = (Ipp8u*)bs->Data + startbspos;
  m_codec.output_buffer_size = (Ipp32s)(bs->MaxLength - startbspos);

  if (in->Data.Y == 0)
  {
      //external frame
      sts = m_core->LockExternalFrame(in->Data.MemId, &in->Data);
      MFX_CHECK_STS(sts);
      bLockedInput = true;
  }
  MFX_CHECK(in->Data.Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
  MFX_CHECK(in->Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);
  MFX_CHECK(in->Data.Y != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
  MFX_CHECK(in->Data.UV != 0, MFX_ERR_UNDEFINED_BEHAVIOR);

  CHECK_VERSION(in->Version);

  if (!frames.bCopyInputFrames)
  {
    curenc = in;
  }
  else // if (frames. bCopyInputFrames && pCurr)
  {
      // release input has been copied
      if (bLockedInput)
      {
          sts = m_core->UnlockExternalFrame(in->Data.MemId, &in->Data);
          MFX_CHECK_STS(sts);
          bLockedInput = false;
      }

      mfxFrameSurface1 * pCpy = 0;
      mfxFrameSurface1 * pInp = in;

      curenc = frames.buf_aux;
      pCpy   = curenc;

      sts = m_core->IncreaseReference(&pCpy->Data);
      MFX_CHECK_STS(sts);

      sts = m_core->DoFastCopyWrapper(pCpy, 
                                      MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                      pInp,
                                      mfxU16((m_IOPattern & MFX_MEMTYPE_OPAQUE_FRAME) ? MFX_MEMTYPE_INTERNAL_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME) | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
      MFX_CHECK_STS(sts);

      pCpy->Data.FrameOrder = pInp->Data.FrameOrder;
      pCpy->Data.TimeStamp  = pInp->Data.TimeStamp;

      sts = m_core->DecreaseReference(&pInp->Data);
      MFX_CHECK_STS(sts);
  }
  //RotateReferenceFrames();
  if (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE ||
      m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE)
  {
      mfxFrameSurface1* ptr = frames.buf_ref[0];

      frames.buf_ref[0]       = frames.buf_ref[1];
      frames.FrameOrderRef[0] = frames.FrameOrderRef[1];

      frames.buf_ref[1]       =  ptr; // rotated
      frames.FrameOrderRef[1] =  pInternalParams->FrameOrder;

#ifdef ME_REF_ORIGINAL
      if (frames.buf_org[0] && !frames.bFirstFrame)
      {
          sts = m_core->DecreaseReference(&frames.buf_org[1]->Data);
          MFX_CHECK_STS(sts);
      }
      if (frames.bCopyInputFrames)
      {
         frames.buf_aux=frames.buf_org[0];
      }
      frames.buf_org[0] = frames.buf_org[1];
      frames.buf_org[1] = curenc;
      if (frames.buf_org[1])
      {
        sts = m_core->IncreaseReference(&frames.buf_org[1]->Data);
        MFX_CHECK_STS(sts);
      }
#endif /* ME_REF_ORIGINAL */
  }

#ifdef ME_REF_ORIGINAL
  if(m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE && !frames.bCopyInputFrames)
  {
    if(m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE &&!frames.buf_org[1]->Data.Y) // need check others as well
    {
      sts == m_core->LockExternalFrame(frames.buf_org[1]->Data.MemId, &frames.buf_org[1]->Data); // bwd orig ref for B
      MFX_CHECK_STS(sts);
      bLockedOriginal[1] = 1;
    }
    if(!frames.buf_org[0]->Data.Y) // need check others as well
    {
      sts == m_core->LockExternalFrame(frames.buf_org[0]->Data.MemId, &frames.buf_org[0]->Data); // bwd orig ref for B
      MFX_CHECK_STS(sts);
      bLockedOriginal[0] = 1;
    }
  }
#endif

  Ipp32s i;

  m_codec.YFramePitchSrc  = curenc->Data.Pitch;
  m_codec.UVFramePitchSrc = m_codec.YFramePitchSrc;

  if(curenc->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    m_codec.UVFramePitchSrc >>= 1;

  m_codec.Y_src = (Ipp8u*)curenc->Data.Y;
  if (m_codec.encodeInfo.info.color_format == UMC::NV12) {
    m_codec.U_src = (Ipp8u*)curenc->Data.UV;
    m_codec.V_src = m_codec.U_src+1;
  } else {
    m_codec.U_src = (Ipp8u*)curenc->Data.U;
    m_codec.V_src = (Ipp8u*)curenc->Data.V;
  }
  if (m_codec.Y_src == NULL || m_codec.U_src == NULL || m_codec.V_src == NULL) {
    return MFX_ERR_LOCK_MEMORY;
  }
  m_codec.YFramePitchRef  = frames.buf_ref[0][0].Data.Pitch;
  m_codec.UVFramePitchRef = m_codec.YFramePitchRef;

#ifdef ME_REF_ORIGINAL
  m_codec.YFramePitchRec  = frames.buf_org[0][0].Data.Pitch;
  m_codec.UVFramePitchRec = m_codec.YFramePitchRec;
#else
  m_codec.YFramePitchRec  = frames.buf_ref[0][0].Data.Pitch;
  m_codec.UVFramePitchRec = m_codec.YFramePitchRec;
#endif

  if(curenc->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
  {
    m_codec.UVFramePitchRef >>= 1;
    m_codec.UVFramePitchRec >>= 1;
  }

  for(i=0; i<2; i++) { // forward/backward
    m_codec.YRefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].Data.Y);
    if (m_codec.encodeInfo.info.color_format == UMC::NV12) {
      m_codec.URefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].Data.UV);
      m_codec.VRefFrame[0][i] = m_codec.URefFrame[0][i] + 1;
    } else {
      m_codec.URefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].Data.U);
      m_codec.VRefFrame[0][i] = (Ipp8u*)(frames.buf_ref[i][0].Data.V);
    }
    m_codec.YRefFrame[1][i] = m_codec.YRefFrame[0][i] + m_codec.YFramePitchRef;
    if (m_codec.encodeInfo.info.color_format == UMC::NV12) {
      m_codec.URefFrame[1][i] = m_codec.URefFrame[0][i] + 2*m_codec.UVFramePitchRef;
      m_codec.VRefFrame[1][i] = m_codec.VRefFrame[0][i] + 2*m_codec.UVFramePitchRef;
    } else {
      m_codec.URefFrame[1][i] = m_codec.URefFrame[0][i] + m_codec.UVFramePitchRef;
      m_codec.VRefFrame[1][i] = m_codec.VRefFrame[0][i] + m_codec.UVFramePitchRef;
    }
  }
  for(i=0; i<2; i++) { // forward/backward
#ifdef ME_REF_ORIGINAL
    m_codec.YRecFrame[0][i] = (Ipp8u*)(frames.buf_org[i][0].Data.Y);
    if (m_codec.encodeInfo.info.color_format == UMC::NV12) {
      m_codec.URecFrame[0][i] = (Ipp8u*)(frames.buf_org[i][0].Data.UV);
      m_codec.VRecFrame[0][i] = m_codec.URecFrame[0][i] + 1;
    } else {
      m_codec.URecFrame[0][i] = (Ipp8u*)(frames.buf_org[i][0].Data.U);
      m_codec.VRecFrame[0][i] = (Ipp8u*)(frames.buf_org[i][0].Data.V);
    }
#else /* ME_REF_ORIGINAL */
    m_codec.YRecFrame[0][i] = m_codec.YRefFrame[0][i];
    m_codec.URecFrame[0][i] = m_codec.URefFrame[0][i];
    m_codec.VRecFrame[0][i] = m_codec.VRefFrame[0][i];
#endif /* ME_REF_ORIGINAL */
    m_codec.YRecFrame[1][i] = m_codec.YRecFrame[0][i] + m_codec.YFramePitchRec;
    if (m_codec.encodeInfo.info.color_format == UMC::NV12) {
      m_codec.URecFrame[1][i] = m_codec.URecFrame[0][i] + 2*m_codec.UVFramePitchRec;
      m_codec.VRecFrame[1][i] = m_codec.VRecFrame[0][i] + 2*m_codec.UVFramePitchRec;
    } else {
      m_codec.URecFrame[1][i] = m_codec.URecFrame[0][i] + m_codec.UVFramePitchRec;
      m_codec.VRecFrame[1][i] = m_codec.VRecFrame[0][i] + m_codec.UVFramePitchRec;
    }
  }

  m_codec.mEncodedSize = 0;

  bs->FrameType = (mfxU16)((m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE)?
                            MFX_FRAMETYPE_I|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_REF:
                            (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE)? MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF: MFX_FRAMETYPE_B);

  sts = EncodeFrameReordered(pInternalParams);
  MFX_CHECK_STS(sts);




  bs->DataLength += m_codec.mEncodedSize;
  bs->TimeStamp = curenc->Data.TimeStamp;
  bs->DecodeTimeStamp = (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)? 
      CalcDTSForNonRefFrameMpeg2(bs->TimeStamp):
      CalcDTSForRefFrameMpeg2(bs->TimeStamp,m_codec.P_distance, m_codec.encodeInfo.IPDistance, m_codec.encodeInfo.info.framerate);


  frames.bFirstFrame = false;

  //UnlockBuffers

  if (bLockedInput)
  {
      m_core->UnlockExternalFrame(in->Data.MemId, &in->Data);
      // bLockedInput = false;
  }

#ifdef ME_REF_ORIGINAL
  if (bLockedOriginal[0])
  {
      m_core->UnlockExternalFrame(frames.buf_ref[0]->Data.MemId, &frames.buf_ref[0]->Data);
      bLockedOriginal[0] = false;
  }
  if (bLockedOriginal[1])
  {
      m_core->UnlockExternalFrame(frames.buf_ref[1]->Data.MemId, &frames.buf_ref[1]->Data);
      bLockedOriginal[1] = false;
  }
#endif

  sts = m_core->DecreaseReference(&curenc->Data);
  MFX_CHECK_STS(sts);

  return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMPEG2::EncodeFrameReordered(mfxEncodeInternalParams *pInternalParams)
{
  MFX_CHECK_NULL_PTR1(pInternalParams)

  if (m_codec.brc && m_bConstantQuant)
  {
    if (pInternalParams->QP > 0)
    {
      if  (m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE)
         m_codec.brc->SetQP(pInternalParams->QP, UMC::I_PICTURE);
      else if  (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE)
         m_codec.brc->SetQP(pInternalParams->QP, UMC::P_PICTURE);
      else if  (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)
         m_codec.brc->SetQP(pInternalParams->QP, UMC::B_PICTURE);
    }
  }  
  m_codec.ipflag = 0;

  if (m_codec.picture_structure == MPEG2_FRAME_PICTURE) {
    m_codec.curr_field = 0;
    m_codec.second_field = 0;

#ifdef _NEW_THREADING
    mfxStatus sts = MFX_ERR_NONE;
    sts = PutPicture(pInternalParams->Payload,pInternalParams->NumPayload, true,true);
    MFX_CHECK_STS(sts);
#else
    UMC::Status sts = UMC::UMC_OK;
    sts = m_codec.PutPicture();
    MFX_CHECK_UMC_STS(sts);
#endif

  } else { // TOP_FIELD or BOTTOM_FIELD
    Ipp8u *pSrc[3] = {m_codec.Y_src, m_codec.U_src, m_codec.V_src};
    UMC::MBInfo *pMBInfo0 = m_codec.pMBInfo;
    // Ipp64s field_endpos = 0;
    Ipp32s ifield;

    m_codec.YFrameVSize >>= 1;
    m_codec.UVFrameVSize >>= 1;
    m_codec.MBcountV >>= 1;

    m_codec.YFramePitchSrc<<=1;
    m_codec.UVFramePitchSrc<<=1;
    m_codec.YFramePitchRef<<=1;
    m_codec.UVFramePitchRef<<=1;
    m_codec.YFramePitchRec<<=1;
    m_codec.UVFramePitchRec<<=1;

    for (ifield = 0; ifield < 2; ifield++) {
//      printf("\n ------- field %d\n", ifield);
      if (ifield == 1)
      {
        m_codec.picture_structure = (m_codec.picture_structure == MPEG2_BOTTOM_FIELD) ? MPEG2_TOP_FIELD : MPEG2_BOTTOM_FIELD;
      }
      m_codec.curr_field   = (m_codec.picture_structure == MPEG2_BOTTOM_FIELD) ? 1 :0;
      m_codec.second_field = ifield;
      if (m_codec.second_field && m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE) {
        m_codec.ipflag = 1;
        m_codec.picture_coding_type = UMC::MPEG2_P_PICTURE;
      }

      if (m_codec.picture_structure == MPEG2_TOP_FIELD) {
        m_codec.Y_src = pSrc[0];
        m_codec.U_src = pSrc[1];
        m_codec.V_src = pSrc[2];
        if (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE && ifield) {
          m_codec.YRefFrame[1][0] = m_codec.YRefFrame[1][1];
          m_codec.URefFrame[1][0] = m_codec.URefFrame[1][1];
          m_codec.VRefFrame[1][0] = m_codec.VRefFrame[1][1];
          m_codec.YRecFrame[1][0] = m_codec.YRecFrame[1][1];
          m_codec.URecFrame[1][0] = m_codec.URecFrame[1][1];
          m_codec.VRecFrame[1][0] = m_codec.VRecFrame[1][1];
        }
        m_codec.pMBInfo = pMBInfo0;
      } else {
        m_codec.Y_src = pSrc[0] + (m_codec.YFramePitchSrc >> 1);
        if (m_codec.encodeInfo.info.color_format == UMC::NV12) {
          m_codec.U_src = pSrc[1] + m_codec.UVFramePitchSrc;
          m_codec.V_src = pSrc[2] + m_codec.UVFramePitchSrc;
        } else {
          m_codec.U_src = pSrc[1] + (m_codec.UVFramePitchSrc >> 1);
          m_codec.V_src = pSrc[2] + (m_codec.UVFramePitchSrc >> 1);
        }
        if (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE && ifield) {
          m_codec.YRefFrame[0][0] = m_codec.YRefFrame[0][1];
          m_codec.URefFrame[0][0] = m_codec.URefFrame[0][1];
          m_codec.VRefFrame[0][0] = m_codec.VRefFrame[0][1];
          m_codec.YRecFrame[0][0] = m_codec.YRecFrame[0][1];
          m_codec.URecFrame[0][0] = m_codec.URecFrame[0][1];
          m_codec.VRecFrame[0][0] = m_codec.VRecFrame[0][1];
        }
        m_codec.pMBInfo = pMBInfo0 + (m_codec.YFrameSize/(2*16*16));
      }

 #ifdef _NEW_THREADING
        mfxStatus sts = MFX_ERR_NONE;
        sts = PutPicture(pInternalParams->Payload,pInternalParams->NumPayload, ifield == 0,ifield == 1);
        MFX_CHECK_STS(sts);
#else
        UMC::Status sts = UMC::UMC_OK;
        sts = m_codec.PutPicture();
        MFX_CHECK_UMC_STS(sts);
#endif

      // field_endpos = 8*m_codec.mEncodedSize;
      m_codec.ipflag = 0;
    }

    m_codec.Y_src = pSrc[0];
    m_codec.U_src = pSrc[1];
    m_codec.V_src = pSrc[2];

    // restore params
    m_codec.pMBInfo = pMBInfo0;

    m_codec.YFrameVSize <<= 1;
    m_codec.UVFrameVSize <<= 1;

    m_codec.YFramePitchSrc>>=1;
    m_codec.UVFramePitchSrc>>=1;
    m_codec.YFramePitchRef>>=1;
    m_codec.UVFramePitchRef>>=1;
    m_codec.YFramePitchRec>>=1;
    m_codec.UVFramePitchRec>>=1;

    m_codec.MBcountV <<= 1;
  }

#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
  if (!m_codec.bMEdone)
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME
    if (m_codec.encodeInfo.me_auto_range)
    { // adjust ME search range
      if (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE) {
        m_codec.AdjustSearchRange(0, 0);
      } else if (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE) {
        m_codec.AdjustSearchRange(m_codec.B_count, 0);
        m_codec.AdjustSearchRange(m_codec.B_count, 1);
      }
    }

    m_codec.encodeInfo.numEncodedFrames++;

    return MFX_ERR_NONE;
}
#ifdef _NEW_THREADING

static
mfxStatus MPEG2ENCODERoutine(void *pState, void *param, mfxU32 /*n*/, mfxU32 /*callNumber*/)
{
  mfxStatus sts = MFX_ERR_NONE;
  MFXVideoENCODEMPEG2* th = (MFXVideoENCODEMPEG2*)pState;
  sExtTask1 *pTask = (sExtTask1 *)param;

  mfxU32 nTask = 0;
  sts = th->m_pExtTasks->CheckTask(pTask);
  MFX_CHECK_STS(sts);

  if(VM_TIMEOUT != vm_event_timed_wait(&pTask->m_new_frame_event, 0))
  {
      sts = th->EncodeFrame(0, &pTask->m_inputInternalParams, pTask->m_pInput_surface, pTask->m_pBs);
      MFX_CHECK_STS(sts);
      return MFX_TASK_DONE;
  }
  else if ((sts = th->m_IntTasks.GetIntTask(nTask)) == MFX_ERR_NONE)
  {
     th->StartIntTask (nTask);
     return MFX_TASK_NEED_CONTINUE;
  }
  else if (sts == MFX_ERR_NOT_FOUND)
  {
    return MFX_TASK_BUSY;
  }
  return MFX_TASK_BUSY;
}


 mfxStatus MFXVideoENCODEMPEG2::EncodeFrameCheck(   mfxEncodeCtrl *ctrl,
                                                    mfxFrameSurface1 *surface,
                                                    mfxBitstream *bs,
                                                    mfxFrameSurface1 **reordered_surface,
                                                    mfxEncodeInternalParams *pInternalParams,
                                                    MFX_ENTRY_POINT *pEntryPoint)
 {
     mfxStatus sts_ret = MFX_ERR_NONE;
     mfxStatus sts = MFX_ERR_NONE;
     sExtTask1 *pTask = 0;


     MFX::AutoTimer timer("MPEG2Enc_check");

     // pointer to the task processing object
    pEntryPoint->pState = this;

    // pointer to the task processing routine
    pEntryPoint->pRoutine = MPEG2ENCODERoutine;
    // task name
    pEntryPoint->pRoutineName = "EncodeMPEG2";

    // number of simultaneously allowed threads for the task
    pEntryPoint->requiredNumThreads = m_IntTasks.m_NumTasks;
    mfxFrameSurface1 *pOriginalSurface = GetOriginalSurface(surface);
    if (pOriginalSurface != surface)
    {
        if (pOriginalSurface == 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        pOriginalSurface->Info = surface->Info;
        pOriginalSurface->Data.Corrupted = surface->Data.Corrupted;
        pOriginalSurface->Data.DataFlag = surface->Data.DataFlag;
        pOriginalSurface->Data.TimeStamp = surface->Data.TimeStamp;
        pOriginalSurface->Data.FrameOrder = surface->Data.FrameOrder;
    }


    sts_ret = EncodeFrameCheck(ctrl, pOriginalSurface, bs, reordered_surface, pInternalParams);

    if (sts_ret != MFX_ERR_NONE && sts_ret !=(mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK && sts_ret<0)
        return sts_ret;

    sts = m_pExtTasks->AddTask( pInternalParams,*reordered_surface, bs, &pTask);
    MFX_CHECK_STS(sts);


    // pointer to the task's parameter
    pEntryPoint->pParam = pTask;


    return sts_ret;
 }



mfxStatus MFXVideoENCODEMPEG2::PutPicture(mfxPayload **pPayloads, mfxU32 numPayloads, bool bEvenPayloads, bool bOddPayloads)
{

  Ipp32s isfield = 0;
  Ipp32s bitsize = 0;
  Ipp64f target_size = 0;
  Ipp32s wanted_size = 0;
  Ipp32s size = 0;
  UMC::Status status = UMC::UMC_OK;
  UMC::FrameType frType = UMC::I_PICTURE;

  Ipp32s prevq=0, oldq = 0;
  Ipp32s ntry = 0;
  Ipp32s recode = 0;

  bool  bIntraAdded = false;
  bool  bNotEnoughBuffer = false;

  m_codec.bQuantiserChanged = false;
  m_codec.bSceneChanged = false;
  m_codec.bExtendGOP = false;

  MFX::AutoTimer timer("MPEG2Enc_PutPicture");

  if (!m_codec.second_field || m_codec.ipflag)
    m_codec.nLimitedMBMode = 0;

  if ((m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE || m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE)&&!m_codec.second_field)
  {
     m_codec.m_bSkippedMode = 0;
  }
  else if (m_codec.m_bSkippedMode == 1 && m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)
  {
     m_codec.nLimitedMBMode = 3;
  }

  if(m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE) {
    m_codec.mp_f_code = 0;
    m_codec.m_GOP_Start_tmp = m_codec.m_GOP_Start;
    m_codec.m_GOP_Start = m_codec.encodeInfo.numEncodedFrames;
  } else {
    m_codec.mp_f_code = m_codec.pMotionData[m_codec.B_count].f_code;
  }

  if (m_codec.picture_structure == MPEG2_FRAME_PICTURE) {
    m_codec.curr_frame_pred = m_codec.curr_frame_dct = m_codec.encodeInfo.frame_pred_frame_dct[m_codec.picture_coding_type - 1];
  } else {
    m_codec.curr_frame_pred = !m_codec.encodeInfo.allow_prediction16x8;
    m_codec.curr_frame_dct = 1;
  }
  if (m_codec.progressive_frame)
  {
    m_codec.curr_frame_pred = 1;
    m_codec.curr_frame_dct    = 1;
  
  }
  if(m_codec.curr_frame_dct == 1)
    m_codec.curr_scan = m_codec.encodeInfo.altscan_tab[m_codec.picture_coding_type - 1] = (m_codec.picture_structure == MPEG2_FRAME_PICTURE)? 0:1;
  else
    m_codec.curr_scan = m_codec.encodeInfo.altscan_tab[m_codec.picture_coding_type - 1];

  UMC::MPEG2FrameType original_picture_coding_type = m_codec.picture_coding_type;
  m_codec.curr_intra_vlc_format = m_codec.encodeInfo.intraVLCFormat[m_codec.picture_coding_type - 1];
  for (ntry=0; ; ntry++) {
    // Save position after headers (only 0th thread)
    Ipp32s DisplayFrameNumber;
    status = UMC::UMC_OK;
    bNotEnoughBuffer = false;

    m_codec.PrepareBuffers();
    size = 0;

    // Mpeg2 sequence header
    try
    {
        if (m_codec.m_FirstFrame || (m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE))
        {
          if(m_codec.m_bSH || m_codec.m_FirstFrame)
          {
            mfxU8 *pBuff = 0;
            mfxU32 len = 0;
            pSHEx->GetSH(&pBuff,&len);
           
            if (!pBuff || len == 0)
            {
                m_codec.PutSequenceHeader();
                m_codec.PutSequenceExt();
                if (m_bAddDisplayExt)
                    m_codec.PutSequenceDisplayExt(); // no need
            }
            else
            {
                m_codec.PutSequenceHeader(pBuff,len);
                
            }

            // optionally output some text data
            m_codec.PutUserData(0);
          }

          if (original_picture_coding_type == UMC::MPEG2_I_PICTURE)
            m_codec.PutGOPHeader(m_codec.encodeInfo.numEncodedFrames);

          size = (32+7 + BITPOS(m_codec.threadSpec[0]))>>3;
        }
    }
    catch (MPEG2_Exceptions exception)
    {
        return ConvertStatusUmc2Mfx(exception.GetType());
    }


    if (m_codec.brc) {

      Ipp32s quant;
      Ipp64f hrdBufFullness;
      UMC::VideoBrcParams brcParams;


      frType = (m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE ? UMC::I_PICTURE : (m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE ? UMC::P_PICTURE : UMC::B_PICTURE));
      m_codec.brc->SetPictureFlags(frType, m_codec.picture_structure, m_codec.repeat_first_field, m_codec.top_field_first, m_codec.second_field);
      m_codec.brc->PreEncFrame(frType, recode);
      quant = m_codec.brc->GetQP(frType);
      m_codec.brc->GetHRDBufferFullness(&hrdBufFullness, recode);
      m_codec.brc->GetParams(&brcParams);

      if (UMC::BRC_VBR == brcParams.BRCMode)
        m_codec.vbv_delay = 0xffff; // TODO: check the possibility of VBR with vbv_delay != 0xffff
      if (m_codec.vbv_delay != 0xffff)
        m_codec.vbv_delay = (Ipp32s)((hrdBufFullness - size*8 - 32) * 90000.0 / brcParams.maxBitrate);

      if (m_bConstantQuant)
        m_codec.quantiser_scale_value = -1;
      m_codec.changeQuant(quant);
    } else
      m_codec.PictureRateControl(size*8+32);

    DisplayFrameNumber = m_codec.encodeInfo.numEncodedFrames;
    if (m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE)
      DisplayFrameNumber--;
    else if (!m_codec.m_FirstFrame) {
      DisplayFrameNumber += m_codec.P_distance-1;
    }
    m_codec.temporal_reference = DisplayFrameNumber - m_codec.m_GOP_Start;

    if(m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE) {
      if(m_codec.quantiser_scale_value < 5)
         m_codec.encodeInfo.intraVLCFormat[m_codec.picture_coding_type - UMC::MPEG2_I_PICTURE] = 1;
      else if(m_codec.quantiser_scale_value > 15)
        m_codec.encodeInfo.intraVLCFormat[m_codec.picture_coding_type - UMC::MPEG2_I_PICTURE] = 0;
      m_codec.curr_intra_vlc_format = m_codec.encodeInfo.intraVLCFormat[m_codec.picture_coding_type - UMC::MPEG2_I_PICTURE];
    }

    try
    {
        m_codec.PutPictureHeader();
        m_codec.PutPictureCodingExt();
        if (pPayloads !=0 && numPayloads != 0)
        {
            mfxU32 start = bEvenPayloads ? 0 : 1;
            mfxU32 step =  bEvenPayloads == bOddPayloads? 1 : 2;
            for (mfxU32 i = start; i < numPayloads; i += step)
            {
                if(pPayloads[i]!=0 && pPayloads[i]->Type == USER_START_CODE)
                {
                    m_codec.setUserData(pPayloads[i]->Data, (pPayloads[i]->NumBit+7)>>3);           
                    m_codec.PutUserData(2);
                }
            }           
        }
    }
    catch (MPEG2_Exceptions exception)
    {
        return ConvertStatusUmc2Mfx (exception.GetType());
    }

    GetBlockOffsetFrm (m_codec.block_offset_frm_src, m_codec.YFramePitchSrc);
    GetBlockOffsetFld (m_codec.block_offset_fld_src, m_codec.YFramePitchSrc);

    GetBlockOffsetFrm (m_codec.block_offset_frm_ref, m_codec.YFramePitchRef);
    GetBlockOffsetFld (m_codec.block_offset_fld_ref, m_codec.YFramePitchRef);

    GetBlockOffsetFrm (m_codec.block_offset_frm_rec, m_codec.YFramePitchRec);
    GetBlockOffsetFld (m_codec.block_offset_fld_rec, m_codec.YFramePitchRec);


#ifdef MPEG2_USE_DUAL_PRIME
    m_codec.dpflag=m_codec.encodeInfo.enable_Dual_prime && (m_codec.P_distance==1) && !m_codec.progressive_frame && !m_codec.curr_frame_pred && !m_codec.ipflag && !m_codec.bQuantiserChanged && m_codec.is_interlaced_queue;
#endif //MPEG2_USE_DUAL_PRIME
    //logi(encodeInfo.numThreads);       
    

   m_IntTasks.m_CurrTask = 0;
   for (mfxU32 i = 0; i < m_IntTasks.m_NumTasks; i++) {
        vm_event_signal(&m_IntTasks.m_TaskInfo[i].task_prepared_event);
   }
   m_core->INeedMoreThreadsInside(this); 


   Ipp8u* p = m_codec.out_pointer;
   size = 0;


   for (mfxU32 i = 0; i < m_IntTasks.m_NumTasks; i++)
   {
       if (VM_TIMEOUT != vm_event_timed_wait(&m_IntTasks.m_exit_event, 0))
       {
           return MFX_ERR_ABORTED;
       }
       while (VM_TIMEOUT == vm_event_timed_wait(&m_IntTasks.m_TaskInfo[i].task_ready_event, 0))
       {
           mfxU32 nTask;
           if (MFX_ERR_NONE == m_IntTasks.GetIntTask(nTask))
           {
                StartIntTask(nTask);
           }
           else
           {
               MFX::AutoTimer timer2;
               timer2.Start("MPEG2Enc_wait0");
               vm_time_sleep(0);
               timer2.Stop(0);
           }
       }
       if (i == 0)
       {
            if (UMC::UMC_OK != m_codec.threadSpec[i].sts)
            {
               status = m_codec.threadSpec[i].sts;
            }
            else
            {
                FLUSH_BITSTREAM(m_codec.threadSpec[0].bBuf.current_pointer, m_codec.threadSpec[0].bBuf.bit_offset);
                size += BITPOS(m_codec.threadSpec[0])>>3;
                p += size;
            }
       }
       else
       {
           if (UMC::UMC_OK != m_codec.threadSpec[i].sts){
               status = m_codec.threadSpec[i].sts;
            }
            if (status == UMC::UMC_OK && size < m_codec.output_buffer_size){
                MFX::AutoTimer timer1;
                timer1.Start("MPEG2Enc_CopyPic");
                Ipp32s t = 0;
                FLUSH_BITSTREAM(m_codec.threadSpec[i].bBuf.current_pointer, m_codec.threadSpec[i].bBuf.bit_offset);
                t = BITPOS(m_codec.threadSpec[i])>>3;
                size += t;
                if (size < m_codec.output_buffer_size)            {
                    memcpy_s(p, m_codec.output_buffer_size - size +t, m_codec.threadSpec[i].bBuf.start_pointer, t);
                    p += t;
                }
                timer1.Stop(0);
            }

       }

   }

    if (status == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || size >= m_codec.output_buffer_size - 8){

        bNotEnoughBuffer = true;
        size = (size <= m_codec.output_buffer_size) ? m_codec.output_buffer_size + 1 : size; // for bitrate
    }
    else if (status != UMC::UMC_OK) {
       return ConvertStatusUmc2Mfx(status);
    }

    m_IntTasks.m_CurrTask = 0;

    // flush buffer
    if (!bNotEnoughBuffer)
        status = m_codec.FlushBuffers(&size);

    isfield = (m_codec.picture_structure != MPEG2_FRAME_PICTURE);
    bitsize = size*8;

    if(!m_codec.brc)
    {
        if(m_codec.encodeInfo.rc_mode == RC_CBR) {
          target_size = m_codec.rc_tagsize[m_codec.picture_coding_type-UMC::MPEG2_I_PICTURE];
          wanted_size = (Ipp32s)(target_size - m_codec.rc_dev / 3 * target_size / m_codec.rc_tagsize[0]);
          wanted_size >>= isfield;
        } else {
          target_size = wanted_size = bitsize; // no estimations
        }
    }

    if(m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE) {
      if(bitsize > (m_codec.MBcount*128>>isfield))
        m_codec.encodeInfo.intraVLCFormat[0] = 1;
      else //if(bitsize < MBcount*192)
        m_codec.encodeInfo.intraVLCFormat[0] = 0;
    }
#ifdef SCENE_DETECTION
    m_codec.bSceneChanged = false;
    if ( m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE && m_codec.second_field == 0 && m_codec.bExtendGOP == false && m_codec.encodeInfo.strictGOP <2) { //
      Ipp32s numIntra = 0;
#ifndef UMC_RESTRICTED_CODE
      Ipp32s weightIntra = 0;
      Ipp32s weightInter = 0;
#endif // UMC_RESTRICTED_CODE
      Ipp32s t;
      for (t = 0; t < m_codec.encodeInfo.numThreads; t++) {
        numIntra += m_codec.threadSpec[t].numIntra;
#ifndef UMC_RESTRICTED_CODE
        weightIntra += m_codec.threadSpec[t].weightIntra;
        weightInter += m_codec.threadSpec[t].weightInter;
#endif // UMC_RESTRICTED_CODE
      }
      if(m_codec.picture_structure != MPEG2_FRAME_PICTURE)
        numIntra <<= 1; // MBcount for the frame picture

      if (numIntra*2 > m_codec.MBcount*1 // 2/3 Intra: I frame instead of P
#ifndef UMC_RESTRICTED_CODE
        // || weightIntra*4 < weightInter*3 // 1/2 length: I frame instead of P
#endif // UMC_RESTRICTED_CODE
         ) {
#ifndef UMC_RESTRICTED_CODE_CME
#ifdef M2_USE_CME
toI:
#endif // M2_USE_CME
#endif // UMC_RESTRICTED_CODE_CME
        m_codec.picture_coding_type = UMC::MPEG2_I_PICTURE;
//        frType = I_PICTURE;
        //curr_gop = 0;
        m_codec.GOP_count = 0;
        m_codec.bSceneChanged = true;
        m_codec.mp_f_code = 0;
        m_codec.m_GOP_Start_tmp = m_codec.m_GOP_Start;
        m_codec.m_GOP_Start = m_codec.encodeInfo.numEncodedFrames;
        ntry = -1;
        bIntraAdded = true;
        //logi(encodeInfo.numEncodedFrames);
        continue;
      }
    }
    // scene change not detected
#endif
    if (!m_codec.brc) {
      oldq = prevq;
      prevq = m_codec.quantiser_scale_value;
      m_codec.bQuantiserChanged = false;

      if (status == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || bitsize > m_codec.rc_vbv_max
        || (bitsize > wanted_size*2 && (ntry==0 || oldq<prevq))
        )
      {
        m_codec.changeQuant(m_codec.quantiser_scale_value + 1);
        if(prevq == m_codec.quantiser_scale_value) {
          if (m_codec.picture_coding_type == UMC::MPEG2_I_PICTURE &&
             !m_codec.m_FirstFrame && m_codec.encodeInfo.gopSize > 1 &&
             (status == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || bitsize > m_codec.rc_vbv_max) &&
             !m_codec.encodeInfo.strictGOP) {
            m_codec.picture_coding_type = UMC::MPEG2_P_PICTURE;
            m_codec.m_GOP_Start = m_codec.m_GOP_Start_tmp;
            m_codec.mp_f_code = m_codec.pMotionData[0].f_code;
            m_codec.bExtendGOP = true;
            m_codec.GOP_count -= m_codec.encodeInfo.IPDistance; // add PB..B group
            ntry = -1;
            continue;
          }
          if (m_codec.nLimitedMBMode > 2) {
            status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
          } else {
            //Ipp32s maxMBSize = (wanted_size/MBcount);
            m_codec.nLimitedMBMode ++;
            if ((m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE || m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE) && m_codec.nLimitedMBMode == 2)
            {
                m_codec.m_bSkippedMode = 1;
                m_codec.nLimitedMBMode = 3;
            }
            continue;
          }

        } else {
          m_codec.bQuantiserChanged = true;
          continue;
        }
      } else if (bitsize < m_codec.rc_vbv_min) {
        if(ntry==0 || oldq>prevq)
          m_codec.changeQuant(m_codec.quantiser_scale_value - 1);
        if(prevq == m_codec.quantiser_scale_value) {
          Ipp8u *p = m_codec.out_pointer + size;
          while(bitsize < m_codec.rc_vbv_min && size < m_codec.output_buffer_size - 4) {
            *(Ipp32u*)p = 0;
            p += 4;
            size += 4;
            bitsize += 32;
          }
          status = UMC::UMC_OK; // already minimum;
        } else {
          m_codec.bQuantiserChanged = true;
          continue;
        }
      }else if ( bNotEnoughBuffer){
        status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
      }
    }

    if (m_codec.brc) {
      UMC::BRCStatus hrdSts;
      Ipp32s framestoI;
      Ipp32s gopSize;
      if (m_codec.encodeInfo.numEncodedFrames >= m_codec.m_FirstGopSize) {
        gopSize = m_codec.encodeInfo.gopSize;
        framestoI = gopSize - ((m_codec.encodeInfo.numEncodedFrames - m_codec.m_FirstGopSize) % gopSize) - 1;
      } else {
        gopSize = m_codec.m_FirstGopSize;
        framestoI = gopSize - m_codec.encodeInfo.numEncodedFrames - 1;
      }
      m_codec.brc->SetQP(m_codec.quantiser_scale_value, frType); // pass actually used qp to BRC
//      brc->SetPictureFlags(frType, picture_structure, repeat_first_field, top_field_first, second_field);

      hrdSts = m_codec.brc->PostPackFrame(frType, size*8, 0, recode);
      /*  Debug  */
      Ipp32s maxSize = 0, minSize = 0;
      Ipp64f buffullness;
      Ipp32s buffullnessbyI;
      UMC::VideoBrcParams brcParams;
      m_codec.brc->GetHRDBufferFullness(&buffullness, 0);
      m_codec.brc->GetMinMaxFrameSize(&minSize, &maxSize);
      m_codec.brc->GetParams(&brcParams);
//      printf("\n frame # %d / %d hrdSts %d |||||||||| fullness %d bIntraAdded %d \n", encodeInfo.numEncodedFrames, GOP_count, hrdSts, (Ipp32s)buffullness, bIntraAdded);
      if (hrdSts == UMC::BRC_OK && !m_bConstantQuant) {
        Ipp32s inputbitsPerPic = m_codec.m_InputBitsPerFrame;
        Ipp32s minbitsPerPredPic, minbitsPerIPic;
        if (m_codec.picture_structure != MPEG2_FRAME_PICTURE) {
          minbitsPerPredPic = m_codec.m_MinFieldSizeBits[1];
          minbitsPerIPic = m_codec.m_MinFieldSizeBits[0];
          inputbitsPerPic >>= 1;
          framestoI *= 2;
          if (!m_codec.second_field)
            framestoI--;
        } else {
          minbitsPerPredPic = m_codec.m_MinFrameSizeBits[1];
          minbitsPerIPic = m_codec.m_MinFrameSizeBits[0];
        }
        buffullnessbyI = (Ipp32s)buffullness + framestoI * (inputbitsPerPic - minbitsPerPredPic); /// ??? +- 1, P/B?

        { //  I frame is coming
//          printf("\n buffullnessbyI %d \n", buffullnessbyI);
          if (buffullnessbyI < minbitsPerIPic ||
            (m_codec.picture_structure != MPEG2_FRAME_PICTURE && !m_codec.second_field && size*8*2 > inputbitsPerPic + maxSize && m_codec.picture_coding_type != UMC::MPEG2_I_PICTURE)) {
            if (!m_codec.nLimitedMBMode) {
              Ipp32s quant = m_codec.quantiser_scale_value; // brc->GetQP(frType);
              m_codec.changeQuant(quant+2);
              recode = UMC::BRC_RECODE_EXT_QP;
              if (m_codec.quantiser_scale_value == quant) // ??
              {
                m_codec.nLimitedMBMode++;
                if ((m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE || m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE) && m_codec.nLimitedMBMode == 2)
                {
                    m_codec.m_bSkippedMode = 1;
                    m_codec.nLimitedMBMode = 3;
                }
                recode = UMC::BRC_RECODE_EXT_PANIC;
//                printf ("\n Recalculate 0: Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);


              }
              m_codec.brc->SetQP(m_codec.quantiser_scale_value, frType);
              m_codec.bQuantiserChanged = true; // ???
              continue;
            } else {
               if (frType == UMC::I_PICTURE && !m_codec.m_FirstFrame && m_codec.encodeInfo.gopSize > 1 && (!m_codec.encodeInfo.strictGOP || bIntraAdded)) {
                frType = UMC::P_PICTURE;
                m_codec.picture_coding_type = UMC::MPEG2_P_PICTURE;
                m_codec.m_GOP_Start = m_codec.m_GOP_Start_tmp;
                m_codec.mp_f_code = m_codec.pMotionData[0].f_code;
                m_codec.bExtendGOP = true;
                //curr_gop += encodeInfo.IPDistance;
                m_codec.GOP_count -= m_codec.encodeInfo.IPDistance; // add PB..B group
                ntry = -1;
                recode = UMC::BRC_RECODE_EXT_QP;
                continue;
              } else if (m_codec.nLimitedMBMode < 3) {
                recode = UMC::BRC_RECODE_EXT_PANIC;
                m_codec.nLimitedMBMode++;
                if ((m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE || m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE) && m_codec.nLimitedMBMode == 2)
                {
                    m_codec.m_bSkippedMode = 1;
                    m_codec.nLimitedMBMode = 3;
                }
//                printf ("\n Recalculate 1: Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);
                continue;
              }
            } //  else if (maxSize < m_MinFrameSizeBits[0]) - can return with error already
          }
        }
      }


      /* ------ */
      if (UMC::BRC_OK != hrdSts) {
        if (!(hrdSts & UMC::BRC_NOT_ENOUGH_BUFFER)) {
          recode = UMC::BRC_RECODE_QP;
          m_codec.bQuantiserChanged = true;
          continue;
        } else {
          if (hrdSts & UMC::BRC_ERR_SMALL_FRAME) {
            Ipp32s maxSize, minSize;
            Ipp8u *p = m_codec.out_pointer + size;
            m_codec.brc->GetMinMaxFrameSize(&minSize, &maxSize);
            Ipp32s nBytes = (minSize - bitsize + 7) >> 3;
            if (nBytes < 0) 
              nBytes = 0; // just in case
            if (size + nBytes <= m_codec.output_buffer_size) {
              memset(p,0,nBytes);
              size    += nBytes;
              bitsize += (nBytes*8);
              p += nBytes;
              m_codec.brc->PostPackFrame(frType, bitsize, 0, recode);
              recode = UMC::BRC_RECODE_NONE;
            } else
              status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
//            status = UMC_OK;
          } else {
             if (frType == UMC::I_PICTURE && !m_codec.m_FirstFrame && m_codec.encodeInfo.gopSize > 1 && (!m_codec.encodeInfo.strictGOP || bIntraAdded)) {
              frType = UMC::P_PICTURE; // ???
              m_codec.picture_coding_type = UMC::MPEG2_P_PICTURE;
              m_codec.m_GOP_Start = m_codec.m_GOP_Start_tmp;
              m_codec.mp_f_code = m_codec.pMotionData[0].f_code;
              m_codec.bExtendGOP = true;
              //curr_gop += encodeInfo.IPDistance;
              m_codec.GOP_count -= m_codec.encodeInfo.IPDistance; // add PB..B group
              ntry = -1;
              recode = UMC::BRC_RECODE_QP;
              if (bIntraAdded)
              {
                 m_codec.nLimitedMBMode = 3;
                 m_codec.m_bSkippedMode = 1;
                 recode = UMC::BRC_RECODE_PANIC;
              }
              continue;
            } else {
              if (m_codec.nLimitedMBMode>2){
                status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
//                printf ("\nError... Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);
              }
              else {
               m_codec.nLimitedMBMode ++;
               recode = UMC::BRC_RECODE_PANIC; // ???
                if ((m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE || m_codec.picture_coding_type == UMC::MPEG2_B_PICTURE) && m_codec.nLimitedMBMode == 2)
                {
                    m_codec.m_bSkippedMode = 1;
                    m_codec.nLimitedMBMode = 3;
                }
//                printf ("\n Recalculate 2: Frame %d. Limited mode: %d, max size = %d (current %d), max_size on MB = %d (current %d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);


               continue;
              }
            }
          }
        }
      } else{
        if (bNotEnoughBuffer)
        {
           status = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
        }
        recode = 0;
//        printf ("\nOK... Frame %d. Limited mode: %d, max size = %d (%d), max_size on MB = %d (%d)\n",frType,nLimitedMBMode,maxSize,bitsize,maxSize/MBcount,bitsize/MBcount);
      }
    }//BRC

    // if (!bQuantiserChanged && !bSceneChanged)
    m_codec.m_FirstFrame = 0;
    m_codec.qscale[m_codec.picture_coding_type-UMC::MPEG2_I_PICTURE] = m_codec.quantiser_scale_value;
    m_codec.out_pointer += size;
    m_codec.output_buffer_size -= size;
    m_codec.mEncodedSize += size;
    if (!m_codec.brc)
      m_codec.PostPictureRateControl(size*8);
#ifdef MPEG2_USE_DUAL_PRIME
    if (m_codec.bQuantiserChanged != true && m_codec.picture_coding_type == UMC::MPEG2_P_PICTURE && !m_codec.ipflag &&
        m_codec.encodeInfo.enable_Dual_prime && (m_codec.P_distance==1) && !m_codec.progressive_frame)
        m_codec.IsInterlacedQueue();
#endif //MPEG2_USE_DUAL_PRIME
    return ConvertStatusUmc2Mfx(status);

#ifndef UMC_RESTRICTED_CODE
    /*
    printf("fr: %d, quantizer changed from %d to %d; tag:%d want:%d sz:%d\n", encodeInfo.numEncodedFrames, prevq, quantiser_scale_value,
      (Ipp32s)target_size, wanted_size, bitsize);
    printf("picture_coding_type = %d\n", picture_coding_type);
    */
#endif // UMC_RESTRICTED_CODE
    }
}

mfxStatus MFXVideoENCODEMPEG2::CreateTaskInfo()
{
  mfxU32 num_tasks =  1;
  Ipp32s mb_shift = (m_codec.encodeInfo.info.interlace_type == UMC::PROGRESSIVE) ? 4 : 5;

  if (m_codec.encodeInfo.numThreads != 1) {
    if (m_codec.encodeInfo.numThreads == 0)
      m_codec.encodeInfo.numThreads = vm_sys_info_get_cpu_num();
    if (m_codec.encodeInfo.numThreads < 1) {
      m_codec.encodeInfo.numThreads = 1;
    }
  }
  if (m_codec.encodeInfo.numThreads != 1)
  {
    if (m_codec.MBcountH > 45)
    {
        num_tasks = (m_codec.encodeInfo.numThreads >= m_codec.MBcountV)? m_codec.MBcountV: m_codec.MBcountV/2;
        if (m_codec.encodeInfo.info.interlace_type != UMC::PROGRESSIVE) {
            num_tasks= (2*m_codec.encodeInfo.numThreads >= m_codec.MBcountV) ? m_codec.MBcountV/2 : m_codec.MBcountV/4;
        }
    }
    else
    {
        num_tasks = (2*m_codec.encodeInfo.numThreads >= m_codec.MBcountV)? m_codec.MBcountV/2 : m_codec.MBcountV/4 ;
        if (m_codec.encodeInfo.info.interlace_type != UMC::PROGRESSIVE) {
            num_tasks= (2*m_codec.encodeInfo.numThreads >= m_codec.MBcountV) ? m_codec.MBcountV/2 : ((4*m_codec.encodeInfo.numThreads > m_codec.MBcountV) ? m_codec.MBcountV/4: m_codec.MBcountV/8);
        }  
    }
  }
  num_tasks = num_tasks!=0 ? num_tasks : 1;

  if (m_IntTasks.m_TaskSpec == 0 && m_IntTasks.m_TaskInfo == 0)
  {
       m_IntTasks.m_NumTasks = num_tasks;
       m_IntTasks.m_TaskSpec = new UMC::MPEG2VideoEncoderBase::threadSpecificData [m_IntTasks.m_NumTasks];
       m_IntTasks.m_TaskInfo = new sIntTaskInfo [m_IntTasks.m_NumTasks];

       for (mfxU32 i = 0; i < m_IntTasks.m_NumTasks; i++)
       {
            if (i==0)
            {
                m_IntTasks.m_TaskSpec[i].pDst = 0;
                m_IntTasks.m_TaskSpec[i].dstSz = 0;
            }
            else
            {
                m_IntTasks.m_TaskSpec[i].dstSz = ((m_codec.MBcountV + num_tasks - 1)/num_tasks)*m_codec.MBcountH*64*6*3;
                m_IntTasks.m_TaskSpec[i].pDst  = MP2_ALLOC(Ipp8u, m_IntTasks.m_TaskSpec[i].dstSz);                
            }
            m_IntTasks.m_TaskSpec[i].pDiff   = MP2_ALLOC(Ipp16s, 3*256);
            m_IntTasks.m_TaskSpec[i].pDiff1  = MP2_ALLOC(Ipp16s, 3*256);
            m_IntTasks.m_TaskSpec[i].pMBlock = MP2_ALLOC(Ipp16s, 3*256);
            m_IntTasks.m_TaskSpec[i].me_matrix_size = 0;
            m_IntTasks.m_TaskSpec[i].me_matrix_buff = NULL;
            m_IntTasks.m_TaskSpec[i].me_matrix_id   = 0;
            m_IntTasks.m_TaskSpec[i].start_row = ((16*m_codec.MBcountV>>mb_shift)*i/m_IntTasks.m_NumTasks) << mb_shift;

#ifdef MPEG2_USE_DUAL_PRIME
            m_IntTasks.m_TaskSpec[i].fld_vec_count  = 0;
#endif //MPEG2_USE_DUAL_PRIME

           m_IntTasks.m_TaskInfo[i].numTask   = i;
           m_IntTasks.m_TaskInfo[i].m_lpOwner = this;


           vm_event_set_invalid(&m_IntTasks.m_TaskInfo[i].task_prepared_event);
            if (VM_OK != vm_event_init(&m_IntTasks.m_TaskInfo[i].task_prepared_event, 0, 0))
              return MFX_ERR_MEMORY_ALLOC;

           vm_event_set_invalid(&m_IntTasks.m_TaskInfo[i].task_ready_event);
            if (VM_OK != vm_event_init(&m_IntTasks.m_TaskInfo[i].task_ready_event, 0, 0))
              return MFX_ERR_MEMORY_ALLOC;
        }
  }
  else 
  {
      MFX_CHECK_NULL_PTR1(m_IntTasks.m_TaskSpec);

      for (mfxU32 i = 0; i < m_IntTasks.m_NumTasks; i++)
      {
            m_IntTasks.m_TaskSpec[i].start_row = ((16*m_codec.MBcountV>>mb_shift)*i/m_IntTasks.m_NumTasks) << mb_shift;
      }
  }

  if (m_IntTasks.m_NumTasks < num_tasks)
  {
     return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  m_codec.encodeInfo.numThreads = m_IntTasks.m_NumTasks;
  m_codec.threadSpec = m_IntTasks.m_TaskSpec;

  if (!vm_event_is_valid(&m_IntTasks.m_exit_event))
    vm_event_init (&m_IntTasks.m_exit_event,0,0);
  vm_event_reset (&m_IntTasks.m_exit_event);


  if (!vm_mutex_is_valid(&m_IntTasks.m_mGuard))
    vm_mutex_init(&m_IntTasks.m_mGuard);

  if (!m_pExtTasks)
  {
    m_pExtTasks = new clExtTasks1;
  }

  return MFX_ERR_NONE;
}

void MFXVideoENCODEMPEG2::DeleteTaskInfo()
{
    m_codec.encodeInfo.numThreads = 0;
    m_codec.threadSpec = 0;

    if ( m_IntTasks.m_TaskSpec)
    {
        for (mfxU32 i=0; i< m_IntTasks.m_NumTasks; i++)
        {
            if (i > 0)
            {
                MP2_FREE (m_IntTasks.m_TaskSpec[i].pDst);
            }
            MP2_FREE (m_IntTasks.m_TaskSpec[i].pDiff);
            MP2_FREE (m_IntTasks.m_TaskSpec[i].pDiff1);
            MP2_FREE (m_IntTasks.m_TaskSpec[i].pMBlock);
            if (m_IntTasks.m_TaskSpec[i].me_matrix_buff)
            {
                MP2_FREE(m_IntTasks.m_TaskSpec[i].me_matrix_buff);
            }
        }    
        delete [] m_IntTasks.m_TaskSpec;
        m_IntTasks.m_TaskSpec = 0;

    }
    if (m_IntTasks.m_TaskInfo)
    {
        for (mfxU32 i=0; i< m_IntTasks.m_NumTasks; i++)
        {
            if (vm_event_is_valid(&m_IntTasks.m_TaskInfo[i].task_prepared_event)) 
            {
                vm_event_destroy(&m_IntTasks.m_TaskInfo[i].task_prepared_event);
                vm_event_set_invalid(&m_IntTasks.m_TaskInfo[i].task_prepared_event);
            }
            if (vm_event_is_valid(&m_IntTasks.m_TaskInfo[i].task_ready_event)) 
            {
                vm_event_destroy(&m_IntTasks.m_TaskInfo[i].task_ready_event);
                vm_event_set_invalid(&m_IntTasks.m_TaskInfo[i].task_ready_event);
            }
        }
        delete [] m_IntTasks.m_TaskInfo;
        m_IntTasks.m_TaskInfo = 0;
    }
    if (m_pExtTasks)
    {
        delete m_pExtTasks;
        m_pExtTasks = 0;
    }
    m_IntTasks.m_NumTasks = 0;
    if (vm_mutex_is_valid(&m_IntTasks.m_mGuard))
    {
        vm_mutex_destroy(&m_IntTasks.m_mGuard);
        vm_mutex_set_invalid(&m_IntTasks.m_mGuard);
    }
    if (vm_event_is_valid(&m_IntTasks.m_exit_event))
    {
        vm_event_destroy(&m_IntTasks.m_exit_event);
        vm_event_set_invalid(&m_IntTasks.m_exit_event);
    }
}

void MFXVideoENCODEMPEG2::ConfigPolarSurface()
{
    frames.bCopyInputFrames = true;
    frames.buf_aux = &m_pFramesStore[2];

#ifdef ME_REF_ORIGINAL

    for (mfxU32 i=0; i<2; i++)
    {
        frames.buf_org[i] = &m_pFramesStore[i+3];
    }
#endif
}

#endif // _NEW_THREADING
#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE
