/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

struct empty_struct
{
};

template <class T>
struct destructor_counter_tmpl
    : private mfx_no_copy 
    , public T
{
    int &_nTimesDestructorCalled;
    destructor_counter_tmpl(int &nTimesDestructorCalled)
        : _nTimesDestructorCalled(nTimesDestructorCalled)
    {
    }
    static void operator delete(void * p)throw()
    {
        reinterpret_cast<destructor_counter_tmpl<T>*>(p)->_nTimesDestructorCalled++;
    }
    static void operator delete(void * p, const std::nothrow_t&)throw()
    {
        reinterpret_cast<destructor_counter_tmpl<T>*>(p)->_nTimesDestructorCalled++;
    }
    static void operator delete(void* pMem, int /*x*/, const char* /*pszFilename*/, int /*nLine*/)throw()
    {
        reinterpret_cast<destructor_counter_tmpl<T>*>(pMem)->_nTimesDestructorCalled++;
        //free(pMem);
    }
    ~destructor_counter_tmpl()
    {
    }
};

typedef destructor_counter_tmpl <empty_struct> destructor_counter;
