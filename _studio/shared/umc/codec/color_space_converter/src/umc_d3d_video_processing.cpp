//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_d3d_video_processing.h"
#include "umc_color_space_conversion.h"
#include "umc_video_data.h"
#include "umc_defs.h"

using namespace UMC;

#define CHECK_HRESULT(hr) \
{ \
    VM_ASSERT(SUCCEEDED(hr)); \
    if (FAILED(hr)) return UMC_ERR_FAILED; \
}

ColorFormat FourCC2ColorFormat(int fourcc)
{
    switch (fourcc)
    {
    case MAKEFOURCC('Y','V','1','2'): return YV12;
    case MAKEFOURCC('N','V','1','2'): return NV12;
    case MAKEFOURCC('I','4','2','0'): return YUV420;
    case MAKEFOURCC('I','Y','U','V'): return YUV420;
    case MAKEFOURCC('Y','U','Y','V'): return YUY2;
    case MAKEFOURCC('Y','U','Y','2'): return YUY2;
    case MAKEFOURCC('Y','V','Y','U'): return YUY2;
    case MAKEFOURCC('U','Y','V','Y'): return UYVY;
    case MAKEFOURCC('Y','4','1','P'): return Y41P;
    case MAKEFOURCC('I','M','C','3'): return IMC3;
    }
    return NONE;
}

Status D3DVideoProcessing::CreateVideoProcessor()
{
    HRESULT                     hr = S_OK;
    D3DFORMAT                   *pFormats = NULL;
    UINT                        cFormats = NULL;
    D3DFORMAT                   Format;
    GUID                        *pGuids = NULL;
    UINT                        cGuids;
    GUID                        ProcessorGuid = DXVA2_VideoProcProgressiveDevice;
    DXVA2_ValueRange            Brightness;
    DXVA2_ValueRange            Contrast;
    DXVA2_ValueRange            Saturation;
    DXVA2_ValueRange            Hue;

    // create DirectXVideoProcessorService
    hr = m_pDirect3DDeviceManager9->GetVideoService(NULL, IID_IDirectXVideoProcessorService, (void**)&m_pDXVAVideoProcessorService);
    CHECK_HRESULT(hr);

    // GetVideoProcessorDeviceGuids
    hr = m_pDXVAVideoProcessorService->GetVideoProcessorDeviceGuids(&m_VideoDesc,(UINT*)&cGuids,&pGuids);
    //CHECK_HRESULT(hr); !!

#if 0
    int i;
    for(ui32Loop=0;ui32Loop<cGuids;ui32Loop++)
    {
        vm_trace_GUID(pGuids[i]);
    }
#endif

    // select first Guid
    if (pGuids)
    {
        ProcessorGuid = pGuids[0];
        CoTaskMemFree(pGuids);
    }

    hr = m_pDXVAVideoProcessorService->GetVideoProcessorRenderTargets(ProcessorGuid,&m_VideoDesc,&cFormats,&pFormats);
    CHECK_HRESULT(hr);

    // select first render format
    Format = pFormats[0];
    CoTaskMemFree(pFormats);

    // CreateVideoProcessor
    hr = m_pDXVAVideoProcessorService->CreateVideoProcessor(ProcessorGuid,&m_VideoDesc,Format,0,&m_pDXVAVideoProcessor);
    CHECK_HRESULT(hr);

    // Get ProcAmpRange including default values
    hr = m_pDXVAVideoProcessorService->GetProcAmpRange(ProcessorGuid, &m_VideoDesc, Format, DXVA2_ProcAmp_Brightness, &Brightness);
    hr = m_pDXVAVideoProcessorService->GetProcAmpRange(ProcessorGuid, &m_VideoDesc, Format, DXVA2_ProcAmp_Contrast, &Contrast);
    hr = m_pDXVAVideoProcessorService->GetProcAmpRange(ProcessorGuid, &m_VideoDesc, Format, DXVA2_ProcAmp_Saturation, &Saturation);
    hr = m_pDXVAVideoProcessorService->GetProcAmpRange(ProcessorGuid, &m_VideoDesc, Format, DXVA2_ProcAmp_Hue, &Hue);

    // Blit parameters
    memset(&msBlitParams,0,sizeof(msBlitParams));

    // Alpha
    msBlitParams.Alpha = DXVA2FloatToFixed(1.0);

    // Background is black
    msBlitParams.BackgroundColor.Y  = 16;
    msBlitParams.BackgroundColor.Cr = 128;
    msBlitParams.BackgroundColor.Cb = 128;
    msBlitParams.BackgroundColor.Alpha = 0xffff;

    msBlitParams.StreamingFlags = 0xf;

    // Default values
    msBlitParams.ProcAmpValues.Brightness = Brightness.DefaultValue;
    msBlitParams.ProcAmpValues.Contrast   = Contrast.DefaultValue;
    msBlitParams.ProcAmpValues.Saturation = Saturation.DefaultValue;
    msBlitParams.ProcAmpValues.Hue        = Hue.DefaultValue;

    // Target rect
    msBlitParams.TargetRect.left = 0;
    msBlitParams.TargetRect.top = 0;
    msBlitParams.TargetRect.right = m_VideoDesc.SampleWidth;
    msBlitParams.TargetRect.bottom = m_VideoDesc.SampleHeight;

    return UMC_OK;
}

Status D3DVideoProcessing::GetFrame(MediaData *in, MediaData *out)
{
    HRESULT hr = S_OK;
    VideoData *pInput = DynamicCast<VideoData>(in);
    VideoData *pOutput = DynamicCast<VideoData>(out);
    D3DSurface *pInputSurface = NULL;
    D3DSurface *pOutputSurface = NULL;
    D3DSURFACE_DESC SurfaceDescription;

    UMC_CHECK_PTR(pInput);
    UMC_CHECK_PTR(pOutput);

    UMC_CHECK(pInput->GetColorFormat() == D3D_SURFACE, NULL);
    pInputSurface = (D3DSurface*)pInput->GetBufferPointer();
    UMC_CHECK_PTR(pInputSurface);
    UMC_CHECK_PTR(pInputSurface->pSurface);

    if (pOutput->GetColorFormat() != D3D_SURFACE)
    { // dst is general CPU memory
        VideoData tmp_frame;
        Status umcRes = UMC_OK;
        D3DLOCKED_RECT LockedRect;

        // Get surface description
        UMC_CALL(pInputSurface->pSurface->GetDesc(&SurfaceDescription));

        // Lock surface
        UMC_CALL(pInputSurface->pSurface->LockRect(&LockedRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY));
        // now surface is locked!

        // Init UMC::VideoData (with NV12 format)
        if (UMC_SUCCEEDED(umcRes))
        {
//#ifdef ELK
    //        umcRes = tmp_frame.Init(SurfaceDescription.Width, SurfaceDescription.Height, UMC::IMC3);
//#else
            umcRes = tmp_frame.Init(SurfaceDescription.Width, SurfaceDescription.Height, UMC::NV12);
//#endif
            //umcRes = tmp_frame.Init(SurfaceDescription.Width, SurfaceDescription.Height, UMC::YUV420);
        }

        // Set pointers and pitch
        if (UMC_SUCCEEDED(umcRes))
        {
            umcRes = tmp_frame.SetSurface((Ipp8u*)LockedRect.pBits, LockedRect.Pitch);
        }

        // Apply crop rect
        if (UMC_SUCCEEDED(umcRes))
        {
            if (pInput->GetWidth()  != (Ipp32s)SurfaceDescription.Width ||
                pInput->GetHeight() != (Ipp32s)SurfaceDescription.Height)
            {
                UMC::sRECT CropArea;
                CropArea.left = (Ipp16s)pInputSurface->offset_x;
                CropArea.top  = (Ipp16s)pInputSurface->offset_y;
                CropArea.right  = (Ipp16s)(pInputSurface->offset_x + pInput->GetWidth());
                CropArea.bottom = (Ipp16s)(pInputSurface->offset_y + pInput->GetHeight());
                umcRes = tmp_frame.Crop(CropArea);
            }
        }

        // Copy data (or color conversion)
        if (!m_pColorConverter)
        {
            m_pColorConverter = new ColorSpaceConversion;
            if (!m_pColorConverter) umcRes = UMC_ERR_ALLOC;
        }
        if (UMC_SUCCEEDED(umcRes))
        {
            umcRes = m_pColorConverter->GetFrame(&tmp_frame, out);
        }

        // Unlock surface
        hr = pInputSurface->pSurface->UnlockRect();

        UMC_RETURN(umcRes);
    }

    pOutputSurface = (D3DSurface*)pOutput->GetBufferPointer();
    UMC_CHECK_PTR(pOutputSurface);

    if (pOutputSurface->type != DXVA2_SurfaceType_ProcessorRenderTarget)
    { // just copy
        MFX_INTERNAL_CPY(pOutputSurface, pInputSurface, sizeof(D3DSurface));
        return UMC_OK;
    }

    // below is D3D video processing
    if (!m_pDXVAVideoProcessor) // create VideoProcessor
    {
        m_pDirect3DDeviceManager9 = pOutputSurface->pDirect3DDeviceManager9;

        // Get VideoDesc
        if (pInputSurface->pDirectXVideoDecoder)
        {
            GUID                        DecoderGuid;
            DXVA2_ConfigPictureDecode   DecoderConfig;
            hr = pInputSurface->pDirectXVideoDecoder->GetCreationParameters(&DecoderGuid, &m_VideoDesc, &DecoderConfig, NULL, NULL);
            CHECK_HRESULT(hr);
        }

        UMC_CALL(CreateVideoProcessor());
    }

    //pInputSurface->pSurface->GetDesc(&SurfaceDescription);
    //::RECT SrcRect = {0, 0, SurfaceDescription.Width, SurfaceDescription.Height};
    ::RECT SrcRect;
    SrcRect.left = pInputSurface->offset_x;
    SrcRect.top  = pInputSurface->offset_y;
    SrcRect.right  = pInputSurface->offset_x + pInput->GetWidth();
    SrcRect.bottom = pInputSurface->offset_y + pInput->GetHeight();

    pOutputSurface->pSurface->GetDesc(&SurfaceDescription);
    // next line we do explicit  UINT -> LONG conversion i.e. sing -> unsign.
    // it is ok while surface W and H less than 2147483648
    ::RECT TrgtRect = {0, 0, static_cast<LONG>(SurfaceDescription.Width), static_cast<LONG>(SurfaceDescription.Height)};

    //GetClientRect(m_hFocusWindow, &TrgtRect);
    msBlitParams.TargetRect = TrgtRect;

    DXVA2_VideoSample sample = {0};
    sample.Start = 0;//StartTime;
    sample.End   = 0;//EndTime;
    sample.SampleFormat = m_VideoDesc.SampleFormat;
    sample.SrcSurface = pInputSurface->pSurface;
    sample.SrcRect = SrcRect;
    sample.DstRect = TrgtRect;
    sample.PlanarAlpha = DXVA2FloatToFixed(1.);
    //sample.SampleData

    /* Setup some of the other values in the blit structure */
    //msBlitParams.DestFormat.SampleFormat    =    sSampleFormat;

    // VideoProcessBlt
    hr = m_pDXVAVideoProcessor->VideoProcessBlt(pOutputSurface->pSurface, &msBlitParams, &sample, 1, NULL);
    CHECK_HRESULT(hr);

    return UMC_OK;
}

#endif // UMC_VA_DXVA
#endif // UMC_RESTRICTED_CODE_VA
