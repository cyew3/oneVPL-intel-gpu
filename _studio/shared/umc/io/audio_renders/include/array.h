// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __ARRAY_H__
#define __ARRAY_H__

#include <string.h>    //    for memmove function
#include <stdlib.h> //    for realloc/free functions

#include "vm_debug.h"
#include "umc_malloc.h"
#include "umc_dynamic_cast.h"

template <class T>
class SimpleArray
{
    DYNAMIC_CAST_DECL_BASE(SimpleArray)
public:
    T* m_pArray;
    Ipp32u m_uiSize;
    Ipp32u m_uiAllocSize;

    // Constructor  & destructor
    SimpleArray() : m_pArray(NULL), m_uiSize(0), m_uiAllocSize(0) {}

    ~SimpleArray()
    {    DeleteAll();    }

    // Main functionality
    inline Ipp32u GetSize() const
    {    return m_uiSize;    }

    bool Insert(const Ipp32u ruiIndex, T t)
    {
        VM_ASSERT(ruiIndex <= m_uiSize);

        if(m_uiSize == m_uiAllocSize)
        {
            T* pT;
            Ipp32u uiNewAllocSize = (m_uiAllocSize == 0) ? 1 : (2 * m_uiSize);
            pT = (T*)realloc(m_pArray, uiNewAllocSize * sizeof(T));
            if(pT == NULL)
            {   return false;   }
            m_uiAllocSize = uiNewAllocSize;
            m_pArray = pT;
        }
        m_uiSize++;
        if (m_uiSize - 1 > ruiIndex)
        {
            memmove((void*)&m_pArray[ruiIndex + 1],
                    (void*)&m_pArray[ruiIndex],
                    (m_uiSize - ruiIndex - 1) * sizeof(T));
        }
        SetAtIndex(ruiIndex, t);
        return true;
    }

    bool Add(T& t)
    {    return Insert(m_uiSize, t);    }

    bool Delete(T& t)
    {
        Ipp32s iIndex = Seek(t);
        if (-1 == iIndex)
        {   return false;   }
        return DeleteAt(iIndex);
    }

    bool DeleteAt(Ipp32u uiIndex)
    {
        if ((m_uiSize - 1) != uiIndex)
        {
            m_pArray[uiIndex].~T();
            memmove((void*)&m_pArray[uiIndex],
                    (void*)&m_pArray[uiIndex + 1],
                    (m_uiSize - (uiIndex + 1)) * sizeof(T));
        }
        m_uiSize--;
        return true;
    }

    void DeleteAll()
    {
        if(m_pArray != NULL)
        {
            for (Ipp32u i = 0; i < m_uiSize; i++)
            {   m_pArray[i].~T();   }
            free(m_pArray);
            m_pArray = NULL;
        }
        m_uiAllocSize = 0;
        m_uiSize = 0;
    }

    T& operator[] (Ipp32u uiIndex) const
    {
        VM_ASSERT(uiIndex < m_uiSize);
        return m_pArray[uiIndex];
    }

    T* GetArray() const
    {   return m_pArray;    }

    // Implementation
    class Wrapper
    {
    public:
        Wrapper(T& _t): t(_t) {}
        template <class _Tw>
        #undef new // To avoid redefining umc_malloc's operator new
        void* operator new(size_t, _Tw* p)
        {   return p;   }
        template <class _Tw>
        void operator delete(void* /*p*/, _Tw* /*pBuffer*/){}
        T t;
    };

    void SetAtIndex(Ipp32s nIndex, T& t)
    {
        VM_ASSERT((nIndex >= 0) && (nIndex < (Ipp32s) m_uiSize));
        new(&m_pArray[nIndex]) Wrapper(t);
    }

    Ipp32s Seek(T& t) const
    {
        for (Ipp32u i = 0; i < m_uiSize; i++)
        {
            if (m_pArray[i] == t)
            {   return i;   }
        }
        return -1;  // not found
    }
};

#endif // __ARRAY_H__
