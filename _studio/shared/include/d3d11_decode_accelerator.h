/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.

File Name: d3d11_decode_accelerator.h

\* ****************************************************************************** */

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef _D3D11_DECODE_ACCELERATOR_H_
#define _D3D11_DECODE_ACCELERATOR_H_

#define UMC_VA_DXVA

#include <atlbase.h>
#include <d3d11.h>


#include "umc_va_base.h"
#include "mfxvideo.h"

struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoDecoder;


struct D3D11_VIDEO_DECODER_DESC;
struct D3D11_VIDEO_DECODER_CONFIG;

class  D3D11VideoCORE;
class mfx_UMC_FrameAllocator;

class MFXD3D11Accelerator : public UMC::VideoAccelerator
{
    
public:
    MFXD3D11Accelerator(ID3D11VideoDevice  *pVideoDevice, 
                        ID3D11VideoContext *pVideoContext);

    virtual ~MFXD3D11Accelerator()
    {
        Close();
    };

    // I/F between core and accelerator
    virtual UMC::Status Init(UMC::VideoAcceleratorParams *pParams) {pParams; return UMC::UMC_ERR_UNSUPPORTED;};
    
    // Will use this function instead of previos two: FindConfiguration, Init
    mfxStatus CreateVideoAccelerator(mfxU32 hwProfile, const mfxVideoParam *param, UMC::FrameAllocator *allocator);

    // I/F between UMC decoders and accelerator
    virtual UMC::Status BeginFrame(Ipp32s index);
    virtual void*       GetCompBuffer(Ipp32s buffer_type, UMC::UMCVACompBuffer **buf = NULL, Ipp32s size = -1, Ipp32s index = -1);
    virtual UMC::Status Execute();
    virtual UMC::Status ExecuteExtensionBuffer(void * buffer){buffer;return UMC::UMC_ERR_UNSUPPORTED;};
    virtual HRESULT GetVideoDecoderDriverHandle(HANDLE *pDriverHandle) {return m_pDecoder->GetDriverHandle(pDriverHandle);};
    virtual UMC::Status ExecuteStatusReportBuffer(void * buffer, Ipp32s size);
    virtual UMC::Status SyncTask(Ipp32s index, void * error = NULL) { index; error; return UMC::UMC_ERR_UNSUPPORTED;}
    virtual UMC::Status QueryTaskStatus(Ipp32s , void *, void *) { return UMC::UMC_ERR_UNSUPPORTED;}
    
    virtual UMC::Status ReleaseAllBuffers();
    virtual UMC::Status ReleaseBuffer(Ipp32s type);
    virtual UMC::Status EndFrame(void * handle = 0);

    virtual bool IsIntelCustomGUID() const;

    void GetVideoDecoder(void **handle)
    {
        *handle = m_pDecoder;
    };

    virtual UMC::Status Close();
    
protected:

    mfxStatus GetSuitVideoDecoderConfig(const mfxVideoParam            *param,        //in
                                        D3D11_VIDEO_DECODER_DESC *video_desc,   //in
                                        D3D11_VIDEO_DECODER_CONFIG     *pConfig);     //out


    D3D11_VIDEO_DECODER_BUFFER_TYPE MapDXVAToD3D11BufType(const Ipp32s DXVABufType) const;
                                    

    ID3D11VideoDevice               *m_pVideoDevice;
    ID3D11VideoContext              *m_pVideoContext;
    
    // we own video decoder, let using com pointer 
    CComPtr<ID3D11VideoDecoder>      m_pDecoder;

    // current decoer
    GUID                              m_DecoderGuid;

    UMC::UMCVACompBuffer    m_pCompBuffer[MAX_BUFFER_TYPES];

    std::vector<Ipp32s>     m_bufferOrder;

    ID3D11VideoDecoderOutputView *m_pVDOView;
};


#endif
#endif
#endif
