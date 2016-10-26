//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include <assert.h>

#include "h263d_plugin.h"
#include "h263d_plugin_utils.h"
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
    { printf("h263d: %s:%d: %s: +\n", __FILE__, __LINE__, __FUNCTION__); fflush(NULL); }
  #define DBG_LEAVE \
    { printf("h263d: %s:%d: %s: -\n", __FILE__, __LINE__, __FUNCTION__); fflush(NULL); }
  #define DBG_LEAVE_STS(sts) \
    { printf("h263d: %s:%d: %s: %s=%d: -\n", __FILE__, __LINE__, __FUNCTION__, #sts, sts); fflush(NULL); }
  #define DBG_STS(sts) \
    { printf("h263d: %s:%d: %s: %s=%d\n", __FILE__, __LINE__, __FUNCTION__, #sts, sts); fflush(NULL); }
  #define DBG_VAL_I(val) \
    { printf("h263d: %s:%d: %s: %s=%d\n", __FILE__, __LINE__, __FUNCTION__, #val, val); fflush(NULL); }
#endif

#define GET_DEFAULT_BUFFER_SIZE_IN_KB(w, h) \
  (3 * (w) * (h) / 2 + 999) / 1000;

PluginModuleTemplate g_PluginModule = {
  &MFX_H263D_Plugin::Create,
  NULL,
  NULL,
  NULL,
  &MFX_H263D_Plugin::CreateByDispatcher
};

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
  if (!g_PluginModule.CreateDecoderPlugin) {
    return 0;
  }
  return g_PluginModule.CreateDecoderPlugin();
}

MSDK_PLUGIN_API(MFXPlugin*) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {
  if (!g_PluginModule.CreatePlugin) {
    return 0;
  }
  return (MFXPlugin*) g_PluginModule.CreatePlugin(uid, plugin);
}

const mfxPluginUID MFX_H263D_Plugin::g_PluginGuid = MFX_H263DEC::MFX_PLUGINID_H263D_HW;

MFX_H263D_Plugin::MFX_H263D_Plugin(bool CreateByDispatcher)
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
  m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_DECODE;
  m_PluginParam.CodecId = MFX_H263DEC::MFX_CODEC_H263;
  m_PluginParam.PluginVersion = 1;
  m_createdByDispatcher = CreateByDispatcher;

  memset(&m_bitstream, 0, sizeof(m_bitstream));
  memset(&m_mfxpar, 0, sizeof(m_mfxpar));

  m_frame_order = 0;
}

mfxStatus MFX_H263D_Plugin::PluginInit(mfxCoreInterface * pCore)
{
  DBG_ENTER;
  if (!pCore)
    return MFX_ERR_NULL_PTR;

  m_pmfxCore = pCore;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::PluginClose()
{
  DBG_ENTER;
  if (m_createdByDispatcher) {
    delete this;
  }

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::GetPluginParam(mfxPluginParam *par)
{
  DBG_ENTER;
  if (!par) {
    return MFX_ERR_NULL_PTR;
  }
  *par = m_PluginParam;

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::DecodeHeader(mfxBitstream *bitstream, mfxVideoParam *par)
{
  DBG_ENTER;
  mfxStatus sts = MFX_ERR_NONE;

  if (bitstream->DataLength) {
    UMC::MediaData data;

    data.SetBufferPointer(bitstream->Data + bitstream->DataOffset, bitstream->MaxLength);
    data.SetDataSize(bitstream->DataLength);

    DBG_VAL_I(bitstream->DataLength);

    m_umc_params.lFlags = UMC::FLAG_VDEC_REORDER;
    m_umc_params.m_pData = &data;

    UMC::Status umc_sts = m_umc_decoder.Init(&m_umc_params);
    DBG_STS(umc_sts);

    if (umc_sts == UMC::UMC_OK) {
      DBG_VAL_I(m_umc_params.info.clip_info.width);
      DBG_VAL_I(m_umc_params.info.clip_info.height);

      par->mfx.FrameInfo.Width = m_umc_params.info.clip_info.width;
      par->mfx.FrameInfo.Height = m_umc_params.info.clip_info.height;
      par->mfx.FrameInfo.CropW = m_umc_params.info.clip_info.width;
      par->mfx.FrameInfo.CropH = m_umc_params.info.clip_info.height;
      //m_umc_params.info.framerate = par->mfx.FrameInfo.FrameRateExtN/par->mfx.FrameInfo.FrameRateExtD;

    } else if (umc_sts != UMC::UMC_OK) {
      sts = MFX_ERR_INVALID_VIDEO_PARAM;
    }
  } else {
    sts = MFX_ERR_MORE_DATA;
  }

  DBG_LEAVE_STS(sts);
  return sts;
}

mfxStatus MFX_H263D_Plugin::GetPayload(mfxU64 *ts, mfxPayload *payload)
{
  DBG_ENTER;
  mfxStatus sts = MFX_ERR_NONE;

  DBG_LEAVE_STS(sts);
  return sts;
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

mfxStatus MFX_H263D_Plugin::CopyBitstream(mfxU8* data, mfxU32 size)
{
  assert(!m_bitstream.DataOffset);

  if (!size) return MFX_ERR_NONE;
  if ((m_bitstream.MaxLength - m_bitstream.DataLength) < size) {
    mfxU8* newdata = (mfxU8*)realloc(m_bitstream.Data, m_bitstream.MaxLength + size);
    if (!newdata) {
      return MFX_ERR_MEMORY_ALLOC;
    }
    m_bitstream.Data = newdata;
    m_bitstream.MaxLength += size;
  }
  memcpy(m_bitstream.Data + m_bitstream.DataLength, data, size);
  m_bitstream.DataLength += size;
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::DecodeFrameCheck(mfxBitstream*& bitstream)
{
  DBG_ENTER;
  mfxStatus sts = MFX_ERR_NONE;

  if (!bitstream) {
    if (m_bitstream.DataLength) bitstream = &m_bitstream;
  } else {
    MFX_H263DEC::h263_bitstream info;
    mfxU32 skip;

    info.buffer = bitstream->Data + bitstream->DataOffset;
    info.bufptr = info.buffer;
    info.buflen = bitstream->DataLength;

    if (!m_bitstream.DataLength) {
      mfxU8* ptr1 = info.findStartCodePtr();
      if (!ptr1) {
        // no start code found at all
        skip = info.getSkip();
        bitstream->DataOffset += skip;
        bitstream->DataLength -= skip;
        sts = MFX_ERR_MORE_DATA;
        DBG_LEAVE_STS(sts);
        return sts;
      }
      info.bufptr = ptr1 + 3; // skipping found start code
      mfxU8* ptr2 = info.findStartCodePtr();
      if (!ptr2) {
        // no second start code: copying bitstream into internal memory
        skip = info.getSkip();
        sts = CopyBitstream(ptr1, skip - (mfxU32)(ptr1 - info.buffer));
        if (MFX_ERR_NONE == sts) {
          m_bitstream.TimeStamp = bitstream->TimeStamp;
          bitstream->DataOffset += skip;
          bitstream->DataLength -= skip;
          sts = MFX_ERR_MORE_DATA;
        }
        DBG_LEAVE_STS(sts);
        return sts;
      }
      // if we are here, we can decode from the input buffer which contains
      // 1 complete from and at least part of the next frame
      //frame_size = (mfxU32)(ptr2 - info.buffer);
    } else {
      mfxU8* ptr = info.findStartCodePtr();

      skip = (ptr)? (mfxU32)(ptr - info.buffer): info.getSkip();

      sts = CopyBitstream(info.buffer, skip);
      if (MFX_ERR_NONE == sts) {
        bitstream->DataOffset += skip;
        bitstream->DataLength -= skip;
        if (!ptr) sts = MFX_ERR_MORE_DATA;
      }
      bitstream = &m_bitstream;
      //frame_size = m_bitstream.DataLength;
    }
  }
  DBG_LEAVE_STS(sts);
  return sts;
}

mfxStatus MFX_H263D_Plugin::DecodeFrameSubmit(
  mfxBitstream *bitstream,
  mfxFrameSurface1 *surface_work,
  mfxFrameSurface1 **surface_out,
  mfxThreadTask *task)
{
  DBG_ENTER;
  mfxStatus sts = MFX_ERR_NONE;
  UMC::Status umc_sts = UMC::UMC_OK;

  if (!surface_out) {
    return MFX_ERR_NULL_PTR;
  }

  sts = DecodeFrameCheck(bitstream);
  if (MFX_ERR_NONE != sts) {
    DBG_LEAVE_STS(sts);
    return sts;
  }

  UMC::MediaData in;
  UMC::VideoData out;
  mfxFrameSurface1 src;

  if (surface_work) {
    if (surface_work->Info.FourCC == MFX_FOURCC_NV12) {
      if ((!surface_work->Data.Y || !surface_work->Data.UV) && !surface_work->Data.MemId) {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        DBG_LEAVE_STS(sts);
        return sts;
      }
    } else if (surface_work->Info.FourCC == MFX_FOURCC_YV12) {
      if ((!surface_work->Data.Y || !surface_work->Data.U || !surface_work->Data.V) && !surface_work->Data.MemId) {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        DBG_LEAVE_STS(sts);
        return sts;
      }
    } else {
      sts = MFX_ERR_UNDEFINED_BEHAVIOR;
      DBG_LEAVE_STS(sts);
      return sts;
    }

    memcpy(&src, surface_work, sizeof(src));
    if (surface_work->Data.MemId) {
      sts = m_pmfxCore->FrameAllocator.Lock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &src.Data);
      if (sts != MFX_ERR_NONE) {
        DBG_LEAVE_STS(sts);
        return sts;
      }
    }

    if (surface_work->Info.FourCC == MFX_FOURCC_NV12) {
      out.Init(m_umc_params.info.clip_info.width, m_umc_params.info.clip_info.height, UMC::NV12, 8);
      out.SetPlanePointer(src.Data.Y, 0);
      out.SetPlanePointer(src.Data.UV, 1);

      out.SetPlanePitch(src.Data.Pitch, 0);
      out.SetPlanePitch(src.Data.Pitch, 1);
    } else if (surface_work->Info.FourCC == MFX_FOURCC_YV12) {
      out.Init(m_umc_params.info.clip_info.width, m_umc_params.info.clip_info.height, UMC::YUV420, 8);
      out.SetPlanePointer(src.Data.Y, 0);
      out.SetPlanePointer(src.Data.U, 1);
      out.SetPlanePointer(src.Data.V, 2);

      out.SetPlanePitch(src.Data.Pitch, 0);
      out.SetPlanePitch(src.Data.Pitch/2, 1);
      out.SetPlanePitch(src.Data.Pitch/2, 2);
    }
  }

  if (bitstream) {
    DBG_VAL_I(bitstream->DataLength);
    in.SetBufferPointer(bitstream->Data + bitstream->DataOffset,
                        bitstream->MaxLength - bitstream->DataOffset);
    in.SetDataSize(bitstream->DataLength);
    in.SetTime(GetUmcTimeStamp(bitstream->TimeStamp));
  }

  umc_sts = m_umc_decoder.GetFrame((bitstream)? &in: NULL, &out);
  DBG_STS(umc_sts);

  if (bitstream) {
    mfxU32 ReadLength = bitstream->DataLength - (mfxU32)in.GetDataSize();

    bitstream->DataOffset += ReadLength;
    bitstream->DataLength -= ReadLength;
    if (bitstream == &m_bitstream) {
      assert(!bitstream->DataLength); // some data left after decoding: corrupted bitstream?
      m_bitstream.DataOffset = 0;
      m_bitstream.DataLength = 0;
    }
    DBG_VAL_I(ReadLength);
  }

  if (umc_sts == UMC::UMC_ERR_NOT_ENOUGH_DATA) {
    sts = MFX_ERR_MORE_DATA;
  } else if (umc_sts == UMC::UMC_ERR_SYNC) {
    sts = MFX_ERR_MORE_DATA;
  } else if (umc_sts != UMC::UMC_OK) {
    sts = MFX_ERR_UNDEFINED_BEHAVIOR;
  }

  if (sts == MFX_ERR_NONE) {
    DBG_VAL_I(surface_work->Info.Width);
    DBG_VAL_I(surface_work->Info.Height);
    DBG_VAL_I(surface_work->Info.CropW);
    DBG_VAL_I(surface_work->Info.CropH);

    surface_work->Data.TimeStamp = GetMfxTimeStamp(out.GetTime());

    *surface_out = surface_work;

    *task = (mfxThreadTask*)this;
  }
  if (surface_work && surface_work->Data.MemId) {
    mfxStatus _sts = m_pmfxCore->FrameAllocator.Unlock(m_pmfxCore->FrameAllocator.pthis, surface_work->Data.MemId, &src.Data);
    if (sts == MFX_ERR_NONE) sts = _sts;
  }

  DBG_LEAVE_STS(sts);
  return sts;
}

mfxStatus MFX_H263D_Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
  DBG_ENTER;
  MFX_H263D_Plugin *self = (MFX_H263D_Plugin*)task;

  if (self != this) {
    DBG_LEAVE_STS(MFX_ERR_UNKNOWN);
    return MFX_ERR_UNKNOWN;
  }

  DBG_LEAVE_STS(MFX_TASK_DONE);
  return MFX_TASK_DONE;
}

mfxStatus MFX_H263D_Plugin::FreeResources(mfxThreadTask /*task*/, mfxStatus )
{
  DBG_ENTER;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
  DBG_ENTER;
  MFX_CHECK_NULL_PTR1(out);

  memcpy(&(out->mfx), &(in->mfx), sizeof(mfxInfoMFX));

  out->AsyncDepth = in->AsyncDepth;
  out->IOPattern = in->IOPattern;

  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
  DBG_ENTER;
  MFX_CHECK_NULL_PTR2(par,out);

  out->Type = mfxU16((par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)? MFX_H263DEC::MFX_MEMTYPE_SYS_EXT: MFX_H263DEC::MFX_MEMTYPE_D3D_EXT) ;

  out->NumFrameMin =  (par->AsyncDepth ? par->AsyncDepth: 1)  + 1; // default AsyncDepth is 1
  out->NumFrameSuggested = out->NumFrameMin + 1;

  out->Info = par->mfx.FrameInfo;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::Init(mfxVideoParam *par)
{
  DBG_ENTER;
  mfxStatus sts  = MFX_ERR_NONE;

  m_frame_order = 0;
  m_video = *par;

/*  m_umc_params.m_Param.Width = par->mfx.FrameInfo.Width;
  m_umc_params.m_Param.Height = par->mfx.FrameInfo.Height;
  m_umc_params.info.clip_info.width = par->mfx.FrameInfo.Width;
  m_umc_params.info.clip_info.height = par->mfx.FrameInfo.Height;
  m_umc_params.info.framerate = par->mfx.FrameInfo.FrameRateExtN/par->mfx.FrameInfo.FrameRateExtD;
*/

  DBG_VAL_I(m_video.mfx.FrameInfo.Width);
  DBG_VAL_I(m_video.mfx.FrameInfo.Height);
  DBG_VAL_I(m_video.mfx.FrameInfo.FrameRateExtN);
  DBG_VAL_I(m_video.mfx.FrameInfo.FrameRateExtD);

/*  if (m_video.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12) {
    inframe = malloc(3*m_video.mfx.FrameInfo.Width*m_video.mfx.FrameInfo.Height/2);
    if (!inframe) {
      return MFX_ERR_MEMORY_ALLOC;
    }
  } else if (m_video.mfx.FrameInfo.FourCC != MFX_FOURCC_YV12) {
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  UMC::Status umc_sts = m_umc_decoder.Init(&m_umc_params);
  DBG_STS(umc_sts);

  if (umc_sts != UMC::UMC_OK) {
    sts = MFX_ERR_INVALID_VIDEO_PARAM;
  }
*/
  DBG_LEAVE_STS(sts);
  return sts;
}

mfxStatus MFX_H263D_Plugin::Reset(mfxVideoParam *par)
{
  DBG_ENTER;
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::Close()
{
  DBG_ENTER;

  if (m_bitstream.Data) {
    free(m_bitstream.Data);
  }
  memset(&m_bitstream, 0, sizeof(m_bitstream));
  DBG_LEAVE_STS(MFX_ERR_NONE);
  return MFX_ERR_NONE;
}

mfxStatus MFX_H263D_Plugin::GetVideoParam(mfxVideoParam *par)
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
