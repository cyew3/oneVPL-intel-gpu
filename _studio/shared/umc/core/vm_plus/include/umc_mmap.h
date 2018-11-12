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
