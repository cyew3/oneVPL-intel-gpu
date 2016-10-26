//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_D3D_VIDEO_PROCESSING_H__
#define __UMC_D3D_VIDEO_PROCESSING_H__

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "vm_types.h"
#include "ippdefs.h"
#include "umc_base_codec.h"
#include "umc_video_data.h"
#include <d3d9.h>
#include <dxva2api.h>

#define IS_D3D_FORMAT(cf) ((cf) == D3D_SURFACE_DEC || (cf) == D3D_SURFACE)

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
#endif

namespace UMC
{

typedef struct
{
    Ipp32s                  index;
    DXVA2_SurfaceType       type;
    IDirect3DSurface9       *pSurface;
    IDirect3DDeviceManager9 *pDirect3DDeviceManager9;
    IDirectXVideoDecoder    *pDirectXVideoDecoder;
    IDirectXVideoProcessor  *pDirectXVideoProcessor;
    Ipp32s                  offset_x; // offset to actual image
    Ipp32s                  offset_y; // offset to actual image
} D3DSurface;

class D3DVideoProcessing : public BaseCodec
{
    DYNAMIC_CAST_DECL(D3DVideoProcessing, BaseCodec)

public:
    D3DVideoProcessing() :
        m_pDirect3DDeviceManager9(NULL),
        m_pDXVAVideoProcessorService(NULL),
        m_pDXVAVideoProcessor(NULL),
        m_pColorConverter(NULL),
        msBlitParams()
    {
        memset(&m_VideoDesc, 0, sizeof(m_VideoDesc));
    }

    // Destructor
    virtual ~D3DVideoProcessing()
    {
        Close();

        SAFE_RELEASE(m_pDXVAVideoProcessor);
        SAFE_RELEASE(m_pDXVAVideoProcessorService);
        SAFE_RELEASE(m_pDirect3DDeviceManager9);
        UMC_DELETE(m_pColorConverter);
    }

    // Initialize codec with specified parameter(s)
    virtual Status SetColorConverter(BaseCodec *pColorConverter)
    {
        m_pColorConverter = pColorConverter;
        return UMC_OK;
    };

    // Initialize codec with specified parameter(s)
    virtual Status Init(BaseCodecParams *) { return UMC_OK; };

    // Convert frame
    virtual Status GetFrame(MediaData *in, MediaData *out);

    // Get codec working (initialization) parameter(s)
    virtual Status GetInfo(BaseCodecParams *) { return UMC_ERR_NOT_IMPLEMENTED; };

    // Close all codec resources
    virtual Status Close(void) { return UMC_OK; };

    // Set codec to initial state
    virtual Status Reset(void) { return UMC_OK; };

protected:
    Status CreateVideoProcessor();

    IDirect3DDeviceManager9         *m_pDirect3DDeviceManager9;
    IDirectXVideoProcessorService   *m_pDXVAVideoProcessorService;
    IDirectXVideoProcessor          *m_pDXVAVideoProcessor;
    DXVA2_VideoDesc                 m_VideoDesc;
    DXVA2_VideoProcessBltParams     msBlitParams;

    BaseCodec                       *m_pColorConverter;

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    D3DVideoProcessing(const D3DVideoProcessing &);
    D3DVideoProcessing & operator = (const D3DVideoProcessing &);
};

} // namespace UMC

#endif // UMC_VA_DXVA
#endif // UMC_RESTRICTED_CODE_VA
#endif // __UMC_D3D_VIDEO_PROCESSING_H__
