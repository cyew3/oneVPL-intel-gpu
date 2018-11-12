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

#if defined(LINUX32) || defined(__APPLE__)

#if defined(__ANDROID__)
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "vm_mmap.h"

/* Set the mmap handle an invalid value */
void vm_mmap_set_invalid(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return;

    handle->fd= -1;
    handle->address = NULL;
    handle->fAccessAttr = 0;

} /* void vm_mmap_set_invalid(vm_mmap *handle) */

/* Verify if the mmap handle is valid */
int32_t vm_mmap_is_valid(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return 0;

    return (-1 != handle->fd);

} /* int32_t vm_mmap_is_valid(vm_mmap *handle) */

/* Map a file into system meory, return size of the mapped file */
unsigned long long vm_mmap_create(vm_mmap *handle, vm_char *file, int32_t fileAccessAttr)
{
    unsigned long long sizet;

    /* check error(s) */
    if (NULL == handle)
        return 0;

    handle->address = NULL;
    handle->sizet = 0;

    if(FLAG_ATTRIBUTE_READ & fileAccessAttr)
        handle->fd = open(file, O_RDONLY);
    else
        handle->fd = open(file, O_RDWR | O_CREAT,
                                S_IRUSR | S_IWUSR | // rw permissions for user
                                S_IRGRP | S_IWGRP | // rw permissions for group
                                S_IROTH); // r permissions for others

    if (-1 == handle->fd)
        return 0;

    sizet = lseek(handle->fd, 0, SEEK_END);
    lseek(handle->fd, 0, SEEK_SET);

    return sizet;

} /* unsigned long long vm_mmap_create(vm_mmap *handle, vm_char *file, int32_t fileAccessAttr) */

/* Obtain a view of the mapped file, return the adjusted offset & size */
void *vm_mmap_set_view(vm_mmap *handle, unsigned long long *offset, unsigned long long *sizet)
{
    unsigned long long pagesize = getpagesize();
    unsigned long long edge;

    /* check error(s) */
    if (NULL == handle)
        return NULL;

    if (handle->address)
        munmap(handle->address,handle->sizet);

    edge = (*sizet) + (*offset);
    (*offset) = ((unsigned long long)((*offset) / pagesize)) * pagesize;
    handle->sizet = (*sizet) = edge - (*offset);
    handle->address = mmap(0,
                           *sizet,
                           PROT_READ,
                           MAP_SHARED,
                           handle->fd,
                           *offset);

    return (handle->address == (void *)-1) ? NULL : handle[0].address;

} /* void *vm_mmap_set_view(vm_mmap *handle, unsigned long long *offset, unsigned long long *sizet) */

/* Remove the mmap */
void vm_mmap_close(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return;

    if (handle->address)
    {
        munmap(handle->address, handle->sizet);
        handle->address = NULL;
    }

    if (-1 != handle->fd)
    {
        close(handle->fd);
        handle->fd= -1;
    }
} /* void vm_mmap_close(vm_mmap *handle) */

uint32_t vm_mmap_get_page_size(void)
{
    return getpagesize();

} /* uint32_t vm_mmap_get_page_size(void) */

uint32_t vm_mmap_get_alloc_granularity(void)
{
    return 16 * getpagesize();

} /* uint32_t vm_mmap_get_alloc_granularity(void) */

void vm_mmap_unmap(vm_mmap *handle)
{
    /* check error(s) */
    if (NULL == handle)
        return;

    if (handle->address)
    {
        munmap(handle->address, handle->sizet);
        handle->address = NULL;
    }
} /* void vm_mmap_unmap(vm_mmap *handle) */
#else
# pragma warning( disable: 4206 )
#endif /* LINUX32 */
