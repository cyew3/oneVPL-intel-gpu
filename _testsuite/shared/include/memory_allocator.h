/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2010 Intel Corporation. All Rights Reserved.

File Name: memory_allocator.h

\* ****************************************************************************** */

#ifndef __MEMORY_ALLOCATOR_H
#define __MEMORY_ALLOCATOR_H

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "mfxvideo.h"
#include <memory.h>
#include <string.h>
#include <vector>

#define MFX_MEMTYPE_XX_FOR_TEST 0x0080

enum
{
    MFX_NUM_HANDLES             = 20,
};

enum eFrameType
{
    MXF_SW0_FRAME = 1,
    MXF_SW1_FRAME = 2,
    MXF_SW2_FRAME = 3,
};
struct BufferStruct
{
    mfxHDL      allocator;
    mfxU32      id;
    mfxU32      nbytes;
    mfxU16      type;
};
typedef struct HandleList
{
    HandleList*     next;
    mfxHandleType   type;
    mfxHDL          handle;

} *handle_head;
struct FrameStruct
{
    mfxU32          id;
    mfxFrameInfo    info;
};

// --------------------- interface functions -------------------------------------------------------------

/*Creates allocator and return pointer*/

mfxStatus CreateBufferAllocator(mfxBufferAllocator** pBufferAllocator);
mfxStatus CreateFrameAllocator(mfxFrameAllocator** pFrameAllocator, bool bD3DSupport = true);

/*  
    Create external frames in application and fills structure "response".
    Limitation: create only external frames:(request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) must be not zero. 
    Also request->Type defines components witch can use this frame: encode, decode or vpp.
    Created frames can be shared between components.
    Use combination of flags MFX_MEMTYPE_FROM_ENCODE, MFX_MEMTYPE_FROM_DECODE, MFX_MEMTYPE_FROM_VPP;
*/

mfxStatus CreateExternalFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
mfxStatus DeleteAllFrames(mfxHDL pthis);
mfxStatus DeleteBufferAllocator(mfxBufferAllocator** pBufferAllocator);
mfxStatus DeleteFrameAllocator(mfxFrameAllocator** pFrameAllocator);
mfxStatus FreeExternalFrames(mfxHDL pthis, mfxFrameAllocResponse *response);
//----------------------------------------------------------------------------------------------------------



mfxStatus AllocBuffer(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
mfxStatus LockBuffer(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
mfxStatus UnlockBuffer(mfxHDL pthis, mfxMemId mid);
mfxStatus FreeBuffer(mfxHDL pthis, mfxMemId mid);

mfxStatus AllocFrames (mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
mfxStatus LockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus GetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus UnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
mfxStatus FreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response);

struct sHandleInfo
{
    std::vector<mfxMemId> m_pFramesMID;
    mfxU32          m_nFrames;
    mfxU16          m_Type;
    bool            m_bCreatedInApp;
    mfxFrameInfo    m_frameInfo;
};

class mfxSWFrameAllocator
{
public:
    mfxSWFrameAllocator ()
    {
        memset (m_HandleInfoArray,0, sizeof(sHandleInfo)*MFX_NUM_HANDLES);
        m_nHandles = 0;

        for (int i = 0; i<MFX_NUM_HANDLES; i++)
        {
            m_pHandleInfo[i] = &m_HandleInfoArray[i];
        }
    }
    mfxStatus AllocFrames (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool bCreatedInApp, bool *bCreated);
    mfxStatus LockFrame   (mfxMemId mid, mfxFrameData *ptr);
    mfxStatus UnlockFrame (mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFrames  (mfxFrameAllocResponse *response, bool bCreatedInApp);
    mfxStatus FreeFrames  (mfxU32 nHandle);
    mfxStatus FreeFrames  ();

    
    bool        CheckExistMid (mfxMemId mid);
    mfxStatus   CheckResponse(mfxFrameAllocResponse *response, mfxU32* nHandle);


protected:
    mfxStatus CreateFrames (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool bCreatedInApp);
    mfxStatus CheckRequest(mfxFrameAllocRequest *request, mfxU32* nHandle);
 


private:
    sHandleInfo     m_HandleInfoArray [MFX_NUM_HANDLES];

    sHandleInfo*    m_pHandleInfo [MFX_NUM_HANDLES];
    mfxU32          m_nHandles;
};


class mfxBaseFrameAllocator
{
private:
    bool m_bD3DSupport;
protected:
    // list of allocators:
    mfxSWFrameAllocator allocator0;//(MXF_SW0_FRAME) 
    mfxSWFrameAllocator allocator1;//(MXF_SW1_FRAME) 
    mfxSWFrameAllocator allocator2;//(MXF_SW2_FRAME)

public:
    mfxBaseFrameAllocator(bool bD3D)
    {
        m_bD3DSupport = bD3D;
    }
    virtual     ~mfxBaseFrameAllocator()  {};

    mfxStatus AllocFrames (mfxFrameAllocRequest *request, mfxFrameAllocResponse *response,bool bCreatedInApp);
    mfxStatus LockFrame   (mfxMemId mid, mfxFrameData *ptr);
    mfxStatus UnlockFrame (mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFrames  (mfxFrameAllocResponse *response, bool bCreatedInApp);
    mfxStatus FreeFrames  ();
};




#endif

