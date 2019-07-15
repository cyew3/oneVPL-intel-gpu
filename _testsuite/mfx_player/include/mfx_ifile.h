/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxvideo++.h"
#include "mfx_iclonebale.h"
#include "mfx_pipeline_types.h"
#include "mfx_iproxy.h"
#include "mfx_ibitstream_writer.h"
#include "mfx_shared_ptr.h"

//generic file access abstaction, for both binary and text files
#pragma warning (disable: 4505)

struct IFile
    : EnableSharedFromThis<IFile>
    , public EnableProxyForThis<IFile>
    , ICloneable
    , IBitstreamWriter
{
public:
    //icloneable
    virtual IFile * Clone()  =  0;

    //file attributes without opening file
    virtual mfxStatus GetInfo(const tstring & path, mfxU64 &attr) = 0;
    // contract:
    // windows error 1450 = MFX_WRN_DEVICE_BUSY
    // other error except 0 - should break execution
    virtual mfxStatus GetLastError() = 0 ;
    virtual mfxStatus Open(const tstring & path, const tstring& access) = 0;
    virtual mfxStatus Reopen() = 0;
    virtual bool isEOF () = 0;
    virtual bool isOpen() = 0;
    virtual mfxStatus Close() = 0;
    // Helper method to take some actions on the frame's boundary
    // E.g., calculate the hashes per frame, print elapsed time
    // between frames to see if sort of stall took place.
    virtual void FrameBoundary() {}
    /* size - in,out*/
    virtual mfxStatus Read(mfxU8* p, mfxU32 &size) = 0;
    //zero terminated string
    virtual mfxStatus ReadLine(vm_char*pLine, const mfxU32 &size) = 0;
    virtual mfxStatus Write(mfxU8* p, mfxU32 size) = 0;
    //generic filewriter may not use additional flags, however having this method simplify complicated pipelines
    //, say encode->decode providing timestamps, and frametype to decoder
    virtual mfxStatus Write(mfxBitstream * pBs) = 0;
    virtual mfxStatus Seek(Ipp64s position, VM_FILE_SEEK_MODE mode) = 0;
};


//generic proxy for ifile interface
template<>
class InterfaceProxy<IFile>
    : public InterfaceProxyBase<IFile>
{
    IMPLEMENT_CLONE(InterfaceProxy<IFile>);
public:
    InterfaceProxy(std::unique_ptr<IFile> &&pTargetFile)
        : InterfaceProxyBase<IFile>(std::move(pTargetFile))
    {
    }
    virtual mfxStatus GetInfo(const tstring & path, mfxU64 &attr)
    {
        return m_pTarget->GetInfo(path, attr);
    }
    virtual mfxStatus Open(const tstring & path, const tstring& access)
    {
        return m_pTarget->Open(path, access);
    }
    virtual mfxStatus GetLastError()
    {
        return m_pTarget->GetLastError();
    }
    virtual bool isOpen()
    {
        return m_pTarget->isOpen();
    }
    virtual bool isEOF()
    {
        return m_pTarget->isEOF();
    }
    virtual mfxStatus Reopen()
    {
        return m_pTarget->Reopen();
    }
    virtual mfxStatus Close()
    {
        return m_pTarget->Close();
    }
    virtual mfxStatus Read(mfxU8* p, mfxU32 &size)
    {
        return m_pTarget->Read(p, size);
    }
    virtual mfxStatus ReadLine(vm_char* p, const mfxU32 &size)
    {
        return m_pTarget->ReadLine(p, size);
    }
    virtual mfxStatus Write(mfxU8* p, mfxU32 size)
    {
        return m_pTarget->Write(p, size);
    }
    virtual mfxStatus Write(mfxBitstream * pBs)
    {
        return m_pTarget->Write(pBs);
    }
    virtual mfxStatus Seek(Ipp64s position, VM_FILE_SEEK_MODE mode)
    {
        return m_pTarget->Seek(position, mode);
    }

protected:
    InterfaceProxy(const InterfaceProxy<IFile> &that)
        : InterfaceProxyBase<IFile>(that.m_pTarget->Clone())
    {
    }
};
