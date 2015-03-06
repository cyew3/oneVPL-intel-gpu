/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2005-2011 Intel Corporation. All Rights Reserved.
//
*/

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
