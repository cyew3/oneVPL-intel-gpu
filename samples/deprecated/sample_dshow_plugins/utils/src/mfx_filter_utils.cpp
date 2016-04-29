/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mfx_samples_config.h"

#include <atlbase.h>
#include <Windows.h>
#include "guiddef.h"
#include <uuids.h>
#include <mmreg.h>
#include <math.h>

#include "wmsdkidl.h"

#include "mfx_filter_guid.h"

static const struct
{
    mfxU32          color_format;
    GUID            guid;
    mfxU32          bpp;
    mfxU32          fourcc;
}
color_formats[] =
{
    { MFX_FOURCC_YV12,       MEDIASUBTYPE_YV12,    12,  MAKEFOURCC('Y','V','1','2') },
    { MFX_FOURCC_NV12,       MEDIASUBTYPE_NV12,    12,  MAKEFOURCC('N','V','1','2') },
    { MFX_FOURCC_RGB3,       MEDIASUBTYPE_RGB32,   32,  BI_RGB                      },
};

static const struct
{
    mfxU32          codec_type;
    mfxI8           profile;
    GUID            guid;
    int             iPosition;
}
codec_types[] =
{
    { MFX_CODEC_AVC,   -1, MEDIASUBTYPE_H264,        0},
    { MFX_CODEC_MPEG2, -1, MEDIASUBTYPE_MPEG2_VIDEO, 0},
};

const GUID* CodecId2GUID(const mfxU32 fmt, mfxU16 nCodecProfile, int iPosition)
{
    for (int i = 0; i < ARRAY_SIZE(codec_types); i++)
    {
        if (codec_types[i].codec_type == fmt &&  codec_types[i].iPosition == iPosition)
        {
            if (-1 != codec_types[i].profile && codec_types[i].profile != nCodecProfile)
            {
                continue;
            }
            return &codec_types[i].guid;
        }
    }
    return NULL;
}

const GUID* ColorFormat2GUID(const mfxU32 fmt)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(color_formats); i++)
    {
        if (color_formats[i].color_format == fmt) return &color_formats[i].guid;
    }
    return NULL;
}

DWORD ColorFormat2BiCompression(const mfxU32 fmt)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(color_formats); i++)
    {
        if (color_formats[i].color_format == fmt) return color_formats[i].fourcc;
    }
    return 0;
}

WORD ColorFormat2BiBitCount(const mfxU32 fmt)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(color_formats); i++)
    {
        if (color_formats[i].color_format == fmt) return (WORD)color_formats[i].bpp;
    }
    return 0;
}

#define REGISTRY_ROOT                 HKEY_CURRENT_USER

mfxStatus SetRegistryValue(const TCHAR *pName, DWORD value, TCHAR *pPath)
{
    HKEY key = NULL;
    LONG res;

    if (!pPath || !pName)
    {
        return MFX_ERR_NULL_PTR;
    }

    res = RegCreateKeyEx(REGISTRY_ROOT, pPath, 0, 0, 0, KEY_SET_VALUE, NULL, &key, NULL);
    if (res == ERROR_SUCCESS)
    {
        res = RegSetValueEx(key, pName, 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(key);
    }

    return (res == ERROR_SUCCESS) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
}

mfxStatus  GetRegistryValue(const TCHAR *pName, DWORD *pValue, TCHAR *pPath)
{
    HKEY key = NULL;
    LONG res;
    DWORD type;

    if (!pPath || !pName || !pValue)
    {
        return MFX_ERR_NULL_PTR;
    }

    DWORD cbData = sizeof(*pValue);

    res = RegOpenKeyEx(REGISTRY_ROOT, pPath, 0, KEY_QUERY_VALUE, &key);
    if (res == ERROR_SUCCESS)
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &cbData);
        RegCloseKey(key);
    }

    return (res == ERROR_SUCCESS) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
}

REFERENCE_TIME ConvertMFXTime2ReferenceTime(mfxU64 nTime)
{
    if (MFX_TIME_STAMP_INVALID != nTime)
    {
        return (REFERENCE_TIME)((nTime / (DOUBLE)MFX_TIME_STAMP_FREQUENCY) * 1e7);
    }
    else
    {
        return (REFERENCE_TIME)-1e7;
    }
};