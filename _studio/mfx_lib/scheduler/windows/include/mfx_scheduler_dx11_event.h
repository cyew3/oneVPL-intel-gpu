// Copyright (c) 2013-2020 Intel Corporation
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

#if !defined(__MFX_SCHEDULER_DX11_EVENT)
#define __MFX_SCHEDULER_DX11_EVENT

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)

#include <atlbase.h>
#include <d3d11.h>
#include <mfxdefs.h>

#include "libmfx_core_interface.h"
#include "d3d11_decode_accelerator.h"
#include "d3d11_video_processor.h"
#include "libmfx_core_d3d11.h"

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

// this is common class
// it used for both DX9 and DX11
class DX11GlobalEvent
{
public:
    explicit DX11GlobalEvent(VideoCORE *pCore):m_pCore(pCore),
                                               m_event_handle(nullptr)
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
        ID3D11VideoDecoder* pVideoDecoder= nullptr;

        // that should never happened but better to check
        if (m_pCore == nullptr)
        {
            return nullptr;
        }

        ComPtrCore<ID3D11VideoDecoder>* pWrpVideoDecoder = QueryCoreInterface<ComPtrCore<ID3D11VideoDecoder>>(m_pCore, MFXID3D11DECODER_GUID);

        // only D3D11VideoCORE supports MFXID3D11DECODER_GUID
        if (pWrpVideoDecoder == nullptr)
        {
            // it is not DX11Core
            return nullptr;
        }

        // Get D3D11 Video decoder after MSDK encoder's creation
        // any of them can be used to get global event handle
        pVideoDecoder = pWrpVideoDecoder->get();

        // if no decoder from MSDK encoder's
        // Get D3D11 Video decoder after MSDK decoder's creation
        if (pVideoDecoder == nullptr)
        {
            MFXD3D11Accelerator* mpUMCVA = nullptr;

            m_pCore->GetVA((mfxHDL*)&mpUMCVA, MFX_MEMTYPE_FROM_DECODE);

            if (mpUMCVA)
            {
                mpUMCVA->GetVideoDecoder((void**)&pVideoDecoder);
            }
        }

        if (pVideoDecoder == nullptr)
        {
            VPPHWResMng* pVideoProcessor = nullptr;
            m_pCore->GetVA((mfxHDL*)&pVideoProcessor, MFX_MEMTYPE_FROM_VPPIN);
            if (pVideoProcessor)
            {
                MfxHwVideoProcessing::DriverVideoProcessing* pVideoProcessing = pVideoProcessor->GetDevice();
                if (pVideoProcessing)
                {
                    mfxStatus sts = ((MfxHwVideoProcessing::D3D11VideoProcessor*)pVideoProcessing)->GetEventHandle(&m_event_handle);
                    if (sts != MFX_ERR_NONE)
                    {
                        return nullptr;
                    }
                }
            }
            return m_event_handle;
        }

        D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(m_pCore);
        if (!pD3d11)
        {
            return nullptr;
        }

        ID3D11VideoContext* pD3D11Context =  pD3d11->GetD3D11VideoContext();
        if (!pD3D11Context)
        {
            return nullptr;
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


            HRESULT hres = pD3D11Context->DecoderExtension(pVideoDecoder, &dec_ext);

            if (FAILED(hres))
            {
                return nullptr;
            }

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
