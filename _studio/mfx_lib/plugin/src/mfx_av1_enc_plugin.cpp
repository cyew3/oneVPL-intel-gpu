// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_enc_plugin.h"
#include "vm_sys_info.h"
#include "mfx_plugin_module.h"
#include "plugin_version_linux.h"
#include "mfx_av1_encode_api.h"

PluginModuleTemplate g_PluginModule = {
    NULL,
    &MFXAV1EncoderPlugin::Create,
    NULL,
    NULL,
    &MFXAV1EncoderPlugin::CreateByDispatcher
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

#ifdef MFX_VA
const mfxPluginUID MFXAV1EncoderPlugin::g_AV1EncoderGuid = {0x34,0x3d,0x68,0x96,0x65,0xba,0x45,0x48,0x82,0x99,0x42,0xa3,0x2a,0x38,0x65,0x69};
#else //MFX_VA
const mfxPluginUID MFXAV1EncoderPlugin::g_AV1EncoderGuid = {0x34,0x3d,0x68,0x96,0x65,0xba,0x45,0x48,0x82,0x99,0x42,0xa3,0x2a,0x38,0x65,0x69};
#endif //MFX_VA

MFXAV1EncoderPlugin::MFXAV1EncoderPlugin(bool CreateByDispatcher)
{
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));

    m_PluginParam.CodecId = MFX_CODEC_AV1;
    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;
    m_PluginParam.MaxThreadNum = vm_sys_info_get_cpu_num();
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = g_AV1EncoderGuid;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
    m_PluginParam.PluginVersion = 1;
    m_createdByDispatcher = CreateByDispatcher;

    m_encoder = NULL;
    MFX_TRACE_INIT();
}

MFXAV1EncoderPlugin::~MFXAV1EncoderPlugin()
{
    MFX_TRACE_CLOSE();
    if (m_encoder)
    {
        PluginClose();
    }
}

mfxStatus MFXAV1EncoderPlugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;

    m_mfxCore = *core;

    mfxStatus sts;
    m_encoder = new MFXVideoENCODEAV1(&m_mfxCore, &sts);

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1EncoderPlugin::PluginClose()
{
    if (m_encoder) {
        delete m_encoder;
        m_encoder = NULL;
    }
    if (m_createdByDispatcher) {
        delete this;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1EncoderPlugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MFXAV1EncoderPlugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
{
    MFX_CHECK_NULL_PTR1(task);
    mfxFrameSurface1 *reordered_surface = NULL;
    mfxEncodeInternalParams internal_params;
    MFX_ENTRY_POINT *entryPoint = new MFX_ENTRY_POINT;
    mfxStatus sts = m_encoder->EncodeFrameCheck(ctrl,
                                                surface,
                                                bs,
                                                &reordered_surface,
                                                &internal_params,
                                                entryPoint);
    if (sts < MFX_ERR_NONE && sts != MFX_ERR_MORE_DATA_SUBMIT_TASK) {
        delete entryPoint;
        entryPoint = NULL;
    }

    *task = entryPoint;
    return sts;
}

mfxStatus MFXAV1EncoderPlugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    MFX_CHECK_NULL_PTR1(task);
    MFX_ENTRY_POINT *entryPoint = (MFX_ENTRY_POINT *)task;
    return entryPoint->pRoutine(entryPoint->pState, entryPoint->pParam, uid_p, uid_a);
}

mfxStatus MFXAV1EncoderPlugin::FreeResources(mfxThreadTask task, mfxStatus)
{
    if (task) {
        MFX_ENTRY_POINT *entryPoint = (MFX_ENTRY_POINT *)task;
        entryPoint->pCompleteProc(entryPoint->pState, entryPoint->pParam, MFX_ERR_NONE);
        delete entryPoint;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXAV1EncoderPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    return MFXVideoENCODEAV1::Query(&m_mfxCore, in, out);
}

mfxStatus MFXAV1EncoderPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *)
{
    return MFXVideoENCODEAV1::QueryIOSurf(&m_mfxCore, par, in);
}

mfxStatus MFXAV1EncoderPlugin::Init(mfxVideoParam *par)
{
    return m_encoder->Init(par);
}

mfxStatus MFXAV1EncoderPlugin::Reset(mfxVideoParam *par)
{
    return m_encoder->Reset(par);
}

mfxStatus MFXAV1EncoderPlugin::Close()
{
    return m_encoder->Close();
}

mfxStatus MFXAV1EncoderPlugin::GetVideoParam(mfxVideoParam *par)
{
    return m_encoder->GetVideoParam(par);
}

void MFXAV1EncoderPlugin::Release()
{
    delete this;
}

MFXEncoderPlugin* MFXAV1EncoderPlugin::Create() {
    return new MFXAV1EncoderPlugin(false);
}

mfxStatus MFXAV1EncoderPlugin::CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {
    if (memcmp(& guid , &g_AV1EncoderGuid, sizeof(mfxPluginUID))) {
        return MFX_ERR_NOT_FOUND;
    }
    MFXAV1EncoderPlugin* tmp_pplg = 0;
    try
    {
        tmp_pplg = new MFXAV1EncoderPlugin(false);
    }
    catch(std::bad_alloc&)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    try
    {
        tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXEncoderPlugin> (tmp_pplg));
    }
    catch(std::bad_alloc&)
    {
        delete tmp_pplg;
        return MFX_ERR_MEMORY_ALLOC;
    }

    *mfxPlg = tmp_pplg->m_adapter->operator mfxPlugin();
    tmp_pplg->m_createdByDispatcher = true;
    return MFX_ERR_NONE;
}

mfxU32 MFXAV1EncoderPlugin::GetPluginType()
{
    return MFX_PLUGINTYPE_VIDEO_ENCODE;
}

mfxStatus MFXAV1EncoderPlugin::SetAuxParams(void* , int )
{
    return MFX_ERR_UNKNOWN;
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
