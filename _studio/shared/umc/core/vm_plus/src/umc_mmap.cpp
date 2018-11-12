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

#include "umc_mmap.h"

namespace UMC
{

MMap::MMap(void)
    : m_handle()
    , m_address(NULL)
    , m_file_size(0)
    , m_offset(0)
    , m_sizet(0)
{
    vm_mmap_set_invalid(&m_handle);

} // MMap::MMap(void) :

MMap::~MMap(void)
{
    vm_mmap_close(&m_handle);

} // MMap::~MMap(void)

Status MMap::Init(vm_char *sz_file)
{
    vm_mmap_close(&m_handle);

    m_offset = 0;
    m_sizet = 0;
    m_address = NULL;

    m_file_size = vm_mmap_create(&m_handle, sz_file, FLAG_ATTRIBUTE_READ);

    if (0 == m_file_size)
        return UMC_ERR_FAILED;

    return UMC_OK;

} // Status MMap::Init(vm_char *sz_file)

Status MMap::Map(unsigned long long st_offset, unsigned long long st_sizet)
{
    void *pv_addr;
    unsigned long long st_align = st_offset % vm_mmap_get_alloc_granularity();

    // check error(s)
    if (!vm_mmap_is_valid(&m_handle))
        return UMC_ERR_NOT_INITIALIZED;
    if (st_offset + st_sizet > m_file_size)
        return UMC_ERR_NOT_ENOUGH_DATA;

    st_sizet += st_align;
    st_offset -= st_align;

    // set new window
    pv_addr = vm_mmap_set_view(&m_handle, &st_offset, &st_sizet);
    if (NULL == pv_addr)
    {
        m_offset = 0;
        m_sizet = 0;
        return UMC_ERR_FAILED;
    }

    // save setting(s)
    m_address = pv_addr;
    m_offset = st_offset;
    m_sizet = st_sizet;

    return UMC_OK;

} // Status MMap::Map(unsigned long long st_offset, unsigned long long st_sizet)

}   //  namespace UMC
