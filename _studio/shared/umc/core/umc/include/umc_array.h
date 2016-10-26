//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#if !defined(__UMC_ARRAY_H)
#define __UMC_ARRAY_H

#include <umc_structures.h>
#include <vm_types.h>
#include <memory.h>
#include <new>
#include <stdexcept>
#include <string> /* for std::out_of_range(const char*) on Android */

namespace UMC
{

template <class item_t>
class Array
{
public:

    // Default constructor
    Array(void)
    {
        m_pArray = (item_t *) 0;
        m_numItems = 0;
    }

    //Copy constructor
    Array(Array &src)
    {
        m_pArray = (item_t *)0;
        m_numItems = 0;
        *this = src;
    }
    // Destructor
    ~Array(void)
    {
        Release();
    }

    // Allocate the array of given size
    Status Alloc(size_t sizeNew)
    {
        if (m_numItems < sizeNew)
        {
            return Reallocate(sizeNew);
        }

        return UMC_OK;
    }

    // Advance operator
    item_t * operator + (size_t advance)
    {
        if (advance >= m_numItems)
        {
            Status umcRes;

            // allocate new array
            umcRes = Reallocate(advance + 1);
            if (UMC_OK != umcRes)
            {
                throw std::bad_alloc();
            }
        }

        return (m_pArray + advance);
    }

    // Index operator
    item_t & operator [] (size_t index)
    {
        if (index >= m_numItems)
        {
            Status umcRes;

            // allocate new array
            umcRes = Reallocate(index + 1);
            if (UMC_OK != umcRes)
            {
                throw std::bad_alloc();
            }
        }

        return m_pArray[index];
    }

    // Index operator
    const item_t & operator [] (size_t index) const
    {
        if (index >= m_numItems)
        {
            throw std::out_of_range("out of range: index >= m_numItems");
        }

        return m_pArray[index];
    }

    inline
    size_t Size(void) const
    {
        return m_numItems;
    }

    void Clean(void)
    {
        memset(m_pArray, 0, sizeof(size_t) * m_numItems);
    }

    // assignment operator
    Array & operator = (Array &src)
    {
        Status umcRes;

        if (this != &src)
        {
            size_t i;

            // check the array size
            umcRes = Reallocate(src.m_numItems);
            if (UMC_OK != umcRes)
            {
                throw std::bad_alloc();
            }

            // copy the members
            for (i = 0; i < src.m_numItems; i += 1)
            {
                m_pArray[i] = src.m_pArray[i];
            }
        }

        return *this;
    }

protected:
    // Release the object
    void Release(void)
    {
        if (m_pArray)
        {
            try
            {
                delete [] m_pArray;
            }
            catch(...)
            {
            }
        }

        m_pArray = (item_t *) 0;
        m_numItems = 0;
    }

    // reallocate the array
    Status Reallocate(size_t sizeNew)
    {
        item_t *pNewArray;
        size_t i = 0;

        // allocate the new array like the regular memory,
        try
        {
            pNewArray = new item_t[sizeNew];
        }
        catch(...)
        {
            return UMC_ERR_ALLOC;
        }

        // copy the items into the new array
        if (m_pArray)
        {
            for (i = 0; i < UMC::get_min(sizeNew, m_numItems); i += 1)
            {
                pNewArray[i] = m_pArray[i];
            }

            delete [] m_pArray;
        }

        // reset new items to zero
        memset(pNewArray + i, 0, sizeof(item_t) * (sizeNew - i));

        // set the pointer and size
        m_pArray = pNewArray;
        m_numItems = sizeNew;

        return UMC_OK;
    }

    item_t *m_pArray;                                           // (item_t *) pointer to the array
    size_t m_numItems;                                          // (int) number of items in the array
};

} // namespace UMC

#endif // __UMC_ARRAY_H
