/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2011 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __MFX_IO_UTILS_H
#define __MFX_IO_UTILS_H

#include "mfxvideo.h"
#include "vm_file.h"
#include "vm_strings.h"
#include "lucas.h"

mfxStatus FrameRate2Code(mfxF64 framerate, mfxFrameInfo* info);
mfxF64    GetFrameRate(mfxFrameInfo* info);

mfxStatus InitMfxBitstream(mfxBitstream *pBitstream, mfxVideoParam* pParams);
mfxStatus InitMfxFrameSurface(mfxFrameSurface1* pSurface, const mfxFrameInfo* pFrameInfo, mfxU32* pFrameSize, bool bPadding = false);

class CYUVReader
{
public :
    CYUVReader();
    virtual ~CYUVReader();

    virtual void    Close();
    virtual mfxStatus  Init(const vm_char *strFileName);
    virtual mfxStatus  LoadNextFrame(mfxFrameData* pData, mfxFrameInfo* pInfo);
    virtual mfxStatus  LoadBlock(mfxU8* pSubPicInFrame, mfxU32 wSubPic, mfxU32 hSubPic, mfxU32 FramePitch);

protected:
    //unchecked read from input
    virtual mfxU32 RawRead(mfxU8 *buff, mfxU32 count);
    vm_file*       m_fSource;
};

class CBitstreamWriter
{
public :

    CBitstreamWriter();
    ~CBitstreamWriter();

    void    Close();
    mfxStatus  Init(const vm_char *strFileName);
    mfxStatus  WriteNextFrame(mfxBitstream *Bitstream);

private:
#ifdef LUCAS_DLL
    lucas::CLucasCtx *m_lucas;
#endif

    vm_file*       m_fSource;
};

class CBitstreamReader
{
public :
    CBitstreamReader();
    virtual ~CBitstreamReader();

    virtual void      Close();
    virtual mfxStatus Init(const vm_char *strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream *pBS);
    virtual mfxStatus SetFilePos(mfxI64 nPos, VM_FILE_SEEK_MODE seek_flag);
protected:
    //unchecked read from input
    virtual mfxU32 RawRead(mfxU8 *buff, mfxU32 count);
    
    vm_file*    m_fSource;
    bool        m_bInited;
};

#endif // __MFX_IO_UTILS_H
