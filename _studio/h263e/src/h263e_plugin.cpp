//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "h263e_plugin.h"
#include "mfx_plugin_module.h"
#include "mfxvideo++int.h"

#include "umc_media_data.h"
#include "umc_video_data.h"

#if 1
  #define DBG_ENTER
  #define DBG_LEAVE
  #define DBG_LEAVE_STS(sts)
  #define DBG_STS(sts)
  #define DBG_VAL_I(val)
#else
  #define DBG_ENTER \
    { printf("h263e: %s:%d: %s: +\n", __FILE__, __LINE__, __FUNCTION__); fflush(NULL); }
  #define DBG_LEAVE \
    { printf("h263e: %s:%d: %s: -\n", __FILE__, __LINE__, __FUNCTION__); fflush(NULL); }
  #define DBG_LEAVE_STS(sts) \
    { printf("h263e: %s:%d: %s: %s=%d: -\n", __FILE__, __LINE__, __FUNCTION__, #sts, sts); fflush(NULL); }
  #define DBG_STS(sts) \
    { printf("h263e: %s:%d: %s: %s=%d\n", __FILE__, __LINE__, __FUNCTION__, #sts, sts); fflush(NULL); }
  #define DBG_VAL_I(val) \
    { printf("h263e: %s:%d: %s: %s=%d\n", __FILE__, __LINE__, __FUNCTION__, #val, val); fflush(NULL); }
#endif

#define GET_DEFAULT_BUFFER_SIZE_IN_KB(w, h) \
  (3 * (w) * (h) / 2 + 999) / 1000;

PluginModuleTemplate g_PluginModule = {
  NULL,
  &MFX_H263E_Plugin::Create,
  NULL,
  NULL,
  &MFX_H263E_Plugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXEncoderPlugin*) mfxCreateEncoderPlugin() {
  if (!g_PluginModule.CreateEncoderPlugin) {
    return 0;
  }
  return g_PluginModule.CreateEncoderPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
  if (!g_PluginModule.CreatePlugin) {
    return 0;
  }
  return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}

const mfxPluginUID MFX_H263E_Plugin::g_PluginGuid = MFX_H263ENC::MFX_PLUGINID_H263E_HW;

MFX_H263E_Plugin::MFX_H263E_Plugin(bool CreateByDispatcher)
    :m_adapter(0)
{
  m_session = 0;
  m_pmfxCore = NULL;
  memset(&m_PluginParam, 0, sizeof(m_PluginParam));

  m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
  m_PluginParam.MaxThreadNum = 1;
  m_PluginParam.APIVersion.Major = 1;//MFX_VERSION_MAJOR;
  m_PluginParam.APIVersion.Minor = 8;//MFX_VERSION_MINOR;
  m_PluginParam.PluginUID = g_PluginGuid;
  m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
  m_PluginParam.CodecId = MFX_H263ENC::MFX_CODEC_H263;
  m_PluginParam.PluginVersion = 1;
  m_createdByDispatcher = CreateByDispatcher;

  memset(&m_mfxpar, 0, sizeof(m_mfxpar));
  memset(&m_dst, 0, sizeof(m_dst));

  m_frame_order = 0;
}

mfxStatus MFX_H263E_Plugin::PluginInit(mfxCoreInterface * pCore)
{
  DBG_ENTER;
  if (!pCore)
    return MFX_ERR_NULL_PTR;

  m_pmfxCore = pCore;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::PluginClose()
{
  DBG_ENTER;
  if (m_createdByDispatcher) {
    delete this;
  }

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::GetPluginParam(mfxPluginParam *par)
{
  DBG_ENTER;
  if (!par) {
    return MFX_ERR_NULL_PTR;
  }
  *par = m_PluginParam;

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

void copy_nv12_to_i420(mfxFrameSurface1& src, mfxFrameSurface1& dst)
{
    mfxU32 srcPitch = src.Data.PitchLow + ((mfxU32)src.Data.PitchHigh << 16);
    mfxU32 dstPitch = dst.Data.PitchLow + ((mfxU32)dst.Data.PitchHigh << 16);

    mfxU32 roiWidth = src.Info.Width;
    mfxU32 roiHeight = src.Info.Height;

    // copy luma
    mfxU8 * srcLine = src.Data.Y;
    mfxU8 * dstLine = dst.Data.Y;
    for (mfxU32 line = 0; line < roiHeight; line ++)
    {
        memcpy(dstLine, srcLine, roiWidth);
        srcLine += srcPitch;
        dstLine += dstPitch;
    }

    // copy chroma
    mfxU8 * dstU = dst.Data.U;
    mfxU8 * dstV = dst.Data.V;

    srcLine = src.Data.UV;
    roiHeight >>= 1;
    roiWidth >>= 1;
    dstPitch >>= 1;

    for (mfxU32 line = 0; line < roiHeight; line ++) {
        for (mfxU32 pixel = 0; pixel < roiWidth; pixel ++) {
            mfxU32 srcUVPosition = pixel << 1;
            dstU[pixel] = srcLine[srcUVPosition];
            dstV[pixel] = srcLine[srcUVPosition + 1];
        }
        srcLine += srcPitch;
        dstU += dstPitch;
        dstV += dstPitch;
    }
}

mfxStatus MFX_H263E_Plugin::EncodeFrameSubmit(
  mfxEncodeCtrl *ctrl,
  mfxFrameSurface1 *surface,
  mfxBitstream *bitstream,
  mfxThreadTask *task)
{
  DBG_ENTER;
  mfxStatus sts = MFX_ERR_NONE;
  UMC::Status umc_sts = UMC::UMC_OK;

  UMC::VideoData in;
  UMC::MediaData out;
  mfxFrameSurface1 src;

  if (surface) {
    if (surface->Info.FourCC == MFX_FOURCC_NV12) {
      if ((!surface->Data.Y || !surface->Data.UV) && !surface->Data.MemId) {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        DBG_LEAVE_STS(sts);
        return sts;
      }
    } else if (surface->Info.FourCC == MFX_FOURCC_YV12) {
      if ((!surface->Data.Y || !surface->Data.U || !surface->Data.V) && !surface->Data.MemId) {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        DBG_LEAVE_STS(sts);
        return sts;
      }
    } else {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        DBG_LEAVE_STS(sts);
        return sts;
    }

    memcpy(&src, surface, sizeof(src));
    if (surface->Data.MemId) {
      sts = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface->Data.MemId, &src.Data);
      if (sts != MFX_ERR_NONE) {
        DBG_LEAVE_STS(sts);
        return sts;
      }
    }

    mfxFrameData& data = (surface->Info.FourCC == MFX_FOURCC_YV12)? src.Data: m_dst.Data;

    in.Init(m_umc_params.m_Param.Width, m_umc_params.m_Param.Height, UMC::YUV420, 8);

    in.SetPlanePointer(data.Y + surface->Info.CropY * surface->Data.Pitch + surface->Info.CropX, 0);
    in.SetPlanePointer(data.U + surface->Info.CropY * surface->Data.Pitch/2 + surface->Info.CropX/2, 1);
    in.SetPlanePointer(data.V + surface->Info.CropY * surface->Data.Pitch/2 + surface->Info.CropX/2, 2);

    in.SetPlanePitch(data.Pitch, 0);
    in.SetPlanePitch(data.Pitch/2, 1);
    in.SetPlanePitch(data.Pitch/2, 2);

    in.SetTime(m_frame_order/m_umc_params.info.framerate);

    if (surface->Info.FourCC == MFX_FOURCC_NV12) {
      copy_nv12_to_i420(src, m_dst);
    }
  }

  if ((bitstream->MaxLength - bitstream->DataLength) < m_video.mfx.BufferSizeInKB) {
    sts = MFX_ERR_NOT_ENOUGH_BUFFER;
  }
  if (sts == MFX_ERR_NONE) {
    out.SetBufferPointer(bitstream->Data + bitstream->DataOffset + bitstream->DataLength,
                         bitstream->MaxLength - bitstream->DataLength);

    umc_sts = m_umc_encoder.GetFrame((surface)? &in: NULL, &out);
    DBG_STS(umc_sts);

    if (umc_sts == UMC::UMC_ERR_NOT_ENOUGH_DATA) {
      sts = MFX_ERR_MORE_DATA;
    } else if (umc_sts != UMC::UMC_OK) {
      sts = MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (sts == MFX_ERR_NONE) {
      ++m_frame_order;

      DBG_VAL_I(bitstream->DataLength);
      bitstream->DataLength += (mfxU32)out.GetDataSize();
      DBG_VAL_I(bitstream->DataLength);

      bitstream->TimeStamp = GetMfxTimeStamp(out.GetTime());

      *task = (mfxThreadTask*)this;
    }
  }
  if (surface && surface->Data.MemId) {
    mfxStatus _sts = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface->Data.MemId, &src.Data);
    if (sts == MFX_ERR_NONE) sts = _sts;
  }

  DBG_LEAVE_STS(sts);
  return sts;
}

mfxStatus MFX_H263E_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
  DBG_ENTER;
  MFX_H263E_Plugin *self = (MFX_H263E_Plugin*)task;

  if (self != this) {
    DBG_LEAVE_STS(MFX_ERR_UNKNOWN);
    return MFX_ERR_UNKNOWN;
  }

  DBG_LEAVE_STS(MFX_TASK_DONE);
  return MFX_TASK_DONE;
}

mfxStatus MFX_H263E_Plugin::FreeResources(mfxThreadTask /*task*/, mfxStatus )
{
  DBG_ENTER;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
  DBG_ENTER;
  MFX_CHECK_NULL_PTR1(out);

  memcpy(&(out->mfx), &(in->mfx), sizeof(mfxInfoMFX));

  out->mfx.BufferSizeInKB = GET_DEFAULT_BUFFER_SIZE_IN_KB(in->mfx.FrameInfo.CropW, in->mfx.FrameInfo.CropH);
  out->AsyncDepth = in->AsyncDepth;
  out->IOPattern = in->IOPattern;

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
  DBG_ENTER;
  MFX_CHECK_NULL_PTR2(par,in);

  in->Type = mfxU16((par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)? MFX_H263ENC::MFX_MEMTYPE_SYS_EXT: MFX_H263ENC::MFX_MEMTYPE_D3D_EXT) ;

  in->NumFrameMin =  (par->AsyncDepth ? par->AsyncDepth: 1)  + 1; // default AsyncDepth is 1
  in->NumFrameSuggested = in->NumFrameMin + 1;

  in->Info = par->mfx.FrameInfo;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::Init(mfxVideoParam *par)
{
  DBG_ENTER;
  mfxStatus sts  = MFX_ERR_NONE;

  m_frame_order = 0;
  m_video = *par;

  m_umc_params.m_Param.Width = par->mfx.FrameInfo.CropW;
  m_umc_params.m_Param.Height = par->mfx.FrameInfo.CropH;
  m_umc_params.info.clip_info.width = m_umc_params.m_Param.Width;
  m_umc_params.info.clip_info.height = m_umc_params.m_Param.Height;
  m_umc_params.info.framerate = par->mfx.FrameInfo.FrameRateExtN/par->mfx.FrameInfo.FrameRateExtD;
  m_umc_params.info.bitrate = par->mfx.TargetKbps * 1000;
  if (par->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
    m_umc_params.info.bitrate = 0;
    m_umc_params.m_Param.quantIPic = par->mfx.QPI;
    m_umc_params.m_Param.quantPPic = par->mfx.QPP;
    m_umc_params.m_Param.quantBPic = par->mfx.QPB;
  }
  if (par->mfx.GopPicSize) {
    m_umc_params.m_Param.IPicdist = par->mfx.GopPicSize;
  }
  if (par->mfx.GopRefDist) {
    m_umc_params.m_Param.PPicdist = par->mfx.GopRefDist;
  }

  m_video.mfx.BufferSizeInKB = GET_DEFAULT_BUFFER_SIZE_IN_KB(m_video.mfx.FrameInfo.CropW, m_video.mfx.FrameInfo.CropH);

  DBG_VAL_I(m_video.mfx.FrameInfo.CropX);
  DBG_VAL_I(m_video.mfx.FrameInfo.CropY);
  DBG_VAL_I(m_video.mfx.FrameInfo.CropW);
  DBG_VAL_I(m_video.mfx.FrameInfo.CropH);
  DBG_VAL_I(m_video.mfx.FrameInfo.Width);
  DBG_VAL_I(m_video.mfx.FrameInfo.Height);
  DBG_VAL_I(m_video.mfx.FrameInfo.FrameRateExtN);
  DBG_VAL_I(m_video.mfx.FrameInfo.FrameRateExtD);

  if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12) {
    mfxU8* y = (mfxU8*)malloc(3*m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height/2);
    if (!y) {
      return MFX_ERR_MEMORY_ALLOC;
    }
    memcpy(&m_dst.Info, &m_video.mfx.FrameInfo, sizeof(mfxFrameInfo));
    m_dst.Info.FourCC = MFX_FOURCC_YV12;
    m_dst.Data.Y = y;
    m_dst.Data.U = y + m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height;
    m_dst.Data.V = y + m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height*5/4;
    m_dst.Data.PitchLow = m_video.mfx.FrameInfo.Width;
    m_dst.Data.PitchHigh = 0;
  } else if (m_video.mfx.FrameInfo.FourCC != MFX_FOURCC_YV12) {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  UMC::Status umc_sts = m_umc_encoder.Init(&m_umc_params);
  DBG_STS(umc_sts);

  if (umc_sts != UMC::UMC_OK) {
    sts = MFX_ERR_INVALID_VIDEO_PARAM;
  }

  DBG_LEAVE_STS(sts);
  return sts;
}

mfxStatus MFX_H263E_Plugin::Reset(mfxVideoParam *par)
{
  DBG_ENTER;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::Close()
{
  DBG_ENTER;

  if (m_dst.Data.Y) {
    free(m_dst.Data.Y);
  }
  memset(&m_dst, 0, sizeof(m_dst));

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263E_Plugin::GetVideoParam(mfxVideoParam *par)
{
  DBG_ENTER;
  MFX_CHECK_NULL_PTR1(par);

  memcpy(&(par->mfx), &(m_video.mfx), sizeof(mfxInfoMFX));
  par->AsyncDepth = m_video.AsyncDepth;
  par->IOPattern = m_video.IOPattern;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
BOOL APIENTRY DllMain(HMODULE /*hModule*/,
                      DWORD  ul_reason_for_call,
                      LPVOID /*lpReserved*/)
{
  // initialize the IPP library
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
    ippInit();
    break;
  default:
    break;
  }
  return TRUE;
}
#else
void __attribute__ ((constructor)) dll_init(void)
{
  ippInit();
}
#endif
