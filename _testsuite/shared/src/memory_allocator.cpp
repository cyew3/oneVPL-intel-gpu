/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2010 Intel Corporation. All Rights Reserved.

File Name: memory_allocator.cpp

\* ****************************************************************************** */

#include "memory_allocator.h"
#include <stdlib.h>


#define ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define ID_BUFFER MFX_MAKEFOURCC('B','U','F','F')
#define ID_FRAME  MFX_MAKEFOURCC('F','R','M','E')

mfxStatus CreateBufferAllocator(mfxBufferAllocator** pBufferAllocator)
{
    *pBufferAllocator  = new mfxBufferAllocator;
    if (*pBufferAllocator == 0)
        return MFX_ERR_MEMORY_ALLOC;

    (*pBufferAllocator)->Alloc     = AllocBuffer;
    (*pBufferAllocator)->Lock      = LockBuffer;
    (*pBufferAllocator)->Unlock    = UnlockBuffer;
    (*pBufferAllocator)->Free      = FreeBuffer;
    (*pBufferAllocator)->pthis     = 0;

    return MFX_ERR_NONE;
}

mfxStatus CreateFrameAllocator(mfxFrameAllocator** pFrameAllocator, bool bD3DSupport)
{
    *pFrameAllocator  = new mfxFrameAllocator;
    if (*pFrameAllocator == 0)
        return MFX_ERR_MEMORY_ALLOC;

    mfxBaseFrameAllocator* pFA=0;
    pFA = new mfxBaseFrameAllocator(bD3DSupport);
    if (!pFA)
    {
         return MFX_ERR_MEMORY_ALLOC;
    }
    (*pFrameAllocator)->pthis = (mfxHDL)pFA;
    (*pFrameAllocator)->Alloc = AllocFrames;
    (*pFrameAllocator)->Lock  = LockFrame;
    (*pFrameAllocator)->Free  = FreeFrames;
    (*pFrameAllocator)->Unlock = UnlockFrame;
    (*pFrameAllocator)->GetHDL = 0;

    return MFX_ERR_NONE;
}

mfxStatus DeleteBufferAllocator(mfxBufferAllocator** pBufferAllocator)
{
    (*pBufferAllocator)->Alloc     = 0;
    (*pBufferAllocator)->Lock      = 0;
    (*pBufferAllocator)->Unlock    = 0;
    (*pBufferAllocator)->Free      = 0;
    (*pBufferAllocator)->pthis     = 0;

    delete *pBufferAllocator;
    *pBufferAllocator = 0;

    return MFX_ERR_NONE;
}

mfxStatus DeleteFrameAllocator(mfxFrameAllocator** pFrameAllocator)
{
    if (pFrameAllocator!=NULL && (*pFrameAllocator)!= NULL)
    {
        mfxBaseFrameAllocator* pFA = (mfxBaseFrameAllocator*)(*pFrameAllocator)->pthis;
        if (pFA)
            delete pFA;

        (*pFrameAllocator)->pthis  = 0;
        (*pFrameAllocator)->Alloc  = 0;
        (*pFrameAllocator)->Lock   = 0;
        (*pFrameAllocator)->Free   = 0;
        (*pFrameAllocator)->Unlock = 0;
        (*pFrameAllocator)->GetHDL = 0;

        delete *pFrameAllocator;
        *pFrameAllocator = 0;
    }

   return MFX_ERR_NONE;
}

mfxStatus DeleteAllFrames(mfxHDL pthis)
{
    mfxBaseFrameAllocator* pFA = (mfxBaseFrameAllocator*)pthis;
    if (pFA)
    {
        return pFA->FreeFrames();
    }
    return MFX_ERR_NONE;
}


mfxStatus AllocBuffer(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxHDL *mid)
{
    mfxU32 header_size = ALIGN32(sizeof(BufferStruct));
    mfxU8 *buffer_ptr=(mfxU8 *)malloc(header_size + nbytes);

    if (!buffer_ptr) 
        return MFX_ERR_MEMORY_ALLOC;

    BufferStruct *bs=(BufferStruct *)buffer_ptr;
    bs->allocator = pthis;
    bs->id = ID_BUFFER;
    bs->type = type;
    bs->nbytes = nbytes;
    *mid = (mfxHDL) bs;
    return MFX_ERR_NONE;
}

mfxStatus LockBuffer(mfxHDL , mfxHDL mid, mfxU8 **ptr)
{
    BufferStruct *bs=(BufferStruct *)mid;
    try
    {
        if (!bs) 
            return MFX_ERR_INVALID_HANDLE;
        if (bs->id!=ID_BUFFER) 
            return MFX_ERR_INVALID_HANDLE;
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    if (ptr) *ptr=(mfxU8 *)bs+ALIGN32(sizeof(BufferStruct));
    return MFX_ERR_NONE;
}

mfxStatus UnlockBuffer(mfxHDL , mfxHDL mid)
{
    try
    {
        BufferStruct *bs=(BufferStruct *)mid;
        if (!bs) 
            return MFX_ERR_INVALID_HANDLE;
        if (bs->id!=ID_BUFFER) 
            return MFX_ERR_INVALID_HANDLE;
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    return MFX_ERR_NONE;
} 

mfxStatus FreeBuffer(mfxHDL , mfxMemId mid)
{
    try
    {
        BufferStruct *bs=(BufferStruct *)mid;
        if (!bs) 
            return MFX_ERR_INVALID_HANDLE;
        if (bs->id!=ID_BUFFER) 
            return MFX_ERR_INVALID_HANDLE;
        free(bs);
        return MFX_ERR_NONE;
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
}
//------------------------------------------------------------------------------------------------------------//

static mfxStatus GetFrameType(mfxFrameAllocRequest *request,eFrameType * frameType, bool bD3d)
{
  
    if ((request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)&& bD3d)
    {
          *frameType = MXF_SW2_FRAME;
    }
    else if ((request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)&&bD3d)
    {
          *frameType = MXF_SW1_FRAME;  
    }
    else if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
    {
          *frameType = MXF_SW0_FRAME;   
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}


mfxStatus AllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{    
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxBaseFrameAllocator* fa = (mfxBaseFrameAllocator*) pthis;

    try
    {
        return fa->AllocFrames (request,response,false);
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
} 

/*  
    Create external frames in application and fills structure "response".
    Limitation: create only external frames:(request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) must be not zero. 
    Also request->Type defines components witch can use this frame: encode, decode or vpp.
    Created frames can be shared between components.
    Use combination of flags MFX_MEMTYPE_FROM_ENCODE, MFX_MEMTYPE_FROM_DECODE, MFX_MEMTYPE_FROM_VPP;
*/

mfxStatus CreateExternalFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{    
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxBaseFrameAllocator* fa = (mfxBaseFrameAllocator*) pthis;

    try
    {
        if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            return fa->AllocFrames (request,response, true);
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
} 

mfxStatus LockFrame(mfxHDL pthis, mfxHDL mid, mfxFrameData *ptr)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    //ptr->MemId = mid;

    mfxBaseFrameAllocator* fa = (mfxBaseFrameAllocator*) pthis;
    try
    {
       return fa->LockFrame (mid,ptr);
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    } 
}

mfxStatus UnlockFrame(mfxHDL pthis, mfxHDL mid, mfxFrameData *ptr)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxBaseFrameAllocator* fa = (mfxBaseFrameAllocator*) pthis;
    try
    {
       return fa->UnlockFrame (mid,ptr);
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    } 

} 

mfxStatus FreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxBaseFrameAllocator* fa = (mfxBaseFrameAllocator*) pthis;
    try
    {  
       return fa->FreeFrames (response, false);
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    } 
}

mfxStatus FreeExternalFrames(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxBaseFrameAllocator* fa = (mfxBaseFrameAllocator*) pthis;
    try
    {  
        return fa->FreeFrames (response, true);
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    } 
}

//-------------------------------------------------------------------------------------------------------
mfxStatus mfxSWFrameAllocator::CreateFrames (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool bCreatedInApp)
{
    mfxU32 numAllocated = 0, maxNumFrames = 0;

    mfxU32 Pitch    = ALIGN32(request->Info.Width);
    mfxU32 Height2  = ALIGN32(request->Info.Height);
    mfxU32 nbytes;
    
    if (m_nHandles >= MFX_NUM_HANDLES)
        return MFX_ERR_UNSUPPORTED;

    sHandleInfo* pHI = m_pHandleInfo[m_nHandles];

    pHI->m_pFramesMID.resize(request->NumFrameSuggested);
    response->mids = &pHI->m_pFramesMID[0];

    switch (request->Info.FourCC) {
    case MFX_FOURCC_YV12:
        nbytes=Pitch*Height2 + (Pitch>>1)*(Height2>>1) + (Pitch>>1)*(Height2>>1);
        break;
    case MFX_FOURCC_NV12:
        nbytes=Pitch*Height2 + (Pitch>>1)*(Height2>>1) + (Pitch>>1)*(Height2>>1);
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    // allocate frames in cycle
    maxNumFrames = request->NumFrameSuggested;

    for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated += 1)
    {
        mfxStatus sts = AllocBuffer(0, nbytes + ALIGN32(sizeof(FrameStruct)), request->Type, &response->mids[numAllocated]);
        if (sts>MFX_ERR_NONE) break;

        FrameStruct *fs;
        sts = LockBuffer(0, response->mids[numAllocated], (mfxU8 **)&fs);
        if (sts>MFX_ERR_NONE) break;
        fs->id = ID_FRAME;
        fs->info = request->Info;
        UnlockBuffer(0, response->mids[numAllocated]);
    }
    response->NumFrameActual = (mfxU16) numAllocated;
    
    pHI->m_Type          = request->Type;
    pHI->m_nFrames       = response->NumFrameActual;
    pHI->m_bCreatedInApp = bCreatedInApp;
    pHI->m_frameInfo     = request->Info;

    m_nHandles++;

    // check the number of allocated frames
    if (numAllocated < request->NumFrameMin)
    {        
        return MFX_ERR_MEMORY_ALLOC;
    }

    return MFX_ERR_NONE;
}

static bool checkInfo (mfxFrameInfo* pInfo1, mfxFrameInfo* pInfo2)
{
    if (pInfo1->ChromaFormat != pInfo2->ChromaFormat)
        return false;
    if (pInfo1->FourCC != pInfo2->FourCC)
        return false;
    if (pInfo1->Height != pInfo2->Height)
        return false;
    if (pInfo1->Width  != pInfo2->Width)
        return false;

    return true;
}

mfxStatus mfxSWFrameAllocator::CheckRequest(mfxFrameAllocRequest *request, mfxU32* nHandle)
{
    for (mfxU32 i = 0; i < m_nHandles; i++)
    {
        sHandleInfo* pHI = m_pHandleInfo[i];

        if ((pHI->m_Type & request->Type) == request->Type)
        {
            *nHandle = i;
            return (checkInfo (&request->Info, &pHI->m_frameInfo) )? MFX_ERR_NONE:MFX_ERR_MEMORY_ALLOC;        
        }    
    }
    *nHandle = 0;
    return MFX_ERR_NOT_FOUND;
}

bool mfxSWFrameAllocator::CheckExistMid (mfxMemId mid)
{
    for (mfxU32 i = 0; i<m_nHandles; i++)
    {
        sHandleInfo* pHI = m_pHandleInfo[i];

        for (mfxU32 j = 0; j < pHI->m_nFrames; j++)
        {
            if (pHI->m_pFramesMID[j] == mid)
            {
                return true;
            }
        }
    }
    return false;
}

mfxStatus mfxSWFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool bCreatedInApp, bool *bCreated)
{
    mfxU32 nHandle = 0;    

    mfxStatus sts = CheckRequest (request, &nHandle);
    if (MFX_ERR_NONE == sts)
    {
        sHandleInfo* pHI = m_pHandleInfo[nHandle];
        *bCreated  = false;
        
        if (bCreatedInApp || !pHI->m_bCreatedInApp)
            return MFX_ERR_MEMORY_ALLOC;
        
        response->NumFrameActual = (mfxU16)pHI->m_nFrames;
        response->mids           = &pHI->m_pFramesMID[0];
    }
    else if (MFX_ERR_NOT_FOUND == sts)
    {
        *bCreated  = true;
        return CreateFrames (request, response, bCreatedInApp);
    
    }
    return sts;
}

mfxStatus  mfxSWFrameAllocator::CheckResponse(mfxFrameAllocResponse *response, mfxU32* nHandle)
{
   if (response->NumFrameActual == 0)
        return MFX_ERR_NONE;

   for (mfxU32 i = 0; i < m_nHandles; i ++)
   {
        sHandleInfo* pHI = m_pHandleInfo[i];
        // look for first mid 
        if (response->mids[0] == pHI->m_pFramesMID[0])
        {
            if (response->NumFrameActual != pHI->m_nFrames)
                return MFX_ERR_NOT_FOUND;

            for (int j = 0; j < response->NumFrameActual; j++)
            {
                if (response->mids[j] != pHI->m_pFramesMID[j])
                    return MFX_ERR_NOT_FOUND;
            }
            *nHandle = i;
            return MFX_ERR_NONE;
        }
   }
   return MFX_ERR_NOT_FOUND;
}

mfxStatus mfxSWFrameAllocator::LockFrame(mfxHDL mid, mfxFrameData *ptr)
{
    FrameStruct *fs;
    mfxStatus sts = LockBuffer(0, mid,(mfxU8 **)&fs);

    if (sts!=MFX_ERR_NONE) 
        return sts;
    if (fs->id!=ID_FRAME) {
        UnlockBuffer(0, mid);
        return MFX_ERR_INVALID_HANDLE;
    }
    mfxU32 pitch = ALIGN32(fs->info.Width);
    ptr->PitchHigh = (mfxU16)(pitch / (1 << 16));
    ptr->PitchLow  = (mfxU16)(pitch % (1 << 16));
    mfxU32 Height2=ALIGN32(fs->info.Height);
    ptr->Y=(mfxU8 *)fs+ALIGN32(sizeof(FrameStruct));
    switch (fs->info.FourCC) {
    case MFX_FOURCC_NV12:
        ptr->U=ptr->Y+pitch*Height2;
        ptr->V=ptr->U+1;
        break;
    case MFX_FOURCC_YV12:
        ptr->V=ptr->Y+pitch*Height2;
        ptr->U=ptr->V+(pitch>>1)*(Height2>>1);
        break;
    }
    return sts;
}

mfxStatus mfxSWFrameAllocator::UnlockFrame( mfxHDL mid, mfxFrameData *ptr)
{
    mfxStatus sts=UnlockBuffer(0, mid);
    if (sts!=MFX_ERR_NONE) 
        return sts;
    if (ptr) {
        ptr->PitchHigh=0;
        ptr->PitchLow=0;
        ptr->U=ptr->V=ptr->Y=0;
    }
    return sts;

} 

mfxStatus mfxSWFrameAllocator::FreeFrames  (mfxU32 nHandle)
{
    if (nHandle >= m_nHandles)
        return MFX_ERR_NOT_FOUND;

   sHandleInfo* pHI = m_pHandleInfo [nHandle];

   for (mfxU32 i = 0; i < pHI->m_nFrames; i++)
   {
       FreeBuffer(0, pHI->m_pFramesMID[i]);
   }

   pHI->m_pFramesMID.clear();
   pHI->m_nFrames = 0;
   pHI->m_Type = 0;

   m_nHandles --;
   m_pHandleInfo[nHandle]    = m_pHandleInfo[m_nHandles];
   m_pHandleInfo[m_nHandles] = pHI;

   return MFX_ERR_NONE;
}

mfxStatus mfxSWFrameAllocator::FreeFrames  ()
{
    mfxStatus sts = MFX_ERR_NONE;
    while (m_nHandles)
    {
        sts = FreeFrames  (0);
        if (MFX_ERR_NONE != sts)
            return sts;    
    }
    return sts;
}

mfxStatus mfxSWFrameAllocator::FreeFrames(mfxFrameAllocResponse *response, bool bCreatedInApp)
{
    if (response->NumFrameActual>0)
    {
        mfxU32 nHandle = 0;
        mfxStatus sts = CheckResponse(response,&nHandle);
        if (MFX_ERR_NONE == sts)
        {
            if (m_pHandleInfo[nHandle]->m_bCreatedInApp && !bCreatedInApp)
            {
                return MFX_ERR_NONE;
            }
            return FreeFrames(nHandle);
        }
        return sts;                
    }
    return MFX_ERR_NONE;
}


//-------------------------------------------------------------------------------------------------------

mfxStatus mfxBaseFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool bCreatedInApp)
{
    mfxStatus sts;
    
    eFrameType frameType;
    bool bCreated = false;

    sts = GetFrameType(request,&frameType,m_bD3DSupport);
    if (sts != MFX_ERR_NONE) return sts;
 
    switch (frameType)
    {
    case MXF_SW0_FRAME:
        sts = allocator0.AllocFrames (request,response,bCreatedInApp, &bCreated);
        break;
    case MXF_SW1_FRAME:
        sts = allocator1.AllocFrames (request,response,bCreatedInApp,& bCreated);
        break;
    case MXF_SW2_FRAME:
        sts = allocator2.AllocFrames (request,response,bCreatedInApp,& bCreated);
        break;
    default:
        return MFX_ERR_UNSUPPORTED;   
    }

    return sts;
} 

mfxStatus mfxBaseFrameAllocator::LockFrame(mfxHDL mid, mfxFrameData *ptr)
{

    if (allocator0.CheckExistMid ( mid))
        return allocator0.LockFrame (mid,ptr);
    else if (allocator1.CheckExistMid ( mid))
        return allocator1.LockFrame (mid,ptr);
    else if (allocator2.CheckExistMid ( mid))
        return allocator2.LockFrame (mid,ptr);
    else
        return MFX_ERR_NOT_FOUND;
}

mfxStatus mfxBaseFrameAllocator::UnlockFrame( mfxHDL mid, mfxFrameData *ptr)
{
    
   

    if (allocator0.CheckExistMid ( mid))
        return allocator0.UnlockFrame (mid,ptr);
    else if (allocator1.CheckExistMid ( mid))
        return allocator1.UnlockFrame (mid,ptr);
    else if (allocator2.CheckExistMid ( mid))
        return allocator2.UnlockFrame (mid,ptr);
    else
        return MFX_ERR_NOT_FOUND;
}

mfxStatus mfxBaseFrameAllocator::FreeFrames()
{
    mfxStatus   sts = MFX_ERR_NONE;
    sts = allocator0.FreeFrames();
    if (MFX_ERR_NONE!=sts)
        return sts;
    sts = allocator1.FreeFrames();
    if (MFX_ERR_NONE!=sts)
        return sts;
    sts = allocator2.FreeFrames();
   
    return sts;
}
mfxStatus mfxBaseFrameAllocator::FreeFrames(mfxFrameAllocResponse *response, bool bCreatedInApp)
{
    mfxStatus   sts = MFX_ERR_NONE;

    if (response->NumFrameActual == 0)
        return MFX_ERR_NONE;

    sts = allocator0.FreeFrames(response,bCreatedInApp);
    if (MFX_ERR_NOT_FOUND == sts)
    {
        sts = allocator1.FreeFrames(response,bCreatedInApp);
        if (MFX_ERR_NOT_FOUND == sts)
        {
            sts = allocator2.FreeFrames(response,bCreatedInApp);        
        }
    }
    return sts;
}

