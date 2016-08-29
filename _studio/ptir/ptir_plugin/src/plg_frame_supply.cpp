/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: plg_frame_supply.cpp

\* ****************************************************************************** */

#include "ptir_wrap.h"

frameSupplier::frameSupplier(std::vector<mfxFrameSurface1*>* _inSurfs,
                             std::vector<mfxFrameSurface1*>* _workSurfs,
                             std::vector<mfxFrameSurface1*>* _outSurfs, 
                             std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap,
                             CmDeviceEx* _pCMdevice, 
                             mfxCoreInterface* _mfxCore, 
                             mfxU16 _IOPattern, 
                             bool _isD3D11, 
                             UMC::Mutex& _guard)
    : pCMdevice(_pCMdevice),
      m_pCmQueue(0),
      mfxCoreIfce(_mfxCore),
      inSurfs  (_inSurfs),
      workSurfs(_workSurfs),
      outSurfs (_outSurfs),
      CmToMfxSurfmap(_CmToMfxSurfmap),
      frmBuffer(0),
      IOPattern(_IOPattern),
      opqFrTypeIn(0),
      opqFrTypeOut(0),
      isD3D11(_isD3D11),
      m_guard(_guard)
{
}
mfxStatus frameSupplier::SetDevice(CmDeviceEx* _pCMdevice)
{
    mfxI32 result = 0;
    pCMdevice     = _pCMdevice;
    if(_pCMdevice)
        result = (*pCMdevice)->CreateQueue(m_pCmQueue);
    if(!result)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_DEVICE_FAILED;
}
void frameSupplier::SetMap(std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap)
{
    CmToMfxSurfmap = _CmToMfxSurfmap;
    return;
}

void frameSupplier::SetFrmBuffer(Frame *_frmBuffer[LASTFRAME])
{
    frmBuffer = _frmBuffer;
    return;
}

void frameSupplier::SetFrmType(const mfxU16& _in, const mfxU16& _out)
{
    opqFrTypeIn    = _in;
    opqFrTypeOut   = _out;
    return;
}

CmSurface2D* frameSupplier::GetWorkSurfaceCM()
{
    mfxFrameSurface1* s_out = 0;
    CmSurface2D* cm_out = 0;
    if(0 == workSurfs->size())
    {
        assert(false);
        return 0;
    }
    else
    {
        s_out = workSurfs->front();
        workSurfs->erase(workSurfs->begin());
        mfxI32 result = -1;

        std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
        for (it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); ++it)
        {
            if(s_out == it->second)
            {
                cm_out = it->first;
                break;
            }
        }
        if(!cm_out)
        {
            CMCreateSurfaceOut(s_out,cm_out,false);
            //result = (*pCMdevice)->CreateSurface2D((IDirect3DSurface9 *) s_out->Data.MemId, cm_out);
            //assert(result == 0);
            //(*CmToMfxSurfmap)[cm_out] = s_out;
        }
        //result = (*pCMdevice)->CreateSurface2D((IDirect3DSurface9 *) s_out->Data.MemId, cm_out);
        //assert(result == 0);
        //if(result)
        //    std::cout << "CM ERROR while creating CM surface\n";
        //mfxCoreIfce->IncreaseReference(mfxCoreIfce->pthis, &s_out->Data);
        //(*CmToMfxSurfmap)[cm_out] = s_out;
        return cm_out;
    }
    return 0;
}
mfxFrameSurface1* frameSupplier::GetWorkSurfaceMfx()
{
    mfxFrameSurface1* s_out = 0;
    if(0 == workSurfs->size())
        return 0;
    else
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); ++it)
        {
            s_out = *it;
            workSurfs->erase(it);
            break;
        }
        return s_out;
    }
    return 0;
}
mfxStatus frameSupplier::AddCPUPtirOutSurf(mfxU8* buffer, mfxFrameSurface1* surface)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxFrameSurface1* realSurf = surface;
    if(IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxSts = mfxCoreIfce->GetRealSurface(mfxCoreIfce->pthis, surface, &realSurf);
        if(mfxSts)
            return mfxSts;
    }

    bool unlockreq = false;
    if(realSurf->Data.MemId)
    {
        unlockreq = true;
        mfxSts = mfxCoreIfce->FrameAllocator.Lock(mfxCoreIfce->FrameAllocator.pthis, realSurf->Data.MemId, &(realSurf->Data));
        if(mfxSts)
            return mfxSts;
    }
    mfxSts = Ptir420toMfxNv12(buffer, realSurf);
    if(mfxSts)
        return mfxSts;
    if(unlockreq)
    {
        mfxSts = mfxCoreIfce->FrameAllocator.Unlock(mfxCoreIfce->FrameAllocator.pthis, realSurf->Data.MemId, &(realSurf->Data));
        if(mfxSts)
            return mfxSts;
    }
    mfxSts = AddOutputSurf(surface);
    return mfxSts;
}
mfxStatus frameSupplier::AddOutputSurf(mfxFrameSurface1* outSurf, mfxFrameSurface1* exp_surf)
{
#if defined(_DEBUG)
    for(std::vector<mfxFrameSurface1*>::iterator it = outSurfs->begin(); it != outSurfs->end(); ++it)
    {
        assert((*it) != outSurf);
    }
#endif
    CmSurface2D* freeSurf = 0;
    CmSurface2D* delSurf = 0;
    if(pCMdevice && (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        CmSurface2D* cmSurf = 0;
        //change to .find
        std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
        for (it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); ++it)
        {
            if(outSurf == it->second)
            {
                cmSurf = it->first;
                break;
            }
        }
        for (it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); ++it)
        {
            if(exp_surf == it->second)
            {
                delSurf = it->first;
                break;
            }
        }

        assert(0 != cmSurf);
        if(cmSurf)
        {
            if(exp_surf && (outSurf != exp_surf))
            {
                outSurf = exp_surf;
                freeSurf = cmSurf;
            }
            mfxStatus mfxSts = MFX_ERR_NONE;
            mfxSts = CMCopyGpuToSys(cmSurf, outSurf);
            assert(MFX_ERR_NONE == mfxSts);
        }
    }

    //In VPPFrameSubmit function in the begining of processing we "promise" to return a certain frame (plugin does not have any output at this point, so that returned frame is the first work frame).
    //but PTIR generates different output frame
    //This code is for restoring the right order. This code invokes additional CM frame copy (generated -> expected) (slower the execution), but it will happen only once and only in rare number of cases.
    if(exp_surf && (outSurf != exp_surf))
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        CmSurface2D* cmSurfExpOut = 0;
        CmSurface2D* cmSurfOut = 0;
        std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
        for (it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); ++it)
        {
            if(exp_surf == it->second)
            {
                cmSurfExpOut = it->first;
                delSurf = cmSurfExpOut;
                break;
            }
        }
        if(!cmSurfExpOut)
        {
            CMCreateSurfaceOut(exp_surf,cmSurfExpOut,false);
            delSurf = cmSurfExpOut;
        }
        for (it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); ++it)
        {
            if(outSurf == it->second)
            {
                cmSurfOut = it->first;
                break;
            }
        }

        //mfxSts = CMCopyGPUToGpu(cmSurfOut, cmSurfExpOut);
        //assert(MFX_ERR_NONE == mfxSts);
        //outSurf = exp_surf;
        //freeSurf = cmSurfOut;
        mfxSts = CMCopyGPUToGpu(cmSurfExpOut, cmSurfOut);
        assert(MFX_ERR_NONE == mfxSts);
        outSurf = exp_surf;
        freeSurf = cmSurfOut;
    }

    outSurfs->push_back(outSurf);

    if(freeSurf && frmBuffer)
    {
        for(mfxU32 i = 0; i < LASTFRAME; ++i)
        {
            if(frmBuffer[i] && (static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D == freeSurf))
            {
                static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D = 0;
                static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D = GetWorkSurfaceCM();
            }
            if(delSurf && frmBuffer[i] && (static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D == delSurf))
            {
                static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D = 0;
                static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D = GetWorkSurfaceCM();
            }
        }
        FreeSurface(freeSurf);
    }
#if defined(_DEBUG)
    for(std::vector<mfxFrameSurface1*>::iterator wSurf = workSurfs->begin(); wSurf != workSurfs->end(); ++wSurf)
    {
        for(std::vector<mfxFrameSurface1*>::iterator oSurf = outSurfs->begin(); oSurf != outSurfs->end(); ++oSurf)
        {
            assert(*wSurf != *oSurf);
        }
        assert((*wSurf)->Data.Locked != 0);
    }
    for(std::vector<mfxFrameSurface1*>::iterator it = outSurfs->begin(); it != outSurfs->end(); ++it)
    {
        assert((*it)->Data.Locked != 0);
    }
#endif
    return MFX_ERR_NONE;
}
mfxStatus frameSupplier::FreeFrames()
{
    UMC::AutomaticUMCMutex guard(m_guard);
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxU32 iii = 0;
    mfxU32 refs = 0;
    if(CmToMfxSurfmap && pCMdevice)
    {
        if(frmBuffer && *frmBuffer && *frmBuffer)
        {
            for(mfxU32 i = 0; i < LASTFRAME; ++i)
            {
                if(static_cast<CmSurface2DEx*>(frmBuffer[i]->inSurf)->pCmSurface2D)
                    FreeSurface(static_cast<CmSurface2DEx*>(frmBuffer[i]->inSurf)->pCmSurface2D);
                if(static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D)
                    FreeSurface(static_cast<CmSurface2DEx*>(frmBuffer[i]->outSurf)->pCmSurface2D);
            }
        }
        for(std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); ++it) {
            CmSurface2D* cm_surf = it->first;
            //mfxFrameSurface1* mfxSurf = it->second;
            //if(mfxSurf)
            //    mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &mfxSurf->Data);
            if(cm_surf)
            {
                int result = 0;
                try{
                    result = (*pCMdevice)->DestroySurface(cm_surf);
                    assert(result == 0);
                }
                catch(...)
                {
                }
            }
        }
    }
    if(CmToMfxSurfmap)
        CmToMfxSurfmap->clear();

    if(workSurfs && workSurfs->size() > 0)
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); ++it) {
            mfxSts = mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &(*it)->Data);
        }
        workSurfs->clear();
    }
    if(inSurfs && inSurfs->size() > 0)
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = inSurfs->begin(); it != inSurfs->end(); ++it) {
            while((*it)->Data.Locked)
            {
                refs = (*it)->Data.Locked;
                mfxSts = mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &(*it)->Data);
                if(mfxSts) break;
                iii++;
                if(iii > (refs+1)) break; //hang is unacceptable
            }
        }
        inSurfs->clear();
    }
    if(outSurfs && outSurfs->size() > 0)
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = outSurfs->begin(); it != outSurfs->end(); ++it) {
            while((*it)->Data.Locked)
            {
                refs = (*it)->Data.Locked;
                mfxSts = mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &(*it)->Data);
                if(mfxSts) break;
                iii++;
                if(iii > (refs+1)) break; //hang is unacceptable
            }
        }
        outSurfs->clear();
    }

    return mfxSts;
}
mfxStatus frameSupplier::FreeSurface(CmSurface2D*& cmSurf)
{
    if(0 == cmSurf)
        return MFX_ERR_NULL_PTR;
    mfxFrameSurface1* mfxSurf;
    std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it;
    it = CmToMfxSurfmap->find(cmSurf);
    if(CmToMfxSurfmap->end() != it)
    {
        mfxSurf = (*CmToMfxSurfmap)[cmSurf];
        if(mfxSurf)
            mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &mfxSurf->Data);
        //CmToMfxSurfmap->erase(it);
        cmSurf = 0;
    }
    else
    {
        int result = -1;
        result = (*pCMdevice)->DestroySurface(cmSurf);
        assert(0 == result);
        if(result)
            return MFX_ERR_DEVICE_FAILED;
        else
            return MFX_ERR_NONE;
    }
    return MFX_ERR_NONE;
}

mfxStatus frameSupplier::CMCopyGPUToGpu(CmSurface2D* cmSurfOut, CmSurface2D* cmSurfIn)
{
    mfxI32 cmSts = 0;
    CM_STATUS sts;
    mfxStatus mfxSts = MFX_ERR_NONE;
    CmEvent* e = NULL;
    cmSts = m_pCmQueue->EnqueueCopyGPUToGPU(cmSurfOut, cmSurfIn, e);

    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED) 
        {
            e->GetStatus(sts);
        }
    }else{
        mfxSts = MFX_ERR_DEVICE_FAILED;
    }
    if(e) m_pCmQueue->DestroyEvent(e);

    return mfxSts;
}

mfxStatus frameSupplier::CMCopySysToGpu(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut)
{
    if(!mfxSurf)
        return MFX_ERR_NULL_PTR;

    if(mfxSurf->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(mfxSurf->Info.FourCC != MFX_FOURCC_NV12)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    mfxI32 cmSts = CM_SUCCESS;
    CM_STATUS sts;
    mfxStatus mfxSts = MFX_ERR_NONE;

    bool unlock_req = false;
    if(mfxSurf->Data.MemId)
    {
        mfxSts = mfxCoreIfce->FrameAllocator.Lock(mfxCoreIfce->FrameAllocator.pthis, mfxSurf->Data.MemId, &mfxSurf->Data);
        if(MFX_ERR_NONE != mfxSts)
            return mfxSts;
        unlock_req = true;
        if(!mfxSurf->Data.Y || !mfxSurf->Data.UV)
            return MFX_ERR_LOCK_MEMORY;
    }

    mfxI64 verticalPitch = (mfxI64)(mfxSurf->Data.UV - mfxSurf->Data.Y);
    verticalPitch = (verticalPitch % mfxSurf->Data.Pitch)? 0 : verticalPitch / mfxSurf->Data.Pitch;
    mfxI64& srcUVOffset = verticalPitch;
    mfxU32 srcPitch = mfxSurf->Data.Pitch;

    //if(mfxSurf->Info.Width != mfxSurf->Info.CropW)
    //    srcUVOffset = mfxSurf->Info.Width;

    CmEvent* e = NULL;
    if(srcUVOffset != 0){
        cmSts = m_pCmQueue->EnqueueCopyCPUToGPUFullStride(cmSurfOut, mfxSurf->Data.Y, srcPitch, (mfxU32) srcUVOffset, 0, e);
    }
    else{
        cmSts = m_pCmQueue->EnqueueCopyCPUToGPUStride(cmSurfOut, mfxSurf->Data.Y, srcPitch, e);
    }

    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }else{
        mfxSts = MFX_ERR_DEVICE_FAILED;
    }
    if(e) m_pCmQueue->DestroyEvent(e);

    if(mfxSts)
        return mfxSts;

    if(unlock_req)
    {
        mfxSts = mfxCoreIfce->FrameAllocator.Unlock(mfxCoreIfce->FrameAllocator.pthis, mfxSurf->Data.MemId, &mfxSurf->Data);
    }

    return mfxSts;
}

mfxStatus frameSupplier::CMCopyGpuToSys(CmSurface2D*& cmSurfIn, mfxFrameSurface1*& mfxSurf)
{
    if(!mfxSurf)
        return MFX_ERR_NULL_PTR;

    if(mfxSurf->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(mfxSurf->Info.FourCC != MFX_FOURCC_NV12)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    mfxI32 cmSts = CM_SUCCESS;
    CM_STATUS sts;
    mfxStatus mfxSts = MFX_ERR_NONE;

    bool unlock_req = false;
    if(mfxSurf->Data.MemId)
    {
        mfxSts = mfxCoreIfce->FrameAllocator.Lock(mfxCoreIfce->FrameAllocator.pthis, mfxSurf->Data.MemId, &mfxSurf->Data);
        if(MFX_ERR_NONE != mfxSts)
            return mfxSts;
        unlock_req = true;
        if(!mfxSurf->Data.Y || !mfxSurf->Data.UV)
            return MFX_ERR_LOCK_MEMORY;
    }

    mfxI64 verticalPitch = (mfxI64)(mfxSurf->Data.UV - mfxSurf->Data.Y);
    verticalPitch = (verticalPitch % mfxSurf->Data.Pitch)? 0 : verticalPitch / mfxSurf->Data.Pitch;
    mfxI64& dstUVOffset = verticalPitch;
    mfxU32 dstPitch = mfxSurf->Data.Pitch;

    //if(mfxSurf->Info.Width != mfxSurf->Info.CropW)
    //    dstUVOffset = mfxSurf->Info.Width;

    CmEvent* e = NULL;
    if(dstUVOffset != 0){
        cmSts = m_pCmQueue->EnqueueCopyGPUToCPUFullStride(cmSurfIn, mfxSurf->Data.Y, dstPitch, (mfxU32) dstUVOffset, 0, e);
    }
    else{
        cmSts = m_pCmQueue->EnqueueCopyGPUToCPUStride(cmSurfIn, mfxSurf->Data.Y, dstPitch, e);
    }
    if (CM_SUCCESS == cmSts && e)
    {
        e->GetStatus(sts);

        while (sts != CM_STATUS_FINISHED)
        {
            e->GetStatus(sts);
        }
    }else{
        mfxSts = MFX_ERR_DEVICE_FAILED;
    }
    if(e) m_pCmQueue->DestroyEvent(e);

    if(mfxSts)
        return mfxSts;

    if(unlock_req)
    {
        mfxSts = mfxCoreIfce->FrameAllocator.Unlock(mfxCoreIfce->FrameAllocator.pthis, mfxSurf->Data.MemId, &mfxSurf->Data);
    }

    return mfxSts;
}

size_t frameSupplier::countFreeWorkSurfs()
{
    return (*workSurfs).size();
}

mfxStatus frameSupplier::CMCreateSurfaceIn(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut, bool bCopy)
{
    if(!mfxSurf)
        return MFX_ERR_NULL_PTR;

    if(mfxSurf->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(mfxSurf->Info.FourCC != MFX_FOURCC_NV12)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    mfxI32 cmSts = CM_SUCCESS;
    mfxStatus mfxSts = MFX_ERR_NONE;

    mfxFrameSurface1* realSurf = mfxSurf;

    if(IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxSts = mfxCoreIfce->GetRealSurface(mfxCoreIfce->pthis, mfxSurf, &realSurf);
        if(mfxSts)
            return mfxSts;
        if(!realSurf)
            return MFX_ERR_NULL_PTR;
    }

    if(!realSurf->Data.MemId && !(realSurf->Data.Y && realSurf->Data.UV))
        return MFX_ERR_NULL_PTR;

    if((IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) || (opqFrTypeIn & MFX_MEMTYPE_SYSTEM_MEMORY))
    {
        cmSts = (*pCMdevice)->CreateSurface2D(mfxSurf->Info.Width, mfxSurf->Info.Height, CM_SURFACE_FORMAT_NV12, cmSurfOut);
        assert(cmSts == 0);
        if(CM_SUCCESS != cmSts)
            return MFX_ERR_DEVICE_FAILED;
        (*CmToMfxSurfmap)[cmSurfOut] = mfxSurf;

        if(bCopy)
        {
            mfxSts = CMCopySysToGpu(realSurf, cmSurfOut);
            if(MFX_ERR_NONE != mfxSts)
                return mfxSts;
        }
    }
    else if((IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) || ((opqFrTypeIn & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET) || (opqFrTypeIn & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
    {
        mfxHDLPair native_surf = {};
        mfxSts = mfxCoreIfce->FrameAllocator.GetHDL(mfxCoreIfce->FrameAllocator.pthis, realSurf->Data.MemId, (mfxHDL*)&native_surf);
        if(MFX_ERR_NONE > mfxSts)
            return mfxSts;

        if(!isD3D11)
            cmSts = (*pCMdevice)->CreateSurface2D(native_surf.first, cmSurfOut);
        else
            cmSts = (*pCMdevice)->CreateSurface2D( ((mfxHDLPair*) &native_surf)->first, cmSurfOut);

        assert(cmSts == 0);
        if(CM_SUCCESS != cmSts)
            return MFX_ERR_DEVICE_FAILED;
        (*CmToMfxSurfmap)[cmSurfOut] = mfxSurf;

        //useless when associating native video surface to CM surface
        //if(bCopy)
        //    ;
    }

    else
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

mfxStatus frameSupplier::CMCreateSurfaceOut(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut, bool bCopy)
{
    if(!mfxSurf)
        return MFX_ERR_NULL_PTR;

    if(mfxSurf->Info.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if(mfxSurf->Info.FourCC != MFX_FOURCC_NV12)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    mfxI32 cmSts = CM_SUCCESS;
    mfxStatus mfxSts = MFX_ERR_NONE;

    mfxFrameSurface1* realSurf = mfxSurf;

    if(IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxSts = mfxCoreIfce->GetRealSurface(mfxCoreIfce->pthis, mfxSurf, &realSurf);
        if(mfxSts)
            return mfxSts;
    }

    if(!realSurf->Data.MemId && !(realSurf->Data.Y && realSurf->Data.UV))
        return MFX_ERR_NULL_PTR;

    if((IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (opqFrTypeOut & MFX_MEMTYPE_SYSTEM_MEMORY))
    {
        cmSts = (*pCMdevice)->CreateSurface2D(mfxSurf->Info.Width, mfxSurf->Info.Height, CM_SURFACE_FORMAT_NV12, cmSurfOut);
        assert(cmSts == 0);
        if(CM_SUCCESS != cmSts)
            return MFX_ERR_DEVICE_FAILED;
        (*CmToMfxSurfmap)[cmSurfOut] = mfxSurf;

        if(bCopy)
        {
            mfxSts = CMCopySysToGpu(realSurf, cmSurfOut);
            if(MFX_ERR_NONE != mfxSts)
                return mfxSts;
        }
    }
    else if((IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) || ((opqFrTypeOut & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET) || (opqFrTypeOut & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
    {
        mfxHDLPair native_surf = {};
        mfxSts = mfxCoreIfce->FrameAllocator.GetHDL(mfxCoreIfce->FrameAllocator.pthis, realSurf->Data.MemId, (mfxHDL*)&native_surf);
        if(MFX_ERR_NONE > mfxSts)
            return mfxSts;

        if(!isD3D11)
            cmSts = (*pCMdevice)->CreateSurface2D(native_surf.first, cmSurfOut);
        else
            cmSts = (*pCMdevice)->CreateSurface2D( ((mfxHDLPair*) &native_surf)->first, cmSurfOut);

        assert(cmSts == 0);
        if(CM_SUCCESS != cmSts)
            return MFX_ERR_DEVICE_FAILED;
        (*CmToMfxSurfmap)[cmSurfOut] = mfxSurf;

        //useless when associating native video surface to CM surface
        //if(bCopy)
        //    ;
    }
    else
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;
}