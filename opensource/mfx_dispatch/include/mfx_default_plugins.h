/* ****************************************************************************** *\

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

File Name: mfx_default_plugins.h

\* ****************************************************************************** */

#if !defined(__MFX_DEFAULT_PLUGINS_H)
#define __MFX_DEFAULT_PLUGINS_H

#include "mfx_plugin_hive.h"
#include "mfxvp8.h"

namespace
{

struct DefaultPluginInfo
{
    mfxPluginUID PluginUID;
    mfxU32       Type;
    mfxU32       CodecId;
};

const DefaultPluginInfo defaultPluginRecords[] = 
{
    { MFX_PLUGINID_VP8D_SW, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP8 },
    { MFX_PLUGINID_VP8D_HW, MFX_PLUGINTYPE_VIDEO_DECODE, MFX_CODEC_VP8 },
    { MFX_PLUGINID_VP8E_HW, MFX_PLUGINTYPE_VIDEO_ENCODE, MFX_CODEC_VP8 }
};

}

#endif // __MFX_DEFAULT_PLUGINS_H
