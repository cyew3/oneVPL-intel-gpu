//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2018 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_VP9_DEC_PLUGIN_INCLUDED__)
#define __MFX_VP9_DEC_PLUGIN_INCLUDED__

#include "mfx_stub_dec_plugin.h"

#if defined( AS_VP9D_PLUGIN )

class MFXVP9DecoderPlugin : MFXStubDecoderPlugin
{
public:

    static const mfxPluginUID g_VP9DecoderGuid;

    static mfxStatus _GetPluginParam(mfxHDL /*pthis*/, mfxPluginParam *);

    static MFXDecoderPlugin* Create();
    static mfxStatus CreateByDispatcher(mfxPluginUID, mfxPlugin*);

    mfxStatus GetPluginParam(mfxPluginParam* par) override;
#ifndef MFX_VA
protected:

    friend class MFXStubDecoderPlugin;

    MFXVP9DecoderPlugin(bool CreateByDispatcher);

    std::unique_ptr<MFXPluginAdapter<MFXDecoderPlugin> > m_adapter;
#endif
};
#endif //#if defined( AS_VP9D_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#endif

#endif  // __MFX_VP9_DEC_PLUGIN_INCLUDED__
