/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: plg_frame_supply.cpp

\* ****************************************************************************** */

#include "ptir_wrap.h"

frameSupplier::frameSupplier(std::vector<mfxFrameSurface1*>* _inSurfs, std::vector<mfxFrameSurface1*>* _workSurfs, 
    std::vector<mfxFrameSurface1*>* _outSurfs, std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap,
    CmDeviceEx* _pCMdevice, mfxCoreInterface* _mfxCore, mfxU16 _IOPattern)
{
    inSurfs        = _inSurfs;
    workSurfs      = _workSurfs;
    outSurfs       = _outSurfs;
    CmToMfxSurfmap = _CmToMfxSurfmap;
    pCMdevice      = _pCMdevice;
    mfxCoreIfce    = _mfxCore;
    IOPattern      = _IOPattern;
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
void* frameSupplier::SetMap(std::map<CmSurface2D*,mfxFrameSurface1*>* _CmToMfxSurfmap)
{
    CmToMfxSurfmap = _CmToMfxSurfmap;
    return 0;
}

CmSurface2D* frameSupplier::GetWorkSurfaceCM()
{
    mfxFrameSurface1* s_out = 0;
    CmSurface2D* cm_out = 0;
    if(0 == workSurfs->size())
        return 0;
    else
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); ++it) {
            if((*it)->Data.Locked < 4)
            {
                s_out = *it;
                workSurfs->erase(it);
                break;
            }
        }
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
            CMCreateSurface2D(s_out,cm_out,false);
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
        for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); ++it) {
            if((*it)->Data.Locked < 4)
            {
                s_out = *it;
                workSurfs->erase(it);
                break;
            }
        }
        return s_out;
    }
    return 0;
}
mfxStatus frameSupplier::AddCPUPtirOutSurf(mfxU8* buffer, mfxFrameSurface1* surface)
{
    bool unlockreq = false;
    mfxStatus mfxSts = MFX_ERR_NONE;
    if(surface->Data.MemId)
    {
        unlockreq = true;
        mfxSts = mfxCoreIfce->FrameAllocator.Lock(mfxCoreIfce->FrameAllocator.pthis, surface->Data.MemId, &(surface->Data));
        if(mfxSts)
            return mfxSts;
    }
    mfxSts = Ptir420toMfxNv12(buffer, surface);
    if(mfxSts)
        return mfxSts;
    if(unlockreq)
    {
        mfxSts = mfxCoreIfce->FrameAllocator.Unlock(mfxCoreIfce->FrameAllocator.pthis, surface->Data.MemId, &(surface->Data));
        if(mfxSts)
            return mfxSts;
    }
    mfxSts = AddOutputSurf(surface);
    return mfxSts;
}
mfxStatus frameSupplier::AddOutputSurf(mfxFrameSurface1* outSurf)
{
    if(pCMdevice && (IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
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
        assert(0 != cmSurf);
        if(cmSurf)
        {
            mfxStatus mfxSts = MFX_ERR_NONE;
            mfxSts = CMCopyGpuToSys(cmSurf, outSurf);
            assert(MFX_ERR_NONE == mfxSts);
        }
    }

    outSurfs->push_back(outSurf);
    return MFX_ERR_NONE;
}
mfxStatus frameSupplier::FreeFrames()
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxU32 iii = 0;
    mfxU32 refs = 0;
    if(CmToMfxSurfmap && pCMdevice)
    {
        for(std::map<CmSurface2D*,mfxFrameSurface1*>::iterator it = CmToMfxSurfmap->begin(); it != CmToMfxSurfmap->end(); it++) {
            CmSurface2D* cm_surf = it->first;
            mfxFrameSurface1* mfxSurf = it->second;
            if(mfxSurf)
                mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &mfxSurf->Data);
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
        for(std::vector<mfxFrameSurface1*>::iterator it = workSurfs->begin(); it != workSurfs->end(); it++) {
            while((*it)->Data.Locked)
            {
                refs = (*it)->Data.Locked;
                mfxSts = mfxCoreIfce->DecreaseReference(mfxCoreIfce->pthis, &(*it)->Data);
                if(mfxSts) break;
                iii++;
                if(iii > (refs+1)) break; //hang is unacceptable
            }
        }
        workSurfs->clear();
    }
    if(inSurfs && inSurfs->size() > 0)
    {
        for(std::vector<mfxFrameSurface1*>::iterator it = inSurfs->begin(); it != inSurfs->end(); it++) {
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
        for(std::vector<mfxFrameSurface1*>::iterator it = outSurfs->begin(); it != outSurfs->end(); it++) {
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

    return MFX_ERR_NONE;
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

    mfxU32 verticalPitch = (mfxI64)(mfxSurf->Data.UV - mfxSurf->Data.Y);
    verticalPitch = (verticalPitch % mfxSurf->Data.Pitch)? 0 : verticalPitch / mfxSurf->Data.Pitch;
    mfxU32& srcUVOffset = verticalPitch;
    mfxU32 srcPitch = mfxSurf->Data.Pitch;

    //if(mfxSurf->Info.Width != mfxSurf->Info.CropW)
    //    srcUVOffset = mfxSurf->Info.Width;

    CmEvent* e = NULL;
    if(srcUVOffset != 0){
        cmSts = m_pCmQueue->EnqueueCopyCPUToGPUFullStride(cmSurfOut, mfxSurf->Data.Y, srcPitch, srcUVOffset, 0, e);
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

    mfxU32 verticalPitch = (mfxI64)(mfxSurf->Data.UV - mfxSurf->Data.Y);
    verticalPitch = (verticalPitch % mfxSurf->Data.Pitch)? 0 : verticalPitch / mfxSurf->Data.Pitch;
    mfxU32& dstUVOffset = verticalPitch;
    mfxU32 dstPitch = mfxSurf->Data.Pitch;

    //if(mfxSurf->Info.Width != mfxSurf->Info.CropW)
    //    dstUVOffset = mfxSurf->Info.Width;

    CmEvent* e = NULL;
    if(dstUVOffset != 0){
        cmSts = m_pCmQueue->EnqueueCopyGPUToCPUFullStride(cmSurfIn, mfxSurf->Data.Y, dstPitch, dstUVOffset, 0, e);
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

mfxU32 frameSupplier::countFreeWorkSurfs()
{
    return (*workSurfs).size();
}

mfxStatus frameSupplier::CMCreateSurface2D(mfxFrameSurface1*& mfxSurf, CmSurface2D*& cmSurfOut, bool bCopy)
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

    if(!mfxSurf->Data.MemId && !(mfxSurf->Data.Y && mfxSurf->Data.UV))
        return MFX_ERR_NULL_PTR;

    if((IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        cmSts = (*pCMdevice)->CreateSurface2D(mfxSurf->Info.Width, mfxSurf->Info.Height, CM_SURFACE_FORMAT_NV12, cmSurfOut);
        assert(cmSts == 0);
        if(CM_SUCCESS != cmSts)
            return MFX_ERR_DEVICE_FAILED;
        (*CmToMfxSurfmap)[cmSurfOut] = mfxSurf;

        if(bCopy)
        {
            mfxSts = CMCopySysToGpu(mfxSurf, cmSurfOut);
            if(MFX_ERR_NONE != mfxSts)
                return mfxSts;
        }
    }
    else if((IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) && (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        cmSts = (*pCMdevice)->CreateSurface2D((IDirect3DSurface9 *) mfxSurf->Data.MemId, cmSurfOut);
        assert(cmSts == 0);
        if(CM_SUCCESS != cmSts)
            return MFX_ERR_DEVICE_FAILED;
        (*CmToMfxSurfmap)[cmSurfOut] = mfxSurf;

        if(bCopy)
            ; //useless when associating d3d to CM surface
    }
    else
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return MFX_ERR_NONE;
}