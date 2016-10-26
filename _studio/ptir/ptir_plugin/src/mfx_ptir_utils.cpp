//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "ptir_vpp_plugin.h"
#include "ippcc.h"

mfxStatus ColorSpaceConversionWCopy(mfxFrameSurface1* surface_in, mfxFrameSurface1* surface_out, mfxU32 dstFormat)
{
    IppStatus status;
    mfxU32 srcFormat = surface_in->Info.FourCC;

    mfxU16  inW = 0;
    mfxU16  inH = 0;
    mfxU16 outW = 0;
    mfxU16 outH = 0;

    inW = surface_in->Info.CropX + surface_in->Info.CropW;
    inH = surface_in->Info.CropY + surface_in->Info.CropH;

    outW = surface_out->Info.CropX + surface_in->Info.CropW;
    outH = surface_out->Info.CropY + surface_in->Info.CropH;

    switch (srcFormat) {
    case MFX_FOURCC_NV12:
        switch (dstFormat) {
        case MFX_FOURCC_I420:
            {
                const Ipp8u *pSrcY = (Ipp8u*)surface_in->Data.Y;
                const Ipp8u *pSrcUV = (Ipp8u*)surface_in->Data.UV;
                Ipp32u pSrcYStep = surface_in->Data.Pitch;
                Ipp32u pSrcUVStep = surface_in->Data.Pitch;

                Ipp8u *(pDst[3]) = {(Ipp8u*)surface_out->Data.Y,
                      (Ipp8u*)surface_out->Data.U,
                      (Ipp8u*)surface_out->Data.V};

                int pDstStep[3] = { surface_out->Data.Pitch,
                                    surface_out->Data.Pitch/2,
                                    surface_out->Data.Pitch/2};

                IppiSize roiSize = {inW, inH};

                if(surface_in->Info.Width != surface_out->Info.Width   ||
                   surface_in->Info.Height != surface_out->Info.Height)
                   return MFX_ERR_MEMORY_ALLOC;

                status = ippiYCbCr420_8u_P2P3R(pSrcY, pSrcYStep, pSrcUV, pSrcUVStep, pDst, pDstStep, roiSize);
                break;
            }
        default:
            return MFX_ERR_UNSUPPORTED;
        }
        break;
    case MFX_FOURCC_I420:
        switch (dstFormat) {
        case MFX_FOURCC_NV12:
            {
                const Ipp8u *(pSrc[3]) = {(Ipp8u*)surface_in->Data.Y,
                                          (Ipp8u*)surface_in->Data.U,
                                          (Ipp8u*)surface_in->Data.V};

                int pSrcStep[3] = {surface_in->Data.Pitch,
                                   surface_in->Data.Pitch / 2,
                                   surface_in->Data.Pitch / 2};

                Ipp8u *pDstY = (Ipp8u*)surface_out->Data.Y;
                Ipp8u *pDstUV = (Ipp8u*)surface_out->Data.UV;
                Ipp32u pDstYStep = surface_out->Data.Pitch;
                Ipp32u pDstUVStep = surface_out->Data.Pitch;

                IppiSize roiSize = {inW, inH};

                if(surface_in->Info.Width > surface_out->Info.Width   ||
                   surface_in->Info.Height > surface_out->Info.Height)
                   return MFX_ERR_MEMORY_ALLOC;

                status = ippiYCbCr420_8u_P3P2R(pSrc, pSrcStep, pDstY, pDstYStep, pDstUV, pDstUVStep, roiSize);
                break;
            }
        default:
            return MFX_ERR_UNSUPPORTED;
        }
        break;
    default:
      return MFX_ERR_UNSUPPORTED;
    }

    if(ippStsNoErr == status)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus Ptir420toMfxNv12(mfxU8* buffer, mfxFrameSurface1* surface_out)
{
    mfxU16 outW = 0;
    mfxU16 outH = 0;

    outW = surface_out->Info.CropX + surface_out->Info.CropW;
    outH = surface_out->Info.CropY + surface_out->Info.CropH;
    mfxFrameSurface1 work420_surface;
    memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
    ptir_memcpy_s(&(work420_surface.Info), sizeof(work420_surface.Info), &(surface_out->Info), sizeof(surface_out->Info));
    work420_surface.Info.FourCC = MFX_FOURCC_I420;
    mfxU16& w = outW;
    mfxU16& h = outH;
    work420_surface.Data.Y = buffer;
    work420_surface.Data.U = work420_surface.Data.Y+w*h;
    work420_surface.Data.V = work420_surface.Data.U+w*h/4;
    work420_surface.Data.Pitch = w;

    return ColorSpaceConversionWCopy(&work420_surface, surface_out, MFX_FOURCC_NV12);
}

mfxStatus MfxNv12toPtir420(mfxFrameSurface1* surface_in, mfxU8* buffer)
{
    mfxU16  inW = 0;
    mfxU16  inH = 0;

    inW = surface_in->Info.CropX + surface_in->Info.CropW;
    inH = surface_in->Info.CropY + surface_in->Info.CropH;
    mfxFrameSurface1 work420_surface;
    memset(&work420_surface, 0, sizeof(mfxFrameSurface1));
    ptir_memcpy_s(&(work420_surface.Info), sizeof(work420_surface.Info), &(surface_in->Info), sizeof(surface_in->Info));
    work420_surface.Info.FourCC = MFX_FOURCC_I420;
    mfxU16& w = inW;
    mfxU16& h = inH;
    work420_surface.Data.Y = buffer;
    work420_surface.Data.U = work420_surface.Data.Y+w*h;
    work420_surface.Data.V = work420_surface.Data.U+w*h/4;
    work420_surface.Data.Pitch = w;

    return ColorSpaceConversionWCopy(surface_in, &work420_surface, MFX_FOURCC_I420);
}