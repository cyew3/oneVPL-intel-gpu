/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: automatic_pointer.h

\* ****************************************************************************** */
#include "assert.h"
template <class T>
class AutomaticPointer
{
public:
    // Default constructor
    AutomaticPointer(void)
    {
        m_p = (T *) 0;
    }

    // Constructor
    AutomaticPointer(T *p)
    {
        m_p = p;
    }

    // Destructor
    virtual
    ~AutomaticPointer(void)
    {
        Release();
    }

    // Assignment operator
    T * operator = (T *p)
    {
        // Release the object before setting
        Release();

        m_p = p;

        return p;
    }

    // Casting operator
    operator T * (void)
    {
        return m_p;
    }

    T * operator -> (void)
    {
        assert(NULL != m_p);
        return m_p;
    }

    //release ownership and returns pointer
    T * release (void)
    {
        T * obj = m_p;
        m_p = NULL;
        return obj;
    }

protected:
    void Release(void)
    {
        if (m_p)
        {
            delete m_p;
        }

        m_p = (T *) 0;
    }

    T *m_p;                                                     // (T *) guarded pointer
};
