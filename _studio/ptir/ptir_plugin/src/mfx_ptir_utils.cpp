/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_ptir_utils.cpp

\* ****************************************************************************** */

#include "ptir_vpp_plugin.h"
#include "ippcc.h"

mfxStatus ColorSpaceConversionWCopy(mfxFrameSurface1* surface_in, mfxFrameSurface1* surface_out, mfxU32 dstFormat)
{
    IppStatus status;
    mfxU32 srcFormat = surface_in->Info.FourCC;


    switch (srcFormat) {
    case MFX_FOURCC_NV12:
        switch (dstFormat) {
        case MFX_FOURCC_I420:
            {
                const Ipp8u *pSrcY = (Ipp8u*)surface_in->Data.Y;
                const Ipp8u *pSrcUV = (Ipp8u*)surface_in->Data.UV;
                Ipp32u pSrcYStep = surface_in->Info.CropW;
                //Ipp32u pSrcUVStep = surface_in->Info.CropW / 2;
                Ipp32u pSrcUVStep = surface_in->Info.CropW;

                Ipp8u *(pDst[3]) = {(Ipp8u*)surface_out->Data.Y,
                      (Ipp8u*)surface_out->Data.U,
                      (Ipp8u*)surface_out->Data.V};

                int pDstStep[3] = { surface_out->Data.Pitch,
                                    surface_out->Data.Pitch/2,
                                    surface_out->Data.Pitch/2};

                IppiSize roiSize = {surface_in->Info.CropW, surface_in->Info.CropH};

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

                int pSrcStep[3] = {surface_in->Info.CropW,
                                   surface_in->Info.CropW / 2,
                                   surface_in->Info.CropW / 2};

                Ipp8u *pDstY = (Ipp8u*)surface_out->Data.Y;
                Ipp8u *pDstUV = (Ipp8u*)surface_out->Data.UV;
                Ipp32u pDstYStep = surface_out->Info.CropW;
                Ipp32u pDstUVStep = surface_out->Info.CropW;

                IppiSize roiSize = {surface_in->Info.CropW, surface_in->Info.CropH};

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