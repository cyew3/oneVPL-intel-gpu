/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: mfx_user_plugin.cpp

\* ****************************************************************************** */

#include <mfx_user_plugin.h>

#include <memory.h> // declaration of memset on Windows
#include <string.h> // declaration of memset on Android (and maybe on Linux)

VideoUSERPlugin::VideoUSERPlugin(void)
{
    // reset the structure(s)
    memset(&m_param, 0, sizeof(m_param));
    memset(&m_plugin, 0, sizeof(m_plugin));

    memset(&m_entryPoint, 0, sizeof(m_entryPoint));

} // VideoUSERPlugin::VideoUSERPlugin(void)

VideoUSERPlugin::~VideoUSERPlugin(void)
{
    Release();

} // VideoUSERPlugin::~VideoUSERPlugin(void)

void VideoUSERPlugin::Release(void)
{
    // call 'close' method
    if (m_plugin.PluginClose)
    {
        m_plugin.PluginClose(m_plugin.pthis);
    }

    // reset the structure(s)
    memset(&m_param, 0, sizeof(m_param));
    memset(&m_plugin, 0, sizeof(m_plugin));

    memset(&m_entryPoint, 0, sizeof(m_entryPoint));

} // void VideoUSERPlugin::Release(void)

mfxStatus VideoUSERPlugin::Init(const mfxPlugin *pParam,
                                mfxCoreInterface *pCore)
{
    mfxStatus mfxRes;

    // check error(s)
    if ((0 == pParam->PluginInit) ||
        (0 == pParam->GetPluginParam) ||
        (0 == pParam->Submit) ||
        (0 == pParam->Execute))
    {
        return MFX_ERR_NULL_PTR;
    }

    // release the object before initialization
    Release();

    // save the parameters
    m_plugin = *pParam;

    // initialize the plugin
    mfxRes = m_plugin.PluginInit(m_plugin.pthis,
                                 pCore);
    if (MFX_ERR_NONE != mfxRes)
    {
        return mfxRes;
    }

    // get the default plugin's parameters
    mfxRes = m_plugin.GetPluginParam(m_plugin.pthis, &m_param);
    if (MFX_ERR_NONE != mfxRes)
    {
        return mfxRes;
    }

    // initialize the default 'entry point' structure
    m_entryPoint.pState = m_plugin.pthis;
    m_entryPoint.pRoutine = m_plugin.Execute;
    m_entryPoint.pCompleteProc = m_plugin.FreeResources;
    m_entryPoint.requiredNumThreads = m_param.MaxThreadNum;

    return MFX_ERR_NONE;

} // mfxStatus VideoUSERPlugin::Init(const mfxPlugin *pParam,

mfxStatus VideoUSERPlugin::Close(void)
{
    Release();

    return MFX_ERR_NONE;

} // mfxStatus VideoUSERPlugin::Close(void)

mfxTaskThreadingPolicy VideoUSERPlugin::GetThreadingPolicy(void)
{
    mfxTaskThreadingPolicy threadingPolicy;

    switch (m_param.ThreadPolicy)
    {
    case MFX_THREADPOLICY_PARALLEL:
        threadingPolicy = MFX_TASK_THREADING_INTER;
        break;

        // MFX_THREADPOLICY_SERIAL is the default threading mode
    default:
        threadingPolicy = MFX_TASK_THREADING_INTRA;
        break;
    }

    return threadingPolicy;

} // mfxTaskThreadingPolicy VideoUSERPlugin::GetThreadingPolicy(void)

mfxStatus VideoUSERPlugin::Check(const mfxHDL *in, mfxU32 in_num,
                                 const mfxHDL *out, mfxU32 out_num,
                                 MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus mfxRes;
    mfxThreadTask userParam;

    // check the parameters with user object
    mfxRes = m_plugin.Submit(m_plugin.pthis,
                             in, in_num,
                             out, out_num,
                             &userParam);
    if (MFX_ERR_NONE != mfxRes)
    {
        return mfxRes;
    }

    // fill the 'entry point' structure
    *pEntryPoint = m_entryPoint;
    pEntryPoint->pParam = userParam;

    return MFX_ERR_NONE;

} // mfxStatus VideoUSERPlugin::Check(const mfxHDL *in, mfxU32 in_num,
