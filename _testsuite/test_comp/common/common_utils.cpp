//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2016 Intel Corporation. All Rights Reserved.
//

#include "common_utils.h"

// =================================================================
// Utility functions, not directly tied to Intel Media SDK functionality
//

mfxStatus ReadPlaneData(mfxU16 w, mfxU16 h, mfxU8 *buf, mfxU8 *ptr, mfxU16 pitch, mfxU16 offset, FILE* fSource)
{
    mfxU32 nBytesRead;
    for (mfxU16 i = 0; i < h; i++) 
    {
        nBytesRead = (mfxU32)fread(buf, 1, w, fSource);
        if (w != nBytesRead)
            return MFX_ERR_MORE_DATA;
        for (mfxU16 j = 0; j < w; j++)
            ptr[i * pitch + j * 2 + offset] = buf[j];
    }
    return MFX_ERR_NONE;
}

mfxStatus LoadRawFrame(mfxFrameSurface1* pSurface, FILE* fSource, mfxU16 width, mfxU16 height, bool /*bSim*/)
{/*
    if(bSim) {
        // Simulate instantaneous access to 1000 "empty" frames. 
        static int frameCount = 0;
        if(1000 == frameCount++) return MFX_ERR_MORE_DATA;
        else return MFX_ERR_NONE;   
    }
    */
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 nBytesRead;
    mfxU16 w, h, i, pitch; 
    mfxU8 *ptr;
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;

    w = width;
    h = height;

    /* IF we have RGB4/AYUV/Y210/Y410 input*/
    if (pInfo->FourCC == MFX_FOURCC_RGB4 || pInfo->FourCC == MFX_FOURCC_AYUV || pInfo->FourCC == MFX_FOURCC_Y210 || pInfo->FourCC == MFX_FOURCC_Y410)
    {
        ptr = TESTCOMP_MIN( TESTCOMP_MIN(pSurface->Data.R, pSurface->Data.G), pSurface->Data.B );
        //ptr = ptr + pInfo->CropX + pInfo->CropY * pData->Pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pSurface->Data.Pitch, 1, w*4, fSource);
            if ((mfxU32) w*4 != nBytesRead)
                return MFX_ERR_MORE_DATA;
        }
        return MFX_ERR_NONE;
    }

    /* ELSE we have NV12 or YV12 format*/
    pitch = pData->Pitch;
    //ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;
    ptr = pData->Y;

    // read luminance plane
    for(i = 0; i < h; i++) 
    {
        nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, fSource);
        if (w != nBytesRead)
            return MFX_ERR_MORE_DATA;
    }

    //ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;
    ptr = pData->UV;
    if (pInfo->FourCC == MFX_FOURCC_YV12)
    {
        mfxU8 buf[2048]; // maximum supported chroma width for nv12
        w /= 2;
        h /= 2;
        if (w > 2048)
            return MFX_ERR_UNSUPPORTED;

        // load U
        sts = ReadPlaneData(w, h, buf, ptr, pitch, 0, fSource);
        if(MFX_ERR_NONE != sts)
            return sts;

        // load V
        ReadPlaneData(w, h, buf, ptr, pitch, 1, fSource);
        if(MFX_ERR_NONE != sts)
            return sts;
    }
    else if (pInfo->FourCC == MFX_FOURCC_NV12)
    {
        for(i = 0; i < h/2; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, fSource);
            if (w != nBytesRead)
                return MFX_ERR_MORE_DATA;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus LoadRawRGBFrame(mfxFrameSurface1* pSurface, FILE* fSource, bool bSim)
{
    if(bSim) {
        // Simulate instantaneous access to 1000 "empty" frames. 
        static int frameCount = 0;
        if(1000 == frameCount++) return MFX_ERR_MORE_DATA;
        else return MFX_ERR_NONE;   
    }

    size_t nBytesRead;
    mfxU16 w, h; 
    mfxFrameInfo* pInfo = &pSurface->Info;

//    if (pInfo->CropH > 0 && pInfo->CropW > 0) {
//        w = pInfo->CropW;
//        h = pInfo->CropH;
//    }
//    else {
        w = pInfo->Width;
        h = pInfo->Height;
//    }

    for(mfxU16 i = 0; i < h; i++) 
    {
        nBytesRead = (mfxU32)fread(pSurface->Data.B + i * pSurface->Data.Pitch, 1, w*4, fSource);
        if ((mfxU32) w*4 != nBytesRead)
            return MFX_ERR_MORE_DATA;
    }

    return MFX_ERR_NONE;   
}

mfxStatus WriteBitStreamFrame(mfxBitstream *pMfxBitstream, FILE* fSink)
{    
    mfxU32 nBytesWritten = (mfxU32)fwrite(pMfxBitstream->Data + pMfxBitstream->DataOffset, 1, pMfxBitstream->DataLength, fSink);
    if(nBytesWritten != pMfxBitstream->DataLength)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    pMfxBitstream->DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus ReadBitStreamData(mfxBitstream *pBS, FILE* fSource)
{
    memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;

    mfxU32 nBytesRead = (mfxU32)fread(pBS->Data + pBS->DataLength, 1, pBS->MaxLength - pBS->DataLength, fSource);
    if (0 == nBytesRead)
        return MFX_ERR_MORE_DATA;

    pBS->DataLength += nBytesRead;    

    return MFX_ERR_NONE;
}

mfxStatus WriteSection(mfxU8* plane, mfxU16 factor, mfxU16 chunksize, mfxFrameInfo *pInfo, mfxFrameData *pData, mfxU32 i, mfxU32 j, FILE* fSink)
{
    if(chunksize != fwrite( plane + (pInfo->CropY * pData->Pitch / factor + pInfo->CropX)+ i * pData->Pitch + j,
                            1,
                            chunksize,
                            fSink))
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    return MFX_ERR_NONE;
}

mfxStatus WriteRawFrame(mfxFrameSurface1 *pSurface, FILE* fSink)
{
    mfxFrameInfo *pInfo = &pSurface->Info;
    mfxFrameData *pData = &pSurface->Data;
    mfxU32 i, j, h, w;
    mfxStatus sts = MFX_ERR_NONE;

    /* IF we have RGB4/AYUV/Y210/Y410 output*/
    if (pInfo->FourCC == MFX_FOURCC_RGB4 || pInfo->FourCC == MFX_FOURCC_AYUV || pInfo->FourCC == MFX_FOURCC_Y210 || pInfo->FourCC == MFX_FOURCC_Y410)
    {
        h = pInfo->CropH;
        w = pInfo->CropW;

        for(i = 0; i < h; i++)
        {
            size_t nBytesWrite;
            //if (w*4 != nBytesRead)
            //    return MFX_ERR_MORE_DATA;
            mfxU8* ptr = TESTCOMP_MIN( TESTCOMP_MIN(pData->R, pData->G), pData->B );
            ptr = ptr + pInfo->CropX + pInfo->CropY * pData->Pitch;

            for(i = 0; i < h; i++)
            {
                nBytesWrite = fwrite(ptr + i * pData->Pitch, 1, 4*w, fSink);
                if (w*4 != nBytesWrite)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        return MFX_ERR_NONE;
    }

    /* ELSE we have NV12 or YV12 ouput*/
    for (i = 0; i < pInfo->CropH; i++)
        sts = WriteSection(pData->Y, 1, pInfo->CropW, pInfo, pData, i, 0, fSink);

    h = pInfo->CropH / 2;
    w = pInfo->CropW;
    if (pInfo->FourCC == MFX_FOURCC_YV12)
    {
        for (i = 0; i < h; i++)
        {
            for (j = 0; j < w; j += 2)
            {
                sts = WriteSection(pData->UV, 2, 1, pInfo, pData, i, j, fSink);
                if(MFX_ERR_NONE != sts)
                    return sts;
            }
        }
        for (i = 0; i < h; i++)
        {
            for (j = 1; j < w; j += 2)
            {
                sts = WriteSection(pData->UV, 2, 1, pInfo, pData, i, j, fSink);
                if(MFX_ERR_NONE != sts)
                    return sts;
            }
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_NV12)
    {
        for (i = 0; i < (mfxU32) pInfo->CropH/2; i++)
        {
            sts = WriteSection(pData->UV, 1, pInfo->CropW, pInfo, pData, i, 0, fSink);
            if(MFX_ERR_NONE != sts)
                return sts;
        }
    }


    return sts;
}
