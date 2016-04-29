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

#include "streams.h"
#include <initguid.h>
#include <tchar.h>
#include <stdio.h>
#include <dvdmedia.h>

#include "h264_dec_filter.h"
#include "filter_defs.h"
#include "mfx_filter_register.h"
#include "mfxmvc.h"


CH264DecVideoFilter::CH264DecVideoFilter(TCHAR *tszName,LPUNKNOWN punk,HRESULT *phr) :
    CDecVideoFilter(tszName,punk, FILTER_GUID,phr)
{
    m_bstrFilterName = FILTER_NAME;
    m_mfxParamsVideo.mfx.CodecId = MFX_CODEC_AVC;
}

CUnknown * WINAPI CH264DecVideoFilter::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CH264DecVideoFilter *pNewObject = new CH264DecVideoFilter(FILTER_NAME, punk, phr);

    if (NULL == pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }

    return pNewObject;
}

HRESULT CH264DecVideoFilter::CheckInputType(const CMediaType *mtIn)
{
    MSDK_CHECK_POINTER(mtIn, E_POINTER);

    if (MEDIASUBTYPE_H264 == *mtIn->Subtype())
    {
    }
    else if (MEDIASUBTYPE_AVC1 == *mtIn->Subtype()  || MEDIASUBTYPE_avc1 == *mtIn->Subtype())
    {
        MSDK_SAFE_DELETE(m_pFrameConstructor);
        m_pFrameConstructor = new CAVCFrameConstructor;
    }
    else
    {
        return VFW_E_INVALIDMEDIATYPE;
    }

    HRESULT hr = CDecVideoFilter::CheckInputType(mtIn);
    if (FAILED(hr))
        return hr;

    if (m_mfxParamsVideo.mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH ||
        m_mfxParamsVideo.mfx.CodecProfile == MFX_PROFILE_AVC_STEREO_HIGH)
    {
        // CH264DecVideoFilter is not suited for MVC decoding,
        // you should use CMVCDecVideoFilter for this task
        return VFW_E_INVALIDMEDIATYPE;
    }

    return hr;
};