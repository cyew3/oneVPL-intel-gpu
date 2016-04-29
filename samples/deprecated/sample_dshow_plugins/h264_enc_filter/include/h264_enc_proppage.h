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

#pragma once

#include "streams.h"
#include "atlbase.h"
#include "mfx_video_enc_filter_utils.h"

// {C2ADDA23-674B-4000-A174-B715877CB90B}
static const GUID IID_IH246EncPropertyPage =
{ 0xc2adda23, 0x674b, 0x4000, { 0xa1, 0x74, 0xb7, 0x15, 0x87, 0x7c, 0xb9, 0xb } };

// {AD6FCBD1-3915-45e0-8EBA-E4B2ABEDE6CE}
static const GUID CLSID_H246EncPropertyPage =
{ 0xad6fcbd1, 0x3915, 0x45e0, { 0x8e, 0xba, 0xe4, 0xb2, 0xab, 0xed, 0xe6, 0xce } };

#include "resource_enc.h"
#include "atlstr.h"
#include "mfx_video_enc_proppage.h"

class CH264EncPropPage : public CConfigPropPage
{
public:

    CH264EncPropPage(IUnknown *pUnk);

    HRESULT                     OnConnect(IUnknown *pUnk);
    HRESULT                     OnActivate(void);
    HRESULT                     OnDeactivate(void);
    INT_PTR                     OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HRESULT                     OnApplyChanges(void);
    HRESULT                     OnDisconnect(void);

    static CUnknown* WINAPI     CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr);

private:

    static unsigned int         _stdcall ThreadProc(void* ptr);

    HRESULT                     FillCombos(void);
    HRESULT                     ConfigureEditsSpins(void);

    //from CConfigPropPage
    virtual void                LoadResults(mfxVideoParam*);
    virtual void                InitDlgItemsArray(void);
    virtual HRESULT             UpdateProfileControls(void);
    virtual HRESULT             UpdateECControls(void);

private:

    IConfigureVideoEncoder*                  m_pH264EncProps;    // Pointer to the filter's custom interface.
    IConfigureVideoEncoder::Params           m_EncoderParams;
};
