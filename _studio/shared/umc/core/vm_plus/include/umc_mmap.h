//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MMAP_H__
#define __UMC_MMAP_H__

#include "vm_debug.h"
#include "vm_mmap.h"
#include "umc_structures.h"

namespace UMC
{

class MMap
{
public:
    // Default constructor
    MMap(void);
    // Destructor
    virtual ~MMap(void);

    // Initialize object
    Status Init(vm_char *sz_file);
    // Map memory
    Status Map(unsigned long long st_offset, unsigned long long st_sizet);
    // Get addres of mapping
    void *GetAddr(void);
    // Get offset of mapping
    unsigned long long GetOffset(void);
    // Get size of mapping
    unsigned long long GetSize(void);
    // Get size of mapped file
    unsigned long long GetFileSize(void);

protected:
    vm_mmap m_handle;                                         // (vm_mmap) handle to system mmap object
    void *m_address;                                          // (void *) addres of mapped window
    unsigned long long m_file_size;                                       // (unsigned long long) file size
    unsigned long long m_offset;                                          // (unsigned long long) offset of mapping
    unsigned long long m_sizet;                                           // (unsigned long long) size of window
};

inline
void *MMap::GetAddr(void)
{
    return m_address;

} // void *MMap::GetAddr(void)

inline
unsigned long long MMap::GetOffset(void)
{
    return m_offset;

} // unsigned long long MMap::GetOffset(void)

inline
unsigned long long MMap::GetSize(void)
{
    return m_sizet;

} // unsigned long long MMap::GetSize(void)

inline
unsigned long long MMap::GetFileSize(void)
{
    return m_file_size;

} // unsigned long long MMap::GetFileSize(void)

} // namespace UMC

#endif // __UMC_MMAP_H__
