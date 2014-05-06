/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

//intrusive ptr concept 
//usage examples same as smart pointers except user has to define addref and release routine for that class

//inline void intrusive_ptr_addref(UserClassA * pResource);
//inline void intrusive_ptr_release(UserClassA * pResource);

template <class T>
class mfx_intrusive_ptr
{
    T * m_pResource;
public:
    mfx_intrusive_ptr(T* pResource = NULL)
        : m_pResource(pResource) {
            intrusive_ptr_addref(m_pResource);
    }
    mfx_intrusive_ptr(const mfx_intrusive_ptr<T> & rhs) 
        : m_pResource(rhs.m_pResource) {
            intrusive_ptr_addref(m_pResource);
    }
    void reset(T* pResource) {
        if (m_pResource){
            intrusive_ptr_release(m_pResource);
        }
        m_pResource = pResource;
        intrusive_ptr_addref(m_pResource);
    }
    T* operator *() {
        return m_pResource;
    }
    T* operator ->() {
        return m_pResource;
    }
    T* get(){
        return m_pResource;
    }
    ~mfx_intrusive_ptr(){
        if (m_pResource) {
            intrusive_ptr_release(m_pResource);
        }
    }
};
