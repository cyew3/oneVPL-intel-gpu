//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2018 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_SCHEDULER_DX11_EVENT)
#define __MFX_SCHEDULER_DX11_EVENT

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)

#include <atlbase.h>
#include <d3d11.h>
#include <mfxdefs.h>

#include "libmfx_core_interface.h"
#include "d3d11_decode_accelerator.h"

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

// this is common class
// it used for both DX9 and DX11
class DX11GlobalEvent
{
public:
    explicit DX11GlobalEvent(VideoCORE *pCore):m_pCore(pCore),
                                               m_event_handle(NULL)
    {
    };
    virtual ~DX11GlobalEvent()
    {
        // handle is managed by UMD - don't need to touch
        //if (m_event_handle)
        //    CloseHandle(m_event_handle);
    };
    HANDLE CreateBatchBufferEvent()
    {
        HRESULT hres = 0;
        ID3D11VideoDecoder* pVideoDecoder=NULL;

        // that should never happened but better to check
        if (m_pCore == NULL)
        {
            return NULL;
        }

        ComPtrCore<ID3D11VideoDecoder>* pWrpVideoDecoder = QueryCoreInterface<ComPtrCore<ID3D11VideoDecoder>>(m_pCore, MFXID3D11DECODER_GUID);

        // only D3D11VideoCORE supports MFXID3D11DECODER_GUID
        if (pWrpVideoDecoder == NULL)
        {
            // it is not DX11Core
            return NULL;
        }

        // Get D3D11 Video decoder after MSDK encoder's creation
        // any of them can be used to get global event handle
        pVideoDecoder = pWrpVideoDecoder->get();

        // if no decoder from MSDK encoder's
        // Get D3D11 Video decoder after MSDK decoder's creation
        if (pVideoDecoder == NULL)
        {
            MFXD3D11Accelerator* mpUMCVA = NULL;

            m_pCore->GetVA((mfxHDL*)&mpUMCVA, MFX_MEMTYPE_FROM_DECODE);

            if (mpUMCVA == NULL)
            {
                return NULL;
            }

            mpUMCVA->GetVideoDecoder((void**)&pVideoDecoder);
        }

        if (pVideoDecoder == NULL)
        {
            return NULL;
        }

        D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_pCore);
        if (!pD3d11)
        {
            return NULL;
        }

        ID3D11VideoContext* pD3D11Context =  pD3d11->GetD3D11VideoContext();
        if (!pD3D11Context)
        {
            return NULL;
        }

        {
            D3D11_VIDEO_DECODER_EXTENSION dec_ext;

            memset(&dec_ext, 0, sizeof(D3D11_VIDEO_DECODER_EXTENSION));

            dec_ext.Function = 5;
            dec_ext.ppResourceList = 0;
            dec_ext.PrivateInputDataSize = 0;
            dec_ext.pPrivateOutputData =  &m_event_handle;
            dec_ext.PrivateOutputDataSize = sizeof(m_event_handle);
            dec_ext.ppResourceList = 0;


            hres = pD3D11Context->DecoderExtension(pVideoDecoder, &dec_ext);

            if (FAILED(hres))
                return NULL;
   
            return m_event_handle;
        }
           
    };
    
protected:

    HANDLE m_event_handle;
    VideoCORE*              m_pCore;
   
    DX11GlobalEvent & operator = (DX11GlobalEvent &);
};

#endif
#endif
#endif // __MFX_SCHEDULER_DX11_EVENT
