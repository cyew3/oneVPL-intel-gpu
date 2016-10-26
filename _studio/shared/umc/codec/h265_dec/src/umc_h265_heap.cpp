//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_structures.h"
#include "umc_h265_heap.h"
#include "umc_h265_dec_defs.h"
#include <cstdarg>

namespace UMC_HEVC_DECODER
{

enum
{
    COEFFS_BUFFER_ALIGN_VALUE_H265                 = 128
};

CoeffsBuffer::CoeffsBuffer(void)
{
    // reset variables
    m_pbAllocatedBuffer = 0;
    m_lAllocatedBufferSize = 0;

    m_pbBuffer = 0;
    m_lBufferSize = 0;
    m_pbFree = 0;
    m_lFreeSize = 0;
    m_pBuffers = 0;
    m_lItemSize = 0;

} // CoeffsBuffer::CoeffsBuffer(void)

CoeffsBuffer::~CoeffsBuffer(void)
{
    Close();
} // CoeffsBuffer::~CoeffsBuffer(void)

void CoeffsBuffer::Lock()
{
    mutex.Lock();
}

void CoeffsBuffer::Unlock()
{
    mutex.Unlock();
}

void CoeffsBuffer::Close(void)
{
    // delete buffer
    if (m_pbAllocatedBuffer)
        delete[] m_pbAllocatedBuffer;

    // reset variables
    m_pbAllocatedBuffer = 0;
    m_lAllocatedBufferSize = 0;
    m_pbBuffer = 0;
    m_lBufferSize = 0;
    m_pbFree = 0;
    m_lFreeSize = 0;
    m_pBuffers = 0;
} // void CoeffsBuffer::Close(void)

UMC::Status CoeffsBuffer::Init(Ipp32s numberOfItems, Ipp32s sizeOfItem)
{
    Ipp32s lMaxSampleSize = sizeOfItem + COEFFS_BUFFER_ALIGN_VALUE_H265 + (Ipp32s)sizeof(BufferInfo);
    Ipp32s lAllocate = lMaxSampleSize * numberOfItems;

    if ((Ipp32s)m_lBufferSize != lAllocate)
    {
        Close();

        // allocate buffer
        m_pbAllocatedBuffer = h265_new_array_throw<Ipp8u>(lAllocate + COEFFS_BUFFER_ALIGN_VALUE_H265);
        m_lBufferSize = lAllocate;

        m_lAllocatedBufferSize = lAllocate + COEFFS_BUFFER_ALIGN_VALUE_H265;

        // align buffer
        m_pbBuffer = UMC::align_pointer<Ipp8u *> (m_pbAllocatedBuffer, COEFFS_BUFFER_ALIGN_VALUE_H265);
    }
    
    m_pBuffers = 0;
    m_pbFree = m_pbBuffer;
    m_lFreeSize = m_lBufferSize;

    // save preferred size(s)
    m_lItemSize = sizeOfItem;
    return UMC::UMC_OK;

} // Status CoeffsBuffer::Init(MediaReceiverParams *init)

bool CoeffsBuffer::IsInputAvailable() const
{
    size_t lFreeSize;

    // check error(s)
    if (NULL == m_pbFree)
        return false;

    // get free size
    if (m_pbFree >= m_pbBuffer + (m_lBufferSize - m_lFreeSize))
    {
        lFreeSize = m_pbBuffer + m_lBufferSize - m_pbFree;
    }
    else
    {
        lFreeSize = m_lFreeSize;
        if (lFreeSize < m_lItemSize + COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo))
            return false;
    }

    // check free size
    if (lFreeSize < m_lItemSize + COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo))
    {
        // when used data is present,
        // concatenate dummy bytes to last buffer info
        if (m_pBuffers)
        {
            return (m_lFreeSize - lFreeSize > m_lItemSize + COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo));
        }
        // when no used data,
        // simple move pointer(s)
        else
        {
            return (m_lFreeSize == m_lBufferSize);
        }
    }

    return true;
} // bool CoeffsBuffer::IsInputAvailable() const

Ipp8u* CoeffsBuffer::LockInputBuffer()
{
    size_t lFreeSize;
    //size_t size = m_lItemSize;

#ifdef LIGHT_SYNC
    AutomaticUMCMutex guard(mutex);
#endif

    // check error(s)
    if (NULL == m_pbFree)
        return 0;

    if (m_pBuffers == 0)
    {  // We could do it, because only one thread could get input buffer
        m_pbFree = m_pbBuffer;
        m_lFreeSize = m_lBufferSize;
    }

    // get free size
    if (m_pbFree >= m_pbBuffer + (m_lBufferSize - m_lFreeSize))
    {
        lFreeSize = m_pbBuffer + m_lBufferSize - m_pbFree;
    }
    else
    {
        lFreeSize = m_lFreeSize;
        if (lFreeSize < m_lItemSize + COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo))
            return 0;
    }

    // check free size
    if (lFreeSize < m_lItemSize + COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo))
    {
        // when used data is present,
        // concatenate dummy bytes to last buffer info
        if (m_pBuffers)
        {
            BufferInfo *pTemp;

            // find last sample info
            pTemp = m_pBuffers;
            while (pTemp->m_pNext)
                pTemp = pTemp->m_pNext;
            pTemp->m_Size += lFreeSize;

            // update variable(s)
            m_pbFree = m_pbBuffer;
            m_lFreeSize -= lFreeSize;

            return LockInputBuffer();
        }
        // when no used data,
        // simple move pointer(s)
        else
        {
            if (m_lFreeSize == m_lBufferSize)
            {
                m_pbFree = m_pbBuffer;
                return m_pbFree;
            }
            return 0;
        }
    }

    // set free pointer
    //pointer = m_pbFree;
    //size = lFreeSize - COEFFS_BUFFER_ALIGN_VALUE_H265 - sizeof(BufferInfo);
    return m_pbFree;
} // bool CoeffsBuffer::LockInputBuffer(MediaData* in)

bool CoeffsBuffer::UnLockInputBuffer(size_t size)
{
    size_t lFreeSize;
    BufferInfo *pTemp;
    Ipp8u *pb;

#ifdef LIGHT_SYNC
    AutomaticUMCMutex guard(mutex);
#endif

    // check error(s)
    if (NULL == m_pbFree)
        return false;

    // when no data given
    //if (0 == size) // even for size = 0 we should add buffer
      //  return true;

    // get free size
    if (m_pbFree >= m_pbBuffer + (m_lBufferSize - m_lFreeSize))
        lFreeSize = m_pbBuffer + m_lBufferSize - m_pbFree;
    else
        lFreeSize = m_lFreeSize;

    // check free size
    if (lFreeSize < m_lItemSize)
        return false;

    // check used data
    if (size + COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo) > lFreeSize) // DEBUG : should not be !!!
    {
        VM_ASSERT(false);
        return false;
    }

    // get new buffer info
    pb = UMC::align_pointer<Ipp8u *> (m_pbFree + size, COEFFS_BUFFER_ALIGN_VALUE_H265);
    pTemp = reinterpret_cast<BufferInfo *> (pb);

    size += COEFFS_BUFFER_ALIGN_VALUE_H265 + sizeof(BufferInfo);

    // fill buffer info
    pTemp->m_Size = size;
    pTemp->m_pPointer = m_pbFree;
    pTemp->m_pNext = 0;

    // add sample to end of queue
    if (m_pBuffers)
    {
        BufferInfo *pWork = m_pBuffers;

        while (pWork->m_pNext)
            pWork = pWork->m_pNext;

        pWork->m_pNext = pTemp;
    }
    else
        m_pBuffers = pTemp;

    // update variable(s)
    m_pbFree += size;
    if (m_pbBuffer + m_lBufferSize == m_pbFree)
        m_pbFree = m_pbBuffer;
    m_lFreeSize -= size;

    return true;
} // bool CoeffsBuffer::UnLockInputBuffer(size_t size)

bool CoeffsBuffer::IsOutputAvailable() const
{
    return (0 != m_pBuffers);
} // bool CoeffsBuffer::IsOutputAvailable() const

bool CoeffsBuffer::LockOutputBuffer(Ipp8u* &pointer, size_t &size)
{
    // check error(s)
    if (0 == m_pBuffers)
        return false;

    // set used pointer
    pointer = m_pBuffers->m_pPointer;
    size = m_pBuffers->m_Size;
    return true;
} // bool CoeffsBuffer::LockOutputBuffer(Ipp8u* &pointer, size_t &size)

bool CoeffsBuffer::UnLockOutputBuffer()
{
#ifdef LIGHT_SYNC
    AutomaticUMCMutex guard(mutex);
#endif

    // no filled data is present
    if (0 == m_pBuffers)
        return false;

    // update variable(s)
    m_lFreeSize += m_pBuffers->m_Size;
    m_pBuffers = m_pBuffers->m_pNext;

    return true;
} // bool CoeffsBuffer::UnLockOutputBuffer()

void CoeffsBuffer::Reset()
{
#ifdef LIGHT_SYNC
    AutomaticUMCMutex guard(mutex);
#endif

    // align buffer
    m_pbBuffer = UMC::align_pointer<Ipp8u *> (m_pbAllocatedBuffer, COEFFS_BUFFER_ALIGN_VALUE_H265);

    m_pBuffers = 0;

    m_pbFree = m_pbBuffer;
    m_lFreeSize = m_lBufferSize;
} //void CoeffsBuffer::Reset()

void CoeffsBuffer::Free()
{
    HeapObject::Free();
}

/*class Allocator
{
public:
    template<typename T>
    T * AllocateObject()
    {
        T * t = new T();
        if (!t)
            throw h265_exception(UMC_ERR_ALLOC);
        return t;
    }

    template<typename T>
    T * AllocateArray(Ipp32s size)
    {
        T * t = new T();
        if (!t)
            throw h265_exception(UMC_ERR_ALLOC);
        return t;
    }

    template <typename T>
    T* Allocate(Ipp32s size = sizeof(T))
    {
        if (!m_pFirstFree)
        {
            Item_H265 * item = Item_H265::Allocate<T>(size);
            return (T*)(item->m_Ptr);
        }

        Item_H265 * temp = m_pFirstFree;

        while (temp->m_pNext)
        {
            if (temp->m_pNext->m_Size == size)
            {
                void * ptr = temp->m_pNext->m_Ptr;
                temp->m_pNext = temp->m_pNext->m_pNext;
                return (T*)ptr;
            }

            temp = temp->m_pNext;
        }

        Item_H265 * item = Item_H265::Allocate<T>(size);
        return (T*)(item->m_Ptr);
    }

    template <typename T>
    T* AllocateObject()
    {
        void * ptr = Allocate<T>();
        return new(ptr) T();
    }

    template <typename T>
    void FreeObject(T * obj, bool force = false)
    {
        if (!obj)
            return;
        obj->~T();
        Free(obj, force);
    }

    void Free(void * obj, bool force = false)
    {
        if (!obj)
            return;

        Item_H265 * item = (Item_H265 *) ((Ipp8u*)obj - sizeof(Item_H265));

        if (force)
        {
            Item_H265::Free(item);
            return;
        }

        item->m_pNext = m_pFirstFree;
        m_pFirstFree = item;
    }
};*/


void HeapObject::Free()
{
    Item * item = (Item *) ((Ipp8u*)this - sizeof(Item));
    item->m_heap->Free(this);
}

void RefCounter::IncrementReference() const
{
    m_refCounter++;
}

void RefCounter::DecrementReference()
{
    m_refCounter--;

    VM_ASSERT(m_refCounter >= 0);
    if (!m_refCounter)
    {
        Free();
    }
}

// Allocate several arrays inside of one memory buffer
Ipp8u * CumulativeArraysAllocation(int n, int align, ...)
{
    va_list args;
    va_start(args, align);

    int cumulativeSize = 0;
    for (int i = 0; i < n; i++)
    {
        void * ptr = va_arg(args, void *);
        ptr; // just skip it

        int currSize = va_arg(args, int);
        cumulativeSize += currSize;
    }

    va_end(args);

    Ipp8u *cumulativePtr = h265_new_array_throw<Ipp8u>(cumulativeSize + align*n);
    Ipp8u *cumulativePtrSaved = cumulativePtr;

    va_start(args, align);

    for (int i = 0; i < n; i++)
    {
        void ** ptr = va_arg(args, void **);
        
        *ptr = align ? UMC::align_pointer<void*> (cumulativePtr, align) : cumulativePtr;

        int currSize = va_arg(args, int);
        cumulativePtr = (Ipp8u*)*ptr + currSize;
    }

    va_end(args);
    return cumulativePtrSaved;
}

// Free memory allocated by CumulativeArraysAllocation
void CumulativeFree(Ipp8u * ptr)
{
    delete[] ptr;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
