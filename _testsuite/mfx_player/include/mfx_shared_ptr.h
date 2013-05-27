/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once

#include "vm_interlocked.h"

template<class T>
class mfx_shared_ptr
{
    T * m_obj;
    mfxU32 *m_cRefCount;

public:
    mfx_shared_ptr(T * p = NULL)
        : m_obj(p)
        , m_cRefCount (new mfxU32 (1))
    {
    }
    ~mfx_shared_ptr()
    {
        _release();
    }
    mfx_shared_ptr(const mfx_shared_ptr<T> & that)
        : m_obj(that.m_obj)
        , m_cRefCount(that.m_cRefCount)
    {
        _addref();
    }
    mfx_shared_ptr<T>& operator = (const mfx_shared_ptr<T> & that)
    {
        if (that.m_obj == m_obj)
            return *this;
        
        _release();
        m_obj = that.m_obj;
        m_cRefCount = that.m_cRefCount;
        _addref();

        return *this;
    }
    void reset(T * obj)
    {
        if (m_obj == obj)
            return;

        _release();
        m_obj = obj;
        m_cRefCount = new mfxU32(1);
    }
    //compatibility with auto_ptr behavior, just drops all binded counters from this shared pointer
    T* release()
    {
        m_cRefCount = NULL;
        T* obj = m_obj;
        m_obj = NULL;
        return obj;
    }
    operator T*()
    {
        return m_obj;
    }
    operator const T*()const
    {
        return m_obj;
    }
    T& operator *()
    {
        return *m_obj;
    }
    T* operator ->()
    {
        return m_obj;
    }
    T* get()     
    {
        return m_obj;
    }
    const T& operator *()const
    {
        return *m_obj;
    }
    const T* operator ->()const
    {
        return m_obj;
    }
    const T* get() const
    {
        return m_obj;
    }


private:

    void _release()
    {
        if (NULL == m_cRefCount)
            return;
        mfxU32 val = vm_interlocked_dec32(m_cRefCount);

        if (0 ==val)
        {
            delete m_obj;
            delete m_cRefCount;
        }
    }

    void _addref()
    {
        vm_interlocked_inc32(m_cRefCount);
    }
};

//helper frunctions for automatically wrap into shared_ptr
template<class T>
mfx_shared_ptr<typename T::SharedPtrType> mfx_make_shared()
{
    return mfx_shared_ptr<typename T::SharedPtrType>(new T());
}

template<class T, class Arg>
mfx_shared_ptr<typename T::SharedPtrType> mfx_make_shared(Arg value)
{
    return mfx_shared_ptr<typename T::SharedPtrType>(new T(value));
}

template<class T, class Arg, class Arg2>
mfx_shared_ptr<typename T::SharedPtrType> mfx_make_shared(Arg value, Arg2 value2)
{
    return mfx_shared_ptr<typename T::SharedPtrType>(new T(value, value2));
}

template<class T, class Arg, class Arg2, class Arg3>
mfx_shared_ptr<typename T::SharedPtrType> mfx_make_shared(Arg value, Arg2 value2, Arg3 value3)
{
    return mfx_shared_ptr<typename T::SharedPtrType>(new T(value, value2, value3));
}


//rationale is same as for enable proxy for this
template<class T>
class EnableSharedFromThis
{
public:
    virtual ~EnableSharedFromThis(){}
    typedef typename T SharedPtrType;
};