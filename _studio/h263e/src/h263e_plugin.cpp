/**********************************************************************************

Copyright (C) 2014 Intel Corporation.  All rights reserved.

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

File Name: h263e_plugin.cpp

***********************************************************************************/

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
  memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

  m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
  m_PluginParam.MaxThreadNum = 1;
  m_PluginParam.APIVersion.Major = 1;//MFX_VERSION_MAJOR;
  m_PluginParam.APIVersion.Minor = 8;//MFX_VERSION_MINOR;
  m_PluginParam.PluginUID = g_PluginGuid;
  m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
  m_PluginParam.CodecId = MFX_H263ENC::MFX_CODEC_H263;
  m_PluginParam.PluginVersion = 1;
  m_createdByDispatcher = CreateByDispatcher;

  memset(&m_mfxpar, 0, sizeof(mfxVideoParam));

  m_frame_order = 0;
  inframe = NULL;
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

  if (surface) {
    if (surface->Info.FourCC == MFX_FOURCC_NV12) {
      if (!surface->Data.Y || !surface->Data.UV) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
      }
    } else if (surface->Info.FourCC == MFX_FOURCC_YV12) {
      if (!surface->Data.Y || !surface->Data.U || !surface->Data.V) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
      }
    } else {
      return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    in.Init(m_umc_params.m_Param.Width, m_umc_params.m_Param.Height, UMC::YUV420, 8);
    if (surface->Info.FourCC == MFX_FOURCC_NV12) {
      mfxU8* y = (mfxU8*)inframe;
      mfxU8* u = (mfxU8*)inframe + m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height;
      mfxU8* v = (mfxU8*)inframe + m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height*5/4;

      in.SetPlanePointer(y, 0);
      in.SetPlanePointer(u, 1);
      in.SetPlanePointer(v, 2);

      in.SetPlanePitch(surface->Data.Pitch, 0);
      in.SetPlanePitch(surface->Data.Pitch/2, 1);
      in.SetPlanePitch(surface->Data.Pitch/2, 2);

      mfxFrameSurface1 dst;
      dst.Data.Y = y;
      dst.Data.U = u;
      dst.Data.V = v;
      dst.Data.Pitch = m_video.mfx.FrameInfo.Width;

      copy_nv12_to_i420(*surface, dst);

    } else if (surface->Info.FourCC == MFX_FOURCC_YV12) {
      in.SetPlanePointer(surface->Data.Y, 0);
      in.SetPlanePointer(surface->Data.U, 1);
      in.SetPlanePointer(surface->Data.V, 2);

      in.SetPlanePitch(surface->Data.Pitch, 0);
      in.SetPlanePitch(surface->Data.Pitch/2, 1);
      in.SetPlanePitch(surface->Data.Pitch/2, 2);
    }
    in.SetTime(m_frame_order/m_umc_params.info.framerate);
  }

  if ((bitstream->MaxLength - bitstream->DataLength) < m_video.mfx.BufferSizeInKB) {
    return MFX_ERR_NOT_ENOUGH_BUFFER;
  }
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

    DBG_VAL_I(bitstream->DataLength)
    bitstream->DataLength += out.GetDataSize();
    DBG_VAL_I(bitstream->DataLength)

    *task = (mfxThreadTask*)this;
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

  out->mfx.BufferSizeInKB = GET_DEFAULT_BUFFER_SIZE_IN_KB(in->mfx.FrameInfo.Width, in->mfx.FrameInfo.Height);
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

  m_umc_params.m_Param.Width = par->mfx.FrameInfo.Width;
  m_umc_params.m_Param.Height = par->mfx.FrameInfo.Height;
  m_umc_params.info.clip_info.width = par->mfx.FrameInfo.Width;
  m_umc_params.info.clip_info.height = par->mfx.FrameInfo.Height;
  m_umc_params.info.framerate = par->mfx.FrameInfo.FrameRateExtN/par->mfx.FrameInfo.FrameRateExtD;

  m_video.mfx.BufferSizeInKB = GET_DEFAULT_BUFFER_SIZE_IN_KB(m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);

  DBG_VAL_I(m_video.mfx.FrameInfo.Width);
  DBG_VAL_I(m_video.mfx.FrameInfo.Height);
  DBG_VAL_I(m_video.mfx.FrameInfo.FrameRateExtN);
  DBG_VAL_I(m_video.mfx.FrameInfo.FrameRateExtD);

  if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12) {
    inframe = malloc(3*m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height/2);
    if (!inframe) {
      return MFX_ERR_MEMORY_ALLOC;
    }
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

  if (inframe) {
    free(inframe);
    inframe = NULL;
  }
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
