//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <stdio.h>
#include <memory>
#include <vector>
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_utils.h"
#include <umc_mutex.h>

#include "umc_h263_video_encoder.h"

namespace MFX_H263ENC
{
  static const mfxPluginUID MFX_PLUGINID_H263E_HW =
      {{0x49, 0xC9, 0x70, 0xC3, 0xF9, 0x2B, 0x4D, 0x44, 0x8D, 0x2C, 0x3C, 0x21, 0xE2, 0x09, 0xF6, 0x39}};

  enum {
    MFX_CODEC_H263 = MFX_MAKEFOURCC('H','2','6','3'),
  };

  enum
  {
    MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
    MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
    MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_EXTERNAL_FRAME,
    MFX_MEMTYPE_SYS_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_INTERNAL_FRAME
  };
} // namespace MFX_H263ENC

class MFX_H263E_Plugin : public MFXEncoderPlugin
{
    static const mfxPluginUID g_PluginGuid;
public:
    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 );
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus );
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual void Release()
    {
        delete this;
    }
    static MFXEncoderPlugin* Create() {
        return new MFX_H263E_Plugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
        if (memcmp(& guid , &g_PluginGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }
        MFX_H263E_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFX_H263E_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;;
        }

        try
        {
            tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXEncoderPlugin> (tmp_pplg));
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        *mfxPlg = tmp_pplg->m_adapter->operator mfxPlugin();
        tmp_pplg->m_createdByDispatcher = true;
        return MFX_ERR_NONE;
    }
    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_VPP;
    }
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    MFX_H263E_Plugin(bool CreateByDispatcher);
    virtual ~MFX_H263E_Plugin(){};

    mfxVideoParam m_video;
    mfxU32 m_frame_order;

    UMC::H263VideoEncoder  m_umc_encoder;
    UMC::H263EncoderParams m_umc_params;

    mfxFrameSurface1 m_dst;
    mfxCoreInterface* m_pmfxCore;

    mfxSession m_session;
    mfxPluginParam m_PluginParam;
    mfxVideoParam m_mfxpar;
    bool m_createdByDispatcher;
    std::auto_ptr<MFXPluginAdapter<MFXEncoderPlugin> > m_adapter;
};

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif
