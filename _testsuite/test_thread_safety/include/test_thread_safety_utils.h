/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: test_thread_safety_utils.h

\* ****************************************************************************** */

#ifndef __TEST_THREAD_SAFETY_UTILS_H__
#define __TEST_THREAD_SAFETY_UTILS_H__

#include "vm_file.h"
#include "mfxvideo.h"

template<typename T>
struct AutoArray
{
    explicit AutoArray(T* ptr = 0)
        : m_ptr(ptr)
    {
    }

    ~AutoArray()
    {
        delete[] m_ptr;
    }

    T& operator[](size_t i)
    {
        return m_ptr[i];
    }

    const T& operator[](size_t i) const
    {
        return m_ptr[i];
    }

private:
    AutoArray(const AutoArray&);
    const AutoArray& operator=(const AutoArray&);

    T* m_ptr;
};

struct AutoFile
{
    explicit AutoFile(vm_file* ptr)
        : m_ptr(ptr)
    {
    }

    ~AutoFile()
    {
        if (m_ptr)
        {
            vm_file_fclose(m_ptr);
        }
    }

    vm_file* Extract()
    {
        return m_ptr;
    }

private:
    AutoFile(const AutoFile&);
    const AutoFile& operator=(const AutoFile&);

    vm_file* m_ptr;
};

struct AutoArrayOfFiles : public AutoArray<vm_file *>
{
    explicit AutoArrayOfFiles(size_t num)
        : AutoArray<vm_file*>(new vm_file*[num])
        , m_num(num)
    {
        memset(&(*this)[0], 0, m_num * sizeof(vm_file*));
    }

    ~AutoArrayOfFiles()
    {
        CloseAll();
    }

    void CloseAll()
    {
        for (size_t i = 0; i < m_num; i++)
            if ((*this)[i])
                vm_file_fclose((*this)[i]);
    }

private:
    size_t m_num;
};


struct MFXSessionWrapper
{
    MFXSessionWrapper()
    : mfx(0)
    {
        MFXInit(MFX_IMPL_SOFTWARE, 0, &mfx);
    }

    ~MFXSessionWrapper()
    {
        MFXClose(mfx);
    }

    mfxSession mfx;
};

#endif //__TEST_THREAD_SAFETY_UTILS_H__
