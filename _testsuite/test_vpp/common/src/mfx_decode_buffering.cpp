/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/
   
#include <stdlib.h>

#include <mfx_decode_buffering.h>

CDecodingBuffering::CDecodingBuffering():
    m_SurfacesNumber(0),
    m_OutputSurfacesNumber(0),
    m_pSurfaces(NULL),
    m_pFreeSurfaces(NULL),
    m_pUsedSurfacesHead(NULL),
    m_pUsedSurfacesTail(NULL),
    m_pFreeOutputSurfaces(NULL),
    m_pOutputSurfacesHead(NULL),
    m_pOutputSurfacesTail(NULL),
    m_OutputSurfacesCount(0),
    m_pDeliveredSurfacesHead(NULL),
    m_pDeliveredSurfacesTail(NULL),
    m_DeliveredSurfacesCount(0)
{
}

CDecodingBuffering::~CDecodingBuffering()
{
}

mfxStatus
CDecodingBuffering::AllocBuffers(mfxU32 SurfaceNumber)
{
    if (!SurfaceNumber) return MFX_ERR_MEMORY_ALLOC;

    m_OutputSurfacesNumber = m_SurfacesNumber = SurfaceNumber;

    m_pSurfaces = (msdkFrameSurface*)calloc(m_SurfacesNumber, sizeof(msdkFrameSurface));
    if (!m_pSurfaces) return MFX_ERR_MEMORY_ALLOC;

    msdkOutputSurface* p = NULL;
    msdkOutputSurface* tail = NULL;
    for (mfxU32 i = 0; i < m_OutputSurfacesNumber; ++i) {
        p = (msdkOutputSurface*)calloc(1, sizeof(msdkOutputSurface));
        if (!m_pFreeOutputSurfaces) {
            tail = m_pFreeOutputSurfaces = p;
        }
        else {
            tail->next = p;
            tail = p;
        }
    }

    ResetBuffers();
    return MFX_ERR_NONE;
}

void
CDecodingBuffering::AllocOutputBuffer()
{
    AutomaticMutex lock(m_Mutex);
    
    m_pFreeOutputSurfaces = (msdkOutputSurface*)calloc(1, sizeof(msdkOutputSurface));
}

static void
FreeList(msdkOutputSurface* head) {
    msdkOutputSurface* next;
    if (head) {
        while (head) {
            next = head->next;
            free(head);
            head = next;
        }
    }
}

void
CDecodingBuffering::FreeBuffers()
{
    m_pUsedSurfacesHead = NULL;
    m_pUsedSurfacesTail = NULL;

    if (m_pSurfaces) {
        free(m_pSurfaces);
        m_pSurfaces = NULL;
    }
    
    FreeList(m_pFreeOutputSurfaces);
    FreeList(m_pOutputSurfacesHead);
    FreeList(m_pDeliveredSurfacesHead);
}

void
CDecodingBuffering::ResetBuffers()
{
    mfxU32 i;
    m_pFreeSurfaces = m_pSurfaces;
    
    for (i = 0; i < m_SurfacesNumber; ++i) {
        if (i < (m_SurfacesNumber-1)) {
            m_pFreeSurfaces[i].next = &(m_pFreeSurfaces[i+1]);
            m_pFreeSurfaces[i+1].prev = &(m_pFreeSurfaces[i]);
        }
    }

    m_pUsedSurfacesHead = NULL;
    m_pUsedSurfacesTail = NULL;
    m_pOutputSurfacesHead = NULL;
    m_pOutputSurfacesTail = NULL;
}

void
CDecodingBuffering::SyncFrameSurfaces()
{
    AutomaticMutex lock(m_Mutex);
    msdkFrameSurface *prev = NULL;
    msdkFrameSurface *next = NULL;
    msdkFrameSurface *cur = m_pUsedSurfacesHead;
    
    while (cur) {
        if (cur->frame.Data.Locked || cur->render_lock) {
            // frame is still locked: just moving to the next one
            cur = cur->next;
        } else {
            // frame was unlocked: moving it to the free surfaces array
            prev = cur->prev;
            next = cur->next;

            if (prev) {
                prev->next = next;
            }
            else {
                MSDK_SELF_CHECK(cur == m_pUsedSurfacesHead);
                m_pUsedSurfacesHead = next;
            }
            if (next) {
                next->prev = prev;
            } else {
                MSDK_SELF_CHECK(cur == m_pUsedSurfacesTail);
                m_pUsedSurfacesTail = prev;
            }

            cur->prev = cur->next = NULL;
            MSDK_SELF_CHECK(!cur->prev);
            MSDK_SELF_CHECK(!cur->next);

            m_Mutex.Unlock();
            AddFreeSurface(cur);
            m_Mutex.Lock();

            cur = next;
        }
    }
}
