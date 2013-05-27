/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: mfx_user_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_USER_PLUGIN_H)
#define __MFX_USER_PLUGIN_H

#include <mfxvideo++int.h>

#include <mfxplugin.h>
#include <mfx_task.h>

class VideoUSERPlugin : public VideoUSER
{
public:
    // Default constructor
    VideoUSERPlugin(void);
    // Destructor
    virtual
    ~VideoUSERPlugin(void);

    // Initialize the user's plugin
    virtual
    mfxStatus Init(const mfxPlugin *pParam,
                   mfxCoreInterface *pCore);
    // Release the user's plugin
    virtual
    mfxStatus Close(void);
    // Get the plugin's threading policy
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void);

    // Check the parameters to start a new tasl
    mfxStatus Check(const mfxHDL *in, mfxU32 in_num,
                    const mfxHDL *out, mfxU32 out_num,
                    MFX_ENTRY_POINT *pEntryPoint);

protected:
    void Release(void);

    // User's plugin parameters
    mfxPluginParam m_param;
    mfxPlugin m_plugin;

    // Default entry point structure
    MFX_ENTRY_POINT m_entryPoint;
};

#endif // __MFX_USER_PLUGIN_H
