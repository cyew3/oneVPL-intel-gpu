//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#pragma once
#include <typeinfo>
#include "vm_interlocked.h"

class mfx_ref_count
{
private:
    volatile Ipp32u m_count;
public:
    mfx_ref_count() : m_count(0) {}
    void AddReference() { vm_interlocked_inc32(&m_count); }
    int ReleaseReference() { return vm_interlocked_dec32(&m_count); }
};

template <class T>
class mfx_shared_ptr
{
private:
    T*  m_pData;
    mfx_ref_count* m_reference; //ref count

public:
    mfx_shared_ptr()
        : m_pData(0)
        , m_reference(0)
    {
        m_reference = new mfx_ref_count();
        m_reference->AddReference();
    }

    mfx_shared_ptr(T* pValue)
        : m_pData(pValue)
        , m_reference(0)
    {
        m_reference = new mfx_ref_count();
        m_reference->AddReference();
    }

    mfx_shared_ptr(const mfx_shared_ptr<T>& sp) //copy ctor - copy the data and reference pointer, and increment the reference count
        : m_pData(sp.m_pData)
        , m_reference(sp.m_reference)
    {
        m_reference->AddReference();
    }

    void reset()
    {
        delete m_pData;
        delete m_reference;
    }

    ~mfx_shared_ptr()
    {
        if (m_reference->ReleaseReference() == 0)
        {
            this->reset();
        }
    }

    T* operator->() const { return m_pData; }
    T* get() const { return m_pData; }


    mfx_shared_ptr<T>& operator = (const mfx_shared_ptr<T>& sp)
    {
        if (this != &sp) //avoid self assignment
        {
            //decrement the old reference count
            //if reference become zero delete the old data
            if (m_reference->ReleaseReference() == 0)
            {
                delete m_pData;
                delete m_reference;
            }
            m_pData = sp.m_pData;
            m_reference = sp.m_reference;
            m_reference->AddReference();
        }
        return *this;
    }

    bool operator==(const mfx_shared_ptr<T>& r) const
    {
        return (this->get() == r.get());
    }

    bool operator==(const T* t) const
    {
        return (this->get() == t);
    }

    operator T*() const
    {
        return this->get();
    }
};

class mfx_type_index
{    // this classs is needed for using as a key for std::map, original class std::type_index is available starting from C++11, which is not available for Linux build.
public:
    mfx_type_index(const std::type_info& typeInfo)
        : m_type_info_ptr(&typeInfo)
    {    // construct from type_info
    }
    const char *name() const
    {
        return (m_type_info_ptr->name());
    }
    bool operator==(const mfx_type_index& rvalue) const
    {
        return (*m_type_info_ptr == *rvalue.m_type_info_ptr);
    }
    bool operator<(const mfx_type_index& rvalue) const
    {
        return (m_type_info_ptr->before(*rvalue.m_type_info_ptr));
    }
private:
    const std::type_info *m_type_info_ptr;
};

template <typename T>
struct mfx_remove_const
{
    typedef T type;
};

template <typename T>
struct mfx_remove_const<const T>
{
    typedef T type;
};

template <typename T>
struct mfx_remove_volatile
{
    typedef T type;
};

template <typename T>
struct mfx_remove_volatile<volatile T>
{
    typedef T type;
};

template <typename T>
struct mfx_remove_cv : mfx_remove_const<typename mfx_remove_volatile<T>::type> {};

template <typename T>
struct mfx_is_explicit_pointer
{
    enum { value = false };
};

template <typename T>
struct mfx_is_explicit_pointer<T*>
{
    enum { value = true };
};

template <typename T>
struct mfx_is_pointer_type : mfx_is_explicit_pointer<typename mfx_remove_cv<T>::type> {};

template <typename T>
bool mfx_is_pointer()
{
    return mfx_is_pointer_type<T>::value;
}