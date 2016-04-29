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

// {4BA8E452-C7E5-48e9-965B-D150AA722D71}
static const GUID IID_IMPEG2EncPropertyPage =
{ 0x4ba8e452, 0xc7e5, 0x48e9, { 0x96, 0x5b, 0xd1, 0x50, 0xaa, 0x72, 0x2d, 0x71 } };

// {2D3AA453-0814-4ca3-8E5A-7728D4800746}
static const GUID CLSID_MPEG2EncPropertyPage =
{ 0x2d3aa453, 0x814, 0x4ca3, { 0x8e, 0x5a, 0x77, 0x28, 0xd4, 0x80, 0x7, 0x46 } };

#include "resource_enc.h"
#include "atlstr.h"

#include "mfx_video_enc_proppage.h"

class CMPEG2EncPropPage : public CConfigPropPage
{
public:

    CMPEG2EncPropPage(IUnknown *pUnk);

    HRESULT                     OnConnect(IUnknown *pUnk);
    HRESULT                     OnActivate(void);
    HRESULT                     OnDeactivate(void);
    INT_PTR                     OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HRESULT                     OnApplyChanges(void);
    HRESULT                     OnDisconnect(void);

    static CUnknown* WINAPI     CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr);

protected:

    static unsigned int         _stdcall ThreadProc(void* ptr);

    HRESULT                     FillCombos(void);
    HRESULT                     ConfigureEditsSpins(void);

    //from CConfigPropPage
    virtual void                LoadResults(mfxVideoParam*);
    virtual void                InitDlgItemsArray(void);

private:

    IConfigureVideoEncoder*                  m_pMPEG2EncProps;    // Pointer to the filter's custom interface.
    IConfigureVideoEncoder::Params           m_EncoderParams;
};
