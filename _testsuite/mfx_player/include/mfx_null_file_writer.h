/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ifile.h"

//class works as dungle in filewriting
//known examples: crc calculating without writing yuv/bitstream to file
class NullFileWriter
    : public IFile
{
    IMPLEMENT_CLONE(NullFileWriter);
public:
    NullFileWriter()
    {
    }
    virtual mfxStatus GetLastError() {return MFX_ERR_NONE;}
    virtual mfxStatus GetInfo(const tstring & /*path*/, mfxU64 & /*attr*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Open(const tstring & /*path*/, const tstring& /*access*/) 
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Reopen()
    {
        return MFX_ERR_NONE;
    }
    virtual bool isOpen()
    {
        return true;
    }
    virtual bool isEOF()
    {
        return false;
    }
    virtual mfxStatus Close()
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Read(mfxU8* /*p*/, mfxU32 &size)
    {
        size = 0;
        return MFX_ERR_NONE;
    }
    virtual mfxStatus ReadLine(vm_char*  /*p*/, const mfxU32 & /*size*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Write(mfxU8* /*p*/, mfxU32 /*size*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Write(mfxBitstream * /*pBs*/)
    {
        return MFX_ERR_NONE;
    }
};