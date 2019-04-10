/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: mfx_adapters.h

*******************************************************************************/

#pragma  once

#include "mfx_iadapter.h"
#include "mfx_ibitstream_writer.h"
#include "mfx_ipipeline.h"
#include "mfx_iadapter.h"
#include "mfx_iprocess_command.h"
#include "mfx_iallocator_factory.h"

//adapter to process command
template<>
class Adapter<IProcessCommand, IMFXPipeline>  
    : public AdapterBase<IProcessCommand, IMFXPipeline>
{
public:
    Adapter(IMFXPipeline *pDecPipeline) : AdapterBase<IProcessCommand, IMFXPipeline>(pDecPipeline){}
    ~Adapter()
    {
        //no need to delete pipeline
        m_pTo.release();
    }
    virtual mfxStatus  ProcessCommand( vm_char **argv, mfxI32 argc)
    {
        MFX_CHECK_POINTER(m_pTo.get());
        return m_pTo->ProcessCommand(argv, argc);
    }
    virtual bool IsPrintParFile()
    {
        return true;
    }
};


//todo make this adapter like
class DecPipeline2ReconstructInput
    : public Adapter<IProcessCommand, IMFXPipeline>
{
    void * m_pReconstructed;
public:
    DecPipeline2ReconstructInput(IMFXPipeline *pDecPipeline, void* pReconArgs) 
        : Adapter<IProcessCommand, IMFXPipeline>  (pDecPipeline)
        , m_pReconstructed(pReconArgs)
    {
    }
    virtual mfxStatus  ProcessCommand( vm_char **argv, mfxI32 argc)
    {
        MFX_CHECK_POINTER(m_pTo.get());
        return m_pTo->ReconstructInput(argv, argc, m_pReconstructed);
    }
    virtual bool IsPrintParFile()
    {
        return false;
    }
};

template <>
class Adapter<IFile, IBitstreamWriter>
    : public AdapterBase<IFile, IBitstreamWriter>
{
    IMPLEMENT_CLONE(Adapter);
    typedef AdapterBase<IFile, IBitstreamWriter> base;
public:
    Adapter(IBitstreamWriter *pTo)
        : base(pTo)
    {
    }

    //IFile implementation
    virtual mfxStatus GetLastError() {return MFX_ERR_NONE;}
    virtual mfxStatus GetInfo(const tstring & /*path*/, mfxU64 & /*attr*/) {return MFX_ERR_NONE;}
    virtual mfxStatus Open(const tstring & /*path*/, const tstring& /*access*/){return MFX_ERR_NONE;}
    virtual bool      isOpen(){return true;}
    virtual bool      isEOF(){return false;}
    virtual mfxStatus Close(){return MFX_ERR_NONE;}
    virtual mfxStatus Read(mfxU8* /*p*/, mfxU32 & /*size*/){return MFX_ERR_NONE;}
    virtual mfxStatus ReadLine(vm_char* /*p*/, const mfxU32 & /*size*/){return MFX_ERR_NONE;}
    virtual mfxStatus Reopen(){return MFX_ERR_NONE;}
    virtual mfxStatus Write(mfxU8* p, mfxU32 size)
    {
        mfxBitstream bs = {};
        bs.Data       = p;
        bs.DataLength = size;
        bs.MaxLength  = size;

        return m_pTo->Write(&bs);
    }
    virtual mfxStatus Write(mfxBitstream *pBs)
    {
        return m_pTo->Write(pBs);
    }
    virtual mfxStatus Seek(Ipp64s /*position*/, VM_FILE_SEEK_MODE /*mode*/)
    {
        return MFX_ERR_NONE;
    }
};

//lock frames rw wont create special mid in call to lock
template <>
class Adapter<MFXFrameAllocatorRW, MFXFrameAllocator>
    : public AdapterBase<MFXFrameAllocatorRW, MFXFrameAllocator>
{
    typedef AdapterBase<MFXFrameAllocatorRW, MFXFrameAllocator> base;
public:
    typedef MFXFrameAllocatorRW result_type;
    Adapter(MFXFrameAllocator *pTo)
        : base(pTo)
    {
    }
    virtual mfxStatus Init(mfxAllocatorParams *pParams)
    {
        return m_pTo->Init(pParams);
    }
    virtual mfxStatus Close()
    {
        return m_pTo->Close();
    }
    virtual mfxStatus ReallocFrame(mfxMemId midIn, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut)
    {
        return m_pTo->ReallocFrame(midIn, info, memType, midOut);
    }
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
    {
        return m_pTo->Alloc(m_pTo->pthis, request, response);
    }
    //just drop lock flag in generic allocators
    virtual mfxStatus LockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 /*lockFlag*/)
    {
        return LockFrame(mid, ptr);
    }
    //just drop lock flag in generic allocators
    virtual mfxStatus UnlockFrameRW(mfxMemId mid, mfxFrameData *ptr, mfxU8 /*writeflag*/)
    {
        return UnlockFrame(mid, ptr);
    }
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        return m_pTo->LockFrame(mid, ptr);
    }
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
    {
        return m_pTo->UnlockFrame(mid, ptr);
    }
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle)
    {
        return m_pTo->GetFrameHDL(mid, handle);
    }
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response)
    {
        return m_pTo->FreeFrames(response);
    }
};

//adapter for factory->factoryrw
template<class TFrom, class TTo>
class Adapter <IAllocatorFactoryTmpl<TFrom>, IAllocatorFactoryTmpl<TTo> >
    : public AdapterBase<IAllocatorFactoryTmpl<TFrom>, IAllocatorFactoryTmpl<TTo> >
{
    typedef Adapter<TFrom, TTo> RetTypeAdapter;
public:
    typedef AdapterBase<IAllocatorFactoryTmpl<TFrom>, IAllocatorFactoryTmpl<TTo> > base;
    typedef typename base::base root;
    Adapter(IAllocatorFactoryTmpl<TTo>* pTo)
        : base(pTo)
    {
    }
    virtual TFrom* CreateD3DAllocator()
    {
        return new RetTypeAdapter (base::m_pTo->CreateD3DAllocator());
    }
    virtual TFrom* CreateD3D11Allocator()
    {
        return new RetTypeAdapter (base::m_pTo->CreateD3D11Allocator());
    }
    virtual TFrom* CreateVAAPIAllocator()
    {
        return new RetTypeAdapter (base::m_pTo->CreateVAAPIAllocator());
    }
    virtual TFrom* CreateSysMemAllocator()
    {
        return new RetTypeAdapter (base::m_pTo->CreateSysMemAllocator());
    }
};
