/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include <stdio.h>
#include "vm_file.h"
#include "mfx_io_utils.h"
#include "app_defs.h"
#include <memory>

#define MFX_TIME_STAMP_INVALID   (mfxU64)-1; 

inline mfxF64 my_fabs(mfxF64 f)
{
    if (f < 0) f = -f;
    return f;
}

mfxStatus FrameRate2Code(mfxF64 framerate, mfxFrameInfo* info)
{
    mfxU32 fr;
    if (!info)
        return MFX_ERR_NULL_PTR;

    fr = (mfxU32)(framerate + .5);
    if (my_fabs(fr - framerate) < 0.0001) {
        info->FrameRateExtN = fr;
        info->FrameRateExtD = 1;
        return MFX_ERR_NONE;
    }

    fr = (mfxU32)(framerate * 1.001 + .5);
    if (my_fabs(fr * 1000 - framerate * 1001) < 10) {
        info->FrameRateExtN = fr * 1000;
        info->FrameRateExtD = 1001;
        return MFX_ERR_NONE;
    }

    // can do more
    // simple for now
    info->FrameRateExtN = (mfxU32)(framerate * 10000 + .5);
    info->FrameRateExtD = 10000;
    return MFX_ERR_NONE;
}

mfxF64 GetFrameRate(mfxFrameInfo* info)
{
    if (!info || info->FrameRateExtD == 0)
        return 0;
    return ( (mfxF64)info->FrameRateExtN / info->FrameRateExtD);
}

///////////////////////////////////////////////////////////////////////////////////////

CYUVReader::CYUVReader()
{
    m_fSource = 0;
}

mfxStatus CYUVReader::Init(const vm_char *strFileName)
{
    Close();

    CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    //CHECK_ERROR(_tclen(strFileName), 0, ERROR_PARAMS);

    m_fSource = vm_file_fopen(strFileName, VM_STRING("rb"));
    CHECK_POINTER(m_fSource, ERROR_FILE_OPEN);

    return ERROR_NOERR;
}

CYUVReader::~CYUVReader()
{
    Close();
}

void CYUVReader::Close()
{
    if (m_fSource != 0)
    {
        vm_file_fclose(m_fSource);
        m_fSource = 0;
    }
}

mfxU32 CYUVReader::RawRead(mfxU8 *buff, mfxU32 count)
{
    return vm_file_fread(buff, 1, count, m_fSource);
}

//mfxU32  Init(const vm_char *strFileName);
//mfxU32  LoadNextFrame(mfxU8* pBuffer, mfxU32 buferSize);
// Requires FrameSuface::FrameInfo, not VideoParams'
mfxStatus CYUVReader::LoadNextFrame(mfxFrameData* pData, mfxFrameInfo* pInfo)
{
    mfxU32 planeSize;
    CHECK_POINTER(pData, ERROR_NOTINIT);
    CHECK_POINTER(pInfo, ERROR_NOTINIT);
    mfxU32 nBytesRead;
    mfxU32 w, h, i, pitch;
    mfxU8 *ptr, *ptr2;

    if (pInfo->FourCC != MFX_FOURCC_YV12 && pInfo->FourCC != MFX_FOURCC_NV12 && pInfo->FourCC != MFX_FOURCC_P010)
        return MFX_ERR_UNSUPPORTED;

    if (pInfo->CropH > 0 && pInfo->CropW > 0) {
        w = pInfo->CropW;
        h = pInfo->CropH;
    } else {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    if (pInfo->FourCC == MFX_FOURCC_P010) w = w << 1;

    pitch = pData->PitchLow + ((mfxU32)pData->PitchHigh << 16);
    ptr = pData->Y + pInfo->CropX + pInfo->CropY*pitch;
    if(pitch == w) {
        planeSize = w*h;
        nBytesRead = RawRead(ptr, planeSize);
        CHECK_NOT_EQUAL(nBytesRead, planeSize, ERROR_FILE_READ);
    } else {
        for(i=0; i<h; i++) {
            nBytesRead = RawRead(ptr + i*pitch, w);
            CHECK_NOT_EQUAL(nBytesRead, w, ERROR_FILE_READ);
        }
    }

    w >>= 1;
    h >>= 1;

    if (pInfo->FourCC == MFX_FOURCC_NV12 || pInfo->FourCC == MFX_FOURCC_P010) {
        mfxU32 w_real = pInfo->FourCC == MFX_FOURCC_P010 ? w/2 : w;
        std::auto_ptr<mfxU8> buf(new mfxU8[w]); // maximum supported chroma width for nv12
        mfxU32 j;

        ptr = pData->UV + (pInfo->CropX) + (pInfo->CropY>>1)*pitch;
        // load U
        for (i=0; i<h; i++) {
            nBytesRead = RawRead(buf.get(), w);
            CHECK_NOT_EQUAL(nBytesRead, w, ERROR_FILE_READ);
            for (j=0; j<w_real; j++)
            {
                if (pInfo->FourCC == MFX_FOURCC_NV12) *((mfxU8* )ptr + i*pitch+j*2) = *((mfxU8* )buf.get() + j);
                if (pInfo->FourCC == MFX_FOURCC_P010) *((mfxU16*)ptr + i*pitch/2 + j*2) = *((mfxU16*)buf.get() + j);
            }
        }
        // load V
        for (i=0; i<h; i++) {
            nBytesRead = RawRead(buf.get(), w);
            CHECK_NOT_EQUAL(nBytesRead, w, ERROR_FILE_READ);
            for (j=0; j<w_real; j++)
            {
                if (pInfo->FourCC == MFX_FOURCC_NV12) *((mfxU8* )ptr + i*pitch+j*2+1) = *((mfxU8* )buf.get() + j);
                if (pInfo->FourCC == MFX_FOURCC_P010) *((mfxU16*)ptr + i*pitch/2+j*2+1) = *((mfxU16*)buf.get() + j);
            }

        }
        return ERROR_NOERR;
    }

    mfxU32 cropX =  pInfo->CropX>>1;
    pitch >>= 1;

    ptr  = pData->U + cropX + (pInfo->CropY >>1)*pitch;
    ptr2 = pData->V + cropX + (pInfo->CropY>> 1)*pitch;
    if(pitch == w) {
        planeSize = w*h;
        nBytesRead = RawRead(ptr, planeSize);
        CHECK_NOT_EQUAL(nBytesRead, planeSize, ERROR_FILE_READ);
        nBytesRead = RawRead(ptr2, planeSize);
        CHECK_NOT_EQUAL(nBytesRead, planeSize, ERROR_FILE_READ);
    } else {
        for(i=0; i<h; i++) {
            nBytesRead = RawRead(ptr + i*pitch, w);
            CHECK_NOT_EQUAL(nBytesRead, w, ERROR_FILE_READ);
        }
        for(i=0; i<h; i++) {
            nBytesRead = RawRead(ptr2 + i*pitch, w);
            CHECK_NOT_EQUAL(nBytesRead, w, ERROR_FILE_READ);
        }
    }

    return ERROR_NOERR;
}

mfxStatus CYUVReader::LoadBlock(mfxU8* pSubPicInFrame, mfxU32 wSubPic, mfxU32 hSubPic, mfxU32 FramePitch)
{
    CHECK_ERROR(pSubPicInFrame, false, ERROR_NOTINIT);
    mfxU32 nBytesRead   = 0;
    mfxU32 i = 0;
    for (i = 0 ; i < hSubPic; i++)
    {
        nBytesRead = RawRead(pSubPicInFrame, wSubPic);
        CHECK_NOT_EQUAL(nBytesRead, wSubPic, ERROR_FILE_READ);
        pSubPicInFrame += FramePitch;
    }

    return ERROR_NOERR;
}

/////////////////////////////////////////////////////////////////////////////////////////

CBitstreamReader::CBitstreamReader()
    : m_fSource()
{
}

CBitstreamReader::~CBitstreamReader()
{
    Close();
}

void CBitstreamReader::Close()
{
    if (m_fSource)
    {
        vm_file_close(m_fSource);
        m_fSource = NULL;
    }

    m_bInited = false;
}

mfxStatus CBitstreamReader::Init(const vm_char *strFileName)
{
    CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    CHECK_ERROR(vm_string_strlen(strFileName), 0, MFX_ERR_UNKNOWN);

    Close();

    //open file to read input stream
    m_fSource = vm_file_fopen(strFileName, VM_STRING("rb"));
    CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);

    m_bInited = true;
    return MFX_ERR_NONE;
}

mfxU32 CBitstreamReader::RawRead(mfxU8 *buff, mfxU32 count)
{
    return vm_file_fread(buff, 1, count, m_fSource);
}

mfxStatus CBitstreamReader::ReadNextFrame(mfxBitstream *pBS)
{
    CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    mfxU32 nBytesRead = 0;

    memcpy(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;
    //invalid timestamp by default, to make decoder correctly put the timestamp
    pBS->TimeStamp = MFX_TIME_STAMP_INVALID;
    nBytesRead = RawRead(pBS->Data + pBS->DataLength, pBS->MaxLength - pBS->DataLength);
    if (!nBytesRead)
    {
        if (pBS->DataFlag & MFX_BITSTREAM_EOS)
            return MFX_ERR_UNKNOWN;
        pBS->DataFlag |= MFX_BITSTREAM_EOS;
    }

    pBS->DataLength += nBytesRead;
    return MFX_ERR_NONE;
}

mfxStatus CBitstreamReader::SetFilePos(mfxI64 nPos, VM_FILE_SEEK_MODE seek_flag)
{
    return 0 == vm_file_fseek(m_fSource, nPos, seek_flag)? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////////////////////

CBitstreamWriter::CBitstreamWriter()
//#ifdef LUCAS_DLL
//: m_lucas(&lucas::CLucasCtx::Instance())
//#endif
{
    m_fSource = 0;
}

CBitstreamWriter::~CBitstreamWriter()
{
    Close();
}

void CBitstreamWriter::Close()
{
//#ifdef LUCAS_DLL
//    if (m_fSource && !(*m_lucas)->output)
//#else
    if (m_fSource)
//#endif
    {
        vm_file_close(m_fSource);
        m_fSource = 0;
    }
}

mfxStatus CBitstreamWriter::Init(const vm_char *strFileName)
{
    CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

    Close();

//#ifdef LUCAS_DLL
//    if(!(*m_lucas)->output) {
//#endif
    //init file to write encoded data
     m_fSource = vm_file_fopen(strFileName, VM_STRING("wb+"));
    CHECK_POINTER(m_fSource, ERROR_FILE_OPEN);
//#ifdef LUCAS_DLL
//    }
//#endif

    return MFX_ERR_NONE;
}

mfxStatus CBitstreamWriter::WriteNextFrame(mfxBitstream *Bitstream)
{
    mfxU32 nBytesWritten = 0;

    CHECK_POINTER(Bitstream, MFX_ERR_NULL_PTR);
//#ifdef LUCAS_DLL
//    if(!(*m_lucas)->output) {
//#endif
    CHECK_ERROR(m_fSource, 0, MFX_ERR_NULL_PTR);

    nBytesWritten = (mfxU32)vm_file_fwrite(Bitstream->Data, 1, Bitstream->DataLength, m_fSource);     //write encoded frame
    vm_file_fflush(m_fSource);
//#ifdef LUCAS_DLL
//    }
//    else {
//        (*m_lucas)->output((*m_lucas)->fmWrapperPtr, 0, (char *)Bitstream->Data, Bitstream->DataLength, 0);
//        nBytesWritten = Bitstream->DataLength;
//    }
//#endif

    if (nBytesWritten != Bitstream->DataLength)                                             //check result
    {
        return ERROR_FILE_READ; // write really
    }

    // need to clear, probably not here
    Bitstream->DataLength = 0;
    Bitstream->DataFlag = 0;
    //Bitstream->HeaderSize = 0;
    Bitstream->DataOffset = 0;

    return MFX_ERR_NONE;
}
