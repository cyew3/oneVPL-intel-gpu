/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#ifndef __MFX_DECODE_BUFFERING_H__
#define __MFX_DECODE_BUFFERING_H__

#include <stdio.h>

#include "mfxstructures.h"

#include "vm/strings_defs.h"
#include "vm/thread_defs.h"
#include "vm/time_defs.h"
#include "vm/atomic_defs.h"

struct msdkFrameSurface
{
    mfxFrameSurface1 frame; // NOTE: this _should_ be the first item (see CDecodingBuffering::FindUsedSurface())
    msdk_tick submit;       // tick when frame was submitted for processing
    mfxU16 render_lock;     // signifies that frame is locked for rendering
    msdkFrameSurface* prev;
    msdkFrameSurface* next;
};

struct msdkOutputSurface
{
    msdkFrameSurface* surface;
    mfxSyncPoint syncp;
    msdkOutputSurface* next;
};

/** \brief Debug purpose macro to terminate execution if buggy situation happenned.
 *
 * Use this macro to check impossible, buggy condition which should not occur under
 * normal circumstances. Macro should be used where check in release mode is not
 * desirable and atually needed.
 */
#if 0 //_DEBUG
    #define MSDK_SELF_CHECK(C) \
        { \
            if (!(C)) { \
                msdk_printf(MSDK_STRING("bug: %s:%d: self-check failed: '%s' is not true\n"), \
                    __FILE__, __LINE__, #C); \
                exit(-1); \
            } \
        }
#else
    #define MSDK_SELF_CHECK(C)
#endif

/** \brief Helper class defining optimal buffering operations for the Media SDK decoder.
 */
class CDecodingBuffering
{
public:
    CDecodingBuffering();
    virtual ~CDecodingBuffering();

protected: // functions
    mfxStatus AllocBuffers(mfxU32 SurfaceNumber);
    void AllocOutputBuffer();
    void FreeBuffers();
    void ResetBuffers();

    /** \brief The function syncs arrays of free and used surfaces.
     *
     * If Media SDK used surface for internal needs and unlocked it, the function moves such a surface
     * back to the free surfaces array.
     */
    void SyncFrameSurfaces();

    /** \brief The function adds free surface to the free surfaces array.
     *
     * @note That's caller responsibility to pass valid surface.
     * @note We always add and get free surface from the array head. In case not all surfaces
     * will be actually used we have good chance to avoid actual allocation of the surface memory.
     */
    inline void AddFreeSurface(msdkFrameSurface* surface)
    {
        AutomaticMutex lock(m_Mutex);
        msdkFrameSurface* head;

        MSDK_SELF_CHECK(surface);
        MSDK_SELF_CHECK(!surface->prev);
        MSDK_SELF_CHECK(!surface->next);

        head = m_pFreeSurfaces;
        m_pFreeSurfaces = surface;
        m_pFreeSurfaces->next = head;
    }

    /** \brief The function gets the next free surface from the free surfaces array.
     *
     * @note Surface is detached from the free surfaces array.
     */
    inline msdkFrameSurface* GetFreeSurface()
    {
        AutomaticMutex lock(m_Mutex);
        msdkFrameSurface* surface = NULL;

        if (m_pFreeSurfaces) {
            surface = m_pFreeSurfaces;
            m_pFreeSurfaces = m_pFreeSurfaces->next;
            surface->prev = surface->next = NULL;
            MSDK_SELF_CHECK(!surface->prev);
            MSDK_SELF_CHECK(!surface->next);
        }
        return surface;
    }

    /** \brief The function adds surface to the used surfaces array (m_pUsedSurfaces).
     *
     * @note That's caller responsibility to pass valid surface.
     * @note We can't actually know which surface will be returned by the decoder or unlocked. However,
     * we can make prediction that it will be the oldest surface. Thus, here the function adds new
     * surface (youngest) to the tail of the least. Check operations for the list will run starting from
     * head.
     */
    inline void AddUsedSurface(msdkFrameSurface* surface)
    {
        AutomaticMutex lock(m_Mutex);

        MSDK_SELF_CHECK(surface);
        MSDK_SELF_CHECK(!surface->prev);
        MSDK_SELF_CHECK(!surface->next);

        surface->prev = m_pUsedSurfacesTail;
        surface->next = NULL;
        if (m_pUsedSurfacesTail) {
            m_pUsedSurfacesTail->next = surface;
            m_pUsedSurfacesTail = m_pUsedSurfacesTail->next;
        } else {
            m_pUsedSurfacesHead = m_pUsedSurfacesTail = surface;
        }
    }

    /** \brief The function detaches surface from the used surfaces array.
     *
     * @note That's caller responsibility to pass valid surface.
     */
    inline void DetachUsedSurface(msdkFrameSurface* surface)
    {
        AutomaticMutex lock(m_Mutex);

        MSDK_SELF_CHECK(surface);

        msdkFrameSurface *prev = surface->prev;
        msdkFrameSurface *next = surface->next;

        if (prev) {
            prev->next = next;
        }
        else {
            MSDK_SELF_CHECK(surface == m_pUsedSurfacesHead);
            m_pUsedSurfacesHead = next;
        }
        if (next) {
            next->prev = prev;
        } else {
            MSDK_SELF_CHECK(surface == m_pUsedSurfacesTail);
            m_pUsedSurfacesTail = prev;
        }

        surface->prev = surface->next = NULL;
        MSDK_SELF_CHECK(!surface->prev);
        MSDK_SELF_CHECK(!surface->next);
    }

    /** \brief Returns surface which corresponds to the given one in Media SDK format (mfxFrameSurface1).
     *
     * @note This function will not detach the surface from the array, perform this explicitly.
     */
    inline msdkFrameSurface* FindUsedSurface(mfxFrameSurface1* frame)
    {
        return (msdkFrameSurface*)(frame);
    }

    inline void AddFreeOutputSurface(msdkOutputSurface* surface)
    {
        AutomaticMutex lock(m_Mutex);
        msdkOutputSurface* head = m_pFreeOutputSurfaces;

        MSDK_SELF_CHECK(surface);
        MSDK_SELF_CHECK(!surface->next);
        m_pFreeOutputSurfaces = surface;
        m_pFreeOutputSurfaces->next = head;
    }

    inline msdkOutputSurface* GetFreeOutputSurface()
    {
        AutomaticMutex lock(m_Mutex);
        msdkOutputSurface* surface = NULL;

        if (!m_pFreeOutputSurfaces) {
            m_Mutex.Unlock();
            AllocOutputBuffer();
            m_Mutex.Lock();
        }
        if (m_pFreeOutputSurfaces) {
            surface = m_pFreeOutputSurfaces;
            m_pFreeOutputSurfaces = m_pFreeOutputSurfaces->next;
            surface->next = NULL;
            MSDK_SELF_CHECK(!surface->next);
        }
        return surface;
    }

    inline void AddOutputSurface(msdkOutputSurface* surface)
    {
        AutomaticMutex lock(m_Mutex);

        MSDK_SELF_CHECK(surface);
        MSDK_SELF_CHECK(!surface->next);
        surface->next = NULL;

        if (m_pOutputSurfacesTail) {
            m_pOutputSurfacesTail->next = surface;
            m_pOutputSurfacesTail = m_pOutputSurfacesTail->next;
        } else {
            m_pOutputSurfacesHead = m_pOutputSurfacesTail = surface;
        }
        ++m_OutputSurfacesCount;
    }

    inline msdkOutputSurface* GetOutputSurface()
    {
        AutomaticMutex lock(m_Mutex);
        msdkOutputSurface* surface = NULL;

        if (m_pOutputSurfacesHead) {
            surface = m_pOutputSurfacesHead;
            m_pOutputSurfacesHead = m_pOutputSurfacesHead->next;
            if (!m_pOutputSurfacesHead) {
                // there was only one surface in the array...
                m_pOutputSurfacesTail = NULL;
            }
            --m_OutputSurfacesCount;
            surface->next = NULL;
            MSDK_SELF_CHECK(!surface->next);
        }
        return surface;
    }

    inline void AddDeliveredSurface(msdkOutputSurface* surface)
    {
        AutomaticMutex lock(m_Mutex);

        MSDK_SELF_CHECK(surface);
        MSDK_SELF_CHECK(!surface->next);
        surface->next = NULL;
        if (m_pDeliveredSurfacesTail) {
            m_pDeliveredSurfacesTail->next = surface;
            m_pDeliveredSurfacesTail = m_pDeliveredSurfacesTail->next;
        } else {
            m_pDeliveredSurfacesHead = m_pDeliveredSurfacesTail = surface;
        }
        ++m_DeliveredSurfacesCount;
    }

    inline msdkOutputSurface* GetDeliveredSurface()
    {
        AutomaticMutex lock(m_Mutex);
        msdkOutputSurface* surface = NULL;

        if (m_pDeliveredSurfacesHead) {
            surface = m_pDeliveredSurfacesHead;
            m_pDeliveredSurfacesHead = m_pDeliveredSurfacesHead->next;
            if (!m_pDeliveredSurfacesHead) {
                // there was only one surface in the array...
                m_pDeliveredSurfacesTail= NULL;
            }
            --m_DeliveredSurfacesCount;
            surface->next = NULL;
            MSDK_SELF_CHECK(!surface->next);
        }
        return surface;
    }

    /** \brief Function returns surface data to the corresponding buffers.
     */
    inline void ReturnSurfaceToBuffers(msdkOutputSurface* output_surface)
    {
        MSDK_SELF_CHECK(output_surface);
        MSDK_SELF_CHECK(output_surface->surface);
        MSDK_SELF_CHECK(output_surface->syncp);

        msdk_atomic_dec16(&(output_surface->surface->render_lock));

        output_surface->surface = NULL;
        output_surface->syncp = NULL;

        AddFreeOutputSurface(output_surface);
    }

protected: // variables
    mfxU32                  m_SurfacesNumber;
    mfxU32                  m_OutputSurfacesNumber;
    msdkFrameSurface*       m_pSurfaces;
    MSDKMutex               m_Mutex;

    // LIFO list of surfaces
    msdkFrameSurface*       m_pFreeSurfaces;

    // random access, predicted as FIFO
    msdkFrameSurface*       m_pUsedSurfacesHead; // oldest surface
    msdkFrameSurface*       m_pUsedSurfacesTail; // youngest surface

    // LIFO list of surfaces
    msdkOutputSurface*      m_pFreeOutputSurfaces;

    // FIFO list of surfaces
    msdkOutputSurface*      m_pOutputSurfacesHead; // oldest surface
    msdkOutputSurface*      m_pOutputSurfacesTail; // youngest surface
    mfxU32                  m_OutputSurfacesCount;

    // FIFO list of surfaces
    msdkOutputSurface*      m_pDeliveredSurfacesHead; // oldest surface
    msdkOutputSurface*      m_pDeliveredSurfacesTail; // youngest surface
    mfxU32                  m_DeliveredSurfacesCount;

private:
    CDecodingBuffering(const CDecodingBuffering&);
    void operator=(const CDecodingBuffering&);
};

#endif // __MFX_DECODE_BUFFERING_H__
