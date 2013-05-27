//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2010 Intel Corporation. All Rights Reserved.
//
#include "cc_utils.h"
#include "app_defs.h"



mfxStatus CF_GetChromaFormat(mfxU32 FourCC, mfxU8 *pChromaFormat)
{
    CHECK_POINTER(pChromaFormat, MFX_ERR_NULL_PTR);

    switch(FourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        *pChromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case MFX_FOURCC_RGB3:
        *pChromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    default:
       *pChromaFormat = 0;
       return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus GetPitch(mfxU32 FourCC, mfxU32 Width, mfxU16* pPitch)
{
    CHECK_POINTER(pPitch, MFX_ERR_NULL_PTR);

    switch(FourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        *pPitch = (mfxU16)Width;
        break;
    case MFX_FOURCC_RGB3:
        *pPitch = (mfxU16)(Width*3);
        break;
    default:
       *pPitch = 0;
       return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}


mfxStatus GetFramePointers(mfxU32 FourCC, mfxU16 w, mfxU16 h, mfxU8 *pBuffer,
                           mfxU8** pPointers)
{
  CHECK_POINTER(pPointers, MFX_ERR_NULL_PTR);

   switch(FourCC)
    {
    case MFX_FOURCC_YV12:
        pPointers[0] = pBuffer;
        pPointers[2] = pBuffer + w*h;
        pPointers[1] = pPointers[2] + ((w*h)>>2);
        break;
    case MFX_FOURCC_NV12:
        pPointers[0] = pBuffer;
        pPointers[1] = pBuffer + w*h;
        pPointers[2] = pPointers[1] + 1;
        break;
    case MFX_FOURCC_RGB3:
        pPointers[0] = pBuffer + 1;
        pPointers[1] = pBuffer + 0;
        pPointers[2] = pBuffer + 2;
        break;
    default:
        pPointers[0] = 0;
        pPointers[1] = 0;
        pPointers[2] = 0;
       return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus GetPlanes(mfxU32 FourCC, mfxU8* pNPlanes, mfxU8* pFrameNum)
{
   CHECK_POINTER(pNPlanes, MFX_ERR_NULL_PTR);

   switch(FourCC)
    {
    case MFX_FOURCC_YV12:
        *pNPlanes = 3;
        pFrameNum[0] = 0;
        pFrameNum[1] = 2;
        pFrameNum[2] = 1;
        break;
    case MFX_FOURCC_NV12:
        *pNPlanes = 2;
        pFrameNum[0] = 0;
        pFrameNum[1] = 1;
        pFrameNum[2] = 2;
        break;
    case MFX_FOURCC_RGB3:
        *pNPlanes = 1;
        pFrameNum[0] = 1;
        break;
    default:
       *pNPlanes = 0;
       return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}
mfxI32 GetSubPicOfset(mfxI16 CropX, mfxI16 CropY, mfxU16 Pitch, mfxU16 Step)
{
    return (CropY*Pitch + CropX*Step);
}
