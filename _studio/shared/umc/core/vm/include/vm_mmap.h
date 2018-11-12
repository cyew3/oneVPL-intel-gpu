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

#ifndef __VM_MMAP_H__
#define __VM_MMAP_H__

#include "vm_types.h"
#include "vm_strings.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* flags */
enum
{
    FLAG_ATTRIBUTE_READ         = 0x00000001,
    FLAG_ATTRIBUTE_WRITE        = 0x00000002
};

/* Set the mmap handle to be invalid */
void vm_mmap_set_invalid(vm_mmap *handle);

/* Verify if the mmap handle is valid */
int32_t vm_mmap_is_valid(vm_mmap *handle);

/* Map a file into system memory, return size of the mapped file */
unsigned long long vm_mmap_create(vm_mmap *handle, vm_char *file, int32_t fileAttr);

/* Obtain a view of the mapped file, return the page aligned offset & size */
void *vm_mmap_set_view(vm_mmap *handle, unsigned long long *offset, unsigned long long *size);

/* Delete the mmap handle */
void vm_mmap_close(vm_mmap *handle);

/*  Return page size*/
uint32_t vm_mmap_get_page_size(void);

/*  Return allocation granularity*/
uint32_t vm_mmap_get_alloc_granularity(void);

/* Unmap the mmap handle */
void vm_mmap_unmap(vm_mmap *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_MMAP_H__ */
