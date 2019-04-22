/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <stdlib.h>

#include "mfx_sysmem_allocator.h"
#include "mfx_utils.h"
#include <algorithm>
#include <memory>

#define MSDK_ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define ID_BUFFER MFX_MAKEFOURCC('B','U','F','F')
#define ID_FRAME  MFX_MAKEFOURCC('F','R','M','E')

#pragma warning(disable : 4100)

SysMemFrameAllocator::SysMemFrameAllocator()
    : m_pBufferAllocator(0), m_bOwnBufferAllocator(false)
{
}

SysMemFrameAllocator::~SysMemFrameAllocator()
{
    Close();
}

mfxStatus SysMemFrameAllocator::Init(mfxAllocatorParams *pParams)
{
    // check if any params passed from application
    if (pParams)
    {
        SysMemAllocatorParams *pSysMemParams = dynamic_cast<SysMemAllocatorParams *>(pParams);
        MFX_CHECK(pSysMemParams, MFX_ERR_NOT_INITIALIZED);
        m_pBufferAllocator = pSysMemParams->pBufferAllocator;
        m_bOwnBufferAllocator = false;
    }

    // if buffer allocator wasn't passed from application create own
    if (!m_pBufferAllocator)
    {
        m_pBufferAllocator = new SysMemBufferAllocator;

        m_bOwnBufferAllocator = true;
    }

    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::Close()
{
    if (m_bOwnBufferAllocator)
    {
        delete m_pBufferAllocator;
        m_pBufferAllocator = 0;
    }

    return BaseFrameAllocator::Close();
}

mfxStatus SysMemFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    MFX_CHECK(m_pBufferAllocator, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(ptr);

    sFrame *fs = 0;
    mfxStatus sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, mid, (mfxU8 **)&fs);

    MFX_CHECK_STS(sts);

    if (ID_FRAME != fs->id)
    {
        m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mid);
        return MFX_ERR_INVALID_HANDLE;
    }

    mfxU16 Width2 = (mfxU16)MSDK_ALIGN32(fs->info.Width);
    mfxU16 Height2 = (mfxU16)MSDK_ALIGN32(fs->info.Height);
    ptr->B = ptr->Y = (mfxU8 *)fs + MSDK_ALIGN32(sizeof(sFrame));

    switch (fs->info.FourCC)
    {
    case MFX_FOURCC_P010:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_P016:
#endif
        ptr->U = ptr->Y + Width2 * Height2 * 2;
        ptr->V = ptr->U + 2;
        ptr->PitchHigh = 0;
        ptr->PitchLow = Width2 * 2;
        break;

     case MFX_FOURCC_P210:
        ptr->U = ptr->Y + Width2 * Height2 * 2;
        ptr->V = ptr->U + 2;
        ptr->PitchHigh = 0;
        ptr->PitchLow = Width2 * 2;
        break;

    case MFX_FOURCC_NV12:
        ptr->U = ptr->Y + Width2 * Height2;
        ptr->V = ptr->U + 1;
        ptr->PitchHigh = 0;
        ptr->PitchLow = Width2;
        break;

    case MFX_FOURCC_NV16:
        ptr->U = ptr->Y + Width2 * Height2;
        ptr->V = ptr->U + 1;
        ptr->PitchHigh = 0;
        ptr->PitchLow = Width2;
        break;

    case MFX_FOURCC_YV12:
        ptr->V = ptr->Y + Width2 * Height2;
        ptr->U = ptr->V + (Width2 >> 1) * (Height2 >> 1);
        ptr->PitchHigh = 0;
        ptr->PitchLow = Width2;
        break;

    case MFX_FOURCC_YUY2:
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        ptr->PitchHigh = (mfxU16)((2 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow  = (mfxU16)((2 * (mfxU32)Width2) % (1 << 16));
        break;

    case MFX_FOURCC_RGB3:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->PitchHigh = (mfxU16)((3 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow  = (mfxU16)((3 * (mfxU32)Width2) % (1 << 16));
        break;

    case MFX_FOURCC_RGB4:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        ptr->PitchHigh = (mfxU16)((4 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow  = (mfxU16)((4 * (mfxU32)Width2) % (1 << 16));
        break;

    case MFX_FOURCC_AYUV:
        ptr->V = ptr->B;
        ptr->U = ptr->V + 1;
        ptr->Y = ptr->V + 2;
        ptr->A = ptr->V + 3;
        ptr->PitchHigh = (mfxU16)((4 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow = (mfxU16)((4 * (mfxU32)Width2) % (1 << 16));
        break;

    case MFX_FOURCC_A2RGB10:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        ptr->PitchHigh = (mfxU16)((4 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow  = (mfxU16)((4 * (mfxU32)Width2) % (1 << 16));
        break;

    case MFX_FOURCC_R16:
        ptr->Y16 = (mfxU16 *)ptr->B;
        ptr->Pitch = 2 * Width2;
        break;

    case MFX_FOURCC_ARGB16:
        ptr->V16 = (mfxU16*)ptr->B;
        ptr->U16 = ptr->V16 + 1;
        ptr->Y16 = ptr->V16 + 2;
        ptr->A = (mfxU8*)(ptr->V16 + 3);
        ptr->Pitch = 8 * Width2;
        break;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_Y216:
#endif // #if (MFX_VERSION >= MFX_VERSION_NEXT)
        ptr->Y16 = (mfxU16*)ptr->B;
        ptr->U16 = ptr->Y16 + 1;
        ptr->V16 = ptr->Y16 + 3;
        ptr->PitchHigh = (mfxU16)((4 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow  = (mfxU16)((4 * (mfxU32)Width2) % (1 << 16));
        break;

    case MFX_FOURCC_Y410:
        ptr->Y410 = (mfxY410 *) ptr->B;
        ptr->Y = 0;
        ptr->V = 0;
        ptr->A = 0;
        ptr->Pitch = 4 * Width2;
        break;
#endif // #if (MFX_VERSION >= 1027)
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_Y416:
        ptr->U16 = (mfxU16*)ptr->B;
        ptr->Y16 = ptr->U16 + 1;
        ptr->V16 = ptr->Y16 + 1;
        ptr->A   = (mfxU8 *)ptr->V16 + 1;
        ptr->PitchHigh = (mfxU16)((8 * (mfxU32)Width2) / (1 << 16));
        ptr->PitchLow  = (mfxU16)((8 * (mfxU32)Width2) % (1 << 16));
        break;
#endif
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    ptr->MemType = MFX_MEMTYPE_SYSTEM_MEMORY;
    mfxMemId *pmid = GetMidHolder(mid);
    MFX_CHECK(pmid, MFX_ERR_NOT_FOUND);

    return MFX_ERR_NONE;
}

mfxMemId *SysMemFrameAllocator::GetMidHolder(mfxMemId mid)
{
    for (auto resp : m_vResp)
    {
        mfxMemId *it = std::find(resp->mids, resp->mids + resp->NumFrameActual, mid);
        if (it != resp->mids + resp->NumFrameActual)
            return it;
    }
    return nullptr;
}

mfxStatus SysMemFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    MFX_CHECK(m_pBufferAllocator, MFX_ERR_NOT_INITIALIZED);

    if (!mid && ptr && (ptr->Y
#if (MFX_VERSION >= 1027)
        || ptr->Y410
#endif
    ))
    {
        mfxStatus sts = m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mid);
        MFX_CHECK_STS(sts);
    }

    if (ptr)
    {
        ptr->PitchHigh = 0;
        ptr->PitchLow = 0;
        ptr->Y = 0;
        ptr->U = 0;
        ptr->V = 0;
    }

    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus SysMemFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);

    MFX_CHECK_STS(sts);
    MFX_CHECK(request->Type & MFX_MEMTYPE_SYSTEM_MEMORY, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}


static mfxU32 GetSurfaceSize(mfxU32 FourCC, mfxU32 Width2, mfxU32 Height2)
{
    mfxU32 nbytes = 0;

    switch (FourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2>>1) + (Width2>>1)*(Height2>>1);
        break;
    case MFX_FOURCC_P010:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_P016:
#endif
        nbytes = Width2*Height2 + (Width2>>1)*(Height2>>1) + (Width2>>1)*(Height2>>1);
        nbytes *= 2; // 16bits
        break;
    case MFX_FOURCC_P210:
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y210:
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_Y216:
#endif
        nbytes = Width2*Height2 + (Width2>>1)*(Height2)+(Width2>>1)*(Height2);
        nbytes *= 2; // 16bits
        break;
#if (MFX_VERSION >= 1027)
    case MFX_FOURCC_Y410:
        nbytes = 4 * Width2*Height2;
        break;
#endif
    case MFX_FOURCC_RGB3:
        nbytes = Width2*Height2 + Width2*Height2 + Width2*Height2;
        break;
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_AYUV:
        nbytes = Width2*Height2 + Width2*Height2 + Width2*Height2 + Width2*Height2;
        break;
    case MFX_FOURCC_A2RGB10:
        nbytes = Width2*Height2*4; // 4 bytes per pixel
        break;
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_NV16:
        nbytes = Width2*Height2 + (Width2>>1)*(Height2) + (Width2>>1)*(Height2);
        break;
    case MFX_FOURCC_R16:
        nbytes = 2*Width2*Height2;
        break;
    case MFX_FOURCC_ARGB16:
#if (MFX_VERSION >= MFX_VERSION_NEXT)
    case MFX_FOURCC_Y416:
#endif
        nbytes = 8 * Width2*Height2;
        break;
    default:
        break;
    }
    return nbytes;
}

mfxStatus SysMemFrameAllocator::ReallocImpl(mfxMemId mid, const mfxFrameInfo *info, mfxU16 memType, mfxMemId *midOut)
{
    MFX_CHECK(m_pBufferAllocator, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR2(info, midOut);

    mfxU32 nbytes = GetSurfaceSize(info->FourCC, MSDK_ALIGN32(info->Width), MSDK_ALIGN32(info->Height));
    MFX_CHECK(nbytes, MFX_ERR_UNSUPPORTED);

    // pointer to the record in m_mids structure
    mfxMemId *pmid = GetMidHolder(mid);
    MFX_CHECK(pmid, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = m_pBufferAllocator->Free(m_pBufferAllocator->pthis, *pmid);
    MFX_CHECK_STS(sts);

    sts = m_pBufferAllocator->Alloc(m_pBufferAllocator->pthis,
        MSDK_ALIGN32(nbytes) + MSDK_ALIGN32(sizeof(sFrame)), MFX_MEMTYPE_SYSTEM_MEMORY, pmid);
    MFX_CHECK_STS(sts);

    sFrame *fs;
    sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, *pmid, (mfxU8 **)&fs);
    MFX_CHECK_STS(sts);

    fs->id = ID_FRAME;
    fs->info = *info;
    m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, *pmid);

    *midOut = *pmid;
    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_CHECK(m_pBufferAllocator, MFX_ERR_NOT_INITIALIZED);

    mfxU32 nbytes = GetSurfaceSize(request->Info.FourCC, MSDK_ALIGN32(request->Info.Width), MSDK_ALIGN32(request->Info.Height));
    MFX_CHECK(nbytes, MFX_ERR_UNSUPPORTED);

    std::unique_ptr<mfxMemId[]> umids(new mfxMemId[request->NumFrameSuggested]);

    // allocate frames
    mfxU32 numAllocated = 0;

    for (numAllocated = 0; numAllocated < request->NumFrameSuggested; numAllocated++)
    {
        mfxStatus sts = m_pBufferAllocator->Alloc(m_pBufferAllocator->pthis,
            MSDK_ALIGN32(nbytes) + MSDK_ALIGN32(sizeof(sFrame)), request->Type, &(umids[numAllocated]));

        if (MFX_ERR_NONE != sts)
            break;

        sFrame *fs;
        sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, umids[numAllocated], (mfxU8 **)&fs);

        if (MFX_ERR_NONE != sts)
            break;

        fs->id = ID_FRAME;
        fs->info = request->Info;
        m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, umids[numAllocated]);
    }

    // check the number of allocated frames
    MFX_CHECK(numAllocated == request->NumFrameMin, MFX_ERR_NOT_INITIALIZED);

    response->NumFrameActual = (mfxU16)numAllocated;
    response->mids = umids.get();
    response->AllocId = request->AllocId;

    m_vResp.push_back(response);
    umids.release();

    return MFX_ERR_NONE;
}

mfxStatus SysMemFrameAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(response);
    MFX_CHECK(m_pBufferAllocator, MFX_ERR_NOT_INITIALIZED);

    if (response->mids)
    {
        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            if (response->mids[i])
            {
                mfxStatus sts = m_pBufferAllocator->Free(m_pBufferAllocator->pthis, response->mids[i]);
                MFX_CHECK_STS(sts);
                response->mids[i] = nullptr;
            }
        }
        response->NumFrameActual = 0;

        m_vResp.erase(std::remove(m_vResp.begin(), m_vResp.end(), response), m_vResp.end());

        delete[] response->mids;
        response->mids = 0;
    }

    return MFX_ERR_NONE;
}

SysMemBufferAllocator::SysMemBufferAllocator()
{

}

SysMemBufferAllocator::~SysMemBufferAllocator()
{

}

mfxStatus SysMemBufferAllocator::AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid)
{
    MFX_CHECK_NULL_PTR1(mid);
    MFX_CHECK(type & MFX_MEMTYPE_SYSTEM_MEMORY, MFX_ERR_UNSUPPORTED);

    mfxU32 header_size = MSDK_ALIGN32(sizeof(sBuffer));
    mfxU8 *buffer_ptr = (mfxU8 *)calloc(header_size + nbytes + 32, 1);

    MFX_CHECK_STS_ALLOC(buffer_ptr);

    sBuffer *bs = (sBuffer *)buffer_ptr;
    bs->id = ID_BUFFER;
    bs->type = type;
    bs->nbytes = nbytes;
    *mid = (mfxHDL)bs;

    return MFX_ERR_NONE;
}

mfxStatus SysMemBufferAllocator::LockBuffer(mfxMemId mid, mfxU8 **ptr)
{
    MFX_CHECK_NULL_PTR1(ptr);

    sBuffer *bs = (sBuffer *)mid;

    MFX_CHECK(bs, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(ID_BUFFER == bs->id, MFX_ERR_INVALID_HANDLE);

    *ptr = (mfxU8*)((size_t)((mfxU8 *)bs+MSDK_ALIGN32(sizeof(sBuffer))+31)&(~((size_t)31)));

    return MFX_ERR_NONE;
}

mfxStatus SysMemBufferAllocator::UnlockBuffer(mfxMemId mid)
{
    sBuffer *bs = (sBuffer *)mid;

    MFX_CHECK(bs, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(ID_BUFFER == bs->id, MFX_ERR_INVALID_HANDLE);

    return MFX_ERR_NONE;
}

mfxStatus SysMemBufferAllocator::FreeBuffer(mfxMemId mid)
{
    sBuffer *bs = (sBuffer *)mid;

    MFX_CHECK(bs, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(ID_BUFFER == bs->id, MFX_ERR_INVALID_HANDLE);

    free(bs);
    return MFX_ERR_NONE;
}
