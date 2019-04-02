// Copyright (c) 2017-2019 Intel Corporation
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
#if defined (AS_VPP_SCD_PLUGIN)
#include "vpp_scd.h"
#endif //defined (AS_VPP_SCD_PLUGIN)
#if defined (AS_ENC_SCD_PLUGIN)
#include "enc_scd.h"
#endif //defined (AS_ENC_SCD_PLUGIN)
#include "mediasdk_version.h"

#if defined(_WIN32)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#elif defined(LINUX32)
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#if defined (AS_VPP_SCD_PLUGIN)
MSDK_PLUGIN_API(MFXVPPPlugin*) mfxCreateVPPPlugin()
{
    return MfxVppSCD::Plugin::Create();
}
#endif //defined (AS_VPP_SCD_PLUGIN)

#if defined (AS_ENC_SCD_PLUGIN)
MSDK_PLUGIN_API(MFXEncPlugin*) mfxCreateEncPlugin()
{
    return MfxEncSCD::Plugin::Create();
}
#endif //defined (AS_ENC_SCD_PLUGIN)

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin)
{
    mfxStatus sts = MFX_ERR_NOT_FOUND;

#if defined (AS_VPP_SCD_PLUGIN)
        sts = MfxVppSCD::Plugin::CreateByDispatcher(uid, plugin);
        if (sts != MFX_ERR_NOT_FOUND)
            return sts;
#endif //defined (AS_VPP_SCD_PLUGIN)

#if defined (AS_ENC_SCD_PLUGIN)
        sts = MfxEncSCD::Plugin::CreateByDispatcher(uid, plugin);
        if (sts != MFX_ERR_NOT_FOUND)
            return sts;
#endif //defined (AS_ENC_SCD_PLUGIN)

    return sts;
}