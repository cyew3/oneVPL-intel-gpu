/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_HEAP_H
#define __UMC_H265_HEAP_H

#include <memory>
#include "umc_mutex.h"
#include "umc_h265_dec_defs_dec.h"
#include "umc_media_data.h"

namespace UMC_HEVC_DECODER
{

Ipp8u * CumulativeArraysAllocation(int n, int align, ...);
void CumulativeFree(Ipp8u * ptr);

//*********************************************************************************************/
//
//*********************************************************************************************/
class MemoryPiece
{
    friend class Heap;

public:
    // Default constructor
    MemoryPiece()
    {
        m_pNext = 0;
        Reset();
    }

    // Destructor
    ~MemoryPiece()
    {
        Release();
    }

    static MemoryPiece * AllocateMemoryPiece(size_t nSize)
    {
        Ipp8u * ppp = new Ipp8u[sizeof(Ipp8u)*nSize + sizeof(MemoryPiece)];
        if (!ppp)
            throw h265_exception(UMC::UMC_ERR_ALLOC);
        MemoryPiece * piece = new (ppp) MemoryPiece();

        piece->m_pSourceBuffer = (Ipp8u*)ppp + sizeof(MemoryPiece);
        piece->m_pDataPointer = piece->m_pSourceBuffer;
        piece->m_nSourceSize = nSize;
        return piece;
    }

    static void FreeMemoryPiece(MemoryPiece * piece)
    {
        delete[] (Ipp8u*)piece;
    }

    void SetData(UMC::MediaData *out)
    {
        Release();

        m_pDataPointer = (Ipp8u*)out->GetDataPointer();
        m_nDataSize = out->GetDataSize();
        m_pts = out->GetTime();
    }

    // Allocate memory piece
    bool Allocate(size_t nSize)
    {
        Release();

        // DEBUG : ADB : need to use ExternalMemoryAllocator
        // allocate little more
        m_pSourceBuffer = h265_new_array_throw<Ipp8u>((Ipp32s)nSize);
        m_pDataPointer = m_pSourceBuffer;
        m_nSourceSize = nSize;
        return true;
    }

    // Get next element
    MemoryPiece *GetNext(){return m_pNext;}
    // Obtain data pointer
    Ipp8u *GetPointer(){return m_pDataPointer;}

    size_t GetSize() const {return m_nSourceSize;}

    size_t GetDataSize() const {return m_nDataSize;}
    void SetDataSize(size_t dataSize) {m_nDataSize = dataSize;}

    Ipp64f GetTime() const {return m_pts;}
    void SetTime(Ipp64f pts) {m_pts = pts;}

protected:
    Ipp8u *m_pSourceBuffer;                                     // (Ipp8u *) pointer to source memory
    Ipp8u *m_pDataPointer;                                      // (Ipp8u *) pointer to source memory
    size_t m_nSourceSize;                                       // (size_t) allocated memory size
    size_t m_nDataSize;                                         // (size_t) data memory size
    MemoryPiece *m_pNext;                                   // (MemoryPiece *) pointer to next memory piece
    Ipp64f   m_pts;

    void Reset()
    {
        m_pSourceBuffer = 0;
        m_pDataPointer = 0;
        m_nSourceSize = 0;
        m_nDataSize = 0;
    }

    void Release()
    {
        if (m_pSourceBuffer)
            delete[] m_pSourceBuffer;
        Reset();
    }
};

//*********************************************************************************************/
//
//*********************************************************************************************/
//#define CONST_HEAP_DEFINE

class Heap
{
public:

    Heap()
        : m_pFilledMemory(0)
        , m_pFreeMemory(0)
    {
    }

    virtual ~Heap()
    {
        while (m_pFreeMemory)
        {
            MemoryPiece *pMem = m_pFreeMemory->m_pNext;
            MemoryPiece::FreeMemoryPiece(m_pFreeMemory);
            m_pFreeMemory = pMem;
        }

        while (m_pFilledMemory)
        {
            MemoryPiece *pMem = m_pFilledMemory->m_pNext;
            MemoryPiece::FreeMemoryPiece(m_pFilledMemory);
            m_pFilledMemory = pMem;
        }
    }

    const MemoryPiece * GetLastAllocated() const
    {
        return m_pFilledMemory;
    }

    MemoryPiece * Allocate(size_t nSize)
    {
#ifndef CONST_HEAP_DEFINE
        MemoryPiece *pMem = MemoryPiece::AllocateMemoryPiece(nSize);
#else
        MemoryPiece *pMem = GetFreeMemoryPiece(nSize);
        AddFilledMemoryPiece(pMem);
#endif
        return pMem;
    }

    void Free(MemoryPiece *pMem)
    {
        if (!pMem)
            return;

#ifndef CONST_HEAP_DEFINE
        MemoryPiece::FreeMemoryPiece(pMem);
#else
        //m_pFilledMemory = m_pFilledMemory->m_pNext;
        MemoryPiece *pTemp = m_pFilledMemory;

        if (pTemp == pMem)
        {
            m_pFilledMemory = m_pFilledMemory->m_pNext;
        }
        else
        {
#ifdef _DEBUG
            bool is_found = false;
#endif
            // find pMem at list
            while (pTemp->m_pNext)
            {
                 if (pTemp->m_pNext == pMem)
                 {
                     pTemp->m_pNext = pMem->m_pNext;
#ifdef _DEBUG
                    is_found = true;
#endif
                     break;
                 }
                 pTemp = pTemp->m_pNext;
            }

#ifdef _DEBUG
            VM_ASSERT(is_found);
#endif
        }

        pMem->m_pNext = m_pFreeMemory;
        m_pFreeMemory = pMem;
#endif
    }

    void Reset()
    {
        while (m_pFilledMemory)
        {
            MemoryPiece *pMem = m_pFilledMemory;
            m_pFilledMemory = pMem->GetNext();
            pMem->m_pNext = m_pFreeMemory;
            m_pFreeMemory = pMem;
        }
    }

protected:

    // Get free piece of memory
    MemoryPiece *GetFreeMemoryPiece(size_t nSize)
    {
#ifndef CONST_HEAP_DEFINE
        MemoryPiece *pTemp = h265_new_throw<MemoryPiece>();
        pTemp->m_pNext = 0;
        pTemp->Allocate(nSize);
        return pTemp;
#else
        // try to find suitable already allocated memory
        MemoryPiece *pTemp = m_pFreeMemory;
        MemoryPiece *pPrev = 0;

        MemoryPiece *pGoodPiece = 0;
        size_t currMin = (size_t)-1;
        bool found = false;

        while (pTemp)
        {
            if (pTemp->m_nSourceSize >= nSize && currMin > (pTemp->m_nSourceSize - nSize))
            {
                pGoodPiece = pPrev;
                currMin = pTemp->m_nSourceSize;
                found = true;
            }

            pPrev = pTemp;
            pTemp = pTemp->m_pNext;
        }

        if (found)
        {
            pTemp = pGoodPiece ? pGoodPiece->m_pNext : m_pFreeMemory;

            if (pGoodPiece)
                pGoodPiece->m_pNext = pGoodPiece->m_pNext->m_pNext;
            else
                m_pFreeMemory = m_pFreeMemory->m_pNext;

            return pTemp;
        }

        // we found nothing, try to allocate new
        {
            pTemp = MemoryPiece::AllocateMemoryPiece(nSize * 2);
            // bind to list to avoid memory leak
            pTemp->m_pNext = m_pFreeMemory;
            m_pFreeMemory = pTemp;
            // unbind from list
            m_pFreeMemory = m_pFreeMemory->m_pNext;

            return pTemp;
        }
#endif
    }

    // Add filled piece of memory
    void AddFilledMemoryPiece(MemoryPiece *pMem)
    {
        if (m_pFilledMemory)
        {
            MemoryPiece *pTemp = m_pFilledMemory;

            // find end of list
            while (pTemp->m_pNext)
                pTemp = pTemp->m_pNext;
            pTemp->m_pNext = pMem;
        }
        else
            m_pFilledMemory = pMem;

        pMem->m_pNext = 0;

    }

protected:
    MemoryPiece * m_pFilledMemory;
    MemoryPiece * m_pFreeMemory;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Item class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Heap_Objects;

class Item
{
public:
    Item(Heap_Objects * heap, void * ptr, size_t size, bool isTyped = false)
        : m_pNext(0)
        , m_Ptr(ptr)
        , m_Size(size)
        , m_isTyped(isTyped)
        , m_heap(heap)
    {
    }

    ~Item()
    {
    }

    Item* m_pNext;
    void * m_Ptr;
    size_t m_Size;
    bool   m_isTyped;
    Heap_Objects * m_heap;

    static Item * Allocate(Heap_Objects * heap, size_t size, bool isTyped = false)
    {
        Ipp8u * ppp = new Ipp8u[size + sizeof(Item)];
        if (!ppp)
            throw h265_exception(UMC::UMC_ERR_ALLOC);
        Item * item = new (ppp) Item(heap, 0, size, isTyped);
        item->m_Ptr = (Ipp8u*)ppp + sizeof(Item);
        return item;
    }

    static void Free(Item *item)
    {
        if (item->m_isTyped)
        {
            HeapObject * obj = reinterpret_cast<HeapObject *>(item->m_Ptr);
            obj->~HeapObject();
        }

        item->~Item();
        delete[] (Ipp8u*)item;
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Heap_Objects class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class Heap_Objects
{
public:

    Heap_Objects()
        : m_pFirstFree(0)
    {
    }

    virtual ~Heap_Objects()
    {
        Release();
    }

    Item * GetItemForAllocation(size_t size, bool typed = false)
    {
        UMC::AutomaticUMCMutex guard(m_mGuard);

        if (!m_pFirstFree)
        {
            return 0;
        }

        if (m_pFirstFree->m_Size == size && m_pFirstFree->m_isTyped == typed)
        {
            Item * ptr = m_pFirstFree;
            m_pFirstFree = m_pFirstFree->m_pNext;
            VM_ASSERT(ptr->m_Size == size);
            return ptr;
        }

        Item * temp = m_pFirstFree;

        while (temp->m_pNext)
        {
            if (temp->m_pNext->m_Size == size && temp->m_pNext->m_isTyped == typed)
            {
                Item * ptr = temp->m_pNext;
                temp->m_pNext = temp->m_pNext->m_pNext;
                return ptr;
            }

            temp = temp->m_pNext;
        }

        return 0;
    }

    void* Allocate(size_t size, bool isTyped = false)
    {
        Item * item = GetItemForAllocation(size);
        if (!item)
        {
            item = Item::Allocate(this, size, isTyped);
        }

        return item->m_Ptr;
    }

    template<typename T>
    T* Allocate(size_t size = sizeof(T), bool isTyped = false)
    {
        return (T*)Allocate(size, isTyped);
    }

    template <typename T>
    T* AllocateObject()
    {
        Item * item = GetItemForAllocation(sizeof(T), true);

        if (!item)
        {
            void * ptr = Allocate(sizeof(T), true);
            return new(ptr) T();
        }

        return (T*)(item->m_Ptr);
    }

    void FreeObject(void * obj, bool force = false)
    {
        Free(obj, force);
    }

    void Free(void * obj, bool force = false)
    {
        if (!obj)
            return;

        UMC::AutomaticUMCMutex guard(m_mGuard);
        Item * item = (Item *) ((Ipp8u*)obj - sizeof(Item));

        // check
        Item * temp = m_pFirstFree;

        while (temp)
        {
            if (temp == item) //was removed yet
                return;

            temp = temp->m_pNext;
        }

        if (force)
        {
            Item::Free(item);
            return;
        }
        else
        {
            if (item->m_isTyped)
            {
                HeapObject * obj = reinterpret_cast<HeapObject *>(item->m_Ptr);
                obj->Reset();
            }
        }

        item->m_pNext = m_pFirstFree;
        m_pFirstFree = item;
    }

    void Release()
    {
        UMC::AutomaticUMCMutex guard(m_mGuard);

        while (m_pFirstFree)
        {
            Item *pTemp = m_pFirstFree->m_pNext;
            Item::Free(m_pFirstFree);
            m_pFirstFree = pTemp;
        }
    }

private:

    Item * m_pFirstFree;
    UMC::Mutex m_mGuard;
};


//*********************************************************************************************/
// H265_List implementation
//*********************************************************************************************/
template <typename T>
class H265_List
{
public:
    class Item : public HeapObject
    {
    public:
        Item(T *item, Ipp32s pid)
            : m_pNext(0)
            , m_Item(item)
            , m_pid(pid)
        {
        }

        virtual void Reset()
        {
            m_Item->Reset();
        }

        Item * m_pNext;
        T *m_Item;
        Ipp32s m_pid;
    };

    H265_List(Heap_Objects * pObjHeap)
        : m_pHead(0)
        , m_pObjHeap(pObjHeap)
    {
    }

    ~H265_List()
    {
        Reset();
    }

    void RemoveHead()
    {
        Item * tmp = m_pHead;
        m_pHead = m_pHead->m_pNext;
        m_pObjHeap->FreeObject(tmp);
    }

    void RemoveItem(T * item)
    {
        if (!m_pHead)
        {
            VM_ASSERT(false);
            return;
        }

        Item *tmp = m_pHead;

        if (tmp->m_Item == item)
        {
            m_pHead = m_pHead->m_pNext;
            m_pObjHeap->FreeObject(tmp);
            return;
        }

        while (tmp->m_pNext)
        {
            if (tmp->m_pNext->m_Item == item)
            {
                Item * list_item = tmp->m_pNext;
                tmp->m_pNext = tmp->m_pNext->m_pNext;
                m_pObjHeap->FreeObject(list_item);
                return;
            }

            tmp = tmp->m_pNext;
        }

        // it was removed before
        VM_ASSERT(false);
    }

    T * DetachItemByPid(Ipp32s pid)
    {
        if (!m_pHead)
        {
            return 0;
        }

        T * item = 0;
        Item *tmp = m_pHead;
        for (; tmp; tmp = tmp->m_pNext)
        {
            if (tmp->m_pid == pid)
            {
                item = tmp->m_Item;
                break;
            }
        }

        if (!tmp)
            return 0;

        tmp = m_pHead;

        if (tmp->m_Item == item)
        {
            m_pHead = m_pHead->m_pNext;
            m_pObjHeap->FreeObject(tmp);
            return item;
        }

        while (tmp->m_pNext)
        {
            if (tmp->m_pNext->m_Item == item)
            {
                Item * list_item = tmp->m_pNext;
                tmp->m_pNext = tmp->m_pNext->m_pNext;
                m_pObjHeap->FreeObject(list_item);
                return item;
            }

            tmp = tmp->m_pNext;
        }

        VM_ASSERT(false);
        return 0;
    }

    T* FindByPid(Ipp32s pid)
    {
        for (Item *tmp = m_pHead; tmp; tmp = tmp->m_pNext)
        {
            if (tmp->m_pid == pid)
                return tmp->m_Item;
        }

        return 0;
    }

    T* FindLastByPid(Ipp32s pid) const
    {
        T *last = 0;
        for (Item *tmp = m_pHead; tmp; tmp = tmp->m_pNext)
        {
            if (tmp->m_pid == pid)
                last = tmp->m_Item;
        }

        return last;
    }

    void AddItem(T *item, Ipp32s pid)
    {
        Item * buf = (Item*)m_pObjHeap->Allocate(sizeof(Item));
        Item *newItem = new(buf) Item(item, pid);

        Item *tmp = m_pHead;
        if (m_pHead)
        {
            while (tmp->m_pNext)
            {
                tmp = tmp->m_pNext;
            }

            tmp->m_pNext = newItem;
        }
        else
        {
            m_pHead = newItem;
        }
    }

    T * GetHead()
    {
        return m_pHead->m_Item;
    }

    const T * GetHead() const
    {
        return m_pHead->m_Item;
    }

    void Reset()
    {
        for (Item *tmp = m_pHead; tmp; )
        {
            Item *tmp1 = tmp;
            tmp = tmp->m_pNext;
            m_pObjHeap->FreeObject(tmp1);
        }

        m_pHead = 0;
    }

private:
    Item * m_pHead;
    Heap_Objects * m_pObjHeap;
};

//*********************************************************************************************/
//
//*********************************************************************************************/
class CoeffsBuffer : public HeapObject
{
public:
    // Default constructor
    CoeffsBuffer(void);
    // Destructor
    virtual ~CoeffsBuffer(void);

    // Initialize buffer
    UMC::Status Init(Ipp32s numberOfItems, Ipp32s sizeOfItem);

    bool IsInputAvailable() const;
    // Lock input buffer
    Ipp8u* LockInputBuffer();
    // Unlock input buffer
    bool UnLockInputBuffer(size_t size);

    bool IsOutputAvailable() const;
    // Lock output buffer
    bool LockOutputBuffer(Ipp8u *& pointer, size_t &size);
    // Unlock output buffer
    bool UnLockOutputBuffer();
    // Release object
    void Close(void);
    // Reset object
    virtual void Reset(void);

    virtual void Free();

protected:
    UMC::Mutex mutex;

    Ipp8u *m_pbAllocatedBuffer;       // (Ipp8u *) pointer to allocated unaligned buffer
    size_t m_lAllocatedBufferSize;    // (Ipp32s) size of allocated buffer

    Ipp8u *m_pbBuffer;                // (Ipp8u *) pointer to allocated buffer
    size_t m_lBufferSize;             // (Ipp32s) size of using buffer

    Ipp8u *m_pbFree;                  // (Ipp8u *) pointer to free space
    size_t m_lFreeSize;               // (Ipp32s) size of free space

    size_t m_lItemSize;               // (Ipp32s) size of output data portion

    struct BufferInfo
    {
        Ipp8u * m_pPointer;
        size_t  m_Size;
        BufferInfo *m_pNext;
    };

    BufferInfo *m_pBuffers;           // (Buffer *) queue of filled sample info

    void Lock();
    void Unlock();
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_HEAP_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
