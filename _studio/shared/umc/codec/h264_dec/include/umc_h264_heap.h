//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_HEAP_H
#define __UMC_H264_HEAP_H

#include <memory>
#include "umc_mutex.h"
#include "umc_h264_dec_defs_dec.h"
#include "umc_media_data.h"

namespace UMC
{

//*********************************************************************************************/
//
//*********************************************************************************************/
class H264MemoryPiece
{
public:
    // Default constructor
    H264MemoryPiece()
    {
        m_pNext = 0;
        m_pts = 0;
        Reset();
    }

    // Destructor
    ~H264MemoryPiece()
    {
        Release();
    }

    void Release()
    {
        delete[] m_pSourceBuffer;
        Reset();
    }

    void SetData(MediaData *out)
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

        // allocate little more
        m_pSourceBuffer = h264_new_array_throw<Ipp8u>((Ipp32s)nSize);
        m_pDataPointer = m_pSourceBuffer;
        m_nSourceSize = nSize;
        return true;
    }

    // Get next element
    H264MemoryPiece *GetNext(){return m_pNext;}
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
    H264MemoryPiece *m_pNext;                                   // (H264MemoryPiece *) pointer to next memory piece
    Ipp64f   m_pts;

    void Reset()
    {
        m_pSourceBuffer = 0;
        m_pDataPointer = 0;
        m_nSourceSize = 0;
        m_nDataSize = 0;
    }

private:
    H264MemoryPiece( const H264MemoryPiece &s );                // no copy CTR
    H264MemoryPiece & operator=(const H264MemoryPiece &s );
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Item class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class H264_Heap_Objects;

class Item
{
public:
    Item(H264_Heap_Objects * heap, void * ptr, size_t size, bool isTyped = false)
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
    H264_Heap_Objects * m_heap;

    static Item * Allocate(H264_Heap_Objects * heap, size_t size, bool isTyped = false)
    {
        Ipp8u * ppp = new Ipp8u[size + sizeof(Item)];
        if (!ppp)
            throw h264_exception(UMC_ERR_ALLOC);
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
// H264_Heap_Objects class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class H264_Heap_Objects
{
public:

    H264_Heap_Objects()
        : m_pFirstFree(0)
    {
    }

    virtual ~H264_Heap_Objects()
    {
        Release();
    }

    Item * GetItemForAllocation(size_t size, bool typed = false)
    {
        if (!m_pFirstFree)
        {
            return 0;
        }

        if (m_pFirstFree->m_Size == size && m_pFirstFree->m_isTyped == typed)
        {
            Item * ptr = m_pFirstFree;
            m_pFirstFree = m_pFirstFree->m_pNext;
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

        Item * item = (Item *) ((Ipp8u*)obj - sizeof(Item));

        // check
        Item * temp = m_pFirstFree;

        while (temp)
        {
            if (temp == item)
            { //was removed yet
                return;
            }

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
                HeapObject * object = reinterpret_cast<HeapObject *>(item->m_Ptr);
                object->Reset();
            }
        }

        item->m_pNext = m_pFirstFree;
        m_pFirstFree = item;
    }

    void Release()
    {
        while (m_pFirstFree)
        {
            Item *pTemp = m_pFirstFree->m_pNext;
            Item::Free(m_pFirstFree);
            m_pFirstFree = pTemp;
        }
    }

private:

    Item * m_pFirstFree;
};


//*********************************************************************************************/
// H264_List implementation
//*********************************************************************************************/
template <typename T>
class H264_List
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

    H264_List(H264_Heap_Objects * pObjHeap)
        : m_pHead(0)
        , m_pObjHeap(pObjHeap)
    {
    }

    ~H264_List()
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
    H264_Heap_Objects * m_pObjHeap;
};

//*********************************************************************************************/
//
//*********************************************************************************************/
class H264CoeffsBuffer : public HeapObject
{
public:
    // Default constructor
    H264CoeffsBuffer(void);
    // Destructor
    virtual ~H264CoeffsBuffer(void);

    // Initialize buffer
    Status Init(Ipp32s numberOfItems, Ipp32s sizeOfItem);

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
    Mutex mutex;

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
private:
    H264CoeffsBuffer( const H264CoeffsBuffer &s );              // no copy CTR
    H264CoeffsBuffer & operator=(const H264CoeffsBuffer &s );
};

} // namespace UMC

#endif // __UMC_H264_HEAP_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
