/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: msdka_structures.h

\* ****************************************************************************** */

#include "configuration.h"
#include "msdka_serial.h"
#include <algorithm>

mfxU16 AnalyzerFrameAllocator::SurfacesUsageStat::GetNumLockedSurfaces()
{
    nSurfacesLocked = 0;
    for (size_t i = 0;
        i < surfaces.size(); i++)
    {
        nSurfacesLocked += surfaces[i]->Data.Locked ? 1 : 0;
    }
    return nSurfacesLocked;
}

mfxU16 AnalyzerFrameAllocator::SurfacesUsageStat::GetCachedNumLockedSurfaces()
{
    return nSurfacesLocked;
}

void AnalyzerFrameAllocator::SurfacesUsageStat::RegisterSurface(mfxFrameSurface1 * pSrf)
{
    if (NULL == pSrf)
        return;

    std::vector<mfxFrameSurface1 *> ::iterator it = 
        std::find(surfaces.begin(), surfaces.end(), pSrf);

    if (it != surfaces.end())
        return;

    surfaces.push_back(pSrf);
}

mfxStatus AnalyzerFrameAllocator::AllocFunc(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) {
    StackIncreaser si;
    AnalyzerFrameAllocator *afa=(AnalyzerFrameAllocator *)pthis;
    mfxStatus sts=(*(afa->allocator->Alloc))(afa->allocator->pthis,request, response);
    RECORD_CONFIGURATION({
        dump_mfxFrameAllocRequest(fd, level, TEXT("core.FrameAllocator.Alloc.request"),request);
        if (response) dump_format_wprefix(fd, level, 1,TEXT("core.FrameAllocator.Alloc.response.NumFrameActual="),TEXT("%d"),response->NumFrameActual);
        dump_mfxStatus(fd, level, TEXT("core.FrameAllocator.Alloc"),sts);
    });
    return sts;
}

mfxStatus AnalyzerFrameAllocator::LockFunc(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr) {
    StackIncreaser si;
    AnalyzerFrameAllocator *afa=(AnalyzerFrameAllocator *)pthis;
    mfxStatus sts=(*(afa->allocator->Lock))(afa->allocator->pthis,mid, ptr);
    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.FrameAllocator.Lock.mid=0x"),TEXT("%p"),mid));
        dump_mfxFrameData(fd, level, TEXT("core.FrameAllocator.Lock.ptr"),TEXT(""),ptr);
        dump_mfxStatus(fd, level, TEXT("core.FrameAllocator.Lock"),sts);
    });
    return sts;
}

mfxStatus AnalyzerFrameAllocator::UnlockFunc(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr) {
    StackIncreaser si;
    AnalyzerFrameAllocator *afa=(AnalyzerFrameAllocator *)pthis;
    mfxStatus sts=(*(afa->allocator->Unlock))(afa->allocator->pthis,mid,ptr);
    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.FrameAllocator.Unlock.mid=0x"),TEXT("%p"),mid));
        dump_mfxStatus(fd, level, TEXT("core.FrameAllocator.Unlock"),sts);
    });
    return sts;
}

mfxStatus AnalyzerFrameAllocator::GetHDLFunc(mfxHDL pthis, mfxMemId mid, mfxHDL *handle) {
    StackIncreaser si;
    AnalyzerFrameAllocator *afa=(AnalyzerFrameAllocator *)pthis;
    mfxStatus sts=(*(afa->allocator->GetHDL))(afa->allocator->pthis,mid,handle);
    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.FrameAllocator.GetHDL.mid=0x"),TEXT("%p"),mid));
        dump_mfxStatus(fd, level, TEXT("core.FrameAllocator.GetHDL"),sts);
    });
    return sts;
}

mfxStatus AnalyzerFrameAllocator::FreeFunc(mfxHDL pthis, mfxFrameAllocResponse *response) {
    StackIncreaser si;
    AnalyzerFrameAllocator *afa=(AnalyzerFrameAllocator *)pthis;
    mfxStatus sts=(*(afa->allocator->Free))(afa->allocator->pthis,response);
    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("core.FrameAllocator.Free"),sts);
    });
    return sts;
}

AnalyzerFrameAllocator::AnalyzerFrameAllocator(mfxFrameAllocator *fa) {
    pthis=this;
    allocator=fa;
    Alloc=AllocFunc;
    Lock=LockFunc;
    Unlock=UnlockFunc;
    GetHDL=GetHDLFunc;
    Free=FreeFunc;
}

mfxStatus AnalyzerBufferAllocator::AllocFunc(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid) {
    StackIncreaser si;
    AnalyzerBufferAllocator *aba=(AnalyzerBufferAllocator *)pthis;
    mfxStatus sts=(*(aba->allocator->Alloc))(aba->allocator->pthis,nbytes, type, mid);
    RECORD_CONFIGURATION_PER_FRAME({
        dump_format_wprefix(fd, level, 1,TEXT("core.BufferAllocator.Alloc.nbytes="),TEXT("%d"),nbytes);
        dump_format_wprefix(fd, level, 1,TEXT("core.BufferAllocator.Alloc.type=0x"),TEXT("%x"),type);
        dump_mfxStatus(fd, level, TEXT("core.BufferAllocator.Alloc"),sts);
    });
    return sts;
}
mfxStatus AnalyzerBufferAllocator::LockFunc(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr) {
    StackIncreaser si;
    AnalyzerBufferAllocator *aba=(AnalyzerBufferAllocator *)pthis;
    mfxStatus sts=(*(aba->allocator->Lock))(aba->allocator->pthis,mid, ptr);
    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.BufferAllocator.Lock.mid=0x"),TEXT("%p"),mid))
            dump_mfxStatus(fd, level, TEXT("core.BufferAllocator.Lock"),sts);
    });
    return sts;
}
mfxStatus AnalyzerBufferAllocator::UnlockFunc(mfxHDL pthis, mfxMemId mid) {
    StackIncreaser si;
    AnalyzerBufferAllocator *aba=(AnalyzerBufferAllocator *)pthis;
    mfxStatus sts=(*(aba->allocator->Unlock))(aba->allocator->pthis,mid);
    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.BufferAllocator.Unlock.mid=0x"),TEXT("%p"),mid));
        dump_mfxStatus(fd, level, TEXT("core.BufferAllocator.Unlock"),sts);
    });
    return sts;
}
mfxStatus AnalyzerBufferAllocator::FreeFunc(mfxHDL pthis, mfxMemId mid) {
    StackIncreaser si;
    AnalyzerBufferAllocator *aba=(AnalyzerBufferAllocator *)pthis;
    mfxStatus sts=(*(aba->allocator->Free))(aba->allocator->pthis,mid);
    RECORD_CONFIGURATION_PER_FRAME({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.BufferAllocator.Free.mid=0x"),TEXT("%p"),mid));
        dump_mfxStatus(fd, level, TEXT("core.BufferAllocator.Free"),sts);
    });
    return sts;
}

AnalyzerBufferAllocator::AnalyzerBufferAllocator(mfxBufferAllocator *ba) {
    pthis=this;
    allocator=ba;
    Alloc=AllocFunc;
    Lock=LockFunc;
    Unlock=UnlockFunc;
    Free=FreeFunc;
}
